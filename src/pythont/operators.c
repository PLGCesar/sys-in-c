#include "pythont.h"
#include <string.h>
#include <ctype.h>

void replace_operators(char *expr) {
    char tmp[2048] = "";
    size_t t = 0, len = strlen(expr);
    for (size_t i = 0; i < len && t < sizeof(tmp) - 1; i++) {
        if (strncmp(expr + i, " and ", 5) == 0) { strcat(tmp + t, " && "); t += 4; i += 4; }
        else if (strncmp(expr + i, " or ", 4) == 0) { strcat(tmp + t, " || "); t += 4; i += 3; }
        else if (strncmp(expr + i, "not ", 4) == 0) { strcat(tmp + t, "!"); t++; i += 3; }
        else if (strncmp(expr + i, "True", 4) == 0 && !isalnum((unsigned char)expr[i + 4])) { strcat(tmp + t, "1"); t++; i += 3; }
        else if (strncmp(expr + i, "False", 5) == 0 && !isalnum((unsigned char)expr[i + 5])) { strcat(tmp + t, "0"); t++; i += 4; }
        else if (strncmp(expr + i, "None", 4) == 0 && !isalnum((unsigned char)expr[i + 4])) { strcat(tmp + t, "NULL"); t += 4; i += 3; }
        else if (strncmp(expr + i, "min(", 4) == 0) { strcat(tmp + t, "py_min("); t += 7; i += 3; }
        else if (strncmp(expr + i, "max(", 4) == 0) { strcat(tmp + t, "py_max("); t += 7; i += 3; }
        else if (strncmp(expr + i, "abs(", 4) == 0) { strcat(tmp + t, "py_abs("); t += 7; i += 3; }
        else if (strncmp(expr + i, "int(", 4) == 0) { strcat(tmp + t, "py_int("); t += 7; i += 3; }
        else if (strncmp(expr + i, "input(", 6) == 0) { strcat(tmp + t, "py_input("); t += 9; i += 5; }
        else if (strncmp(expr + i, "//", 2) == 0) { strcat(tmp + t, "/"); t++; i++; }
        else tmp[t++] = expr[i], tmp[t] = '\0';
    }
    strcpy(expr, tmp);
}
