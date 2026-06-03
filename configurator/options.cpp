#include "options.h"
#include <CoreFoundation/CoreFoundation.h>
#include <cstdio>
#include <vector>

static const char* optionsPath() {
    return "/opt/pluginplayground/current.options";
}

static CFDataRef readFile(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return nullptr;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> data(len);
    fread(data.data(), 1, len, f);
    fclose(f);
    return CFDataCreate(kCFAllocatorDefault, data.data(), len);
}

static bool writeFile(const char* path, CFDataRef data) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    bool ok = fwrite(CFDataGetBytePtr(data), 1, CFDataGetLength(data), f) == (size_t)CFDataGetLength(data);
    fclose(f);
    return ok;
}

Options loadOptions() {
    CFDataRef data = readFile(optionsPath());
    if (!data) return {};

    CFPropertyListRef plist = CFPropertyListCreateWithData(
        kCFAllocatorDefault, data, kCFPropertyListImmutable, nullptr, nullptr);
    CFRelease(data);
    if (!plist) return {};
    if (CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
        CFRelease(plist);
        return {};
    }

    CFDictionaryRef dict = (CFDictionaryRef)plist;
    Options opts;

    auto getBool = [&](CFStringRef key, bool fallback) {
        CFBooleanRef val = (CFBooleanRef)CFDictionaryGetValue(dict, key);
        if (!val || CFGetTypeID(val) != CFBooleanGetTypeID())
            return fallback;
        return (bool)CFBooleanGetValue(val);
    };

    opts.useLegacyAmmonia = getBool(CFSTR("useLegacyAmmonia"), false);
    opts.disablePAC = getBool(CFSTR("disablePAC"), false);
    opts.pauseInjection = getBool(CFSTR("pauseInjection"), false);

    CFRelease(dict);
    return opts;
}

static bool fixPermissionsWithAppleScript() {
    int r = system(
        "osascript -e 'do shell script \""
        "mkdir -p /opt/pluginplayground && "
        "touch /opt/pluginplayground/current.options && "
        "chmod 666 /opt/pluginplayground/current.options"
        "\" with administrator privileges' "
        ">/dev/null 2>&1");
    return r == 0;
}

bool saveOptions(const Options& opts) {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 3,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    CFDictionarySetValue(dict, CFSTR("useLegacyAmmonia"),
        opts.useLegacyAmmonia ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(dict, CFSTR("disablePAC"),
        opts.disablePAC ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(dict, CFSTR("pauseInjection"),
        opts.pauseInjection ? kCFBooleanTrue : kCFBooleanFalse);

    CFDataRef data = CFPropertyListCreateData(
        kCFAllocatorDefault, dict, kCFPropertyListXMLFormat_v1_0, 0, nullptr);
    CFRelease(dict);

    if (!data) return false;
    bool ok = writeFile(optionsPath(), data);
    if (!ok) {
        if (fixPermissionsWithAppleScript())
            ok = writeFile(optionsPath(), data);
    }
    CFRelease(data);
    return ok;
}
