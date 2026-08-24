#ifndef FS_H
#define FS_H

void fs_install(void);
void fs_write(const char* name, const char* content);
const char* fs_read(const char* name);
void fs_list(void (*print_fn)(const char*));

#endif