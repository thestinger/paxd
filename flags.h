#ifndef FLAGS_H
#define FLAGS_H

typedef void (*flag_handler)(const char *flags, size_t flags_len, const char *path);
void update_attributes(const char *config, flag_handler handler);
void set_pax_flags(const char *flags, size_t flags_len, const char *path);

#endif
