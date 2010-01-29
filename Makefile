R = ar
CC = gcc
CFLAGS ?= -g -Wall -Os -fPIC
LDFLAGS = -L.
SHLIB_CFLAGS = -shared

INSTALL_EXEC = install -m 755 -o root -g root
INSTALL_DATA = install -m 644 -o root -g root

NAME = simplenet
OBJS = network_socket.o network_server.o
MAJOR = 0
MINOR = 1
MICRO = 0

A_TARGETS = lib$(NAME).a
SO_TARGETS = lib$(NAME).so lib$(NAME).so.$(MAJOR) lib$(NAME).so.$(MAJOR).$(MINOR) lib$(NAME).so.$(MAJOR).$(MINOR).$(MICRO)
BIN_TARGETS = example

PREFIX ?=

TARGETS = $(A_TARGETS) $(SO_TARGETS) $(BIN_TARGETS)

all: $(TARGETS)

lib$(NAME).a: $(OBJS)
	$(AR) rc $@ $+

lib$(NAME).so: lib$(NAME).so.$(MAJOR)
	ln -sf $< $@

lib$(NAME).so.$(MAJOR): lib$(NAME).so.$(MAJOR).$(MINOR)
	ln -sf $< $@

lib$(NAME).so.$(MAJOR).$(MINOR): lib$(NAME).so.$(MAJOR).$(MINOR).$(MICRO)
	ln -sf $< $@

lib$(NAME).so.$(MAJOR).$(MINOR).$(MICRO): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-soname -Wl,lib$(NAME).so.$(MAJOR).$(MINOR).$(MICRO) $(SHLIB_CFLAGS) -o $@ $^


example: main.o $(OBJS) 
	$(CC) $(CFLAGS) -o $@ $+ -lev

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<


.PHONY: lib$(NAME).pc
lib$(NAME).pc: lib$(NAME).pc.in
	sed -e 's;@PREFIX@;/usr;' -e 's;@LIB_VER_MAJOR@;$(MAJOR);' -e 's;@LIB_VER_MINOR@;$(MINOR);' < $< > $@

.PHONY: tests clean install install-bin install-lib
tests: $(NAME)lint
	(cd tests; ./runtest)

install-lib: $(SO_TARGETS) $(A_TARGETS) $(PC_TARGET)
	mkdir -p $(PREFIX)/usr/lib/pkgconfig
	$(INSTALL_DATA) -t $(PREFIX)/usr/lib/pkgconfig $(PC_TARGET)
	mkdir -p $(PREFIX)/usr/include
	$(INSTALL_DATA) -t $(PREFIX)/usr/include $(NAME).h
	mkdir -p $(PREFIX)/usr/lib
	$(INSTALL_EXEC) -t $(PREFIX)/usr/lib $(SO_TARGETS) $(A_TARGETS)

install-bin: $(BIN_TARGETS)
	mkdir -p $(PREFIX)/usr/bin
	$(INSTALL_EXEC) -t $(PREFIX)/usr/bin $(BIN_TARGETS)

install: install-lib install-bin

clean:
	rm -f *.o $(TARGETS)

