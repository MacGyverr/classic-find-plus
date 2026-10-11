#!/bin/sh
set -eu

usage() {
    echo "Usage: scripts/build-deb.sh [--buildtype debug|release] [--sysroot SDK --distribution NAME]" >&2
    exit 2
}
BUILD_TYPE=release
SYSROOT=
DISTRIBUTION=
while [ "$#" -gt 0 ]; do
    case "$1" in
        --buildtype)
            [ "$#" -ge 2 ] || usage
            BUILD_TYPE=$2
            shift 2
            ;;
        --sysroot) [ "$#" -ge 2 ] || usage; SYSROOT=$2; shift 2 ;;
        --distribution) [ "$#" -ge 2 ] || usage; DISTRIBUTION=$2; shift 2 ;;
        *) usage ;;
    esac
done
case "$BUILD_TYPE" in debug|release) ;; *) usage ;; esac

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
SUFFIX=$BUILD_TYPE
if [ -n "$SYSROOT" ]; then
    [ -n "$DISTRIBUTION" ] || usage
    SYSROOT=$(CDPATH= cd -- "$SYSROOT" && pwd)
    [ -f "$SYSROOT/.classic-find-sysroot" ] || { echo "Use prepare-deb-sysroot.py to prepare the SDK" >&2; exit 1; }
    SUFFIX="$DISTRIBUTION-$BUILD_TYPE"
elif [ -n "$DISTRIBUTION" ]; then
    usage
else
    DISTRIBUTION=$(python3 -c 'import platform; d=platform.freedesktop_os_release(); print(d["ID"]+d["VERSION_ID"])')
fi
case "$DISTRIBUTION" in
    ''|*[!a-zA-Z0-9.+]*) echo "Invalid distribution identifier" >&2; exit 1 ;;
esac
BUILD="$ROOT/build-deb-$SUFFIX"
PKG_PARENT="$ROOT/build-package-$SUFFIX"
PKG_ROOT="$PKG_PARENT/root"
MARKER="$PKG_PARENT/.classic-find-package-root"
OUTPUT="$ROOT/dist"

if [ ! -f "$BUILD/meson-private/coredata.dat" ]; then
    if [ -n "$SYSROOT" ]; then
        [ "$(dpkg-architecture -qDEB_HOST_ARCH)" = amd64 ] || { echo "SDK builds currently support amd64 only" >&2; exit 1; }
        mkdir -p "$PKG_PARENT"
        CROSS_FILE="$PKG_PARENT/sdk.ini"
        python3 - "$SYSROOT" "$CROSS_FILE" <<'PY_CROSS'
from pathlib import Path
import sys
root = Path(sys.argv[1])
lib = root / 'usr/lib/x86_64-linux-gnu'
Path(sys.argv[2]).write_text(f'''[binaries]
c = {['cc', '--sysroot=' + str(root), '-B' + str(lib) + '/']!r}
pkg-config = 'pkg-config'
ar = 'ar'
strip = 'strip'
[properties]
sys_root = {str(root)!r}
pkg_config_libdir = {[str(lib / 'pkgconfig'), str(root / 'usr/share/pkgconfig')]!r}
[built-in options]
c_link_args = {['-Wl,-rpath-link,' + str(lib), '-Wl,-rpath-link,' + str(root / 'lib/x86_64-linux-gnu')]!r}
[host_machine]
system = 'linux'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'
''')
PY_CROSS
        meson setup "$BUILD" --cross-file "$CROSS_FILE" --buildtype="$BUILD_TYPE" --prefix=/usr --bindir=bin --datadir=share
        printf '%s\n' "$SYSROOT" > "$PKG_PARENT/.classic-find-build-sdk"
    else
        meson setup "$BUILD" --buildtype="$BUILD_TYPE" --prefix=/usr --bindir=bin --datadir=share
    fi
else
    if [ -n "$SYSROOT" ]; then
        [ -f "$PKG_PARENT/.classic-find-build-sdk" ] && [ "$(cat "$PKG_PARENT/.classic-find-build-sdk")" = "$SYSROOT" ] || {
            echo "This build directory belongs to a different SDK; use a different distribution identifier" >&2
            exit 1
        }
    fi
    meson configure "$BUILD" --prefix=/usr --bindir=bin --datadir=share
fi
meson compile -C "$BUILD"

if [ -e "$PKG_ROOT" ]; then
    [ -f "$MARKER" ] && [ "$(cat "$MARKER")" = "$ROOT" ] || {
        echo "Refusing to replace an unmarked package root: $PKG_ROOT" >&2
        exit 1
    }
    rm -rf -- "$PKG_ROOT"
fi
mkdir -p "$PKG_ROOT/DEBIAN"
mkdir -p "$PKG_PARENT"
printf '%s\n' "$ROOT" > "$MARKER"
DESTDIR="$PKG_ROOT" meson install -C "$BUILD" --no-rebuild

VERSION=$(meson introspect --projectinfo "$BUILD" | python3 -c 'import json,sys; print(json.load(sys.stdin)["version"])')
DEB_VERSION=$(dpkg-parsechangelog -l "$ROOT/debian/changelog" -SVersion)
case "$DEB_VERSION" in
    "$VERSION"-*) ;;
    *) echo "Meson version $VERSION does not match Debian changelog $DEB_VERSION" >&2; exit 1 ;;
esac
# Keep the build distribution visible: shared-library requirements are target-specific.
PACKAGE_VERSION="$DEB_VERSION+$DISTRIBUTION"
ARCH=$(dpkg-architecture -qDEB_HOST_ARCH)
if [ -n "$SYSROOT" ]; then
    SUBSTVARS=$(dpkg-shlibdeps --admindir="$SYSROOT/var/lib/dpkg" -l"$SYSROOT/usr/lib/x86_64-linux-gnu" -l"$SYSROOT/lib/x86_64-linux-gnu" -O -e"$BUILD/classic-find-plus" -e"$BUILD/classic-find-plus-search-worker" -e"$BUILD/classic-find-plus-thunar-admin")
else
    SUBSTVARS=$(dpkg-shlibdeps -O -e"$BUILD/classic-find-plus" -e"$BUILD/classic-find-plus-search-worker" -e"$BUILD/classic-find-plus-thunar-admin")
fi
case "$SUBSTVARS" in
    'shlibs:Depends='*) SHLIB_DEPS=${SUBSTVARS#shlibs:Depends=} ;;
    *) echo "dpkg-shlibdeps returned no dependencies: $SUBSTVARS" >&2; exit 1 ;;
esac
[ -n "$SHLIB_DEPS" ] || { echo "No shared-library dependencies found" >&2; exit 1; }

install -m 0644 "$ROOT/debian/copyright" "$PKG_ROOT/usr/share/doc/classic-find-plus/copyright"
for STAGE in preinst postinst postrm; do
    install -m 0755 "$ROOT/debian/nemo-migration.py" "$PKG_ROOT/DEBIAN/$STAGE"
done
INSTALLED_SIZE=$(du -sk --exclude=DEBIAN "$PKG_ROOT" | cut -f1)

cat > "$PKG_ROOT/DEBIAN/control" <<EOF_CONTROL
Package: classic-find-plus
Version: $PACKAGE_VERSION
Section: utils
Priority: optional
Architecture: $ARCH
Installed-Size: $INSTALLED_SIZE
Maintainer: MacGyverr / ChatGPT <classic-find-plus@localhost>
Pre-Depends: python3
Depends: $SHLIB_DEPS, libgtk-3-0 (>= 3.22) | libgtk-3-0t64 (>= 3.22), libglib2.0-0 (>= 2.70) | libglib2.0-0t64 (>= 2.70), librsvg2-common, shared-mime-info, dbus, policykit-1 | polkitd, libglib2.0-bin
Suggests: nemo, python3-nautilus
Conflicts: classic-find
Replaces: classic-find
Provides: classic-find
Homepage: https://github.com/MacGyverr/classic-find-plus
Description: Classic Find Plus file and folder search with optional desktop menus
 A native GTK3 application for finding files and folders. It searches selected
 directories with name, exclusion, type, selectable-date and size filters, and provides
 desktop-themed results, exact Bytes/Bits columns, file actions, URI drag-and-drop
 and CSV/TXT export.
EOF_CONTROL

mkdir -p "$OUTPUT"
PACKAGE="$OUTPUT/classic-find-plus_${PACKAGE_VERSION}_${ARCH}.deb"
dpkg-deb --root-owner-group --build "$PKG_ROOT" "$PACKAGE"
dpkg-deb --info "$PACKAGE"
printf 'Built %s\n' "$PACKAGE"
