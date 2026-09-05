//Created by Salty on 8/4/26.

#import "PPOptionsLoader.h"
#import <dirent.h>

static NSArray<NSString *> *gGlobalBlacklist = nil;

@implementation PPFangsOptions
@end

@implementation PPFangsLibraries
- (NSString *)libsForDYLD{
    return [[self libs] componentsJoinedByString:@":"];
}

+ (instancetype)fangsLibrariesWithLibraryPathArray:(NSArray<NSString *> *)pathArray{
    PPFangsLibraries *ret = [PPFangsLibraries new];
    ret.libs = pathArray;
    return ret;
}
@end

@implementation PPOptionsLoader
+ (PPFangsOptions *)loadOptions{
    PPFangsOptions *ret = [PPFangsOptions new];
    
    NSString *path = @"/opt/pluginplayground/current.options";
    NSDictionary *plist = [NSDictionary dictionaryWithContentsOfFile:path];
    
    if (!plist)
        return ret;
    
    if ([[plist objectForKey:kPPOptionsDictDisablePAC] boolValue])
        ret.disablePAC = YES;
    
    if ([[plist objectForKey:kPPOptionsDictUseLegacyAmmonia] boolValue])
        ret.useLegacyAmmonia = YES;
    
    if ([[plist objectForKey:kPPOptionsDictPauseInjection] boolValue])
        ret.pauseInjection = YES;
    
    NSArray<NSString *> *globalBlacklist =
        [plist objectForKey:kPPOptionsDictGlobalBlacklist];
    if (globalBlacklist)
        gGlobalBlacklist = globalBlacklist;
    
    return ret;
}

+ (BOOL)checkMatchToListInFile:(NSString *)filePath withProcessName:(NSString *)name{
    NSString *fileText = [[NSString alloc] initWithContentsOfFile:filePath
                                                         encoding:NSUTF8StringEncoding
                                                            error:nil];
    
    if (!fileText)
        return NO;
    
    NSArray<NSString *> *fileLines = [fileText componentsSeparatedByCharactersInSet:[NSCharacterSet newlineCharacterSet]];
    
    for (NSString *line in fileLines){
        if (line.length < 1)
            continue;
        
        if (line.UTF8String[0] == '\0' || line.UTF8String[0] == '#')
            continue;
        
        if ([line isEqualToString:name])
            return YES;
        
        if ([line isEqualToString:@"*"])
            return YES;
    }
    
    return NO;
}

+ (BOOL)shouldLoadTweakInDirectory:(NSString *)dir
                          withName:(NSString *)tweakName
                     toProcessName:(NSString *)processName{
    NSFileManager *fm = [NSFileManager defaultManager];
    processName = [processName lastPathComponent];
    
    NSString *formatWhitelist = [NSString stringWithFormat:@"%@/%@.whitelist", dir, tweakName];
    NSString *formatBlacklist = [NSString stringWithFormat:@"%@/%@.blacklist", dir, tweakName];
    
    BOOL insert = YES;
    
    if ([fm fileExistsAtPath:formatBlacklist])
        insert = [PPOptionsLoader checkMatchToListInFile:formatBlacklist
                                         withProcessName:processName] ? NO : YES;
    
    if ([fm fileExistsAtPath:formatWhitelist])
        insert = [PPOptionsLoader checkMatchToListInFile:formatWhitelist
                                         withProcessName:processName] ? YES : NO;
    
    return insert;
}

+ (BOOL)machoHasFrameworkWithBase:(const char *)base size:(size_t)size framework:(const char *)framework{
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

+ (BOOL)exeAtPath:(NSString *)path linksToFramework:(NSString *)framework{
    int fd = open(path.UTF8String, O_RDONLY);
    
    if (fd < 0)
        return NO;
    
    struct stat st;
    if (fstat(fd, &st) < 0){
        close(fd);
        return NO;
    }
    
    size_t sizeOfStat = st.st_size;
    void *memmoryMappedFd = mmap(NULL, sizeOfStat, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    
    if (memmoryMappedFd == MAP_FAILED)
        return NO;
    
    BOOL frameworkWasFound = NO;
    uint32_t magic = *(const uint32_t *)memmoryMappedFd;
    
    if (magic == FAT_MAGIC || magic == FAT_CIGAM){
        const struct fat_header *header = (const struct fat_header *)memmoryMappedFd;
        uint32_t narch = OSSwapBigToHostInt32(header->nfat_arch);
        const struct fat_arch *archs = (const struct fat_arch *)((const char*)memmoryMappedFd + sizeof(struct fat_header));
        
        for (size_t i = 0; i < narch; i++){
            uint32_t offset = OSSwapBigToHostInt32(archs[i].offset);
            
            if ([PPOptionsLoader machoHasFrameworkWithBase:memmoryMappedFd + offset
                                                      size:sizeOfStat - offset
                                                 framework:framework.UTF8String]){
                frameworkWasFound = YES;
                break;
            }
        }
    } else {
        frameworkWasFound = [PPOptionsLoader machoHasFrameworkWithBase:memmoryMappedFd
                                                                  size:sizeOfStat
                                                             framework:framework.UTF8String];
    }
    
    munmap(memmoryMappedFd, sizeOfStat);
    return frameworkWasFound;
}

+ (BOOL)checkDylibOptionsAtDirectory:(NSString *)directory withName:(NSString *)name exePath:(NSString *)exe{
    NSString *completePath = [NSString stringWithFormat:@"%@/%@.options", directory, name];
    NSURL *URL = [NSURL fileURLWithPath:completePath];
    NSDictionary *optionsDict = [NSDictionary dictionaryWithContentsOfURL:URL];
    
    if (!optionsDict)
        return NO;
    
    BOOL shouldBeLoadedToExe = YES;
    
    NSArray *frameworks = [optionsDict objectForKey:@"frameworkDependencies"];
    if (shouldBeLoadedToExe &&
        frameworks &&
        [frameworks count] > 0){
        shouldBeLoadedToExe = NO;
        for (NSString *framework in frameworks){
            if ([PPOptionsLoader exeAtPath:exe linksToFramework:framework]){
                shouldBeLoadedToExe = YES;
                break;
            }
        }
    }
    
    NSArray *blacklisted = [optionsDict objectForKey:@"blacklistedApps"] ?: [NSArray array];
    if (gGlobalBlacklist && [gGlobalBlacklist count] > 0)
        blacklisted = [blacklisted arrayByAddingObjectsFromArray:gGlobalBlacklist];
    
    if (shouldBeLoadedToExe &&
        [blacklisted count] > 0){
        NSString *lastPathComponent = [exe lastPathComponent];
        for (NSString *app in blacklisted){
            if ([lastPathComponent isEqualToString:app] || [exe isEqualToString:app]){
                shouldBeLoadedToExe = NO;
                break;
            }
            
            if ([app isEqualToString:@"*"]){
                shouldBeLoadedToExe = NO;
                break;
            }
        }
    }
    
    if (shouldBeLoadedToExe){
        NSArray<NSString *> *components = [exe pathComponents];
        for (NSString *component in components){
            if ([component isEqualToString:@"Frameworks"] || [component isEqualToString:@"PrivateFrameworks"]){
                shouldBeLoadedToExe = NO;
            } else if ([component isEqualToString:@"libexec"] || [component isEqualToString:@"sbin"]){
                shouldBeLoadedToExe = NO;
            }
        }
    }
    
    NSArray *whitelisted = [optionsDict objectForKey:@"whitelistedApps"];
    if (!shouldBeLoadedToExe &&
        whitelisted &&
        [whitelisted count] > 0){
        NSString *lastPathComponent = [exe lastPathComponent];
        for (NSString *app in whitelisted){
            if ([lastPathComponent isEqualToString:app] || [exe isEqualToString:app]){
                shouldBeLoadedToExe = YES;
                break;
            }
        }
    }
    
    return shouldBeLoadedToExe;
}

static NSArray<NSURL *> *DylibURLsInDirectory(NSString *directory)
{
    DIR *dir = opendir([directory UTF8String]);
    if (!dir)
        return nil;

    NSMutableArray<NSURL *> *result = [NSMutableArray array];

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR)
            continue;

        const char *name = entry->d_name;

        const char *ext = strrchr(name, '.');
        if (!ext || strcmp(ext, ".dylib") != 0)
            continue;

        NSString *path = [directory stringByAppendingPathComponent:
                          [NSString stringWithUTF8String:name]];

        [result addObject:[NSURL fileURLWithPath:path]];
    }

    closedir(dir);
    return result;
}

+ (PPFangsLibraries *)librariesForInsertionUsingAmmonia:(BOOL)ammonia toExePath:(const char *)path{
    NSString *dir = ammonia ? @"/private/var/ammonia/core/tweaks/" : @"/opt/pluginplayground/tweaks/";
    NSString *exePath = [NSString stringWithUTF8String:path];
    
    NSError *err = nil;
    NSArray<NSURL *> *files = DylibURLsInDirectory(dir);
    
    if (err || [files count] < 1)
        return nil;

    NSMutableArray<NSString *> *insertionRet = [NSMutableArray array];
    for (NSURL *URL in files){
        if (![[URL pathExtension] isEqualToString:@"dylib"])
            continue;

        if (ammonia && ![PPOptionsLoader shouldLoadTweakInDirectory:dir
                                                           withName:[URL lastPathComponent]
                                                      toProcessName:exePath])
            continue;
        
        if (!ammonia && ![PPOptionsLoader checkDylibOptionsAtDirectory:dir
                                                              withName:[URL lastPathComponent]
                                                                exePath:exePath])
            continue;
        
        [insertionRet addObject:[URL path]];
    }
    
    return [PPFangsLibraries fangsLibrariesWithLibraryPathArray:insertionRet];
}

@end
