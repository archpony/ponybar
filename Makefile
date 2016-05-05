.PHONY:install uninstall
CC=gcc
CFLAGS=-O2 -s 
LDFLAGS=-lX11

PREFIX=/usr/local/bin

TARGET=ponybar
SOURCE=$(TARGET:=.c)

$(TARGET): $(SOURCE)
	$(CC) -o $(TARGET) $(CFLAGS) $(LDFLAGS) $(SOURCE)

install:
	install $(TARGET) $(PREFIX)

uninstall:
	rm -rf $(PREFIX)/$(TARGET)
