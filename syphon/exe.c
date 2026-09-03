#include "exe.h"
#include "log.h"
#include <string.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach/mach.h>
#include <mach-o/dyld.h>
#include <mach/message.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/wait.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

extern const CFStringRef kSecCodeSignerIdentity;
extern const CFStringRef kSecCodeSignerEntitlements;
extern const CFStringRef kSecCodeSignerDigestAlgorithm;
typedef struct __SecCodeSigner *SecCodeSignerRef;
extern OSStatus SecCodeSignerCreate(CFDictionaryRef parameters, SecCSFlags flags, SecCodeSignerRef *signer);
extern OSStatus SecCodeSignerAddSignatureWithErrors(SecCodeSignerRef signer, SecStaticCodeRef code, SecCSFlags flags, CFErrorRef *errors);

static bool path_is_bundle(const char *path) {
    if (path == NULL) {
        return false;
    }

    return strstr(path, ".app") != NULL;
}


static void depacify(uint8_t *d, size_t s) {
    uint32_t m = *(uint32_t*)d;
    if (m == MH_MAGIC_64) {
        struct mach_header_64 *h = (struct mach_header_64*)d;
        if (h->cputype == CPU_TYPE_ARM64 && (h->cpusubtype & 0xff) == 2) {
            h->cpusubtype = 0;
            // struct load_command *lc = (struct load_command*)(d + sizeof(*h));
            // for (uint32_t i=0; i<h->ncmds; i++) {
            //     if (lc->cmd == LC_SEGMENT_64) {
            //         struct segment_command_64 *seg = (struct segment_command_64*)lc;
            //         if (seg->initprot & VM_PROT_EXECUTE) {
            //             uintptr_t addr = (uintptr_t)d + seg->fileoff;
            //             for (size_t j=0; j < seg->filesize - 3; j += 4) {
            //                 uint32_t *p = (uint32_t*)(addr + j);
            //                 if ((*p & 0xfffff000) == 0xd5032000) *p = 0xd503201f;
            //             }
            //         }
            //     }
            //     lc = (struct load_command*)((uint8_t*)lc + lc->cmdsize);
            // }
        }
    } else if (m == FAT_MAGIC || m == FAT_CIGAM) {
        struct fat_header *fh = (struct fat_header*)d;
        uint32_t n = (m == FAT_CIGAM) ? __builtin_bswap32(fh->nfat_arch) : fh->nfat_arch;
        struct fat_arch *as = (struct fat_arch*)(d + 8);
        for (uint32_t i=0; i<n; i++) {
            uint32_t off = (m == FAT_CIGAM) ? __builtin_bswap32(as[i].offset) : as[i].offset;
            uint32_t t = (m == FAT_CIGAM) ? __builtin_bswap32(as[i].cputype) : as[i].cputype;
            uint32_t sbt = (m == FAT_CIGAM) ? __builtin_bswap32(as[i].cpusubtype) : as[i].cpusubtype;
            if (t == CPU_TYPE_ARM64 && (sbt & 0xff) == 2) {
                depacify(d + off, s - off);
                as[i].cpusubtype = (m == FAT_CIGAM) ? __builtin_bswap32(0) : 0;
            }
        }
    }
}

static bool strip_code_signature_thin(uint8_t *data, size_t size) {
    if (*(uint32_t *)data != MH_MAGIC_64)
        return true;

    struct mach_header_64 *header = (struct mach_header_64 *)data;
    uint8_t *src = (uint8_t *)(data + sizeof(*header));
    uint8_t *dst = src;
    uint32_t new_ncmds = 0;
    uint32_t new_sizeofcmds = 0;
    uint32_t freed = 0;

    for (uint32_t i = 0; i < header->ncmds; i++) {
        struct load_command *lc = (struct load_command *)src;
        uint32_t cmdsize = lc->cmdsize;

        if (lc->cmd == LC_CODE_SIGNATURE) {
            freed += cmdsize;
        } else {
            if (dst != src)
                memmove(dst, src, cmdsize);
            dst += cmdsize;
            new_ncmds++;
            new_sizeofcmds += cmdsize;
        }
        src += cmdsize;
    }

    if (freed > 0) {
        memset(data + sizeof(*header) + new_sizeofcmds, 0, freed);
        header->ncmds = new_ncmds;
        header->sizeofcmds = new_sizeofcmds;
    }

    (void)size;
    return true;
}

static bool strip_code_signature_file(const char *path) {
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        return false;
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return false;
    }

    uint8_t *data = (uint8_t *)mmap(NULL, (size_t)st.st_size,
                                    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return false;
    }

    bool ok = true;
    uint32_t magic = *(uint32_t *)data;
    if (magic == MH_MAGIC_64) {
        ok = strip_code_signature_thin(data, (size_t)st.st_size);
    } else if (magic == FAT_MAGIC || magic == FAT_CIGAM) {
        struct fat_header *fh = (struct fat_header *)data;
        uint32_t nfat = (magic == FAT_CIGAM) ? __builtin_bswap32(fh->nfat_arch) : fh->nfat_arch;
        struct fat_arch *arches = (struct fat_arch *)(data + sizeof(*fh));
        for (uint32_t i = 0; i < nfat && ok; i++) {
            uint32_t offset = (magic == FAT_CIGAM) ? __builtin_bswap32(arches[i].offset) : arches[i].offset;
            if (*(uint32_t *)(data + offset) == MH_MAGIC_64) {
                ok = strip_code_signature_thin(data + offset, (size_t)st.st_size - offset);
            }
        }
    }

    if (msync(data, (size_t)st.st_size, MS_SYNC) != 0) {
        ok = false;
    }

    munmap(data, (size_t)st.st_size);
    close(fd);
    return ok;
}

static bool sign_file(const char *path, CFDataRef entitlements_blob) {
    CFMutableDictionaryRef params = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (!params) {
        return false;
    }

    CFDictionaryAddValue(params, kSecCodeSignerIdentity, kCFNull);

    if (entitlements_blob) {
        CFDictionaryAddValue(params, kSecCodeSignerEntitlements, entitlements_blob);
    }

    int digest_value = kSecCodeSignatureHashSHA256;
    CFNumberRef digest_number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &digest_value);
    if (!digest_number) {
        CFRelease(params);
        return false;
    }
    const void *digests[] = { digest_number };
    CFArrayRef digest_array = CFArrayCreate(kCFAllocatorDefault, digests, 1, &kCFTypeArrayCallBacks);
    CFRelease(digest_number);
    if (!digest_array) {
        CFRelease(params);
        return false;
    }
    CFDictionaryAddValue(params, kSecCodeSignerDigestAlgorithm, digest_array);
    CFRelease(digest_array);

    SecCodeSignerRef signer = NULL;
    OSStatus status = SecCodeSignerCreate(params, kSecCSDefaultFlags, &signer);
    CFRelease(params);
    if (status != errSecSuccess) {
        return false;
    }

    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        kCFAllocatorDefault,
        (const UInt8 *)path,
        strlen(path),
        false);
    if (!url) {
        CFRelease(signer);
        return false;
    }

    SecStaticCodeRef static_code = NULL;
    status = SecStaticCodeCreateWithPath(url, kSecCSDefaultFlags, &static_code);
    CFRelease(url);
    if (status != errSecSuccess) {
        CFRelease(signer);
        return false;
    }

    CFErrorRef error = NULL;
    status = SecCodeSignerAddSignatureWithErrors(signer, static_code, kSecCSDefaultFlags, &error);
    CFRelease(signer);
    CFRelease(static_code);
    if (error) CFRelease(error);

    return status == errSecSuccess;
}

static bool depacify_file_in_place(const char *file_path) {
    int fd = open(file_path, O_RDWR);
    if (fd < 0) {
        return false;
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return false;
    }

    size_t size = (size_t)st.st_size;
    uint8_t *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return false;
    }

    depacify(data, size);

    if (msync(data, size, MS_SYNC) != 0) {
        munmap(data, size);
        close(fd);
        return false;
    }

    if (fsync(fd) != 0) {
        munmap(data, size);
        close(fd);
        return false;
    }

    munmap(data, size);
    close(fd);
    return true;
}

static bool get_bundle_executable_path(const char *bundle_path, char *exec_path, size_t exec_path_size) {
    CFURLRef bundle_url = NULL;
    CFBundleRef bundle = NULL;
    CFURLRef exec_url = NULL;
    bool result = false;

    bundle_url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
                                                         (const UInt8 *) bundle_path,
                                                         (CFIndex) strlen(bundle_path),
                                                         true);
    if (bundle_url == NULL) {
        goto out;
    }

    bundle = CFBundleCreate(kCFAllocatorDefault, bundle_url);
    if (bundle == NULL) {
        goto out;
    }

    exec_url = CFBundleCopyExecutableURL(bundle);
    if (exec_url == NULL) {
        goto out;
    }

    if (!CFURLGetFileSystemRepresentation(exec_url, true, (UInt8 *)exec_path, (CFIndex)exec_path_size)) {
        goto out;
    }

    result = true;

out:
    if (exec_url) CFRelease(exec_url);
    if (bundle) CFRelease(bundle);
    if (bundle_url) CFRelease(bundle_url);
    return result;
}

static bool copy_dir_recursive(const char *src_path, const char *dst_path);

static bool copy_bundle_to_tmp(const char *bundle_path, char *tmp_path, size_t tmp_path_size) {
    char template_path[PATH_MAX];
    snprintf(template_path, sizeof(template_path), "/tmp/applaunch-XXXXXX");

    char *tmp_dir = mkdtemp(template_path);
    if (!tmp_dir) {
        log_error("[copy_bundle] mkdtemp failed: %s", strerror(errno));
        return false;
    }

    char bundle_name[PATH_MAX];
    const char *base = strrchr(bundle_path, '/');
    if (base) {
        snprintf(bundle_name, sizeof(bundle_name), "%s", base + 1);
    } else {
        snprintf(bundle_name, sizeof(bundle_name), "%s", bundle_path);
    }

    char dst_bundle_path[PATH_MAX];
    snprintf(dst_bundle_path, sizeof(dst_bundle_path), "%s/%s", tmp_dir, bundle_name);

    if (!copy_dir_recursive(bundle_path, dst_bundle_path)) {
        log_error("[copy_bundle] copy_dir_recursive failed: src=%s dst=%s", bundle_path, dst_bundle_path);
        rmdir(tmp_dir);
        return false;
    }

    if (!get_bundle_executable_path(dst_bundle_path, tmp_path, tmp_path_size)) {
        log_error("[copy_bundle] get_bundle_executable_path failed for: %s", dst_bundle_path);
        rmdir(tmp_dir);
        return false;
    }

    return true;
}

typedef struct DirPair {
    char *src;
    char *dst;
    struct DirPair *next;
} DirPair;

static bool copy_dir_iterative(const char *root_src, const char *root_dst) {
    DirPair *stack = NULL;
    bool ok = true;

    DirPair *first = malloc(sizeof(DirPair));
    if (!first) return false;
    first->src  = strdup(root_src);
    first->dst  = strdup(root_dst);
    first->next = NULL;
    if (!first->src || !first->dst) {
        free(first->src); free(first->dst); free(first);
        return false;
    }
    stack = first;

    while (stack && ok) {
        // pop
        DirPair *cur = stack;
        stack = stack->next;

        if (mkdir(cur->dst, 0755) != 0) {
            log_error("[copy_dir] mkdir failed: %s (%s)", cur->dst, strerror(errno));
            ok = false;
            free(cur->src); free(cur->dst); free(cur);
            break;
        }

        DIR *d = opendir(cur->src);
        if (!d) {
            log_error("[copy_dir] opendir failed: %s (%s)", cur->src, strerror(errno));
            ok = false;
            free(cur->src); free(cur->dst); free(cur);
            break;
        }

        struct dirent *entry;
        while ((entry = readdir(d)) != NULL && ok) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            // build paths on heap to avoid large stack buffers
            char *src_sub = NULL;
            char *dst_sub = NULL;
            if (asprintf(&src_sub, "%s/%s", cur->src, entry->d_name) < 0 ||
                asprintf(&dst_sub, "%s/%s", cur->dst, entry->d_name) < 0) {
                free(src_sub); free(dst_sub);
                ok = false;
                break;
            }

            struct stat st;
            if (lstat(src_sub, &st) != 0) {
                log_error("[copy_dir] lstat failed: %s (%s)", src_sub, strerror(errno));
                free(src_sub); free(dst_sub);
                ok = false;
                break;
            }

            if (S_ISDIR(st.st_mode)) {
                // push onto stack
                DirPair *pair = malloc(sizeof(DirPair));
                if (!pair) { free(src_sub); free(dst_sub); ok = false; break; }
                pair->src  = src_sub;
                pair->dst  = dst_sub;
                pair->next = stack;
                stack = pair;
            } else if (S_ISREG(st.st_mode)) {
                int src_fd = open(src_sub, O_RDONLY);
                if (src_fd < 0) { free(src_sub); free(dst_sub); ok = false; break; }

                int dst_fd = open(dst_sub, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
                if (dst_fd < 0) {
                    close(src_fd); free(src_sub); free(dst_sub); ok = false; break;
                }

                char *buf = malloc(65536);
                if (!buf) {
                    close(src_fd); close(dst_fd); free(src_sub); free(dst_sub); ok = false; break;
                }
                while (ok) {
                    ssize_t nr = read(src_fd, buf, 65536);
                    if (nr == 0) break;
                    if (nr < 0) { ok = false; break; }
                    ssize_t written = 0;
                    while (written < nr) {
                        ssize_t nw = write(dst_fd, buf + written, (size_t)(nr - written));
                        if (nw < 0) { ok = false; break; }
                        written += nw;
                    }
                }
                free(buf);
                close(src_fd);
                close(dst_fd);
                free(src_sub); free(dst_sub);
            } else if (S_ISLNK(st.st_mode)) {
                char *link_buf = malloc(PATH_MAX);
                if (!link_buf) { free(src_sub); free(dst_sub); ok = false; break; }
                ssize_t len = readlink(src_sub, link_buf, PATH_MAX - 1);
                if (len < 0) {
                    free(link_buf); free(src_sub); free(dst_sub); ok = false; break;
                }
                link_buf[len] = '\0';
                if (symlink(link_buf, dst_sub) != 0) {
                    free(link_buf); free(src_sub); free(dst_sub); ok = false; break;
                }
                free(link_buf);
                free(src_sub); free(dst_sub);
            } else {
                free(src_sub); free(dst_sub);
            }
        }

        closedir(d);
        free(cur->src); free(cur->dst); free(cur);
    }

    // drain remaining stack entries on early failure
    while (stack) {
        DirPair *tmp = stack;
        stack = stack->next;
        free(tmp->src); free(tmp->dst); free(tmp);
    }

    return ok;
}

static bool copy_dir_recursive(const char *src_path, const char *dst_path) {
    return copy_dir_iterative(src_path, dst_path);
}

static bool resign_bundle(const char *bundle_path) {
    char exec_path[PATH_MAX];
    if (!get_bundle_executable_path(bundle_path, exec_path, sizeof(exec_path))) {
        return false;
    }
    if (!strip_code_signature_file(exec_path)) {
        return false;
    }
    return sign_file(exec_path, NULL);
}

char *getready_process(const char *path) {
    const char *spawn_path = path;
    char bundle_exec_tmp[PATH_MAX];
    char bundle_root[PATH_MAX];

    if (path_is_bundle(path)) {
        snprintf(bundle_root, sizeof(bundle_root), "%s", path);
        char *app_ext = strstr(bundle_root, ".app");
        if (app_ext) {
            app_ext[4] = '\0';
        }

        const char *bundle_name = strrchr(bundle_root, '/');
        if (bundle_name) {
            bundle_name++;
        } else {
            bundle_name = bundle_root;
        }

        char runtime_apps_dir[PATH_MAX];
        char dst_bundle_path[PATH_MAX];
        snprintf(runtime_apps_dir, sizeof(runtime_apps_dir), "/tmp/RuntimeApplications");
        snprintf(dst_bundle_path, sizeof(dst_bundle_path), "%s/%s", runtime_apps_dir, bundle_name);

        log_info("[bootstrap] processing bundle: %s", bundle_root);

        struct stat st;
        if (stat(dst_bundle_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            log_info("[bootstrap] bundle already exists at: %s", dst_bundle_path);
            if (get_bundle_executable_path(dst_bundle_path, bundle_exec_tmp, sizeof(bundle_exec_tmp))) {
                return strdup(bundle_exec_tmp);
            }
        }

        if (mkdir(runtime_apps_dir, 0755) != 0 && errno != EEXIST) {
            log_error("[bootstrap] failed to create RuntimeApplications dir: %s", strerror(errno));
            return strdup(spawn_path);
        }

        log_info("[bootstrap] copying bundle to: %s", dst_bundle_path);
        if (copy_dir_recursive(bundle_root, dst_bundle_path)) {
            if (get_bundle_executable_path(dst_bundle_path, bundle_exec_tmp, sizeof(bundle_exec_tmp))) {
                log_info("[bootstrap] depacifying executable: %s", bundle_exec_tmp);
                if (depacify_file_in_place(bundle_exec_tmp)) {
                    log_info("[bootstrap] resigning bundle: %s", dst_bundle_path);
                    if (resign_bundle(dst_bundle_path)) {
                        log_info("[bootstrap] using depacified bundle");
                    } else {
                        log_error("Warning: failed to resign bundle, continuing anyway");
                    }
                    return strdup(bundle_exec_tmp);
                } else {
                    log_error("Warning: failed to depacify bundle executable");
                }
            } else {
                log_error("[bootstrap] failed to get executable path for: %s", dst_bundle_path);
            }
        } else {
            log_error("[bootstrap] failed to copy bundle to: %s", dst_bundle_path);
        }
    }
    return strdup(spawn_path);
}
