PREFIX ?= /usr/local
CC ?= gcc

all: calc passgen

calc: src/calc/calc.c
	$(CC) $(CFLAGS) src/calc/calc.c -o calc -lm

passgen: src/passgen/passgen.c
	$(CC) $(CFLAGS) src/passgen/passgen.c -o passgen

install:
	install -d $(PREFIX)/bin
	install -m 755 calc $(PREFIX)/bin/calc
	install -m 755 passgen $(PREFIX)/bin/passgen

clean:
	rm -f calc passgen
