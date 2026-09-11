//Created by Salty on 9/11/26. Extension on work by Alex Spaulding

#ifndef ASSIPSTATUS_H
#define ASSIPSTATUS_H

#import <Foundation/Foundation.h>
#import <AppKit/NSImage.h>
#include <string>

enum class SipStatus;

@interface ASSIPStatus : NSObject
@property (assign) SipStatus status;

- (NSImage *)sipStatusIndicatorImage;
- (NSString *)sipStatusString;
@end

#endif
