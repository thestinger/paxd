#define _GNU_SOURCE

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/xattr.h>

static void apply(const char *flags, const char *path) {
    if (setxattr(path, "user.pax.flags", flags, strlen(flags), 0) == -1) {
        if (errno != ENOENT) {
            fprintf(stderr, "could not apply attributes to %s: %m\n", path);
        }
    }
}

static void update_attributes() {
    FILE *conf = fopen("/etc/paxd.conf", "r");
    if (!conf) {
        perror("could not open /etc/paxd.conf");
        return;
    }

    char *line = NULL;
    size_t line_len = 0;

    for (;;) {
        ssize_t bytes_read = getline(&line, &line_len, conf);
        if (bytes_read == -1) {
            if (ferror(conf)) {
                perror("failed to read line from /etc/paxd.conf");
                break;
            } else {
                break;
            }
        }

        if (line[0] == '\n' || line[0] == '\0' || line[0] == '#') {
            continue; // ignore empty lines and comments
        }

        char *saveptr;
        char *flags = strtok_r(line, " \t", &saveptr);
        if (!flags) {
            fprintf(stderr, "ignored invalid line in /etc/paxd.conf: %s", line);
            break;
        }
        char *path = strtok_r(NULL, " \t", &saveptr);
        if (!path || *path != '/') {
            fprintf(stderr, "ignored invalid line in /etc/paxd.conf: %s", line);
            break;
        }

        if (line[bytes_read - 1] == '\n') {
            line[bytes_read - 1] = '\0';
        }

        apply(flags, path);
    }

    free(line);
    fclose(conf);
}

int main(void) {
    update_attributes();

    alignas(struct inotify_event) char buf[sizeof(struct inotify_event) + NAME_MAX + 1];

    int inotify = inotify_init();
    if (inotify == -1) {
        perror("inotify");
        return EXIT_FAILURE;
    }
    int watch_conf = inotify_add_watch(inotify, "/etc/paxd.conf", IN_MODIFY);
    if (watch_conf == -1) {
        perror("inotify_add_watch");
        return EXIT_FAILURE;
    }
    int watch_pacman = inotify_add_watch(inotify, "/var/lib/pacman/", IN_DELETE);
    if (watch_pacman == -1) {
        perror("inotify_add_watch");
        return EXIT_FAILURE;
    }

    for (;;) {
        ssize_t bytes_read = read(inotify, buf, sizeof buf);
        if (bytes_read == -1) {
            perror("read");
            return EXIT_FAILURE;
        }

        for (char *p = buf; p < buf + bytes_read; ) {
            struct inotify_event *event = (struct inotify_event *)p;
            p += sizeof (struct inotify_event) + event->len;

            if (event->wd == watch_conf) {
                fprintf(stderr, "configuration modified, updating attributes\n");
                update_attributes();
                break;
            }

            if (event->wd == watch_pacman && !strcmp(event->name, "db.lck")) {
                fprintf(stderr, "pacman finished a transaction, updating attributes\n");
                update_attributes();
                break;
            }
        }
    }
}
