# Maintainer: Dhruv <dhruv@example.com>
pkgname=khostty
pkgver=0.1.0
_ghostty_ver=1.4.0
pkgrel=1
pkgdesc="A native KDE Plasma 6 terminal emulator powered by libghostty"
arch=('x86_64' 'aarch64')
url="https://github.com/dhruv/khostty"
license=('MIT')
depends=(
    'qt6-base'
    'kxmlgui'
    'kconfig'
    'kconfigwidgets'
    'kcoreaddons'
    'ki18n'
    'knotifications'
    'kwindowsystem'
    'kglobalaccel'
    'kio'
    'layer-shell-qt'
    'fontconfig'
    'freetype2'
    'harfbuzz'
    'oniguruma'
    'zlib'
    'libpng'
    'simdutf'
)
makedepends=(
    'cmake'
    'ninja'
    'extra-cmake-modules'
    'zig>=0.17.0'
    'git'
    'ncurses'
    'pkgconf'
)
source=(
    "ghostty-${_ghostty_ver}.tar.gz::https://release.files.ghostty.org/${_ghostty_ver}/ghostty-${_ghostty_ver}.tar.gz"
)
sha256sums=('SKIP')

prepare() {
    cd "${srcdir}/ghostty-${_ghostty_ver}"
    patch -p1 -i "${startdir}/bridge/embedded_linux.patch"
    cp "${startdir}/bridge/khostty_bridge.zig" "src/khostty_bridge.zig"
    echo 'comptime { _ = @import("khostty_bridge.zig"); }' >> "src/main_c.zig"
    
    # Pre-fetch zig dependencies while network access is permitted in prepare()
    export ZIG_GLOBAL_CACHE_DIR="${srcdir}/zig-cache"
    zig build --fetch
}

build() {
    export ZIG_GLOBAL_CACHE_DIR="${srcdir}/zig-cache"
    
    # 1. Build libghostty.so
    cd "${srcdir}/ghostty-${_ghostty_ver}"
    zig build \
        -Dversion-string="${_ghostty_ver}" \
        -Dapp-runtime=none \
        -Demit-lib-vt=false \
        -Drenderer=opengl \
        -Demit-exe=false \
        -Demit-docs=false \
        -Demit-macos-app=false \
        -Demit-xcframework=false \
        -fsys=fontconfig \
        -fsys=freetype \
        -fsys=harfbuzz \
        -fsys=oniguruma \
        -fsys=zlib \
        -fsys=libpng \
        -Dcpu=baseline \
        -Doptimize=ReleaseFast \
        --prefix "${srcdir}/ghostty-prefix"

    # Install bridge header alongside ghostty.h
    cp "${startdir}/bridge/khostty_bridge.h" "${srcdir}/ghostty-prefix/include/khostty_bridge.h"

    # 2. Build Khostty against the staged libghostty
    cd "${startdir}"
    cmake -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DGHOSTTY_PREFIX="${srcdir}/ghostty-prefix" \
        -DCMAKE_INSTALL_RPATH="\$ORIGIN/../lib/khostty"
    cmake --build build
}

package() {
    cd "${startdir}"
    DESTDIR="${pkgdir}" cmake --install build

    # Install private libghostty.so
    install -Dm755 "${srcdir}/ghostty-prefix/lib/ghostty-internal.so" \
        "${pkgdir}/usr/lib/khostty/libghostty.so"

    # Install Ghostty terminfo and themes
    install -d "${pkgdir}/usr/share"
    cp -r "${srcdir}/ghostty-prefix/share/ghostty" "${pkgdir}/usr/share/"
    cp -r "${srcdir}/ghostty-prefix/share/terminfo" "${pkgdir}/usr/share/"
}
