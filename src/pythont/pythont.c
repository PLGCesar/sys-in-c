#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include "../libutilipc/utilipc.h"

#define MAX_CODE_SZ   (512 * 1024)
#define MAX_VARS      256
#define MAX_INDENTS   64

#define COLOR_RESET   "\033[0m"
#define COLOR_TITLE   "\033[1;35m"
#define COLOR_OK      "\033[1;32m"
#define COLOR_ERR     "\033[1;31m"
#define COLOR_TAG     "\033[1;33m"
#define COLOR_VAL     "\033[1;36m"
#define COLOR_MUTED   "\033[0;90m"

typedef enum {
    BLOCK_FUNC = 1,
    BLOCK_IF,
    BLOCK_LOOP
} block_type_t;

static char func_buffer[MAX_CODE_SZ];
static char main_buffer[MAX_CODE_SZ];
static size_t func_pos = 0;
static size_t main_pos = 0;

static char declared_vars[MAX_VARS][64];
static int declared_var_count = 0;

static int block_indent[MAX_INDENTS];
static block_type_t block_type[MAX_INDENTS];
static int block_top = 0;

static int pending_block = 0;
static block_type_t pending_type = BLOCK_LOOP;
static int inside_function = 0;

static const char *get_tmp_dir(void) {
    const char *tmp = getenv("TMPDIR");
    if (tmp && strlen(tmp) > 0 && access(tmp, W_OK) == 0) return tmp;
    if (access("/data/data/com.termux/files/usr/tmp", W_OK) == 0) return "/data/data/com.termux/files/usr/tmp";
    if (access("/tmp", W_OK) == 0) return "/tmp";
    return ".";
}

static void print_help(void) {
    printf("%s========================================================%s\n", COLOR_TITLE, COLOR_RESET);
    printf("%s[ pythont 2.0 - Python to Native C Transpiler & JIT ]%s\n", COLOR_TITLE, COLOR_RESET);
    printf("%s========================================================%s\n", COLOR_TITLE, COLOR_RESET);
    printf("Usage:\n");
    printf("  pythont <SCRIPT.py>              (Transpila, compila com gcc e executa)\n");
    printf("  pythont <SCRIPT.py> -c, --emit-c (Apenas exibe o codigo C gerado)\n");
    printf("  pythont <SCRIPT.py> -o <BINARIO> (Compila para binario nativo permanente)\n");
    printf("  pythont --help                   (Exibe esta ajuda)\n\n");
    printf("Recursos Suportados:\n");
    printf("  • Listas & Vetores: nums = [1, 2, 3] e acesso nums[0]\n");
    printf("  • Operadores: +=, -=, *=, /=, %=, // (divisao int)\n");
    printf("  • Built-ins: len(), min(), max(), abs(), input(), int()\n");
    printf("  • Controle: break, continue, pass, if/elif/else, while, for range\n");
    printf("  • def funcoes(args): com retornos e recursao\n");
    printf("%s========================================================%s\n", COLOR_TITLE, COLOR_RESET);
}

static int is_var_declared(const char *name) {
    for (int i = 0; i < declared_var_count; i++) {
        if (strcmp(declared_vars[i], name) == 0) return 1;
    }
    return 0;
}

static void register_var(const char *name) {
    if (declared_var_count < MAX_VARS && !is_var_declared(name)) {
        strncpy(declared_vars[declared_var_count++], name, 63);
    }
}

static void emit(const char *fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (inside_function) {
        size_t l = strlen(buf);
        if (func_pos + l < MAX_CODE_SZ - 1) {
            strcpy(func_buffer + func_pos, buf);
            func_pos += l;
        }
    } else {
        size_t l = strlen(buf);
        if (main_pos + l < MAX_CODE_SZ - 1) {
            strcpy(main_buffer + main_pos, buf);
            main_pos += l;
        }
    }
}

static void replace_operators(char *expr) {
    char tmp[2048] = "";
    size_t t = 0;
    size_t len = strlen(expr);

    for (size_t i = 0; i < len; i++) {
        if (strncmp(expr + i, " and ", 5) == 0) {
            strcat(tmp + t, " && "); t += 4; i += 4;
        } else if (strncmp(expr + i, " or ", 4) == 0) {
            strcat(tmp + t, " || "); t += 4; i += 3;
        } else if (strncmp(expr + i, "not ", 4) == 0) {
            strcat(tmp + t, "!"); t += 1; i += 3;
        } else if (strncmp(expr + i, "True", 4) == 0 && !isalnum((unsigned char)expr[i+4])) {
            strcat(tmp + t, "1"); t += 1; i += 3;
        } else if (strncmp(expr + i, "False", 5) == 0 && !isalnum((unsigned char)expr[i+5])) {
            strcat(tmp + t, "0"); t += 1; i += 4;
        } else if (strncmp(expr + i, "None", 4) == 0 && !isalnum((unsigned char)expr[i+4])) {
            strcat(tmp + t, "NULL"); t += 4; i += 3;
        } else if (strncmp(expr + i, "min(", 4) == 0) {
            strcat(tmp + t, "py_min("); t += 7; i += 3;
        } else if (strncmp(expr + i, "max(", 4) == 0) {
            strcat(tmp + t, "py_max("); t += 7; i += 3;
        } else if (strncmp(expr + i, "abs(", 4) == 0) {
            strcat(tmp + t, "py_abs("); t += 7; i += 3;
        } else if (strncmp(expr + i, "int(", 4) == 0) {
            strcat(tmp + t, "py_int("); t += 7; i += 3;
        } else if (strncmp(expr + i, "input(", 6) == 0) {
            strcat(tmp + t, "py_input("); t += 9; i += 5;
        } else if (strncmp(expr + i, "//", 2) == 0) {
            strcat(tmp + t, "/"); t += 1; i += 1;
        } else {
            tmp[t++] = expr[i];
            tmp[t] = '\0';
        }
    }
    strcpy(expr, tmp);
}

static void transpile_print(const char *args_str) {
    char fmt_str[512] = "";
    char val_list[2048] = "";
    int first = 1;

    const char *p = args_str;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        const char *token_start = p;
        int paren_depth = 0;
        int bracket_depth = 0;
        int in_str = 0;
        char quote_char = 0;

        while (*p) {
            char c = *p;
            if (in_str) {
                if (c == '\\' && *(p + 1)) {
                    p += 2;
                    continue;
                }
                if (c == quote_char) {
                    in_str = 0;
                }
            } else {
                if (c == '"' || c == '\'') {
                    in_str = 1;
                    quote_char = c;
                } else if (c == '(') {
                    paren_depth++;
                } else if (c == ')') {
                    if (paren_depth > 0) paren_depth--;
                } else if (c == '[') {
                    bracket_depth++;
                } else if (c == ']') {
                    if (bracket_depth > 0) bracket_depth--;
                } else if (c == ',' && paren_depth == 0 && bracket_depth == 0) {
                    break;
                }
            }
            p++;
        }

        size_t token_len = p - token_start;
        char token[512];
        if (token_len >= sizeof(token)) token_len = sizeof(token) - 1;
        strncpy(token, token_start, token_len);
        token[token_len] = '\0';

        if (*p == ',') p++;

        char *t = token;
        while (*t == ' ' || *t == '\t') t++;
        size_t tl = strlen(t);
        while (tl > 0 && (t[tl-1] == ' ' || t[tl-1] == '\t')) token[--tl] = '\0';
        if (tl == 0) continue;

        if (!first) strcat(fmt_str, " ");

        replace_operators(t);

        if (t[0] == '"' || t[0] == '\'') {
            strcat(fmt_str, "%s");
        } else if (strchr(t, '.') && !strchr(t, '(')) {
            strcat(fmt_str, "%f");
        } else {
            strcat(fmt_str, "%lld");
        }

        if (!first) strcat(val_list, ", ");

        if (t[0] != '"' && t[0] != '\'' && (!strchr(t, '.') || strchr(t, '('))) {
            char cast_val[512];
            snprintf(cast_val, sizeof(cast_val), "(long long)(%s)", t);
            strcat(val_list, cast_val);
        } else {
            strcat(val_list, t);
        }

        first = 0;
    }

    if (strlen(val_list) > 0) {
        emit("    printf(\"%s\\n\", %s);\n", fmt_str, val_list);
    } else {
        emit("    putchar('\\n');\n");
    }
}

static void handle_dedent(int new_indent, int is_else_or_elif) {
    while (block_top > 0 && new_indent < block_indent[block_top - 1]) {
        block_type_t popped = block_type[block_top - 1];
        block_top--;

        if (popped == BLOCK_FUNC) {
            emit("}\n");
            inside_function = 0;
        } else if (!is_else_or_elif || block_top > 0) {
            emit("    }\n");
        }
    }
}

static void transpile_line(char *line, int indent) {
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0' || *line == '#') return;

    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n' || line[len - 1] == ' ')) {
        line[--len] = '\0';
    }

    int is_else = (strcmp(line, "else:") == 0);
    int is_elif = (strncmp(line, "elif ", 5) == 0 && line[len - 1] == ':');

    handle_dedent(indent, is_else || is_elif);

    if (pending_block) {
        if (block_top < MAX_INDENTS) {
            block_indent[block_top] = indent;
            block_type[block_top] = pending_type;
            block_top++;
        }
        pending_block = 0;
    }

    if (strncmp(line, "def ", 4) == 0 && line[len - 1] == ':') {
        inside_function = 1;
        line[len - 1] = '\0';
        char *paren = strchr(line + 4, '(');
        if (paren) {
            *paren = '\0';
            char *fn_name = line + 4;
            char *args_str = paren + 1;
            char *close_p = strrchr(args_str, ')');
            if (close_p) *close_p = '\0';

            emit("\nint64_t %s(", fn_name);
            char *arg = strtok(args_str, ",");
            int fst = 1;
            while (arg) {
                while (*arg == ' ') arg++;
                if (!fst) emit(", ");
                emit("int64_t %s", arg);
                register_var(arg);
                fst = 0;
                arg = strtok(NULL, ",");
            }
            if (fst) emit("void");
            emit(") {\n");
            pending_block = 1;
            pending_type = BLOCK_FUNC;
        }
        return;
    }

    if (strncmp(line, "return ", 7) == 0) {
        char expr[512];
        strncpy(expr, line + 7, sizeof(expr) - 1);
        replace_operators(expr);
        emit("    return %s;\n", expr);
        return;
    }
    if (strcmp(line, "return") == 0) { emit("    return 0;\n"); return; }
    if (strcmp(line, "break") == 0) { emit("    break;\n"); return; }
    if (strcmp(line, "continue") == 0) { emit("    continue;\n"); return; }
    if (strcmp(line, "pass") == 0) { emit("    /* pass */;\n"); return; }

    if (strncmp(line, "for ", 4) == 0 && strstr(line, " in range(") && line[len - 1] == ':') {
        line[len - 1] = '\0';
        char var_name[64] = "";
        char *in_ptr = strstr(line + 4, " in range(");
        if (in_ptr) {
            *in_ptr = '\0';
            strncpy(var_name, line + 4, sizeof(var_name) - 1);
            while (var_name[strlen(var_name)-1] == ' ') var_name[strlen(var_name)-1] = '\0';

            char *r_args = in_ptr + 10;
            char *close_p = strrchr(r_args, ')');
            if (close_p) *close_p = '\0';

            char *arg1 = strtok(r_args, ",");
            char *arg2 = strtok(NULL, ",");
            char *arg3 = strtok(NULL, ",");

            register_var(var_name);
            if (!arg2) {
                emit("    for (int64_t %s = 0; %s < %s; %s++) {\n", var_name, var_name, arg1, var_name);
            } else if (!arg3) {
                emit("    for (int64_t %s = %s; %s < %s; %s++) {\n", var_name, arg1, var_name, arg2, var_name);
            } else {
                emit("    for (int64_t %s = %s; %s < %s; %s += %s) {\n", var_name, arg1, var_name, arg2, var_name, arg3);
            }
            pending_block = 1;
            pending_type = BLOCK_LOOP;
        }
        return;
    }

    if (strncmp(line, "while ", 6) == 0 && line[len - 1] == ':') {
        line[len - 1] = '\0';
        char cond[512];
        strncpy(cond, line + 6, sizeof(cond) - 1);
        replace_operators(cond);
        emit("    while (%s) {\n", cond);
        pending_block = 1;
        pending_type = BLOCK_LOOP;
        return;
    }

    if (strncmp(line, "if ", 3) == 0 && line[len - 1] == ':') {
        line[len - 1] = '\0';
        char cond[512];
        strncpy(cond, line + 3, sizeof(cond) - 1);
        replace_operators(cond);
        emit("    if (%s) {\n", cond);
        pending_block = 1;
        pending_type = BLOCK_IF;
        return;
    }

    if (is_elif) {
        line[len - 1] = '\0';
        char cond[512];
        strncpy(cond, line + 5, sizeof(cond) - 1);
        replace_operators(cond);
        emit("    } else if (%s) {\n", cond);
        pending_block = 1;
        pending_type = BLOCK_IF;
        return;
    }

    if (is_else) {
        emit("    } else {\n");
        pending_block = 1;
        pending_type = BLOCK_IF;
        return;
    }

    if (strncmp(line, "print(", 6) == 0 && line[len - 1] == ')') {
        line[len - 1] = '\0';
        transpile_print(line + 6);
        return;
    }

    char *op_eq = NULL;
    if ((op_eq = strstr(line, "+=")) || (op_eq = strstr(line, "-=")) ||
        (op_eq = strstr(line, "*=")) || (op_eq = strstr(line, "/=")) ||
        (op_eq = strstr(line, "%="))) {
        char op_symbol[3] = { op_eq[0], op_eq[1], '\0' };
        *op_eq = '\0';
        char *vstart = line;
        while (*vstart == ' ') vstart++;
        char *vend = vstart + strlen(vstart) - 1;
        while (vend > vstart && isspace((unsigned char)*vend)) *vend-- = '\0';

        char *val_expr = op_eq + 2;
        while (*val_expr == ' ') val_expr++;

        replace_operators(val_expr);
        emit("    %s %s %s;\n", vstart, op_symbol, val_expr);
        return;
    }

    char *eq = strchr(line, '=');
    if (eq && line[0] != '=' && *(eq + 1) != '=' && *(eq - 1) != '!' && *(eq - 1) != '<' && *(eq - 1) != '>') {
        *eq = '\0';
        char var_name[64];
        char val_expr[512];
        strncpy(var_name, line, sizeof(var_name) - 1);
        strncpy(val_expr, eq + 1, sizeof(val_expr) - 1);

        size_t vl = strlen(var_name);
        while (vl > 0 && (var_name[vl-1] == ' ' || var_name[vl-1] == '\t')) var_name[--vl] = '\0';
        char *vstart = var_name;
        while (*vstart == ' ' || *vstart == '\t') vstart++;

        char *vexpr_start = val_expr;
        while (*vexpr_start == ' ' || *vexpr_start == '\t') vexpr_start++;
        size_t elen = strlen(vexpr_start);
        while (elen > 0 && (vexpr_start[elen-1] == ' ' || vexpr_start[elen-1] == '\t')) vexpr_start[--elen] = '\0';

        if (vexpr_start[0] == '[' && vexpr_start[elen - 1] == ']') {
            vexpr_start[0] = '{';
            vexpr_start[elen - 1] = '}';
            int elem_count = 1;
            for (size_t c = 0; vexpr_start[c]; c++) if (vexpr_start[c] == ',') elem_count++;

            register_var(vstart);
            emit("    int64_t %s[%d] = %s;\n", vstart, elem_count, vexpr_start);
            char len_var[70];
            snprintf(len_var, sizeof(len_var), "len_%s", vstart);
            register_var(len_var);
            emit("    int64_t %s = %d;\n", len_var, elem_count);
            return;
        }

        replace_operators(val_expr);

        if (!is_var_declared(vstart) && !strchr(vstart, '[')) {
            register_var(vstart);
            if (val_expr[0] == '"' || val_expr[0] == '\'') {
                emit("    const char *%s = %s;\n", vstart, val_expr);
            } else if (strchr(val_expr, '.')) {
                emit("    double %s = %s;\n", vstart, val_expr);
            } else {
                emit("    int64_t %s = %s;\n", vstart, val_expr);
            }
        } else {
            emit("    %s = %s;\n", vstart, val_expr);
        }
        return;
    }

    replace_operators(line);
    emit("    %s;\n", line);
}

int main(int argc, char *argv[]) {
    utilipc_init();

    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_help();
        utilipc_close();
        return 0;
    }

    const char *py_file = argv[1];
    const char *out_bin = NULL;
    int emit_c_only = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--emit-c") == 0) {
            emit_c_only = 1;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_bin = argv[++i];
        }
    }

    FILE *fp = fopen(py_file, "r");
    if (!fp) {
        fprintf(stderr, "pythont: erro ao abrir '%s': %s\n", py_file, strerror(errno));
        utilipc_close();
        return 1;
    }

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        int indent = 0;
        while (line[indent] == ' ') indent++;
        if (line[indent] == '\t') indent += 4;

        char *trimmed = line + indent;
        if (*trimmed == '\0' || *trimmed == '\n' || *trimmed == '\r' || *trimmed == '#') continue;

        transpile_line(line, indent);
    }
    fclose(fp);

    handle_dedent(0, 0);

    char final_c_code[MAX_CODE_SZ];
    snprintf(final_c_code, sizeof(final_c_code),
        "/* Código C Gerado Automaticamente pelo pythont 2.0 */\n"
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "#include <stdint.h>\n"
        "#include <stdbool.h>\n"
        "#include <string.h>\n"
        "#include <math.h>\n\n"
        "#define py_min(a, b) (((a) < (b)) ? (a) : (b))\n"
        "#define py_max(a, b) (((a) > (b)) ? (a) : (b))\n"
        "#define py_abs(a)    llabs((int64_t)(a))\n"
        "static inline int64_t py_int(const char *s) { return (int64_t)strtoll(s, NULL, 10); }\n"
        "static inline char *py_input(const char *prompt) {\n"
        "    if (prompt && *prompt) { printf(\"%%s\", prompt); fflush(stdout); }\n"
        "    static char in_buf[1024];\n"
        "    if (!fgets(in_buf, sizeof(in_buf), stdin)) return \"\";\n"
        "    size_t l = strlen(in_buf);\n"
        "    while (l > 0 && (in_buf[l-1] == '\\r' || in_buf[l-1] == '\\n')) in_buf[--l] = '\\0';\n"
        "    return in_buf;\n"
        "}\n\n"
        "%s\n"
        "int main(int argc, char *argv[]) {\n"
        "    (void)argc; (void)argv;\n"
        "%s\n"
        "    return 0;\n"
        "}\n",
        func_buffer, main_buffer);

    if (emit_c_only) {
        printf("%s\n", final_c_code);
        utilipc_close();
        return 0;
    }

    const char *tmp_dir = get_tmp_dir();
    char tmp_c_path[512];
    snprintf(tmp_c_path, sizeof(tmp_c_path), "%s/pythont_%d.c", tmp_dir, getpid());

    FILE *c_fp = fopen(tmp_c_path, "w");
    if (!c_fp) {
        snprintf(tmp_c_path, sizeof(tmp_c_path), "pythont_%d.c", getpid());
        c_fp = fopen(tmp_c_path, "w");
    }
    if (!c_fp) {
        fprintf(stderr, "pythont: falha ao criar arquivo C temporario\n");
        utilipc_close();
        return 1;
    }
    fputs(final_c_code, c_fp);
    fclose(c_fp);

    char bin_path[512];
    int run_after = 0;

    if (out_bin) {
        strncpy(bin_path, out_bin, sizeof(bin_path) - 1);
    } else {
        snprintf(bin_path, sizeof(bin_path), "%s/pythont_bin_%d", tmp_dir, getpid());
        run_after = 1;
    }

    char compile_cmd[1024];
    snprintf(compile_cmd, sizeof(compile_cmd), "gcc -Wall -Wextra -O2 %s -o %s -lm", tmp_c_path, bin_path);

    int comp_res = system(compile_cmd);
    unlink(tmp_c_path);

    if (comp_res != 0) {
        fprintf(stderr, "pythont: erro de compilacao do codigo C gerado\n");
        utilipc_close();
        return 1;
    }

    if (run_after) {
        int ret = system(bin_path);
        unlink(bin_path);
        utilipc_close();
        return WEXITSTATUS(ret);
    } else {
        printf("  \033[1;32m[✔ SUCESSO]\033[0m Binário nativo gerado em: \033[1;36m%s\033[0m\n", bin_path);
    }

    utilipc_close();
    return 0;
}
