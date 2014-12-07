#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/xattr.h>

#include "flags.h"

void apply(const char *flags, size_t flags_len, const char *path) {
    if (setxattr(path, "user.pax.flags", flags, flags_len, 0) == -1) {
        if (errno != ENOENT) {
            fprintf(stderr, "could not apply attributes to %s: %m\n", path);
        }
    }
}

static void line_ignored(off_t n, const char *line) {
    fprintf(stderr, "ignored invalid line %lld in /etc/paxd.conf: %s", (long long)n, line);
}

void update_attributes(flag_handler handler) {
    FILE *conf = fopen("/etc/paxd.conf", "r");
    if (!conf) {
        perror("could not open /etc/paxd.conf");
        return;
    }

    char *line = NULL;
    size_t line_len = 0;

    for (off_t n = 1;; n++) {
        ssize_t bytes_read = getline(&line, &line_len, conf);
        if (bytes_read == -1) {
            if (ferror(conf)) {
                perror("failed to read line from /etc/paxd.conf");
            }
            break;
        }

        if (line[0] == '\n' || line[0] == '\0' || line[0] == '#') {
            continue; // ignore empty lines and comments
        }

        const char *flags = line + strspn(line, " \t"); // skip initial whitespace

        const char *split = flags + strcspn(flags, " \t"); // find the end of the specified flags
        if (*split == '\0') {
            line_ignored(n, line);
            continue;
        }

        const char *valid = "pemrs";
        const char *flag;
        for (flag = flags; flag < split; flag++) {
            if (!strchr(valid, tolower(*flag))) {
                break;
            }
        }
        if (flag != split) {
            line_ignored(n, line);
            continue;
        }

        const char *path = split + strspn(split, " \t"); // find the start of the path
        if (*path != '/') {
            line_ignored(n, line);
            continue;
        }

        if (line[bytes_read - 1] == '\n') {
            line[bytes_read - 1] = '\0';
        }

        size_t flags_len = (size_t)(split - flags);
        handler(flags, flags_len, path);
    }

    free(line);
    fclose(conf);
}
