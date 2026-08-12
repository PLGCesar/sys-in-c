PREFIX ?= /usr/local
CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -fPIC
LDFLAGS_IPC = -L. -lutilipc -Wl,-rpath,. -lpthread

LIB_IPC = libutilipc.so
TOOLS = calc passgen bigfiles portcheck hashcalc b64 sysinfo org netinfo ffind ipcmon utils-help

all: $(LIB_IPC) $(TOOLS)

$(LIB_IPC): src/libutilipc/utilipc.c
	$(CC) $(CFLAGS) -shared src/libutilipc/utilipc.c -o $(LIB_IPC) -lpthread

calc: src/calc/calc.c $(LIB_IPC)
	$(CC) $(CFLAGS) src/calc/calc.c -o calc $(LDFLAGS_IPC) -lm

passgen: src/passgen/passgen.c $(LIB_IPC)
	$(CC) $(CFLAGS) src/passgen/passgen.c -o passgen $(LDFLAGS_IPC)

bigfiles: src/bigfiles/bigfiles.c $(LIB_IPC)
	$(CC) $(CFLAGS) src/bigfiles/bigfiles.c -o bigfiles $(LDFLAGS_IPC)

portcheck: src/portcheck/portcheck.c $(LIB_IPC)
	$(CC) $(CFLAGS) src/portcheck/portcheck.c -o portcheck $(LDFLAGS_IPC)

hashcalc: src/hashcalc/hashcalc.c $(LIB_IPC)
	$(CC) $(CFLAGS) src/hashcalc/hashcalc.c -o hashcalc $(LDFLAGS_IPC)

b64: src/b64/b64.c
	$(CC) $(CFLAGS) src/b64/b64.c -o b64

sysinfo: src/sysinfo/sysinfo.c $(LIB_IPC)
	$(CC) $(CFLAGS) src/sysinfo/sysinfo.c -o sysinfo $(LDFLAGS_IPC)

org: src/org/org.c $(LIB_IPC)
	$(CC) $(CFLAGS) src/org/org.c -o org $(LDFLAGS_IPC)

netinfo: src/netinfo/netinfo.c $(LIB_IPC)
	$(CC) $(CFLAGS) src/netinfo/netinfo.c -o netinfo $(LDFLAGS_IPC)

ffind: src/ffind/ffind.c $(LIB_IPC)
	$(CC) $(CFLAGS) src/ffind/ffind.c -o ffind $(LDFLAGS_IPC)

ipcmon: src/ipcmon/ipcmon.c $(LIB_IPC)
	$(CC) $(CFLAGS) src/ipcmon/ipcmon.c -o ipcmon $(LDFLAGS_IPC)

utils-help: src/utils-help/utils-help.c
	$(CC) $(CFLAGS) src/utils-help/utils-help.c -o utils-help

install: all
	install -d $(DESTDIR)$(PREFIX)/lib
	install -m 755 $(LIB_IPC) $(DESTDIR)$(PREFIX)/lib/$(LIB_IPC)
	install -d $(DESTDIR)$(PREFIX)/bin
	for tool in $(TOOLS); do \
		install -m 755 $$tool $(DESTDIR)$(PREFIX)/bin/$$tool; \
	done

clean:
	rm -f $(TOOLS) $(LIB_IPC)

.PHONY: all install clean
