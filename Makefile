CC = clang
CFLAGS := -std=c11 -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE \
	  -O2 -flto -fuse-ld=gold $(CFLAGS)
LDFLAGS := -O2 -flto -fuse-ld=gold -Wl,--as-needed,--gc-sections $(LDFLAGS)

ifeq ($(CC), clang)
	CFLAGS += -Weverything -Wno-cast-align -Wno-disabled-macro-expansion
else
	CFLAGS += -Wall -Wextra
endif

all: apply-pax-flags paxd
apply-pax-flags: apply-pax-flags.o flags.o
paxd: paxd.o flags.o
flags: flags.c flags.h

clean:
	rm -f apply-pax-flags apply-pax-flags.o paxd paxd.o flags.o

install: paxd paxd.conf
	install -Dm755 apply-pax-flags $(DESTDIR)/usr/bin/apply-pax-flags
	install -Dm755 paxd $(DESTDIR)/usr/bin/paxd
	install -Dm600 paxd.conf $(DESTDIR)/etc/paxd.conf
	install -Dm644 paxd.service $(DESTDIR)/usr/lib/systemd/system/paxd.service
	mkdir -p $(DESTDIR)/usr/lib/systemd/system/sysinit.target.wants
	cd $(DESTDIR)/usr/lib/systemd/system/sysinit.target.wants; ln -sf ../paxd.service .

.PHONY: all clean install
