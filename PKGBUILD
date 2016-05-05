# This is an example PKGBUILD file. Use this as a start to creating your own,
# and remove these comments. For more information, see 'man PKGBUILD'.
# NOTE: Please fill out the license field for your package! If it is unknown,
# then please put 'unknown'.

# Maintainer: Arch Pony <archpony@rambler.ru>
# Contributor: Arch Pony <archpony@rambler.ru>
pkgname=ponybar
pkgver=0.2
pkgrel=1
pkgdesc="A simple statusbar for dwm"
arch=('i686' 'x86_64')
url="https://github.com/archpony/ponybar"
license=('GPL')
depends=('libx11' 'libxcb' 'libxdmcp')
makedepends=('gcc' 'make')
optdepends=('dwm: a window manager')
source=("ponybar.c"
        "Makefile")
noextract=("ponybar.c" "Makefile")
md5sums=("SKIP" "SKIP")

build() {
	make
}


package() {
	install -D $pkgname $pkgdir/usr/bin/$pkgname
}
