# Maintainer: AntonIT228 <zelenukanton225@gmail.com>
pkgname=anfetch
pkgver=1.4.5
pkgrel=1
pkgdesc="A simple, modular system fetch tool written in C++"
arch=('x86_64')
url="https://github.com/AntonIT228/anfetch"
license=('Unlicense')
depends=('gcc-libs')
makedepends=('gcc')
source=("$url/archive/refs/tags/v$pkgver.tar.gz")
sha256sums=('42be275f7546f0a556bbea831ea0e984452ffb27cbbfd9f7548e1ff9267f8271')

build() {
    cd "$pkgname-$pkgver"
    g++ -O2 -std=c++17 main.cpp -o anfetch
}

package() {
    cd "$pkgname-$pkgver"
    install -Dm755 anfetch "$pkgdir/usr/bin/anfetch"
    install -d "$pkgdir/usr/share/anfetch/logos"
    cp -r logos/* "$pkgdir/usr/share/anfetch/logos/"
    install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
