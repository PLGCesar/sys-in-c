#include <stdio.h>
#include <string.h>

static void print_general_help(void) {
    printf("======================\n");
    printf("[utils-in-c - Suite Overview]\n");
    printf("======================\n");
    printf("  calc      - Math expression evaluator (PEMDAS, sin, cos, log, abs, REPL)\n");
    printf("  passgen   - Secure random password generator\n");
    printf("  bigfiles  - Find large files sorted by size with ANSI colors\n");
    printf("  portcheck - Network host and port connection tester\n");
    printf("  hashcalc  - Fast CRC32, FNV-1a, and SHA-256 hash calculator\n");
    printf("  b64       - Fast Base64 encoder and decoder\n");
    printf("  sysinfo   - Minimal system info monitor (Android/Linux auto-detect)\n");
    printf("  org       - File organizer (move by ext, change ext, auto-group)\n");
    printf("  netinfo   - Network interfaces, Public IP, and port test (-p)\n");
    printf("  ffind     - Fast recursive file finder (ignores system dirs, depth 15)\n");
    printf("  ipcmon    - Live/snapshot IPC shared memory monitor\n");
    printf("======================\n");
    printf("Type 'utils-help <command>' for detailed help on a command.\n");
    printf("======================\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_general_help();
        return 0;
    }

    printf("Displaying help for: %s\n", argv[1]);
    print_general_help();
    return 0;
}
