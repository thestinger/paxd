#include <glib.h>
#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/inotify.h>

#include "flags.h"

#define UNUSED __attribute__((unused))

static int inotify_add_watch_x(int fd, const char *pathname, uint32_t mask) {
    int watch = inotify_add_watch(fd, pathname, mask);
    if (watch == -1) {
        perror("inotify_add_watch");
        exit(EXIT_FAILURE);
    }
    return watch;
}

static int inotify = -1;
static int watch_conf = -1;
static int watch_conf_dir = -1;

// int fd -> char *path
static GHashTable *watch_to_path = NULL;

// char *path -> struct dir_watch *info
static GHashTable *path_table = NULL;

// char *path -> char *exception
static GHashTable *exception_table = NULL;

struct dir_watch {
    // char *path -> NULL
    GHashTable *child_set;
    int watch;
};

static void dir_watch_destroy(void *data) {
    struct dir_watch *info = data;
    g_hash_table_destroy(info->child_set);
    g_free(info);
}

static void reinitialize_table(GHashTable **table, GHashFunc hash_fn, GEqualFunc key_equal_fn,
                               GDestroyNotify key_destroy_fn, GDestroyNotify value_destroy_fn) {
    if (*table) {
        g_hash_table_remove_all(*table);
    } else {
        *table = g_hash_table_new_full(hash_fn, key_equal_fn, key_destroy_fn, value_destroy_fn);
    }
}

static void handler(const char *flags, size_t flags_len, const char *path) {
    if (g_hash_table_contains(exception_table, path)) {
        return; // duplicate exception
    }

    char *value = g_malloc(flags_len + 1);
    memcpy(value, flags, flags_len);
    value[flags_len] = '\0';

    g_hash_table_insert(exception_table, g_strdup(path), value);
    apply(flags, flags_len, path);

    char *path_full = g_strdup(path);
    char *path_prefix = g_strdup(path);

    do {
        dirname(path_prefix);

        struct dir_watch *info = g_hash_table_lookup(path_table, path_prefix);
        if (info) {
            if (!g_hash_table_contains(info->child_set, path_prefix)) {
                g_hash_table_insert(info->child_set, g_strdup(path_full), NULL);
            }
        } else {
            info = g_malloc(sizeof(struct dir_watch));
            g_hash_table_insert(path_table, g_strdup(path_prefix), info);

            info->child_set = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
            g_hash_table_insert(info->child_set, g_strdup(path_full), NULL);

            info->watch = inotify_add_watch(inotify, path_prefix, IN_CREATE | IN_MOVED_TO);
            if (info->watch != -1) {
                g_hash_table_insert(watch_to_path, GINT_TO_POINTER(info->watch),
                                    g_strdup(path_prefix));
            }
        }

        strcpy(path_full, path_prefix);
    } while (strcmp(path_prefix, "/"));

    g_free(path_full);
    g_free(path_prefix);
}

static void reinitialize(void) {
    if (inotify != -1) {
        close(inotify);
    }

    inotify = inotify_init();
    if (inotify == -1) {
        perror("inotify");
        return;
    }

    watch_conf = inotify_add_watch(inotify, "/etc/paxd.conf", IN_MODIFY);
    watch_conf_dir = inotify_add_watch_x(inotify, "/etc/", IN_CREATE | IN_MOVED_TO);

    reinitialize_table(&watch_to_path, NULL, NULL, NULL, g_free);
    reinitialize_table(&path_table, g_str_hash, g_str_equal, g_free, dir_watch_destroy);
    reinitialize_table(&exception_table, g_str_hash, g_str_equal, g_free, g_free);

    update_attributes(handler);
}

static void reinitialize_watch_tree(const char *path);

static void reinitialize_watch_tree_cb(void *key, UNUSED void *value, UNUSED void *user_data) {
    reinitialize_watch_tree(key);
}

static void reinitialize_watch_tree(const char *path) {
    struct dir_watch *info = g_hash_table_lookup(path_table, path);
    if (info) {
        g_hash_table_remove(watch_to_path, GINT_TO_POINTER(info->watch));
        int watch = inotify_add_watch(inotify, path, IN_CREATE | IN_MOVED_TO);
        if (watch != -1) {
            g_hash_table_insert(watch_to_path, GINT_TO_POINTER(watch), g_strdup(path));
        }
        g_hash_table_foreach(info->child_set, reinitialize_watch_tree_cb, NULL);
    } else {
        char *flags = g_hash_table_lookup(exception_table, path);
        if (flags) {
            printf("setting `%s` on `%s`", flags, path);
            apply(flags, strlen(flags), path);
        }
    }
}

static void handle_exception_event(struct inotify_event *event) {
    char *path_prefix = g_hash_table_lookup(watch_to_path, GINT_TO_POINTER(event->wd));
    if (!path_prefix) {
        return;
    }
    char *path = g_build_filename(path_prefix, event->name, NULL);
    reinitialize_watch_tree(path);
    g_free(path);
}

int main(void) {
    reinitialize();

    alignas(struct inotify_event) char buf[(sizeof(struct inotify_event) + NAME_MAX + 1) * 64];

    for (;;) {
        ssize_t bytes_read = read(inotify, buf, sizeof buf);
        if (bytes_read == -1) {
            perror("read");
            return EXIT_FAILURE;
        }

        for (char *p = buf; p < buf + bytes_read; ) {
            struct inotify_event *event = (struct inotify_event *)p;
            p += sizeof (struct inotify_event) + event->len;

            if (event->wd == -1) {
                fprintf(stderr, "event queue overflowed\n");
                reinitialize();
            } else if (event->wd == watch_conf) {
                fprintf(stderr, "configuration file modified\n");
                reinitialize();
            } else if (event->wd == watch_conf_dir && !strcmp(event->name, "paxd.conf")) {
                fprintf(stderr, "configuration file created or replaced\n");
                watch_conf = inotify_add_watch(inotify, "/etc/paxd.conf", IN_MODIFY);
                reinitialize();
            } else {
                handle_exception_event(event);
            }
        }
    }
}
