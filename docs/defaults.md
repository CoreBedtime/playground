# Configuration Defaults

The Plugin Playground Configurator saves your preferences in a standard macOS XML Property List (plist) file. This allows you to configure the environment entirely without the GUI if you prefer or if you're writing automated scripts.

**Configuration File Path:**
`/opt/pluginplayground/current.options`

## Available Keys

The following boolean keys are supported:

- `disablePAC`: Disables arm64e PAC signing for spawned processes. This is required if you are compiling without the native arm64e ABI.
- `useLegacyAmmonia`: Uses the legacy tweak path (`/private/var/ammonia/core/tweaks/`) instead of the default `/opt/pluginplayground/tweaks/`.
- `pauseInjection`: Globally pauses tweak injection for all processes.

## Command-Line Usage

Since the configuration is a standard plist file, you can modify it from the terminal using built-in macOS tools like `defaults` or `plutil`.

### Examples using `defaults`

**Enable PAC stripping (disable PAC):**
```bash
defaults write /opt/pluginplayground/current.options disablePAC -bool true
```

**Turn on legacy Ammonia tweaks:**
```bash
defaults write /opt/pluginplayground/current.options useLegacyAmmonia -bool true
```

**Pause injection:**
```bash
defaults write /opt/pluginplayground/current.options pauseInjection -bool true
```

**Read the current configuration:**
```bash
defaults read /opt/pluginplayground/current.options
```

> [!NOTE]
> If the file does not exist yet, you may need to use `sudo defaults write ...` the very first time you create it. Once created by the Configurator GUI, it automatically makes the file writable by all users (`chmod 666`).
