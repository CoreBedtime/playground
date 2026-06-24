# PAC Stripping (`arm64e` Bypass)

## What it is there for

Apple Silicon Macs natively run system processes using the `arm64e` ABI, which incorporates **Pointer Authentication Codes (PAC)**. PAC cryptographically signs function pointers in memory to prevent exploits. 

When `fangs` injects custom tweaks (`.dylib` files) into a system process, the injected library must load into the target process's memory space. If the target is running as `arm64e` with PAC enabled, loading unauthenticated binaries can cause kernel panics or PAC violations.

The **PAC Stripping (Depacify)** feature avoids this by dynamically modifying the target executable *right before* it is spawned.

When enabled, Syphon's `exe.c` logic kicks in and:
1. Copies the target executable to a temporary location.
2. Modifies the raw Mach-O header to remove the `CPUTYPE_ARM64E` flag, forcefully downgrading the executable to standard `arm64`.
3. Strips the `LC_CODE_SIGNATURE` load command entirely, as the signature becomes invalid once the ABI flag is altered.
4. Spawns the modified executable.

Because the process is now running as standard `arm64` without PAC enforcement, it effectively turns PAC off for the target process. This allows the custom tweak dylib to load and operate seamlessly without triggering any PAC-related crashes.

## How to enable it

To enable PAC stripping globally for your injected processes:

1. Open the **Configurator** GUI app (`Plugin Playground.app`).
2. Locate the **Disable arm64e (PAC)** toggle in the options.
3. Switch it to **ON**.
4. The change takes effect immediately for any newly spawned processes handled by Syphon.

*(**Important:** This setting is strictly required if you compiled Plugin Playground using the Nix flake, as the Nix build explicitly outputs standard `arm64` binaries to maintain compatibility across open-source toolchains.)*

## Alternative: Native `arm64e` Support

If you prefer to compile Plugin Playground natively as `arm64e` (using Xcode) and do NOT want to rely on PAC stripping, you must enable the `arm64e` preview ABI on your Mac. Apple disables third-party `arm64e` execution by default.

To enable the native `arm64e` ABI:

1. Open your terminal.
2. Run the following command to append the necessary boot argument:
   ```bash
   sudo nvram boot-args="-arm64e_preview_abi"
   ```
   *(Note: Modifying `boot-args` requires System Integrity Protection (SIP) to be appropriately disabled or adjusted from Recovery Mode.)*
3. **Reboot** your Mac.

Once enabled, your natively-compiled `arm64e` builds of `fangs` and `grant` will be able to inject into system processes with full PAC enforcement intact.
