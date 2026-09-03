//Created by Salty on 8/4/26.

#import <Foundation/Foundation.h>
#import <Swingset/Swingset.h>
#import <mach-o/dyld.h>
#import <mach-o/fat.h>
#import <sys/stat.h>

@interface PPFangsOptions : NSObject
@property (assign) BOOL disablePAC;
@property (assign) BOOL useLegacyAmmonia;
@property (assign) BOOL pauseInjection;
@end

@interface PPFangsLibraries : NSObject
+ (instancetype)fangsLibrariesWithLibraryPathArray:(NSArray<NSString *> *)pathArray;
@property (strong) NSArray<NSString *> *libs;
@property (strong, readonly) NSString *libsForDYLD;
@end

@interface PPOptionsLoader : NSObject
+ (PPFangsOptions *)loadOptions;
+ (PPFangsLibraries *)librariesForInsertionUsingAmmonia:(BOOL)ammonia toExePath:(const char *)path;
@end
