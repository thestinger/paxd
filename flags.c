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

static void line_ignored(const char *path, off_t n, const char *line) {
    fprintf(stderr, "ignored invalid line %lld in %s: %s", (long long)n, path, line);
}

void update_attributes(const char *config, flag_handler handler) {
    FILE *conf = fopen(config, "r");
    if (!conf) {
        fprintf(stderr, "could not open %s: %m\n", config);
        return;
    }

    char *line = NULL;
    size_t line_len = 0;

    for (off_t n = 1;; n++) {
        ssize_t bytes_read = getline(&line, &line_len, conf);
        if (bytes_read == -1) {
            if (ferror(conf)) {
                fprintf(stderr, "failed to read line from %s: %m\n", config);
            }
            break;
        }

        if (line[0] == '\n' || line[0] == '\0' || line[0] == '#') {
            continue; // ignore empty lines and comments
        }

        const char *flags = line + strspn(line, " \t"); // skip initial whitespace

        const char *split = flags + strcspn(flags, " \t"); // find the end of the specified flags
        if (*split == '\0') {
            line_ignored(config, n, line);
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
            line_ignored(config, n, line);
            continue;
        }

        const char *path = split + strspn(split, " \t"); // find the start of the path
        if (*path != '/') {
            line_ignored(config, n, line);
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
