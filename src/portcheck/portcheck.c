#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include "../libutilipc/utilipc.h"

#define MAX_PORTS 1024
#define COLOR_RESET   "\033[0m"
#define COLOR_TITLE   "\033[1;35m"
#define COLOR_OPEN    "\033[1;32m"
#define COLOR_CLOSED  "\033[1;31m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_WHITE   "\033[1;37m"

static int parse_ports(const char *input, int *ports_out, int max_count) {
    int count = 0;
    char buf[512];
    strncpy(buf, input, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    for (size_t i = 0; buf[i]; i++) {
        if (buf[i] == ' ' || buf[i] == '\t') buf[i] = ',';
    }

    char *token = strtok(buf, ",");
    while (token != NULL && count < max_count) {
        while (*token == ' ') token++;
        if (*token == '\0') {
            token = strtok(NULL, ",");
            continue;
        }

        char *dash = strchr(token, '-');
        if (dash) {
            *dash = '\0';
            int start = atoi(token);
            int end = atoi(dash + 1);
            if (start > 0 && end >= start && end <= 65535) {
                for (int p = start; p <= end && count < max_count; p++) {
                    int exists = 0;
                    for (int k = 0; k < count; k++) {
                        if (ports_out[k] == p) { exists = 1; break; }
                    }
                    if (!exists) ports_out[count++] = p;
                }
            }
        } else {
            int p = atoi(token);
            if (p > 0 && p <= 65535) {
                int exists = 0;
                for (int k = 0; k < count; k++) {
                    if (ports_out[k] == p) { exists = 1; break; }
                }
                if (!exists) ports_out[count++] = p;
            }
        }
        token = strtok(NULL, ",");
    }
    return count;
}

static void scan_host_ports(const char *host, const int *ports, int port_count) {
    printf("\n%s========================================================%s\n", COLOR_TITLE, COLOR_RESET);
    printf("%s[ portcheck - Scanning %s (%d ports) ]%s\n", COLOR_TITLE, host, port_count, COLOR_RESET);
    printf("%s========================================================%s\n", COLOR_TITLE, COLOR_RESET);

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, NULL, &hints, &res) != 0) {
        printf("  %s[Erro: Host '%s' não pôde ser resolvido / offline]%s\n\n", COLOR_CLOSED, host, COLOR_RESET);
        return;
    }

    printf("  %s%-8s  %-15s  %-15s%s\n", COLOR_CYAN, "PORTA", "STATUS", "LATÊNCIA", COLOR_RESET);
    printf("  --------------------------------------------------------\n");

    int open_count = 0;
    int closed_count = 0;

    for (int i = 0; i < port_count; i++) {
        int port = ports[i];

        if (res->ai_family == AF_INET) {
            ((struct sockaddr_in *)res->ai_addr)->sin_port = htons(port);
        } else if (res->ai_family == AF_INET6) {
            ((struct sockaddr_in6 *)res->ai_addr)->sin6_port = htons(port);
        }

        int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd < 0) {
            printf("  %-8d  %sSOCKET ERR%s      -\n", port, COLOR_CLOSED, COLOR_RESET);
            closed_count++;
            continue;
        }

        int flags = fcntl(sockfd, F_GETFL, 0);
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        int conn_result = connect(sockfd, res->ai_addr, res->ai_addrlen);
        int is_open = 0;

        if (conn_result == 0) {
            is_open = 1;
        } else if (errno == EINPROGRESS) {
            fd_set fdset;
            FD_ZERO(&fdset);
            FD_SET(sockfd, &fdset);

            struct timeval tv = { .tv_sec = 1, .tv_usec = 500000 };

            if (select(sockfd + 1, NULL, &fdset, NULL, &tv) > 0) {
                int so_error = 0;
                socklen_t len = sizeof(so_error);
                getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);
                if (so_error == 0) is_open = 1;
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &end);
        double time_ms = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;

        if (is_open) {
            printf("  %-8d  %s[OPEN / ABERTA]%s  %s%.2f ms%s\n", port, COLOR_OPEN, COLOR_RESET, COLOR_YELLOW, time_ms, COLOR_RESET);
            open_count++;
        } else {
            printf("  %-8d  %s[CLOSED / TIMEOUT]%s -\n", port, COLOR_CLOSED, COLOR_RESET);
            closed_count++;
        }

        fflush(stdout);
        close(sockfd);
    }

    freeaddrinfo(res);

    printf("  --------------------------------------------------------\n");
    printf("  %sResumo:%s %s%d Abertas%s | %s%d Fechadas%s | Total: %d\n",
           COLOR_WHITE, COLOR_RESET, COLOR_OPEN, open_count, COLOR_RESET, COLOR_CLOSED, closed_count, COLOR_RESET, port_count);
    printf("%s========================================================%s\n\n", COLOR_TITLE, COLOR_RESET);

    char log_msg[UTILIPC_MAX_MSG];
    snprintf(log_msg, sizeof(log_msg), "portcheck: %s (%d ports: %d open, %d closed)", host, port_count, open_count, closed_count);
    utilipc_write_status(-1, -1, -1, log_msg);
}

static void run_tui(void) {
    char host[256] = "";
    char port_input[512] = "";

    printf("\033[H\033[J\033[1;35m");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║         portcheck - Multi-Port TUI Scanner           ║\n");
    printf("╚══════════════════════════════════════════════════════╝\033[0m\n\n");

    printf("  %s• Digite o Host%s (ex: google.com, 127.0.0.1): ", COLOR_CYAN, COLOR_RESET);
    fflush(stdout);
    if (!fgets(host, sizeof(host), stdin)) return;
    host[strcspn(host, "\r\n")] = '\0';
    if (strlen(host) == 0) return;

    printf("  %s• Portas ou Ranges%s (ex: 80,443,8080 ou 80-85): ", COLOR_CYAN, COLOR_RESET);
    fflush(stdout);
    if (!fgets(port_input, sizeof(port_input), stdin)) return;
    port_input[strcspn(port_input, "\r\n")] = '\0';
    if (strlen(port_input) == 0) return;

    int ports[MAX_PORTS];
    int count = parse_ports(port_input, ports, MAX_PORTS);

    if (count == 0) {
        printf("\n  %s[Erro: Nenhuma porta válida especificada]%s\n\n", COLOR_CLOSED, COLOR_RESET);
        return;
    }

    scan_host_ports(host, ports, count);
}

int main(int argc, char *argv[]) {
    utilipc_init();

    if (argc >= 2 && strcmp(argv[1], "-tui") == 0) {
        run_tui();
        utilipc_close();
        return 0;
    }

    if (argc < 3) {
        printf("Usage:\n");
        printf("  portcheck -tui\n");
        printf("  portcheck <host> <port_or_list_or_range>\n");
        printf("Examples:\n");
        printf("  portcheck google.com 443\n");
        printf("  portcheck google.com 80,443,8080\n");
        printf("  portcheck 127.0.0.1 8000-8010\n");
        printf("  portcheck 192.168.1.1 22,80-85,443\n");
        utilipc_close();
        return 1;
    }

    const char *host = argv[1];
    int ports[MAX_PORTS];
    int count = parse_ports(argv[2], ports, MAX_PORTS);

    if (count == 0) {
        printf("Error: Invalid port(s) specified: %s\n", argv[2]);
        utilipc_close();
        return 1;
    }

    scan_host_ports(host, ports, count);

    utilipc_close();
    return 0;
}
