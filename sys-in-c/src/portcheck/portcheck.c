#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <termios.h>

static void check_port(const char *host, int port) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
    char p_str[10]; snprintf(p_str, sizeof(p_str), "%d", port);

    printf("\n  \033[1;33m[Conectando a %s:%d...]\033[0m\n", host, port);
    if (getaddrinfo(host, p_str, &hints, &res) != 0) {
        printf("  \033[1;31m[Resultado: HOST UNRESOLVED]\033[0m\n"); return;
    }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { printf("  \033[1;31m[Resultado: SOCKET ERROR]\033[0m\n"); return; }

    struct timeval tv = { .tv_sec = 3, .tv_usec = 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(fd, res->ai_addr, res->ai_addrlen) == 0) {
        printf("  \033[1;32m[Status: OPEN / ABERTA]\033[0m\n");
    } else {
        printf("  \033[1;31m[Status: CLOSED / TIMEOUT]\033[0m\n");
    }
    close(fd); freeaddrinfo(res);
}

static void run_tui(void) {
    char host[128] = "";
    char port_str[16] = "";
    
    printf("\033[H\033[J\033[1;35m");
    printf("╔══════════════════════════════════════╗\n");
    printf("║      portcheck - TUI Scanner         ║\n");
    printf("╚══════════════════════════════════════╝\033[0m\n\n");
    
    printf("  Digite o Host (ex: google.com): ");
    if(scanf("%127s", host) != 1) return;

    printf("  Digite a Porta (ex: 443): ");
    if(scanf("%15s", port_str) != 1) return;

    check_port(host, atoi(port_str));
    printf("\n\033[1;35m========================================\033[0m\n");
}

int main(int argc, char *argv[]) {
    if (argc >= 2 && strcmp(argv[1], "-tui") == 0) {
        run_tui();
        return 0;
    }
    
    if (argc < 3) {
        printf("Usage:\n  portcheck -tui\n  portcheck <host> <port>\n");
        return 1;
    }
    check_port(argv[1], atoi(argv[2]));
    return 0;
}
