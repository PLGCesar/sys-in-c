#ifndef PYTHONT_H
#define PYTHONT_H

#include <stddef.h>
#include <stdint.h>

#define MAX_CODE_SZ (512 * 1024)
#define MAX_VARS 256
#define MAX_INDENTS 64

typedef enum { BLOCK_FUNC = 1, BLOCK_IF, BLOCK_LOOP } block_type_t;

extern char func_buffer[MAX_CODE_SZ], main_buffer[MAX_CODE_SZ];
extern size_t func_pos, main_pos;
extern char declared_vars[MAX_VARS][64];
extern int declared_var_count;
extern int block_indent[MAX_INDENTS];
extern block_type_t block_type[MAX_INDENTS];
extern int block_top, pending_block, inside_function;
extern block_type_t pending_type;

const char *get_tmp_dir(void);
int is_var_declared(const char *name);
void register_var(const char *name);
void emit(const char *fmt, ...);
void replace_operators(char *expr);
void transpile_print(const char *args_str);
void handle_dedent(int new_indent, int is_else_or_elif);
void transpile_line(char *line, int indent);
void print_help(void);
void reset_state(void);

#endif
