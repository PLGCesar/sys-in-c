PREFIX ?= /usr/local
CC ?= gcc

all: calc passgen bigfiles portcheck utils-help

calc: src/calc/calc.c
	$(CC) $(CFLAGS) src/calc/calc.c -o calc -lm

passgen: src/passgen/passgen.c
	$(CC) $(CFLAGS) src/passgen/passgen.c -o passgen

bigfiles: src/bigfiles/bigfiles.c
	$(CC) $(CFLAGS) src/bigfiles/bigfiles.c -o bigfiles

portcheck: src/portcheck/portcheck.c
	$(CC) $(CFLAGS) src/portcheck/portcheck.c -o portcheck

utils-help: src/utils-help/utils-help.c
	$(CC) $(CFLAGS) src/utils-help/utils-help.c -o utils-help

install:
	install -d $(PREFIX)/bin
	install -m 755 calc $(PREFIX)/bin/calc
	install -m 755 passgen $(PREFIX)/bin/passgen
	install -m 755 bigfiles $(PREFIX)/bin/bigfiles
	install -m 755 portcheck $(PREFIX)/bin/portcheck
	install -m 755 utils-help $(PREFIX)/bin/utils-help

clean:
	rm -f calc passgen bigfiles portcheck utils-help
