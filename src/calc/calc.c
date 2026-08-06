#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h> // Required for pow() when using decimals

const char *expr; // Global pointer to read our expression

// =========================================================
// 1. HYBRID BIT-SHIFT SQUARE ROOT (Supports Decimals)
// =========================================================
double bitwise_sqrt_decimal(double num) {
    if (num < 0.0) {
        printf("Error: Square root of a negative number!\n");
        exit(1);
    }
    if (num == 0.0) return 0.0;

    // Part 1: Use bitwise square root for the integer part
    unsigned long long int_part = (unsigned long long)num;
    unsigned long long res = 0;
    unsigned long long bit = 1ULL << 62; // Second highest bit in 64-bits

    while (bit > int_part && bit != 0) bit >>= 2;

    while (bit != 0) {
        if (int_part >= res + bit) {
            int_part -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2; // Shift by 2
    }

    // Part 2: Refine the decimal precision using Newton-Raphson
    double guess = (double)res;
    if (guess == 0.0) guess = 0.5; // Failsafe for numbers between 0 and 1 (e.g. 0.25)

    // 5 iterations are more than enough to get perfect decimal precision
    for (int i = 0; i < 5; i++) {
        guess = 0.5 * (guess + num / guess);
    }
    
    return guess;
}

// =========================================================
// 2. RECURSIVE DESCENT PARSER (Priority & PEMDAS)
// =========================================================

double eval_priority_1(); // Signature so priority 4 can see priority 1

// PRIORITY 4: Numbers, Negative signs, Parentheses, and Square Root (√ or r)
double eval_priority_4() {
    // Handle negative numbers
    if (*expr == '-') {
        expr++;
        return -eval_priority_4();
    }
    // Handle Parentheses
    if (*expr == '(') {
        expr++;
        double val = eval_priority_1();
        if (*expr == ')') expr++; // Close parenthesis
        return val;
    }
    // Handle Square Root (r, R, or √ mapped to 'r')
    if (*expr == 'r' || *expr == 'R') {
        expr++;
        return bitwise_sqrt_decimal(eval_priority_4());
    }
    
    // Parse decimal numbers using standard C library function
    char *end_ptr;
    double val = strtod(expr, &end_ptr);
    expr = end_ptr; // Move the pointer to the end of the parsed number
    
    return val;
}

// PRIORITY 3: Power (^)
double eval_priority_3() {
    double val = eval_priority_4();
    while (*expr == '^') {
        expr++;
        double exp_val = eval_priority_4();
        val = pow(val, exp_val); // Using math.h for decimal powers (e.g., 2.5 ^ 1.5)
    }
    return val;
}

// PRIORITY 2: Multiplication and Division (*, /)
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

// PRIORITY 1: Addition and Subtraction (+, -)
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

// =========================================================
// 3. MAIN AND STRING CLEANUP
// =========================================================
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s \"expression\"\n", argv[0]);
        printf("Examples: %s \"[2.5 + 2.5] x 3\"\n", argv[0]);
        printf("          %s \"r64 + r25\"\n", argv[0]);
        return 1;
    }

    // Buffer to clean and normalize user expression
    char clean_str[512];
    int j = 0;
    
    // Concatenate all arguments into one single string
    char input[512] = "";
    for(int i = 1; i < argc; i++) {
        strcat(input, argv[i]);
    }

    // Clean string: Convert complex symbols and remove spaces
    for (int i = 0; input[i] != '\0'; i++) {
        if (input[i] == ' ') continue; // Ignore spaces
        else if (input[i] == 'x' || input[i] == 'X') clean_str[j++] = '*'; // x becomes *
        else if (input[i] == '[' || input[i] == '{') clean_str[j++] = '('; // [ or { becomes (
        else if (input[i] == ']' || input[i] == '}') clean_str[j++] = ')'; // ] or } becomes )
        else if ((unsigned char)input[i] == 0xC3 && (unsigned char)input[i+1] == 0xB7) {
            clean_str[j++] = '/'; i++; // ÷ (UTF-8) becomes /
        }
        else if ((unsigned char)input[i] == 0xE2 && (unsigned char)input[i+1] == 0x88 && (unsigned char)input[i+2] == 0x9A) {
            clean_str[j++] = 'r'; i += 2; // √ (UTF-8) becomes 'r'
        }
        else clean_str[j++] = input[i];
    }
    clean_str[j] = '\0';

    expr = clean_str; // Point the global pointer to our clean string
    
    // Calculate the final result
    double result = eval_priority_1();
    
    // Print the formatted output (Using %g to remove trailing zeroes automatically)
    printf("======================\n");
    printf("[Result: %g]\n", result);
    printf("======================\n");

    return 0;
}
