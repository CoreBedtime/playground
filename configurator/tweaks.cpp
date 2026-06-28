#include "tweaks.h"
#include "options.h"
#include <CoreFoundation/CoreFoundation.h>
#include <dirent.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>

std::string tweaksDir() {
    Options opts = loadOptions();
    if (opts.useLegacyAmmonia) {
        return "/private/var/ammonia/core/tweaks";
    }
    return "/opt/pluginplayground/tweaks";
}

static std::string tweakPath(const std::string& name) {
    return tweaksDir() + "/" + name;
}

static std::string tweakDisabledPath(const std::string& name) {
    return tweaksDir() + "/" + name + ".disabled";
}

static std::string tweakOptionsPath(const std::string& name) {
    return tweaksDir() + "/" + name + ".options";
}

static bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::vector<TweakData> scanTweaks() {
    std::vector<TweakData> result;
    DIR* dir = opendir(tweaksDir().c_str());
    if (!dir) return result;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name(entry->d_name);

        if (endsWith(name, ".dylib") && !endsWith(name, ".dylib.disabled")) {
            result.push_back({name, false});
        } else if (endsWith(name, ".dylib.disabled")) {
            std::string base = name.substr(0, name.size() - 9);
            result.push_back({base, true});
        }
    }
    closedir(dir);
    return result;
}

bool toggleTweak(const std::string& name) {
    std::string normal = tweakPath(name);
    std::string disabled = tweakDisabledPath(name);

    FILE* f = fopen(normal.c_str(), "rb");
    if (f) {
        fclose(f);
        return rename(normal.c_str(), disabled.c_str()) == 0;
    }
    f = fopen(disabled.c_str(), "rb");
    if (f) {
        fclose(f);
        return rename(disabled.c_str(), normal.c_str()) == 0;
    }
    return false;
}

bool hasDeveloperTools() {
    return system("/usr/bin/xcode-select -p > /dev/null 2>&1") == 0;
}

bool packageTweak(const std::string& name) {
    std::string dylibPath = tweakPath(name);
    FILE* f = fopen(dylibPath.c_str(), "rb");
    if (!f) {
        dylibPath = tweakDisabledPath(name);
        f = fopen(dylibPath.c_str(), "rb");
    }
    if (!f) return false;
    fclose(f);

    std::string pkgName = name + ".pkg";
    std::string staging = "/tmp/plugintweak_" + name;
    std::string cmd = "mkdir -p '" + staging + "/opt/pluginplayground/tweaks' && "
                      "cp '" + dylibPath + "' '" + staging + "/opt/pluginplayground/tweaks/' && "
                      "/usr/bin/pkgbuild --root '" + staging + "' "
                      "--identifier 'com.pluginplayground.tweak." + name + "' "
                      "--version 1.0.0 "
                      "--install-location '/' "
                      "'/tmp/" + pkgName + "' 2>/dev/null && "
                      "rm -rf '" + staging + "'";
    // failure cleans up staging in temp dir anyway; ignore rm result
    return system(cmd.c_str()) == 0;
}

static CFStringRef strToCF(const std::string& s) {
    return CFStringCreateWithCString(kCFAllocatorDefault, s.c_str(), kCFStringEncodingUTF8);
}

static std::string cfToStr(CFStringRef s) {
    char buf[4096];
    if (CFStringGetCString(s, buf, sizeof(buf), kCFStringEncodingUTF8))
        return buf;
    return {};
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

TweakOptions loadTweakOptions(const std::string& name) {
    TweakOptions opts;
    std::string path = tweakOptionsPath(name);
    CFDataRef data = readFile(path.c_str());
    if (!data) return opts;

    CFPropertyListRef plist = CFPropertyListCreateWithData(
        kCFAllocatorDefault, data, kCFPropertyListImmutable, nullptr, nullptr);
    CFRelease(data);
    if (!plist) return opts;
    if (CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
        CFRelease(plist);
        return opts;
    }

    CFDictionaryRef dict = (CFDictionaryRef)plist;

    auto readArray = [&](CFStringRef key, std::vector<std::string>& out) {
        CFArrayRef arr = (CFArrayRef)CFDictionaryGetValue(dict, key);
        if (!arr || CFGetTypeID(arr) != CFArrayGetTypeID()) return;
        CFIndex count = CFArrayGetCount(arr);
        for (CFIndex i = 0; i < count; i++) {
            CFStringRef s = (CFStringRef)CFArrayGetValueAtIndex(arr, i);
            if (s && CFGetTypeID(s) == CFStringGetTypeID())
                out.push_back(cfToStr(s));
        }
    };

    readArray(CFSTR("blacklistedApps"), opts.blacklistedApps);
    readArray(CFSTR("frameworkDependencies"), opts.frameworkDependencies);

    CFRelease(dict);
    return opts;
}

bool saveTweakOptions(const std::string& name, const TweakOptions& opts) {
    std::string path = tweakOptionsPath(name);

    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 2,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    auto writeArray = [&](CFStringRef key, const std::vector<std::string>& items) {
        CFMutableArrayRef arr = CFArrayCreateMutable(kCFAllocatorDefault, items.size(), &kCFTypeArrayCallBacks);
        for (const auto& item : items) {
            CFStringRef s = strToCF(item);
            CFArrayAppendValue(arr, s);
            CFRelease(s);
        }
        CFDictionarySetValue(dict, key, arr);
        CFRelease(arr);
    };

    writeArray(CFSTR("blacklistedApps"), opts.blacklistedApps);
    writeArray(CFSTR("frameworkDependencies"), opts.frameworkDependencies);

    CFDataRef data = CFPropertyListCreateData(
        kCFAllocatorDefault, dict, kCFPropertyListXMLFormat_v1_0, 0, nullptr);
    CFRelease(dict);

    if (!data) return false;
    bool ok = writeFile(path.c_str(), data);
    CFRelease(data);
    return ok;
}

bool ensurePermissions() {
    std::string dir = tweaksDir();
    if (access(dir.c_str(), R_OK | W_OK) == 0)
        return true;

    std::string script =
        "osascript -e 'display dialog \"Plugin Playground needs permission to write to:\\n"
        + dir + "\\n\\n"
        "Click Fix to authenticate and fix permissions.\" "
        "buttons {\"Exit\", \"Fix\"} default button \"Fix\" with icon caution'";

    int r = system(script.c_str());
    if (r != 0)
        return false;

    std::string fix =
        "osascript -e 'do shell script \""
        "mkdir -p /opt/pluginplayground/tweaks && "
        "chmod -R 777 /opt/pluginplayground/"
        "\" with administrator privileges' >/dev/null 2>&1";

    r = system(fix.c_str());
    if (r != 0)
        return false;

    return access(dir.c_str(), R_OK | W_OK) == 0;
}

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
