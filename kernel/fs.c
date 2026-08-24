#include "fs.h"
#include "kheap.h"
#include <stddef.h>

#define MAX_FILES 16
#define MAX_NAME_LEN 32

typedef struct {
    char name[MAX_NAME_LEN];
    char* content;
    int used;
} file_t;

static file_t files[MAX_FILES];

static int str_eq_fs(const char* a, const char* b) {
    int i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == '\0' && b[i] == '\0';
}

static void str_copy(char* dest, const char* src, int max) {
    int i = 0;
    while (src[i] != '\0' && i < max - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static int str_len(const char* s) {
    int len = 0;
    while (s[len] != '\0') len++;
    return len;
}

void fs_install(void) {
    for (int i = 0; i < MAX_FILES; i++) {
        files[i].used = 0;
    }
}

void fs_write(const char* name, const char* content) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && str_eq_fs(files[i].name, name)) {
            files[i].content = (char*) kmalloc(str_len(content) + 1);
            str_copy(files[i].content, content, str_len(content) + 1);
            return;
        }
    }

    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].used) {
            str_copy(files[i].name, name, MAX_NAME_LEN);
            files[i].content = (char*) kmalloc(str_len(content) + 1);
            str_copy(files[i].content, content, str_len(content) + 1);
            files[i].used = 1;
            return;
        }
    }
}

const char* fs_read(const char* name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && str_eq_fs(files[i].name, name)) {
            return files[i].content;
        }
    }
    return 0;
}

void fs_list(void (*print_fn)(const char*)) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used) {
            print_fn(files[i].name);
            print_fn("\n");
        }
    }
}