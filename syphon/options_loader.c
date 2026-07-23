#include "options_loader.h"
#include <CoreFoundation/CoreFoundation.h>
#include <dirent.h>
#include <fcntl.h>
#include <mach-o/dyld.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syslimits.h>
#include <unistd.h>

FangsOptions fangs_load_options(void) {
    FangsOptions opts = {false, false, false};
    const char* path = "/opt/pluginplayground/current.options";

    FILE* f = fopen(path, "rb");
    if (!f) return opts;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = malloc((size_t)len);
    if (!buf) { fclose(f); return opts; }
    fread(buf, 1, (size_t)len, f);
    fclose(f);

    CFDataRef cfData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const UInt8*)buf, (CFIndex)len, kCFAllocatorNull);
    if (!cfData) { free(buf); return opts; }

    CFPropertyListRef plist = CFPropertyListCreateWithData(kCFAllocatorDefault, cfData, kCFPropertyListImmutable, NULL, NULL);
    CFRelease(cfData);
    free(buf);

    if (!plist || CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
        if (plist) CFRelease(plist);
        return opts;
    }

    CFDictionaryRef dict = (CFDictionaryRef)plist;

    CFBooleanRef val;
    val = (CFBooleanRef)CFDictionaryGetValue(dict, CFSTR("disablePAC"));
    if (val && CFGetTypeID(val) == CFBooleanGetTypeID())
        opts.disablePAC = (bool)CFBooleanGetValue(val);

    val = (CFBooleanRef)CFDictionaryGetValue(dict, CFSTR("useLegacyAmmonia"));
    if (val && CFGetTypeID(val) == CFBooleanGetTypeID())
        opts.useLegacyAmmonia = (bool)CFBooleanGetValue(val);

    val = (CFBooleanRef)CFDictionaryGetValue(dict, CFSTR("pauseInjection"));
    if (val && CFGetTypeID(val) == CFBooleanGetTypeID())
        opts.pauseInjection = (bool)CFBooleanGetValue(val);

    CFRelease(dict);
    return opts;
}

static char* get_exe_path(void) {
    uint32_t bufsize = 0;
    _NSGetExecutablePath(NULL, &bufsize);
    char* path = malloc(bufsize);
    _NSGetExecutablePath(path, &bufsize);
    return path;
}

static bool check_list_match(const char* path, const char* exe) {
    FILE* f = fopen(path, "r");
    if (!f) return false;

    char line[256];
    bool matched = false;
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';

        char* p = line;
        while (*p == ' ' || *p == '\t') p++;   /* skip leading whitespace */
        if (*p == '\0' || *p == '#') continue; /* blank line or comment   */

        if (strstr(exe, p)) { matched = true; break; }
    }
    fclose(f);
    return matched;
}

static bool should_load_tweak(const char* dir, const char* name, const char* exe) {
    char wl[PATH_MAX], bl[PATH_MAX];
    snprintf(wl, sizeof(wl), "%s/%s.whitelist", dir, name);
    snprintf(bl, sizeof(bl), "%s/%s.blacklist", dir, name);

    if (access(wl, F_OK) == 0)
        return check_list_match(wl, exe) ? true : false;

    if (access(bl, F_OK) == 0)
        return check_list_match(bl, exe) ? false : true;

    return true;
}

static bool macho_has_framework(const char* base, size_t size, const char* framework) {
    uint32_t magic = *(const uint32_t*)base;

    uint32_t ncmds;
    const struct load_command* cmds;

    if (magic == MH_MAGIC_64) {
        const struct mach_header_64* mh = (const struct mach_header_64*)base;
        ncmds = mh->ncmds;
        cmds = (const struct load_command*)(base + sizeof(struct mach_header_64));
    } else if (magic == MH_MAGIC) {
        const struct mach_header* mh = (const struct mach_header*)base;
        ncmds = mh->ncmds;
        cmds = (const struct load_command*)(base + sizeof(struct mach_header));
    } else {
        return false;
    }

    char pattern[PATH_MAX];
    snprintf(pattern, sizeof(pattern), "/%s.framework/", framework);
    size_t plen = strlen(pattern);

    const struct load_command* cursor = cmds;
    for (uint32_t i = 0; i < ncmds; i++) {
        if (cursor->cmd == LC_LOAD_DYLIB || cursor->cmd == LC_LOAD_WEAK_DYLIB) {
            const struct dylib_command* dc = (const struct dylib_command*)cursor;
            const char* path = (const char*)cursor + dc->dylib.name.offset;
            if (strstr(path, pattern)) return true;
        }
        cursor = (const struct load_command*)((const char*)cursor + cursor->cmdsize);
    }
    return false;
}

static bool exe_links_to_framework(const char* exe_path, const char* framework) {
    int fd = open(exe_path, O_RDONLY);
    if (fd < 0) return false;

    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); return false; }

    size_t size = (size_t)st.st_size;
    void* mapped = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (mapped == MAP_FAILED) return false;

    bool found = false;
    uint32_t magic = *(const uint32_t*)mapped;

    if (magic == FAT_MAGIC || magic == FAT_CIGAM) {
        const struct fat_header* fh = (const struct fat_header*)mapped;
        uint32_t narch = OSSwapBigToHostInt32(fh->nfat_arch);
        const struct fat_arch* archs = (const struct fat_arch*)((const char*)mapped + sizeof(struct fat_header));
        for (uint32_t i = 0; i < narch; i++) {
            uint32_t offset = OSSwapBigToHostInt32(archs[i].offset);
            if (macho_has_framework((const char*)mapped + offset, size - offset, framework)) {
                found = true;
                break;
            }
        }
    } else {
        found = macho_has_framework((const char*)mapped, size, framework);
    }

    munmap(mapped, size);
    return found;
}

static bool check_dylib_options(const char* dir, const char* name, const char* exe) {
    char optpath[PATH_MAX];
    snprintf(optpath, sizeof(optpath), "%s/%s.options", dir, name);

    FILE* f = fopen(optpath, "rb");
    if (!f) return true;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = malloc((size_t)len);
    fread(buf, 1, (size_t)len, f);
    fclose(f);

    CFDataRef cfData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const UInt8*)buf, (CFIndex)len, kCFAllocatorNull);
    if (!cfData) { free(buf); return true; }

    CFPropertyListRef plist = CFPropertyListCreateWithData(kCFAllocatorDefault, cfData, kCFPropertyListImmutable, NULL, NULL);
    CFRelease(cfData);
    free(buf);

    if (!plist || CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
        if (plist) CFRelease(plist);
        return true;
    }

    CFDictionaryRef dict = (CFDictionaryRef)plist;
    bool should_load = true;

    CFArrayRef frameworks = (CFArrayRef)CFDictionaryGetValue(dict, CFSTR("frameworkDependencies"));
    if (should_load && frameworks && CFGetTypeID(frameworks) == CFArrayGetTypeID() && CFArrayGetCount(frameworks) > 0) {
        should_load = false;
        CFIndex count = CFArrayGetCount(frameworks);
        for (CFIndex i = 0; i < count; i++) {
            CFStringRef str = (CFStringRef)CFArrayGetValueAtIndex(frameworks, i);
            if (str && CFGetTypeID(str) == CFStringGetTypeID()) {
                char fname[256];
                CFStringGetCString(str, fname, sizeof(fname), kCFStringEncodingUTF8);
                if (exe_links_to_framework(exe, fname)) {
                    should_load = true;
                    break;
                }
            }
        }
    }

    CFArrayRef blacklisted = (CFArrayRef)CFDictionaryGetValue(dict, CFSTR("blacklistedApps"));
    if (should_load && blacklisted && CFGetTypeID(blacklisted) == CFArrayGetTypeID() && CFArrayGetCount(blacklisted) > 0) {
        const char* base = strrchr(exe, '/');
        base = base ? base + 1 : exe;
        CFIndex count = CFArrayGetCount(blacklisted);
        for (CFIndex i = 0; i < count; i++) {
            CFStringRef str = (CFStringRef)CFArrayGetValueAtIndex(blacklisted, i);
            if (str && CFGetTypeID(str) == CFStringGetTypeID()) {
                char appname[256];
                CFStringGetCString(str, appname, sizeof(appname), kCFStringEncodingUTF8);
                if (strstr(base, appname) || strstr(exe, appname)) {
                    should_load = false;
                    break;
                }
            }
        }
    }

    CFRelease(dict);
    return should_load;
}

char* fangs_build_dyld_insert_libraries(bool useLegacyAmmonia, char* path) {
    const char* dir = useLegacyAmmonia ? "/private/var/ammonia/core/tweaks" : "/opt/pluginplayground/tweaks";

    char* exe_path = get_exe_path();

    DIR* d = opendir(dir);
    if (!d) { free(exe_path); return NULL; }

    size_t total = 0;
    int count = 0;
    char** paths = NULL;

    struct dirent* entry;
    while ((entry = readdir(d)) != NULL) {
        const char* name = entry->d_name;
        size_t nlen = strlen(name);
        if (nlen <= 6 || strcmp(name + nlen - 6, ".dylib") != 0) continue;

        if (!should_load_tweak(dir, name, path))
            continue;

        if (!useLegacyAmmonia && !check_dylib_options(dir, name, path))
            continue;

        size_t plen = strlen(dir) + 1 + nlen + 1;
        char* full = malloc(plen);
        snprintf(full, plen, "%s/%s", dir, name);
        total += plen;
        count++;
        paths = realloc(paths, sizeof(char*) * (size_t)count);
        paths[count - 1] = full;
    }
    closedir(d);
    free(exe_path);

    if (count == 0) {
        free(paths);
        return NULL;
    }

    size_t needed = total + 1;
    char* result = malloc(needed);
    if (!result) {
        for (int i = 0; i < count; i++) free(paths[i]);
        free(paths);
        return NULL;
    }
    result[0] = '\0';
    for (int i = 0; i < count; i++) {
        if (i > 0) strcat(result, ":");
        strcat(result, paths[i]);
        free(paths[i]);
    }
    free(paths);
    return result;
}
