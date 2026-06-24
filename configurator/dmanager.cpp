#include "dmanager.h"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

std::string daemonStatusString(DaemonStatus s) {
    switch (s) {
        case DaemonStatus::NotInstalled: return "Not Installed";
        case DaemonStatus::InstalledRunning: return "Running";
        case DaemonStatus::InstalledStopped: return "Stopped";
    }
    return "Unknown";
}

std::string DaemonManager::plistPath() {
    return "/Library/LaunchDaemons/com.pluginplayground.grant.plist";
}

DaemonStatus DaemonManager::status() {
    std::string path = plistPath();
    if (access(path.c_str(), F_OK) != 0)
        return DaemonStatus::NotInstalled;

    std::string cmd = "launchctl print system/com.pluginplayground.grant >/dev/null 2>&1";
    if (system(cmd.c_str()) == 0)
        return DaemonStatus::InstalledRunning;
    return DaemonStatus::InstalledStopped;
}

bool DaemonManager::install() {
    const char* plist =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
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

    FILE* f = fopen("/tmp/com.pluginplayground.grant.plist", "w");
    if (!f) return false;
    fputs(plist, f);
    fclose(f);

    std::string script =
        "osascript -e 'do shell script \""
        "mkdir -p /var/log/pluginplayground && "
        "cp /tmp/com.pluginplayground.grant.plist "
        "/Library/LaunchDaemons/com.pluginplayground.grant.plist && "
        "chown root:wheel /Library/LaunchDaemons/com.pluginplayground.grant.plist && "
        "chmod 644 /Library/LaunchDaemons/com.pluginplayground.grant.plist && "
        "launchctl load /Library/LaunchDaemons/com.pluginplayground.grant.plist"
        "\" with administrator privileges' >/dev/null 2>&1";

    int r = system(script.c_str());
    remove("/tmp/com.pluginplayground.grant.plist");
    return r == 0;
}

bool DaemonManager::uninstall() {
    std::string script =
        "osascript -e 'do shell script \""
        "launchctl unload /Library/LaunchDaemons/com.pluginplayground.grant.plist 2>/dev/null; "
        "rm -f /Library/LaunchDaemons/com.pluginplayground.grant.plist"
        "\" with administrator privileges' >/dev/null 2>&1";
    return system(script.c_str()) == 0;
}
