//Created by Salty on 9/2/26.

#ifndef PPDAEMONSTATUS_H
#define PPDAEMONSTATUS_H

#import <Foundation/Foundation.h>

@interface PPDaemonStatus : NSObject
+ (BOOL)fangsIsRunning;
+ (void)removePlist;
+ (void)addPlist;
+ (BOOL)plistExists;
@end

#endif
