# ponybar
Simple statusbar generator for dwm.

## Build
* gcc -o ponybar ponybar.c -O2 -s -lX11
* mv ponybar /usr/local/bin
* sed -i '/dwm/i \ponybar\&' ~/.xinitrc *optional*
