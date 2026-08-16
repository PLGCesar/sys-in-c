#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>
#include <termios.h>
#include "../libutilipc/utilipc.h"

#define MAX_VARS 64
#define MAX_HISTORY 100

typedef struct {
    char name[32];
    double value;
} Variable;

static Variable var_table[MAX_VARS];
static int var_count = 0;
static double last_ans = 0.0;
static const char *expr;

static char history[MAX_HISTORY][512];
static int history_count = 0;

static void add_history(const char *line) {
    if (!line || strlen(line) == 0) return;
    if (history_count > 0 && strcmp(history[history_count - 1], line) == 0) return;

    if (history_count < MAX_HISTORY) {
        strncpy(history[history_count], line, sizeof(history[0]) - 1);
        history[history_count][sizeof(history[0]) - 1] = '\0';
        history_count++;
    } else {
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            strcpy(history[i], history[i + 1]);
        }
        strncpy(history[MAX_HISTORY - 1], line, sizeof(history[0]) - 1);
        history[MAX_HISTORY - 1][sizeof(history[0]) - 1] = '\0';
    }
}

static int custom_readline(const char *prompt, char *out_buf, size_t max_len) {
    struct termios orig_term, raw_term;
    if (tcgetattr(STDIN_FILENO, &orig_term) != 0) {
        // Fallback para pipes e entradas não-interativas
        printf("%s", prompt);
        fflush(stdout);
        if (!fgets(out_buf, max_len, stdin)) return 0;
        out_buf[strcspn(out_buf, "\r\n")] = '\0';
        return 1;
    }

    raw_term = orig_term;
    raw_term.c_lflag &= ~(ICANON | ECHO);
    raw_term.c_cc[VMIN] = 1;
    raw_term.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw_term);

    printf("%s", prompt);
    fflush(stdout);

    size_t len = 0;
    out_buf[0] = '\0';
    int hist_pos = history_count;
    char temp_edit[512] = "";

    while (1) {
        char c;
        if (read(STDIN_FILENO, &c, 1) <= 0) {
            tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);
            return 0;
        }

        if (c == '\r' || c == '\n') {
            printf("\n");
            break;
        } else if (c == 127 || c == '\b') { // Backspace
            if (len > 0) {
                len--;
                out_buf[len] = '\0';
                printf("\b \b");
                fflush(stdout);
            }
        } else if (c == 3) { // Ctrl+C
            tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);
            printf("^C\n");
            out_buf[0] = '\0';
            return 1;
        } else if (c == 4) { // Ctrl+D (EOF)
            if (len == 0) {
                tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);
                return 0;
            }
        } else if (c == 27) { // Sequência ANSI / Teclas especiais
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) <= 0) continue;
            if (read(STDIN_FILENO, &seq[1], 1) <= 0) continue;

            if (seq[0] == '[') {
                if (seq[1] == 'A') { // Seta CIMA
                    if (history_count > 0) {
                        if (hist_pos == history_count) {
                            strncpy(temp_edit, out_buf, sizeof(temp_edit) - 1);
                        }
                        if (hist_pos > 0) {
                            hist_pos--;
                        }
                        strncpy(out_buf, history[hist_pos], max_len - 1);
                        out_buf[max_len - 1] = '\0';
                        len = strlen(out_buf);

                        printf("\r\033[K%s%s", prompt, out_buf);
                        fflush(stdout);
                    }
                } else if (seq[1] == 'B') { // Seta BAIXO
                    if (hist_pos < history_count) {
                        hist_pos++;
                        if (hist_pos == history_count) {
                            strncpy(out_buf, temp_edit, max_len - 1);
                        } else {
                            strncpy(out_buf, history[hist_pos], max_len - 1);
                        }
                        out_buf[max_len - 1] = '\0';
                        len = strlen(out_buf);

                        printf("\r\033[K%s%s", prompt, out_buf);
                        fflush(stdout);
                    }
                }
            }
        } else if (isprint((unsigned char)c)) {
            if (len < max_len - 1) {
                out_buf[len++] = c;
                out_buf[len] = '\0';
                putchar(c);
                fflush(stdout);
            }
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);
    return 1;
}

static double get_var(const char *name, int *found) {
    if (strcmp(name, "pi") == 0) { *found = 1; return 3.14159265358979323846; }
    if (strcmp(name, "e") == 0)  { *found = 1; return 2.71828182845904523536; }
    if (strcmp(name, "tau") == 0){ *found = 1; return 6.28318530717958647692; }
    if (strcmp(name, "ans") == 0){ *found = 1; return last_ans; }

    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_table[i].name, name) == 0) {
            *found = 1;
            return var_table[i].value;
        }
    }
    *found = 0;
    return 0.0;
}

static void set_var(const char *name, double val) {
    if (strcmp(name, "pi") == 0 || strcmp(name, "e") == 0 || strcmp(name, "tau") == 0 || strcmp(name, "ans") == 0) {
        return;
    }
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_table[i].name, name) == 0) {
            var_table[i].value = val;
            return;
        }
    }
    if (var_count < MAX_VARS) {
        strncpy(var_table[var_count].name, name, sizeof(var_table[var_count].name) - 1);
        var_table[var_count].name[sizeof(var_table[var_count].name) - 1] = '\0';
        var_table[var_count].value = val;
        var_count++;
    }
}

static void list_vars(void) {
    printf("==========================================\n");
    printf("[ calc - Stored Variables ]\n");
    printf("==========================================\n");
    printf("  • ans = %g\n", last_ans);
    printf("  • pi  = 3.14159265\n");
    printf("  • e   = 2.71828182\n");
    printf("  • tau = 6.28318530\n");
    for (int i = 0; i < var_count; i++) {
        printf("  • %s = %g\n", var_table[i].name, var_table[i].value);
    }
    printf("==========================================\n");
}

static double eval_priority_1(void);

static double eval_priority_4(void) {
    if (*expr == '-') {
        expr++;
        return -eval_priority_4();
    }
    if (*expr == '+') {
        expr++;
        return eval_priority_4();
    }
    if (*expr == '(') {
        expr++;
        double val = eval_priority_1();
        if (*expr == ')') expr++;
        return val;
    }

    if (expr[0] == '0' && (expr[1] == 'x' || expr[1] == 'X')) {
        expr += 2;
        char *end_ptr;
        unsigned long long hval = strtoull(expr, &end_ptr, 16);
        expr = end_ptr;
        return (double)hval;
    }
    if (expr[0] == '0' && (expr[1] == 'b' || expr[1] == 'B')) {
        expr += 2;
        char *end_ptr;
        unsigned long long bval = strtoull(expr, &end_ptr, 2);
        expr = end_ptr;
        return (double)bval;
    }
    if (expr[0] == '0' && (expr[1] == 'o' || expr[1] == 'O')) {
        expr += 2;
        char *end_ptr;
        unsigned long long oval = strtoull(expr, &end_ptr, 8);
        expr = end_ptr;
        return (double)oval;
    }

    if (strncmp(expr, "sqrt", 4) == 0)  { expr += 4; return sqrt(eval_priority_4()); }
    if (*expr == 'r' || *expr == 'R')   { expr++;    return sqrt(eval_priority_4()); }
    if (strncmp(expr, "sin", 3) == 0)   { expr += 3; return sin(eval_priority_4()); }
    if (strncmp(expr, "cos", 3) == 0)   { expr += 3; return cos(eval_priority_4()); }
    if (strncmp(expr, "tan", 3) == 0)   { expr += 3; return tan(eval_priority_4()); }
    if (strncmp(expr, "asin", 4) == 0)  { expr += 4; return asin(eval_priority_4()); }
    if (strncmp(expr, "acos", 4) == 0)  { expr += 4; return acos(eval_priority_4()); }
    if (strncmp(expr, "atan", 4) == 0)  { expr += 4; return atan(eval_priority_4()); }
    if (strncmp(expr, "log10", 5) == 0) { expr += 5; return log10(eval_priority_4()); }
    if (strncmp(expr, "log2", 4) == 0)  { expr += 4; return log2(eval_priority_4()); }
    if (strncmp(expr, "log", 3) == 0)   { expr += 3; return log(eval_priority_4()); }
    if (strncmp(expr, "abs", 3) == 0)   { expr += 3; return fabs(eval_priority_4()); }
    if (strncmp(expr, "floor", 5) == 0) { expr += 5; return floor(eval_priority_4()); }
    if (strncmp(expr, "ceil", 4) == 0)  { expr += 4; return ceil(eval_priority_4()); }
    if (strncmp(expr, "round", 5) == 0) { expr += 5; return round(eval_priority_4()); }

    if (isalpha((unsigned char)*expr) || *expr == '_') {
        char name[32];
        size_t idx = 0;
        while ((isalnum((unsigned char)*expr) || *expr == '_') && idx < sizeof(name) - 1) {
            name[idx++] = *expr++;
        }
        name[idx] = '\0';

        int found = 0;
        double val = get_var(name, &found);
        if (found) return val;
        printf("Error: Unknown variable or function '%s'\n", name);
        return 0.0;
    }

    char *end_ptr;
    double val = strtod(expr, &end_ptr);
    expr = end_ptr;
    return val;
}

static double eval_priority_3(void) {
    double val = eval_priority_4();
    while (*expr == '^') {
        expr++;
        double exp_val = eval_priority_4();
        val = pow(val, exp_val);
    }
    return val;
}

static double eval_priority_2(void) {
    double val = eval_priority_3();
    while (*expr == '*' || *expr == '/' || *expr == '%') {
        char op = *expr++;
        double next_val = eval_priority_3();
        if (op == '*') {
            val *= next_val;
        } else if (op == '/') {
            if (next_val == 0.0) {
                printf("Error: Division by zero!\n");
                return 0.0;
            }
            val /= next_val;
        } else if (op == '%') {
            if (next_val == 0.0) {
                printf("Error: Modulo by zero!\n");
                return 0.0;
            }
            val = fmod(val, next_val);
        }
    }
    return val;
}

static double eval_priority_1(void) {
    double val = eval_priority_2();
    while (*expr == '+' || *expr == '-') {
        char op = *expr++;
        double next_val = eval_priority_2();
        if (op == '+') val += next_val;
        else val -= next_val;
    }
    return val;
}

static double evaluate_expression(const char *raw_input) {
    char clean_buf[1024];
    size_t j = 0;

    for (size_t i = 0; raw_input[i] != '\0' && j < sizeof(clean_buf) - 1; i++) {
        unsigned char c = (unsigned char)raw_input[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;

        if ((c == 'x' || c == 'X') &&
            (i > 0 && (isdigit((unsigned char)raw_input[i-1]) || raw_input[i-1] == ')')) &&
            (isdigit((unsigned char)raw_input[i+1]) || raw_input[i+1] == '(')) {
            clean_buf[j++] = '*';
        }
        else if (c == '[' || c == '{') clean_buf[j++] = '(';
        else if (c == ']' || c == '}') clean_buf[j++] = ')';
        else if (c == 0xC3 && (unsigned char)raw_input[i+1] == 0xB7) {
            clean_buf[j++] = '/'; i++;
        }
        else if (c == 0xE2 && raw_input[i+1] != '\0' && raw_input[i+2] != '\0' &&
                 (unsigned char)raw_input[i+1] == 0x88 && (unsigned char)raw_input[i+2] == 0x9A) {
            clean_buf[j++] = 'r'; i += 2;
        }
        else clean_buf[j++] = (char)c;
    }
    clean_buf[j] = '\0';

    expr = clean_buf;
    double res = eval_priority_1();
    last_ans = res;
    return res;
}

static void print_result_full(double res) {
    printf("======================\n");
    printf("[Result: %g]\n", res);

    if (res >= 0.0 && res <= 18446744073709551615.0 && fabs(res - floor(res)) < 1e-9) {
        unsigned long long uval = (unsigned long long)res;
        char bin_buf[68];
        size_t bpos = 0;
        unsigned long long temp = uval;
        if (temp == 0) bin_buf[bpos++] = '0';
        else {
            char rev[68];
            int rpos = 0;
            while (temp > 0) {
                rev[rpos++] = (temp & 1) ? '1' : '0';
                temp >>= 1;
            }
            for (int i = rpos - 1; i >= 0; i--) bin_buf[bpos++] = rev[i];
        }
        bin_buf[bpos] = '\0';
        printf("  • Hex: 0x%llX | Bin: 0b%s\n", uval, bin_buf);
    }
    printf("======================\n");
}

static void process_line(const char *input) {
    const char *eq = strchr(input, '=');

    if (eq != NULL) {
        char var_name[32];
        size_t name_len = eq - input;
        while (name_len > 0 && isspace((unsigned char)input[name_len - 1])) name_len--;

        size_t start = 0;
        while (start < name_len && isspace((unsigned char)input[start])) start++;

        if (name_len - start >= sizeof(var_name)) name_len = start + sizeof(var_name) - 1;
        strncpy(var_name, input + start, name_len - start);
        var_name[name_len - start] = '\0';

        int valid = (strlen(var_name) > 0 && (isalpha((unsigned char)var_name[0]) || var_name[0] == '_'));
        for (size_t i = 1; i < strlen(var_name); i++) {
            if (!isalnum((unsigned char)var_name[i]) && var_name[i] != '_') valid = 0;
        }

        if (valid) {
            double val = evaluate_expression(eq + 1);
            set_var(var_name, val);
            printf("  [Saved: %s = %g]\n", var_name, val);
            print_result_full(val);
            return;
        }
    }

    double res = evaluate_expression(input);
    print_result_full(res);
}

static void print_repl_help(void) {
    printf("==========================================\n");
    printf("[ calc REPL - Complete Guide ]\n");
    printf("==========================================\n");
    printf("  Operators : +  -  *  /  %%  ^  sqrt (r, √)\n");
    printf("  Bases     : 0xFF (Hex), 0b1010 (Bin), 0o77 (Oct)\n");
    printf("  Functions : sin, cos, tan, asin, acos, atan\n");
    printf("              log, log10, log2, abs, round, floor, ceil\n");
    printf("  Constants : pi, e, tau, ans (previous result)\n");
    printf("  Variables : x = 10 + 5, area = x * 2\n");
    printf("  History   : Use UP (↑) and DOWN (↓) Arrow Keys\n");
    printf("  Commands  : vars, help, ?, clear, exit, quit\n");
    printf("==========================================\n");
}

int main(int argc, char *argv[]) {
    utilipc_init();

    if (argc < 2) {
        printf("==========================================\n");
        printf("[calc REPL - Use UP/DOWN arrows for history]\n");
        printf("==========================================\n");

        char line[512];
        while (1) {
            if (!custom_readline("calc> ", line, sizeof(line))) break;

            size_t len = strlen(line);
            while (len > 0 && (line[len-1] == ' ' || line[len-1] == '\t')) {
                line[--len] = '\0';
            }

            if (len == 0) continue;
            if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) break;
            if (strcmp(line, "help") == 0 || strcmp(line, "?") == 0) {
                add_history(line);
                print_repl_help();
                continue;
            }
            if (strcmp(line, "vars") == 0) {
                add_history(line);
                list_vars();
                continue;
            }
            if (strcmp(line, "clear") == 0 || strcmp(line, "cls") == 0) {
                add_history(line);
                printf("\033[H\033[J");
                fflush(stdout);
                continue;
            }

            add_history(line);
            process_line(line);

            char log_msg[UTILIPC_MAX_MSG];
            snprintf(log_msg, sizeof(log_msg), "calc: evaluated (Result: %g)", last_ans);
            utilipc_write_status(-1, -1, -1, log_msg);
        }

        utilipc_close();
        return 0;
    }

    char input_buf[1024] = "";
    size_t curr_len = 0;
    for (int i = 1; i < argc; i++) {
        size_t arg_len = strlen(argv[i]);
        if (curr_len + arg_len < sizeof(input_buf) - 1) {
            strcat(input_buf, argv[i]);
            curr_len += arg_len;
        }
    }

    process_line(input_buf);

    char log_msg[UTILIPC_MAX_MSG];
    snprintf(log_msg, sizeof(log_msg), "calc: evaluated '%s' (%g)", input_buf, last_ans);
    utilipc_write_status(-1, -1, -1, log_msg);

    utilipc_close();
    return 0;
}
