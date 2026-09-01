#include "pythont.h"
#include <stdio.h>
#include <string.h>

void transpile_print(const char *args_str) {
    char fmt[512] = "", vals[2048] = "";
    int first = 1;
    const char *p = args_str;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        const char *start = p;
        int pd = 0, bd = 0, in_str = 0; char quote = 0;
        while (*p) {
            char c = *p;
            if (in_str) { if (c == '\\' && p[1]) { p += 2; continue; } if (c == quote) in_str = 0; }
            else if (c == '"' || c == '\'') { in_str = 1; quote = c; }
            else if (c == '(') pd++;
            else if (c == ')' && pd) pd--;
            else if (c == '[') bd++;
            else if (c == ']' && bd) bd--;
            else if (c == ',' && !pd && !bd) break;
            p++;
        }
        size_t n = p - start; char token[512];
        if (n >= sizeof(token)) n = sizeof(token) - 1;
        memcpy(token, start, n); token[n] = '\0';
        if (*p == ',') p++;
        char *t = token; while (*t == ' ' || *t == '\t') t++;
        size_t len = strlen(t); while (len && (t[len-1] == ' ' || t[len-1] == '\t')) t[--len] = '\0';
        if (!len) continue;
        if (!first) strcat(fmt, " "); replace_operators(t);
        if (t[0] == '"' || t[0] == '\'') strcat(fmt, "%s");
        else if (strchr(t, '.') && !strchr(t, '(')) strcat(fmt, "%f");
        else strcat(fmt, "%lld");
        if (!first) strcat(vals, ", ");
        if (t[0] != '"' && t[0] != '\'' && (!strchr(t, '.') || strchr(t, '('))) {
            char cast[512]; snprintf(cast, sizeof(cast), "(long long)(%s)", t); strcat(vals, cast);
        } else strcat(vals, t);
        first = 0;
    }
    if (*vals) emit("    printf(\"%s\\n\", %s);\n", fmt, vals);
    else emit("    putchar('\\n');\n");
}
