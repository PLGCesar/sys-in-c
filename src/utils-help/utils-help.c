#include <stdio.h>
#include <string.h>

void print_general_help() {
    printf("======================\n");
    printf("[utils-in-c - Suite Overview]\n");
    printf("======================\n");
    printf("  calc      - Math expression evaluator (PEMDAS, decimals, sqrt)\n");
    printf("  passgen   - Secure random password generator\n");
    printf("  bigfiles  - Find files larger than a specified MB size\n");
    printf("  portcheck - Network host and port connection tester\n");
    printf("======================\n");
    printf("Type 'utils-help <command>' for detailed help on a command.\n");
    printf("======================\n");
}

void print_calc_help() {
    printf("======================\n");
    printf("[Command: calc]\n");
    printf("======================\n");
    printf("Description: Evaluates mathematical expressions using PEMDAS.\n");
    printf("Operators: +, -, *, /, x, ÷, ^, r, √\n");
    printf("Grouping: (), [], {}\n");
    printf("Usage: calc \"<expression>\"\n");
    printf("Examples:\n");
    printf("  calc \"10.5 + 2.5 x 3\"\n");
    printf("  calc \"r64 + r25\"\n");
    printf("======================\n");
}

void print_passgen_help() {
    printf("======================\n");
    printf("[Command: passgen]\n");
    printf("======================\n");
    printf("Description: Generates cryptographically secure passwords (/dev/urandom).\n");
    printf("Usage: passgen [length]\n");
    printf("Examples:\n");
    printf("  passgen       (Default 16 characters)\n");
    printf("  passgen 32    (32 characters)\n");
    printf("======================\n");
}

void print_bigfiles_help() {
    printf("======================\n");
    printf("[Command: bigfiles]\n");
    printf("======================\n");
    printf("Description: Searches current path for files larger than threshold.\n");
    printf("Note: Limits recursive search to a maximum of 25 subdirectories.\n");
    printf("Usage: bigfiles [min_size_mb]\n");
    printf("Examples:\n");
    printf("  bigfiles      (Files >= 10 MB)\n");
    printf("  bigfiles 50   (Files >= 50 MB)\n");
    printf("======================\n");
}

void print_portcheck_help() {
    printf("======================\n");
    printf("[Command: portcheck]\n");
    printf("======================\n");
    printf("Description: Tests TCP connection and measures latency to host/port.\n");
    printf("Usage: portcheck <host> <port>\n");
    printf("Examples:\n");
    printf("  portcheck google.com 443\n");
    printf("  portcheck 192.168.1.1 80\n");
    printf("======================\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_general_help();
        return 0;
    }

    if (strcmp(argv[1], "calc") == 0) {
        print_calc_help();
    } else if (strcmp(argv[1], "passgen") == 0) {
        print_passgen_help();
    } else if (strcmp(argv[1], "bigfiles") == 0) {
        print_bigfiles_help();
    } else if (strcmp(argv[1], "portcheck") == 0) {
        print_portcheck_help();
    } else {
        printf("Error: Unknown command '%s'.\n", argv[1]);
        print_general_help();
    }

    return 0;
}
