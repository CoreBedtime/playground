//Created by Salty on 9/11/26. Extension on work by Alex Spaulding

#import "ASSIPStatus.h"

enum class SipStatus {
    Enabled,
    Disabled,
    PartiallyDisabled,
    Unknown
};

SipStatus checkSipStatus() {
    FILE* pipe = popen("/usr/bin/csrutil status", "r");
    if (!pipe) return SipStatus::Unknown;

    char buffer[256];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);

    if (result.find("System Integrity Protection status: disabled.") != std::string::npos) {
        return SipStatus::Disabled;
    } else if (result.find("Debugging Restrictions: disabled") != std::string::npos) {
        return SipStatus::PartiallyDisabled;
    } else if (result.find("System Integrity Protection status: enabled.") != std::string::npos) {
        return SipStatus::Enabled;
    }
    return SipStatus::Unknown;
}

std::string sipStatusToString(SipStatus status) {
    switch(status) {
        case SipStatus::Enabled: return "Enabled";
        case SipStatus::Disabled: return "Disabled";
        case SipStatus::PartiallyDisabled: return "Partially Disabled (Debugging Restrictions Off)";
        default: return "Unknown";
    }
}

@implementation ASSIPStatus
- (instancetype)init{
    self = [super init];
    if (self){
        self.status = checkSipStatus();
    }
    return self;
}

- (NSImage *)sipStatusIndicatorImage{
    if (self.status == SipStatus::Enabled || self.status == SipStatus::Unknown)
        return [NSImage imageNamed:@"NSStatusUnavailable"];
    else
        return [NSImage imageNamed:@"NSStatusAvailable"];
}

- (NSString *)sipStatusString{
    return [NSString stringWithUTF8String:sipStatusToString(self.status).c_str()];
}
@end
