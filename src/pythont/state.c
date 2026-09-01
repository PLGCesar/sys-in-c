#include "pythont.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

char func_buffer[MAX_CODE_SZ], main_buffer[MAX_CODE_SZ];
size_t func_pos, main_pos;
char declared_vars[MAX_VARS][64];
int declared_var_count;
int block_indent[MAX_INDENTS];
block_type_t block_type[MAX_INDENTS];
int block_top, pending_block, inside_function;
block_type_t pending_type = BLOCK_LOOP;

const char *get_tmp_dir(void) {
    const char *tmp = getenv("TMPDIR");
    if (tmp && *tmp && access(tmp, W_OK) == 0) return tmp;
    if (access("/data/data/com.termux/files/usr/tmp", W_OK) == 0) return "/data/data/com.termux/files/usr/tmp";
    if (access("/tmp", W_OK) == 0) return "/tmp";
    return ".";
}

int is_var_declared(const char *name) {
    for (int i = 0; i < declared_var_count; i++) if (strcmp(declared_vars[i], name) == 0) return 1;
    return 0;
}

void register_var(const char *name) {
    if (declared_var_count < MAX_VARS && !is_var_declared(name)) {
        strncpy(declared_vars[declared_var_count], name, 63);
        declared_vars[declared_var_count][63] = '\0';
        declared_var_count++;
    }
}

void emit(const char *fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    size_t l = strlen(buf);
    if (inside_function) {
        if (func_pos + l < MAX_CODE_SZ - 1) { strcpy(func_buffer + func_pos, buf); func_pos += l; }
    } else if (main_pos + l < MAX_CODE_SZ - 1) {
        strcpy(main_buffer + main_pos, buf); main_pos += l;
    }
}

void reset_state(void) {
    func_pos = main_pos = 0;
    declared_var_count = block_top = pending_block = inside_function = 0;
    pending_type = BLOCK_LOOP;
    func_buffer[0] = main_buffer[0] = '\0';
}
