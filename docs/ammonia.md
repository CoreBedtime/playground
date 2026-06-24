# Ammonia (Legacy)

What it is: A legacy macOS tweak injection system. A loader daemon injects `.dylib` files into target processes.

Why Playground implements legacy support: For backward compatibility, users migrating from Ammonia can still load tweaks from the old path (`/private/var/ammonia/core/tweaks/`).

Ammonia is now deprecated (unsupported on macOS 26.4 and newer).
Source: [Ammonia Public Archive](https://github.com/coreBedTime/Ammonia)

## How it works

The Ammonia loader performs the following actions:
1. Intercepts process spawning to inject its loader (`libopener.dylib`) into target processes.
2. Scans the tweaks folder for `.dylib` files.
3. Reads `.whitelist` or `.blacklist` text files alongside the dylibs to determine if the tweak should load into the current process.
4. Injects matching `.dylib` files into the target at launch, optionally calling a `LoadFunction` if exported.

## Tweak Packaging

A tweak consists of a compiled dynamic library, accompanied by a `.whitelist` or `.blacklist` text file containing target process name substrings (one per line).

Example `com.example.tweak.dylib.whitelist`:
```text
Safari
Finder
```

Example Initialization:

```cpp
#import <Foundation/Foundation.h>

__attribute__((constructor))
static void ammonia_init() {
    // Initialization logic runs when the dylib is loaded into the target process.
}
```

## Compilation

Tweaks are compiled as universal (FAT) dylibs containing necessary architecture slices (x86_64, arm64, arm64e). 

When compiling with clang, pass `-undefined dynamic_lookup` to allow unresolved symbols to be resolved at runtime by the host process. Multiple architectures are combined using the `lipo` tool. Alternatively, Xcode can produce universal libraries automatically by setting the Architectures build setting.

## Notes

- System Integrity Protection (SIP) blocks arbitrary dylib injection into system-signed processes.
- macOS enforces strict code signature checks. Apple Silicon requires valid signatures for code execution.
