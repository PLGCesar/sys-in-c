#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

const char *expr;

double bitwise_sqrt_decimal(double num) {
    if (num < 0.0) {
        printf("Error: Square root of a negative number!\n");
        exit(1);
    }
    if (num == 0.0) return 0.0;

    unsigned long long int_part = (unsigned long long)num;
    unsigned long long res = 0;
    unsigned long long bit = 1ULL << 62;

    while (bit > int_part && bit != 0) bit >>= 2;

    while (bit != 0) {
        if (int_part >= res + bit) {
            int_part -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }

    double guess = (double)res;
    if (guess == 0.0) guess = 0.5;

    for (int i = 0; i < 5; i++) {
        guess = 0.5 * (guess + num / guess);
    }
    
    return guess;
}

double eval_priority_1();

double eval_priority_4() {
    if (*expr == '-') {
        expr++;
        return -eval_priority_4();
    }
    if (*expr == '(') {
        expr++;
        double val = eval_priority_1();
        if (*expr == ')') expr++;
        return val;
    }
    if (*expr == 'r' || *expr == 'R') {
        expr++;
        return bitwise_sqrt_decimal(eval_priority_4());
    }
    
    char *end_ptr;
    double val = strtod(expr, &end_ptr);
    expr = end_ptr;
    
    return val;
}

double eval_priority_3() {
    double val = eval_priority_4();
    while (*expr == '^') {
        expr++;
        double exp_val = eval_priority_4();
        val = pow(val, exp_val);
    }
    return val;
}

double eval_priority_2() {
    double val = eval_priority_3();
    while (*expr == '*' || *expr == '/') {
        char op = *expr++;
        double next_val = eval_priority_3();
        if (op == '*') val *= next_val;
        else {
            if (next_val == 0.0) {
                printf("Error: Division by zero!\n");
                exit(1);
            }
            val /= next_val;
        }
    }
    return val;
}

double eval_priority_1() {
    double val = eval_priority_2();
    while (*expr == '+' || *expr == '-') {
        char op = *expr++;
        double next_val = eval_priority_2();
        if (op == '+') val += next_val;
        else val -= next_val;
    }
    return val;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s \"expression\"\n", argv[0]);
        printf("Examples: %s \"[2.5 + 2.5] x 3\"\n", argv[0]);
        printf("          %s \"r64 + r25\"\n", argv[0]);
        return 1;
    }

    char clean_str[512];
    int j = 0;
    
    char input[512] = "";
    for(int i = 1; i < argc; i++) {
        strcat(input, argv[i]);
    }

    for (int i = 0; input[i] != '\0'; i++) {
        if (input[i] == ' ') continue;
        else if (input[i] == 'x' || input[i] == 'X') clean_str[j++] = '*';
        else if (input[i] == '[' || input[i] == '{') clean_str[j++] = '(';
        else if (input[i] == ']' || input[i] == '}') clean_str[j++] = ')';
        else if ((unsigned char)input[i] == 0xC3 && (unsigned char)input[i+1] == 0xB7) {
            clean_str[j++] = '/'; i++;
        }
        else if ((unsigned char)input[i] == 0xE2 && (unsigned char)input[i+1] == 0x88 && (unsigned char)input[i+2] == 0x9A) {
            clean_str[j++] = 'r'; i += 2;
        }
        else clean_str[j++] = input[i];
    }
    clean_str[j] = '\0';

    expr = clean_str;
    
    double result = eval_priority_1();
    
    printf("======================\n");
    printf("[Result: %g]\n", result);
    printf("======================\n");

    return 0;
}
