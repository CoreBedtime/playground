#!/bin/sh
set -eu

VERSION="1.0.0"
OUTPUT="${1:-PluginPlayground-${VERSION}.pkg}"
SRC="$(cd "$(dirname "$0")" && pwd)"

echo "[+] Building..."
mkdir -p "$SRC/.build"
cmake -S "$SRC" -B "$SRC/.build" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build "$SRC/.build"

echo "[+] Staging..."
STAGING="$(mktemp -d)"
trap "rm -rf '$STAGING'" EXIT

PKG_ROOT="$STAGING/root/opt/pluginplayground"
mkdir -p "$PKG_ROOT/bin" "$PKG_ROOT/tweaks"
mkdir -p "$STAGING/root/Applications"

cp "$SRC/.build/fangs"          "$PKG_ROOT/bin/"
cp "$SRC/.build/grant"          "$PKG_ROOT/bin/"
cp -R "$SRC/.build/configurator.app" "$STAGING/root/Applications/Plugin Playground.app/"

# tweaks directory owned by the user
chown "$(id -u):$(id -g)" "$PKG_ROOT/tweaks"

chmod 755 "$PKG_ROOT/bin/fangs" "$PKG_ROOT/bin/grant"

echo "[+] Building component package..."
pkgbuild --root "$STAGING/root" \
         --identifier "com.pluginplayground.core" \
         --version "$VERSION" \
         --install-location "/" \
         "$STAGING/PluginPlaygroundCore.pkg" > /dev/null

echo "[+] Building distribution package..."
productbuild --distribution "$SRC/installer/Distribution.xml" \
             --package-path "$STAGING" \
             --resources "$SRC/installer" \
             "$SRC/$OUTPUT"

echo "[+] Created $OUTPUT"
echo "    Install: sudo installer -pkg \"$OUTPUT\" -target /"
open PluginPlayground-1.0.0.pkg
