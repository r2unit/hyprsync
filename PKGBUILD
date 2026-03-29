# Maintainer: r2unit <r2unit@users.noreply.github.com>
pkgname=hyprsync
pkgver=2026.2.12
pkgrel=1
pkgdesc="A lightweight sync daemon for Hyprland users who work across multiple machines"
arch=('x86_64' 'aarch64')
url="https://github.com/r2unit/hyprsync"
license=('MIT')
depends=('openssh' 'rsync' 'git')
makedepends=('cmake' 'gcc')
optdepends=('systemd: daemon support')
source=("$pkgname-$pkgver.tar.gz::$url/archive/v$pkgver.tar.gz")
sha256sums=('SKIP')

build() {
    cd "$pkgname-$pkgver"

    mkdir -p vendor
    git clone --depth 1 https://github.com/cktan/tomlc99.git vendor/tomlc99

    cmake -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DWITH_SYSTEMD=ON
    cmake --build build -j$(nproc)
}

package() {
    cd "$pkgname-$pkgver"
    DESTDIR="$pkgdir" cmake --install build
    install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
