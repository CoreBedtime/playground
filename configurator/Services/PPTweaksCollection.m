//Created by Salty on 8/26/26.

#import "PPTweaksCollection.h"

@implementation PPTweaksCollection
@synthesize isForLegacyAmmonia = _isForLegacyAmmonia;

+ (instancetype)loadCurrentTweaksContentsUsingAmmonia:(BOOL)ammonia{
    PPTweaksCollection *ret = [PPTweaksCollection new];
    [ret setIsForLegacyAmmonia:ammonia];
    return ret;
}

- (void)setIsForLegacyAmmonia:(BOOL)isForLegacyAmmonia{
    _isForLegacyAmmonia = isForLegacyAmmonia;
    
    NSString *dir = isForLegacyAmmonia ? kPPTweaksDirectoryAmmonia : kPPTweaksDirectoryPluginPlayground;
    SOPPObservableDictionary *dict = [[SOPPObservableDictionary alloc] initWithDelegate:self];
    [self setCollectionDictionary:dict];
    [dict setIdentifier:@"TweaksAndStatus"];
    
    NSURL *URL = [NSURL fileURLWithPath:(NSString *)dir
                            isDirectory:YES];
    
    if (!URL)
        return;
    
    [self reloadWithDirURL:URL];
}

- (BOOL)isForLegacyAmmonia{
    return _isForLegacyAmmonia;
}

- (void)reloadWithDirURL:(NSURL *)URL{
    if (!self.collectionDictionary)
        return;
    
    NSFileManager *fm = [NSFileManager defaultManager];
    NSArray<NSURL *> *contentsOfDir = [fm contentsOfDirectoryAtURL:URL
                                        includingPropertiesForKeys:nil
                                                           options:NSDirectoryEnumerationSkipsHiddenFiles
                                                             error:nil];
    
    for (NSURL *itemURL in contentsOfDir){
        if ([[itemURL pathExtension] isEqualToString:@"options"])
            [self.collectionDictionary setObject:@YES forKey:[itemURL path]];
        
        if ([[itemURL pathExtension] isEqualToString:@"disabled"] && [[itemURL path] containsString:@"options"])
            [self.collectionDictionary setObject:@NO forKey:[itemURL path]];
    }
}

- (void)disableTweakAtPath:(NSString *)tweakPath{
    BOOL isEnabled = [[self.collectionDictionary objectForKey:tweakPath] boolValue];
    
    if (!isEnabled)
        return;
    
    [self.collectionDictionary removeObjectForKey:tweakPath];
    NSString *newPath = [tweakPath stringByAppendingPathExtension:@"disabled"];
    [self.collectionDictionary setObject:@NO forKey:newPath];
    
    NSFileManager *fm = [NSFileManager defaultManager];
    [fm moveItemAtPath:tweakPath
                toPath:newPath
                 error:nil];
}

- (void)enableTweakAtPath:(NSString *)tweakPath{
    BOOL isEnabled = [[self.collectionDictionary objectForKey:tweakPath] boolValue];
    
    if (isEnabled)
        return;
    
    [self.collectionDictionary removeObjectForKey:tweakPath];
    NSString *newPath = [tweakPath stringByDeletingPathExtension];
    [self.collectionDictionary setObject:@YES forKey:newPath];
    
    NSFileManager *fm = [NSFileManager defaultManager];
    [fm moveItemAtPath:tweakPath
                toPath:newPath
                 error:nil];
}

- (void)packageTweakAtPath:(NSString *)tweakPath {
    NSString *lastCom = [tweakPath lastPathComponent];
    NSString *name = [self stringByDeletingPathExtensionRecursivelyWithOriginal:lastCom];
    NSString *pkgName = [name stringByAppendingPathExtension:@"pkg"];
    NSString *realDylibName = [name stringByAppendingPathExtension:@"dylib"];
    NSString *staging = [NSString stringWithFormat:@"/tmp/plugintweak_%@", name];

    NSFileManager *fm = [NSFileManager defaultManager];

    NSString *tweakDirectory = [staging stringByAppendingPathComponent:@"opt/pluginplayground/tweaks"];

    [fm createDirectoryAtPath:tweakDirectory
   withIntermediateDirectories:YES
                    attributes:nil
                         error:nil];

    NSURL *dylibURL = [NSURL fileURLWithPath:
        [kPPTweaksDirectoryPluginPlayground stringByAppendingPathComponent:realDylibName]];

    NSURL *dylibCopyTo = [NSURL fileURLWithPath:
        [tweakDirectory stringByAppendingPathComponent:realDylibName]];

    NSURL *optionURL = [NSURL fileURLWithPath:
        [kPPTweaksDirectoryPluginPlayground stringByAppendingPathComponent:lastCom]];

    NSURL *optionCopyTo = [NSURL fileURLWithPath:
        [tweakDirectory stringByAppendingPathComponent:[name stringByAppendingString:@".dylib.options"]]];

    [fm copyItemAtURL:dylibURL
                toURL:dylibCopyTo
                error:nil];

    [fm copyItemAtURL:optionURL
                toURL:optionCopyTo
                error:nil];

    NSString *pkgPath = [@"/tmp" stringByAppendingPathComponent:pkgName];

    NSString *command = [NSString stringWithFormat:
        @"/usr/bin/pkgbuild --root '%@' "
         "--identifier 'com.pluginplayground.tweak.%@' "
         "--version 1.0.0 "
         "--install-location '/opt/pluginplayground/tweaks/' "
         "'%@' 2>/dev/null",
        staging,
        name,
        pkgPath];

    int result = system([command UTF8String]);

    if (result != 0) {
        [fm removeItemAtPath:staging error:nil];
        return;
    }

    NSString *parentDirectory = [kPPTweaksDirectoryPluginPlayground stringByDeletingLastPathComponent];
    NSString *destination = [parentDirectory stringByAppendingPathComponent:pkgName];

    [fm removeItemAtPath:destination error:nil];

    [fm moveItemAtPath:pkgPath
                toPath:destination
                 error:nil];

    [fm removeItemAtPath:staging error:nil];
}

- (NSString *)stringByDeletingPathExtensionRecursivelyWithOriginal:(NSString *)original{
    NSString *ret = [original stringByDeletingPathExtension];
    
    if (![[ret pathExtension] isEqualToString:@""])
        ret = [self stringByDeletingPathExtensionRecursivelyWithOriginal:ret];
    
    return ret;
}

- (void)dictionary:(SOPPObservableDictionary *)dict objectWithKey:(id)aKey didGetSetTo:(id)anObject {
}

- (void)dictionary:(SOPPObservableDictionary *)dict willRemoveObject:(id)anObject forKey:(id)aKey {
}

@end
