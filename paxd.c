#include <err.h>
#include <limits.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>

#include "flags.h"

static int inotify_add_watch_x(int fd, const char *pathname, uint32_t mask) {
    int watch = inotify_add_watch(fd, pathname, mask);
    if (watch == -1) {
        perror("inotify_add_watch");
        exit(EXIT_FAILURE);
    }
    return watch;
}

int main(void) {
    update_attributes();

    alignas(struct inotify_event) char buf[(sizeof(struct inotify_event) + NAME_MAX + 1) * 64];

    int inotify = inotify_init();
    if (inotify == -1) {
        perror("inotify");
        return EXIT_FAILURE;
    }
    int watch_conf = inotify_add_watch_x(inotify, "/etc/paxd.conf", IN_MODIFY);
    int watch_conf_dir = inotify_add_watch_x(inotify, "/etc/", IN_CREATE | IN_MOVED_TO);
    int watch_pacman = inotify_add_watch_x(inotify, "/var/lib/pacman/", IN_DELETE);

    for (;;) {
        ssize_t bytes_read = read(inotify, buf, sizeof buf);
        if (bytes_read == -1) {
            perror("read");
            return EXIT_FAILURE;
        }

        bool update = false;
        for (char *p = buf; p < buf + bytes_read; ) {
            struct inotify_event *event = (struct inotify_event *)p;
            p += sizeof (struct inotify_event) + event->len;

            if (event->wd == -1) {
                fprintf(stderr, "event queue overflowed\n");
                update = true;
            } else if (event->wd == watch_conf) {
                fprintf(stderr, "configuration file modified\n");
                update = true;
            } else if (event->wd == watch_conf_dir && !strcmp(event->name, "paxd.conf")) {
                fprintf(stderr, "configuration file created or replaced\n");
                // Watches are automatically released when the corresponding file is deleted. It is
                // not possible to improve on that with inotify_rm_watch because the descriptor may
                // have already been recycled for another file.
                watch_conf = inotify_add_watch_x(inotify, "/etc/paxd.conf", IN_MODIFY);
                update = true;
            } else if (event->wd == watch_pacman && !strcmp(event->name, "db.lck")) {
                fprintf(stderr, "pacman finished a transaction\n");
                update = true;
            }
        }
        if (update) {
            fprintf(stderr, "updating attributes\n");
            update_attributes();
        }
    }
}
