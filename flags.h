#ifndef FLAGS_H
#define FLAGS_H

typedef void (*flag_handler)(const char *flags, size_t flags_len, const char *path);
void update_attributes(flag_handler handler);
void apply(const char *flags, size_t flags_len, const char *path);

#endif
