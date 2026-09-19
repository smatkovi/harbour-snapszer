#!/bin/sh
# Builds a modern GCC cross toolchain for the Nokia N9 (MeeGo Harmattan).
#
# Harmattan ships GCC 4.4 in MADDE, which cannot compile the C++17 game code.
# A current GCC built against the MADDE sysroot (glibc 2.10, Qt 4.7.4) can,
# with libstdc++ linked statically into the binary.
#
# Harmattan is armv7-a, NEON, *hard-float* (MADDE's gcc: --with-float=hard),
# yet keeps the old dynamic linker /lib/ld-linux.so.3, so executables must be
# linked with -Wl,--dynamic-linker=/lib/ld-linux.so.3 (meego/build.sh does).
#
# Runs on the build machine (see tools/buildhost.sh in nfsshift-sfos):
#   meego/toolchain.sh            # -> $PREFIX (default /tmp/xgcc-harmattan)
#   PREFIX=... SYSROOT=... JOBS=... meego/toolchain.sh
#
# Everything lives under /tmp; the result is about 400 MB. Keep a tarball of
# $PREFIX somewhere persistent, /tmp is wiped by a reboot.
set -e

GCC_VER=${GCC_VER:-14.2.0}
BINUTILS_VER=${BINUTILS_VER:-2.43}
TARGET=arm-none-linux-gnueabi
PREFIX=${PREFIX:-/tmp/xgcc-harmattan}
SYSROOT=${SYSROOT:-$HOME/QtSDK/Madde/sysroots/harmattan_sysroot_10.2011.34-1_slim}
WORK=${WORK:-/tmp/xgcc-build}
JOBS=${JOBS:-8}

[ -d "$SYSROOT/usr/include/qt4" ] || { echo "sysroot not found: $SYSROOT" >&2; exit 1; }
mkdir -p "$WORK" "$PREFIX"
cd "$WORK"

fetch() {
    [ -f "$2" ] || wget -q -O "$2" "$1"
}
fetch "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VER.tar.xz" "binutils-$BINUTILS_VER.tar.xz"
fetch "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VER/gcc-$GCC_VER.tar.xz" "gcc-$GCC_VER.tar.xz"
[ -d "binutils-$BINUTILS_VER" ] || tar xf "binutils-$BINUTILS_VER.tar.xz"
[ -d "gcc-$GCC_VER" ] || tar xf "gcc-$GCC_VER.tar.xz"

export PATH="$PREFIX/bin:$PATH"

if [ ! -x "$PREFIX/bin/$TARGET-ld" ]; then
    echo "== binutils $BINUTILS_VER"
    rm -rf build-binutils && mkdir build-binutils && cd build-binutils
    "../binutils-$BINUTILS_VER/configure" --target=$TARGET --prefix="$PREFIX" \
        --with-sysroot="$SYSROOT" --disable-nls --disable-werror \
        --disable-gdb --disable-sim --disable-gprofng > configure.log 2>&1
    nice make -j"$JOBS" > make.log 2>&1
    make install > install.log 2>&1
    cd ..
fi

if [ ! -x "$PREFIX/bin/$TARGET-g++" ]; then
    echo "== gcc $GCC_VER"
    rm -rf build-gcc && mkdir build-gcc && cd build-gcc
    # A host GCC >= 15 defaults to C++20, where GCC 14's own libcody does not
    # compile (char8_t). Build the compiler itself as C++17.
    CXX="${CXX:-g++} -std=gnu++17" \
    "../gcc-$GCC_VER/configure" --target=$TARGET --prefix="$PREFIX" \
        --with-sysroot="$SYSROOT" --enable-languages=c,c++ \
        --disable-multilib --disable-nls --disable-bootstrap --disable-werror \
        --with-arch=armv7-a --with-fpu=neon --with-float=hard \
        --enable-threads=posix --enable-shared --disable-libstdcxx-pch \
        --disable-libsanitizer --disable-libssp --disable-libquadmath \
        --disable-libgomp --disable-libvtv --disable-libitm \
        --with-gnu-as --with-gnu-ld > configure.log 2>&1
    nice make -j"$JOBS" > make.log 2>&1
    make install > install.log 2>&1
    cd ..
fi

echo "== toolchain ready: $PREFIX/bin/$TARGET-g++"
"$PREFIX/bin/$TARGET-g++" --version | head -1
