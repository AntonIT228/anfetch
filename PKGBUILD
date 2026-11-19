# Maintainer: AntonIT228 <zelenukanton225@gmail.com>
pkgname=anfetch
pkgver=1.5.2
pkgrel=1
pkgdesc="A simple, modular system fetch tool written in C++"
arch=('x86_64')
url="https://github.com/AntonIT228/anfetch"
license=('Unlicense')
depends=('gcc-libs')
makedepends=('gcc')
source=("$url/archive/refs/tags/v$pkgver.tar.gz")
sha256sums=('7ea26fd304154c5b905c0e1581e992f3c5c37f660b315294ff08fa85f4f05143')

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
