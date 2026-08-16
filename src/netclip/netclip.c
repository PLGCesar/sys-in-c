#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include "../libutilipc/utilipc.h"

#define DEFAULT_PORT 7878
#define CHUNK_SIZE 65536

#define COLOR_RESET   "\033[0m"
#define COLOR_TITLE   "\033[1;35m"
#define COLOR_LABEL   "\033[1;36m"
#define COLOR_VAL     "\033[1;32m"
#define COLOR_WARN    "\033[1;33m"
#define COLOR_ERR     "\033[1;31m"
#define COLOR_GRAY    "\033[0;90m"

static void copy_to_clipboard(const char *text) {
    FILE *pipe = NULL;
    if (access("/data/data/com.termux/files/usr/bin/termux-clipboard-set", X_OK) == 0 ||
        system("which termux-clipboard-set >/dev/null 2>&1") == 0) {
        pipe = popen("termux-clipboard-set", "w");
    } else if (system("which wl-copy >/dev/null 2>&1") == 0) {
        pipe = popen("wl-copy", "w");
    } else if (system("which xclip >/dev/null 2>&1") == 0) {
        pipe = popen("xclip -selection clipboard", "w");
    }

    if (pipe) {
        fputs(text, pipe);
        pclose(pipe);
    }
}

static void print_local_ips(int port) {
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) return;

    printf("  %sIPs Locais Disponíveis:%s\n", COLOR_LABEL, COLOR_RESET);
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
        if (strcmp(ifa->ifa_name, "lo") == 0) continue;

        char host[INET_ADDRSTRLEN];
        struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
        inet_ntop(AF_INET, &(sa->sin_addr), host, sizeof(host));
        printf("    • %s%-10s%s : %s%s%s (Porta %d)\n", COLOR_VAL, ifa->ifa_name, COLOR_RESET, COLOR_WARN, host, COLOR_RESET, port);
    }
    freeifaddrs(ifaddr);
}

static void run_listener(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Erro ao criar socket");
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Erro no bind (porta já em uso?)");
        close(server_fd);
        return;
    }

    if (listen(server_fd, 5) < 0) {
        perror("Erro no listen");
        close(server_fd);
        return;
    }

    printf("\n%s========================================================%s\n", COLOR_TITLE, COLOR_RESET);
    printf("%s[ netclip - Receptor P2P Wi-Fi Ativo ]%s\n", COLOR_TITLE, COLOR_RESET);
    printf("%s========================================================%s\n", COLOR_TITLE, COLOR_RESET);
    print_local_ips(port);
    printf("%s--------------------------------------------------------%s\n", COLOR_GRAY, COLOR_RESET);
    printf("  %sPara enviar de outro dispositivo, use:%s\n", COLOR_LABEL, COLOR_RESET);
    printf("    netclip send <IP> \"texto\"\n");
    printf("    netclip send <IP> -f <arquivo>\n");
    printf("%s========================================================%s\n", COLOR_TITLE, COLOR_RESET);
    printf("[Aguardando dados... Pressione Ctrl+C para encerrar]\n\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) continue;

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, sizeof(client_ip));

        char header[256] = "";
        size_t h_idx = 0;
        char ch;
        int newlines = 0;

        // Lê o cabeçalho inicial até encontrar "\n\n"
        while (h_idx < sizeof(header) - 1 && read(client_fd, &ch, 1) == 1) {
            header[h_idx++] = ch;
            if (ch == '\n') {
                newlines++;
                if (newlines == 2 || (h_idx >= 2 && header[h_idx - 2] == '\n')) break;
            } else if (ch != '\r') {
                newlines = 0;
            }
        }
        header[h_idx] = '\0';

        if (strncmp(header, "NETCLIP1\nTEXT", 13) == 0) {
            size_t payload_len = 0;
            sscanf(header, "NETCLIP1\nTEXT\n%zu", &payload_len);

            if (payload_len > 0) {
                char *text_buf = malloc(payload_len + 1);
                if (text_buf) {
                    size_t received = 0;
                    while (received < payload_len) {
                        ssize_t n = read(client_fd, text_buf + received, payload_len - received);
                        if (n <= 0) break;
                        received += n;
                    }
                    text_buf[received] = '\0';

                    time_t now = time(NULL);
                    struct tm *tm_info = localtime(&now);
                    char t_str[32];
                    strftime(t_str, sizeof(t_str), "%H:%M:%S", tm_info);

                    printf("\033[1;32m[%s] Texto Recebido de %s:\033[0m\n", t_str, client_ip);
                    printf("┌──────────────────────────────────────────────────────┐\n");
                    printf("  %s\n", text_buf);
                    printf("└──────────────────────────────────────────────────────┘\n");

                    copy_to_clipboard(text_buf);
                    printf("  \033[0;90m• Copiado para a Área de Transferência!\033[0m\n\n");
                    free(text_buf);
                }
            }
            write(client_fd, "OK\n", 3);
        }
        else if (strncmp(header, "NETCLIP1\nFILE", 13) == 0) {
            char filename[128] = "received_file";
            size_t file_len = 0;
            sscanf(header, "NETCLIP1\nFILE\n%127[^\n]\n%zu", filename, &file_len);

            printf("\033[1;36m[Recebendo Arquivo de %s]:\033[0m '%s' (%.2f MB)...\n",
                   client_ip, filename, (double)file_len / (1024.0 * 1024.0));

            FILE *fp = fopen(filename, "wb");
            if (fp) {
                char buffer[CHUNK_SIZE];
                size_t total_received = 0;

                while (total_received < file_len) {
                    size_t to_read = file_len - total_received;
                    if (to_read > sizeof(buffer)) to_read = sizeof(buffer);

                    ssize_t n = read(client_fd, buffer, to_read);
                    if (n <= 0) break;
                    fwrite(buffer, 1, n, fp);
                    total_received += n;

                    int pct = (int)(((double)total_received / (double)file_len) * 100.0);
                    printf("\r  Progresso: \033[1;32m%d%%\033[0m (%.1f / %.1f MB)",
                           pct, (double)total_received / (1024.0 * 1024.0), (double)file_len / (1024.0 * 1024.0));
                    fflush(stdout);
                }
                fclose(fp);
                printf("\n  \033[1;32m✔ Arquivo salvo com sucesso em ./%s\033[0m\n\n", filename);
                write(client_fd, "OK\n", 3);
            } else {
                printf("  \033[1;31m✖ Erro ao salvar arquivo no disco.\033[0m\n\n");
                write(client_fd, "ERR\n", 4);
            }
        }

        close(client_fd);
    }
    close(server_fd);
}

static int connect_to_host(const char *host, int port) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char p_str[16];
    snprintf(p_str, sizeof(p_str), "%d", port);

    if (getaddrinfo(host, p_str, &hints, &res) != 0) {
        printf("  %sErro: Não foi possível resolver o host '%s'%s\n", COLOR_ERR, host, COLOR_RESET);
        return -1;
    }

    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        freeaddrinfo(res);
        return -1;
    }

    struct timeval tv = { .tv_sec = 4, .tv_usec = 0 };
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(sockfd, res->ai_addr, res->ai_addrlen) != 0) {
        printf("  %sErro: Falha na conexão com %s:%d (O receptor está rodando 'netclip listen'?)%s\n",
               COLOR_ERR, host, port, COLOR_RESET);
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);
    return sockfd;
}

static void send_text(const char *host, int port, const char *text) {
    int sockfd = connect_to_host(host, port);
    if (sockfd < 0) return;

    size_t len = strlen(text);
    char header[128];
    snprintf(header, sizeof(header), "NETCLIP1\nTEXT\n%zu\n\n", len);

    write(sockfd, header, strlen(header));
    write(sockfd, text, len);

    char ack[16] = "";
    read(sockfd, ack, sizeof(ack) - 1);

    if (strncmp(ack, "OK", 2) == 0) {
        printf("  %s✔ Texto enviado com sucesso para %s:%d!%s\n", COLOR_VAL, host, port, COLOR_RESET);
    } else {
        printf("  %s✖ O receptor retornou erro.%s\n", COLOR_ERR, COLOR_RESET);
    }
    close(sockfd);
}

static void send_file(const char *host, int port, const char *filepath) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        printf("  %sErro: Não foi possível abrir o arquivo '%s'%s\n", COLOR_ERR, filepath, COLOR_RESET);
        return;
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fsize <= 0) {
        printf("  %sErro: Arquivo vazio ou inválido.%s\n", COLOR_ERR, COLOR_RESET);
        fclose(fp);
        return;
    }

    const char *filename = strrchr(filepath, '/');
    filename = filename ? filename + 1 : filepath;

    int sockfd = connect_to_host(host, port);
    if (sockfd < 0) {
        fclose(fp);
        return;
    }

    char header[256];
    snprintf(header, sizeof(header), "NETCLIP1\nFILE\n%s\n%ld\n\n", filename, fsize);
    write(sockfd, header, strlen(header));

    printf("  Enviando '%s' (%.2f MB)...\n", filename, (double)fsize / (1024.0 * 1024.0));

    char buffer[CHUNK_SIZE];
    size_t total_sent = 0;
    size_t n;

    while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        write(sockfd, buffer, n);
        total_sent += n;
        int pct = (int)(((double)total_sent / (double)fsize) * 100.0);
        printf("\r  Progresso: %s%d%%%s (%.1f / %.1f MB)", COLOR_VAL, pct, COLOR_RESET,
               (double)total_sent / (1024.0 * 1024.0), (double)fsize / (1024.0 * 1024.0));
        fflush(stdout);
    }
    fclose(fp);

    char ack[16] = "";
    read(sockfd, ack, sizeof(ack) - 1);

    if (strncmp(ack, "OK", 2) == 0) {
        printf("\n  %s✔ Arquivo enviado com sucesso para %s:%d!%s\n", COLOR_VAL, host, port, COLOR_RESET);
    } else {
        printf("\n  %s✖ Falha na confirmação do envio.%s\n", COLOR_ERR, COLOR_RESET);
    }
    close(sockfd);
}

int main(int argc, char *argv[]) {
    utilipc_init();
    int port = DEFAULT_PORT;

    if (argc < 2) {
        printf("Usage:\n");
        printf("  netclip listen [port]                   (Aguardar textos e arquivos na rede)\n");
        printf("  netclip send <IP> \"texto ou link\"       (Enviar texto para outro dispositivo)\n");
        printf("  netclip send <IP> -f <arquivo>          (Enviar arquivo para outro dispositivo)\n\n");
        printf("Examples:\n");
        printf("  netclip listen\n");
        printf("  netclip send 192.168.1.50 \"https://google.com\"\n");
        printf("  netclip send 192.168.1.50 -f foto.png\n");
        utilipc_close();
        return 0;
    }

    if (strcmp(argv[1], "listen") == 0 || strcmp(argv[1], "-l") == 0 || strcmp(argv[1], "rx") == 0) {
        if (argc >= 3) port = atoi(argv[2]);
        if (port <= 0 || port > 65535) port = DEFAULT_PORT;
        run_listener(port);
        utilipc_close();
        return 0;
    }

    if (strcmp(argv[1], "send") == 0 || strcmp(argv[1], "tx") == 0) {
        if (argc < 4) {
            printf("Erro: Uso correto: netclip send <IP> \"texto\" ou netclip send <IP> -f <arquivo>\n");
            utilipc_close();
            return 1;
        }

        const char *host = argv[2];
        if (strcmp(argv[3], "-f") == 0 && argc >= 5) {
            send_file(host, port, argv[4]);
        } else if (argc >= 5 && strcmp(argv[4], "-f") == 0) {
            send_file(host, port, argv[3]);
        } else {
            send_text(host, port, argv[3]);
        }
        utilipc_close();
        return 0;
    }

    // Atalho direto: netclip <IP> "texto" ou netclip <IP> -f <arquivo>
    if (argc >= 3) {
        const char *host = argv[1];
        if (strcmp(argv[2], "-f") == 0 && argc >= 4) {
            send_file(host, port, argv[3]);
        } else {
            send_text(host, port, argv[2]);
        }
        utilipc_close();
        return 0;
    }

    printf("Comando não reconhecido. Use 'netclip' sem argumentos para ver a ajuda.\n");
    utilipc_close();
    return 1;
}
