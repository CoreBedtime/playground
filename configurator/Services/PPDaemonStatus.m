//Created by Salty on 9/2/26.

#import "PPDaemonStatus.h"
#import <libproc.h>

NSString* const plistPath = @"/Library/LaunchDaemons/com.pluginplayground.grant.plist";

@implementation PPDaemonStatus
+ (BOOL)fangsIsRunning {
    NSTask *checkTask = [[NSTask alloc] init];
    [checkTask setArguments:@[@"-x", @"fangs"]];
    [checkTask setLaunchPath:@"/usr/bin/pgrep"];

    [checkTask setStandardOutput:[NSFileHandle fileHandleWithNullDevice]];
    [checkTask setStandardError:[NSFileHandle fileHandleWithNullDevice]];
    
    NSError *err = nil;

    if (![checkTask launchAndReturnError:&err]) {
        return NO;
    }
    
    [checkTask waitUntilExit];
    
    return [checkTask terminationStatus] == 0;
}

+ (BOOL)plistExists {
    NSFileManager *fm = [NSFileManager defaultManager];
    return [fm fileExistsAtPath:plistPath];
}

+ (void)removePlist {
    if (![PPDaemonStatus plistExists])
        return;

    NSString *script = [NSString stringWithFormat:
        @"do shell script \"rm -f %@\" with administrator privileges",
        plistPath];

    NSAppleScript *appleScript = [[NSAppleScript alloc] initWithSource:script];

    NSDictionary *errorInfo = nil;
    [appleScript executeAndReturnError:&errorInfo];

    if (errorInfo) {
        NSLog(@"Failed to remove plist: %@", errorInfo);
    }
}

+ (void)addPlist {
    if ([PPDaemonStatus plistExists])
        return;

    NSString *plist =
        @"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n"
        "<dict>\n"
        "    <key>Label</key>\n"
        "    <string>com.pluginplayground.grant</string>\n"
        "    <key>ProgramArguments</key>\n"
        "    <array>\n"
        "        <string>/opt/pluginplayground/bin/grant</string>\n"
        "    </array>\n"
        "    <key>RunAtLoad</key>\n"
        "    <true/>\n"
        "    <key>KeepAlive</key>\n"
        "    <true/>\n"
        "    <key>StandardOutPath</key>\n"
        "    <string>/var/log/pluginplayground/grant.log</string>\n"
        "    <key>StandardErrorPath</key>\n"
        "    <string>/var/log/pluginplayground/grant.err</string>\n"
        "</dict>\n"
        "</plist>\n";

    NSString *escapedPlist =
        [plist stringByReplacingOccurrencesOfString:@"\\" withString:@"\\\\"];
    escapedPlist =
        [escapedPlist stringByReplacingOccurrencesOfString:@"\"" withString:@"\\\""];
    escapedPlist =
        [escapedPlist stringByReplacingOccurrencesOfString:@"\n" withString:@"\\n"];

    NSString *script =
        [NSString stringWithFormat:
            @"do shell script \"printf \\\"%%s\\\" \\\"%@\\\" > %@ && "
             "chown root:wheel %@ && "
             "chmod 644 %@ && "
             "launchctl load %@\" with administrator privileges",
             escapedPlist,
             plistPath,
             plistPath,
             plistPath,
             plistPath];

    NSAppleScript *appleScript =
        [[NSAppleScript alloc] initWithSource:script];

    NSDictionary *errorInfo = nil;
    [appleScript executeAndReturnError:&errorInfo];

    if (errorInfo)
        NSLog(@"Failed to install plist: %@", errorInfo);
}
@end
