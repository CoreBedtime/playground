# Compilation

The build process is managed by install.sh and CMake.

## install.sh
What it is: The primary build script.
Role: Automates building components and packaging them into the final installer.

## CMakeLists.txt
What it is: The project build configuration.
Role: Specifies targets, fetches dependencies like Slint, and coordinates the compilation of grant, fangs, and the configurator.

## Nix (Optional)
What it is: A declarative build environment using flake.nix.
Why it is: Provides a reproducible alternative to system tools.
How it is used: Run `nix build` or use `nix develop` to build the project.
Optional: Yes, this method is entirely optional. 
Fun-fact: It can also be used to make a nixpkgs package of Plugin Playground available to all nix users. Eventually, `nix-darwin`, `home-manager` can utilize that package to provide easy configuration of Plugin Playground with nix options.

**Important Note for Apple Silicon:** Because the Nix build uses standard open-source toolchains and must compile both Rust (Slint) and C++ components without linker conflicts, it generates standard `arm64` binaries rather than Apple's native `arm64e` ABI. As a result, when using the Nix-compiled version of the Playground, you **must** toggle **Disable arm64e (PAC)** in the Configurator GUI to strip PAC signing from spawned processes, allowing injection to work flawlessly.