CC = clang
CFLAGS := -std=c11 -O2 $(CFLAGS)
LDFLAGS := -Wl,--as-needed $(LDFLAGS)

ifeq ($(CC), clang)
	CFLAGS += -Weverything -Wno-cast-align -Wno-disabled-macro-expansion
else
	CFLAGS += -Wall -Wextra
endif

paxd: paxd.c

install: paxd paxd.conf
	install -Dm755 paxd $(DESTDIR)/usr/bin/paxd
	install -Dm600 paxd.conf $(DESTDIR)/etc/paxd.conf
	install -Dm644 paxd.service $(DESTDIR)/usr/lib/systemd/system/paxd.service
	mkdir -p $(DESTDIR)/usr/lib/systemd/system/sysinit.target.wants
	cd $(DESTDIR)/usr/lib/systemd/system/sysinit.target.wants; ln -sf ../paxd.service .
