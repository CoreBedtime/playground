# Plugin Playground

An open-source general-purpose runtime tweak system for macOS Apple Silicon.

Plugin Playground provides a framework for intercepting and modifying the behavior of
running processes. It's the foundation for building runtime plugins, introspection tools, and
behavior-modification tweaks on modern macOS.

## Core engine: syphon

The `syphon/` library is the low-level interception engine:

- **Mach exception ports** — receive kernel-delivered exceptions for breakpoints and traps
- **ARM64 hardware breakpoints** (DebugState64 `BVR`/`BCR`) — set execution breakpoints in
  any process without modifying code pages (avoids W^X and I-cache coherency issues on
  Apple Silicon shared-cache regions)
- **Per-thread state tracking** — safely manage concurrent breakpoint hits across multiple
  threads
- **Synchronous trapping** — the exception handler completes all work (reads registers,
  reads target memory, logs output) before replying, giving tweaks full control over
  the target process state during each trap

The engine is small, embeddable, and exposes a C API (`syphon.h`) for building custom
tweaks and tracers.

## Components

| Component | Description |
|-----------|-------------|
| `syphon/` | Core interception library — targets, breakpoints, exception handling, stepping state |
| `fangs`   | Reference tweak / tracer — attaches to launchd (PID 1), demonstrates `__posix_spawn` interception |
| `grant`   | Privilege helper — patches amfid so `task_for_pid` works for PID 1 (required once per boot) |
| `configurator` | GUI for managing installed tweaks and runtime settings |
| `libsyphon.a` | Core interception library (not yet distributed) |

## Build Requirements

- macOS Apple Silicon (ARM64)
- Xcode Command Line Tools (`xcode-select --install`)
- CMake 3.16+
- git
- Internet connection (first build fetches Slint via FetchContent)

## Build & Install

```sh
sudo ./install.sh
```

This builds everything and produces `PluginPlayground-1.0.0.pkg`. Run the `.pkg` to install, or pass a custom prefix path to install directly without the GUI installer:

```sh
sudo ./install.sh /opt/pluginplayground
```

Binaries land in `.build/`:
- `.build/fangs`   — signed ad-hoc with `Master.entitlements`
- `.build/grant`   — unsigned
- `.build/configurator.app` — signed ad-hoc macOS bundle
- `.build/libsyphon.a`

Layout after install:

```
/Applications/
└── Plugin Playground.app/     — configurator GUI

/opt/pluginplayground/
├── bin/
│   ├── fangs
│   └── grant
└── tweaks/       (user-owned, for runtime tweak bundles)
```

## Usage

```sh
# Terminal 1: patch amfid (once per boot), then launch fangs
sudo /opt/pluginplayground/bin/grant

# Terminal 2: or run fangs directly
sudo /opt/pluginplayground/bin/fangs

# Launch the tweak configurator GUI
open /Applications/Plugin\ Playground.app
```

## Writing tweaks (internal)

The `syphon` library (not yet distributed) provides the C API for building custom tweaks.
See `syphon/syphon.h` in the source tree for the full API while it remains in-development.

## Project structure

```
├── install.sh                Build + .pkg builder
├── install.sh                .pkg builder
├── CMakeLists.txt            CMake build
├── Master.entitlements       Required entitlements for process introspection
├── installer/                .pkg GUI resources (Distribution.xml, pages, background)
├── syphon/
│   ├── syphon.h              Public C API
│   ├── main.c                fangs entry point and event loop
│   ├── breakpoint.c          HW breakpoint install / clear / template
│   ├── mach_exc.c            send_reply, reset_exception_ports
│   ├── stepping.c            Per-thread stepping state
│   ├── target.c              Target address management
│   └── process.c             find_process helper
├── grant/
│   ├── amfid_handler.h       amfid patch / unpatch API
│   ├── amfid_handler.m       Objective-C amfid hook
│   └── main.m                grant entry point
└── configurator/
    ├── Info.plist.in         macOS bundle metadata
    ├── configurator.slint    Slint UI layout
    └── main.cpp              Configurator entry point
```

## Caveats

- **PID 1 is fragile.** The reference tweak (`fangs`) demonstrates interception on launchd.
  Bugs can freeze the system. Always keep a reboot method handy.
- **`grant` once per boot.** amfid state resets on reboot.
- **6 hardware breakpoint registers** on ARM64 (`BVR`/`BCR` 0–5).
- **Apple Silicon only.** The engine uses ARM64-specific debug register state.
