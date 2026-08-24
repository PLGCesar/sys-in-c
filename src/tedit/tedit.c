#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include "../libutilipc/utilipc.h"

#define MAX_LINES    4000
#define MAX_LINE_LEN 1024

#define COLOR_RESET   "\033[0m"
#define COLOR_TEXT    "\033[1;37m"
#define COLOR_MUTED   "\033[0;90m"

// Sistema de Temas
typedef struct {
    const char *name;
    const char *border;
    const char *header;
    const char *footer;
    const char *text;
} tedit_theme_t;

static const tedit_theme_t themes[] = {
    {"Neon Purple",  "\033[1;35m", "\033[1;37;45m", "\033[1;37;44m", "\033[1;37m"},
    {"Matrix Green", "\033[1;32m", "\033[1;30;42m", "\033[1;30;42m", "\033[1;32m"},
    {"Cyber Cyan",   "\033[1;36m", "\033[1;37;44m", "\033[1;37;46m", "\033[1;37m"},
    {"Dracula Gold", "\033[1;33m", "\033[1;30;43m", "\033[1;30;43m", "\033[1;37m"},
    {"Blood Crimson","\033[1;31m", "\033[1;37;41m", "\033[1;37;41m", "\033[1;37m"}
};
#define THEME_COUNT (sizeof(themes) / sizeof(themes[0]))

static int current_theme_idx = 0;
static struct termios orig_termios;
static char filename[256] = "index.html";
static char lines[MAX_LINES][MAX_LINE_LEN];
static int line_count = 0;
static int cur_line = 0;
static int cur_col = 0;
static int scroll_y = 0;
static int is_modified = 0;
static char notify_status[128] = "";

// Servidor Web Embutido (Thread em Background)
static pthread_t web_server_thread;
static volatile int web_server_running = 0;
static int web_server_port = 8080;
static int web_server_sock = -1;

static void enable_raw_mode(void);
static void disable_raw_mode(void);
static int save_file(void);

static void stop_web_server(void) {
    if (web_server_running) {
        web_server_running = 0;
        if (web_server_sock >= 0) {
            shutdown(web_server_sock, SHUT_RDWR);
            close(web_server_sock);
            web_server_sock = -1;
        }
        pthread_join(web_server_thread, NULL);
    }
}

static void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    printf("\033[?2004l\033[?1049l\033[?25h");
    fflush(stdout);
    stop_web_server();
    utilipc_unregister_process();
    utilipc_close();
}

static void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    printf("\033[?1049h\033[?2004h\033[H");
    fflush(stdout);
}

static void get_window_size(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
    } else {
        *rows = 24;
        *cols = 80;
    }
}

static void load_file(const char *path) {
    strncpy(filename, path, sizeof(filename) - 1);
    FILE *fp = fopen(path, "r");
    line_count = 0;

    if (fp) {
        char buf[MAX_LINE_LEN];
        while (fgets(buf, sizeof(buf), fp) && line_count < MAX_LINES) {
            size_t l = strlen(buf);
            while (l > 0 && (buf[l - 1] == '\r' || buf[l - 1] == '\n')) buf[--l] = '\0';
            strncpy(lines[line_count], buf, MAX_LINE_LEN - 1);
            lines[line_count][MAX_LINE_LEN - 1] = '\0';
            line_count++;
        }
        fclose(fp);
    }

    if (line_count == 0) {
        lines[0][0] = '\0';
        line_count = 1;
    }
}

static int save_file(void) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        snprintf(notify_status, sizeof(notify_status), "[✖ Erro ao Salvar!]");
        return -1;
    }

    for (int i = 0; i < line_count; i++) {
        fprintf(fp, "%s\n", lines[i]);
    }
    fclose(fp);
    is_modified = 0;
    snprintf(notify_status, sizeof(notify_status), "[✔ Salvo com Sucesso!]");

    char log_msg[128];
    snprintf(log_msg, sizeof(log_msg), "tedit: saved '%s' (%d lines)", filename, line_count);
    utilipc_log("tedit", log_msg);
    return 0;
}

static const char *get_mime(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "text/plain";
    if (strcasecmp(dot, ".html") == 0 || strcasecmp(dot, ".htm") == 0) return "text/html; charset=UTF-8";
    if (strcasecmp(dot, ".css") == 0) return "text/css; charset=UTF-8";
    if (strcasecmp(dot, ".js") == 0) return "application/javascript; charset=UTF-8";
    if (strcasecmp(dot, ".json") == 0) return "application/json";
    if (strcasecmp(dot, ".png") == 0) return "image/png";
    if (strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcasecmp(dot, ".svg") == 0) return "image/svg+xml";
    if (strcasecmp(dot, ".ico") == 0) return "image/x-icon";
    return "text/plain";
}

static void *web_server_worker(void *arg) {
    (void)arg;
    while (web_server_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(web_server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) continue;

        struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        char req[2048];
        ssize_t n = recv(client_fd, req, sizeof(req) - 1, 0);
        if (n > 0) {
            req[n] = '\0';
            char method[16] = "", uri[512] = "";
            sscanf(req, "%15s %511s", method, uri);

            if (strstr(uri, "..") == NULL) {
                char file_to_serve[512];
                if (strcmp(uri, "/") == 0) {
                    if (strcasestr(filename, ".html") || strcasestr(filename, ".htm")) {
                        strncpy(file_to_serve, filename, sizeof(file_to_serve) - 1);
                    } else {
                        strncpy(file_to_serve, "index.html", sizeof(file_to_serve) - 1);
                    }
                } else {
                    strncpy(file_to_serve, uri + 1, sizeof(file_to_serve) - 1);
                }
                file_to_serve[sizeof(file_to_serve) - 1] = '\0';

                FILE *fp = fopen(file_to_serve, "rb");
                if (fp) {
                    fseek(fp, 0, SEEK_END);
                    long fsz = ftell(fp);
                    fseek(fp, 0, SEEK_SET);

                    const char *mime = get_mime(file_to_serve);
                    char header[512];
                    snprintf(header, sizeof(header),
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: %s\r\n"
                        "Content-Length: %ld\r\n"
                        "Access-Control-Allow-Origin: *\r\n"
                        "Connection: close\r\n\r\n", mime, fsz);
                    send(client_fd, header, strlen(header), 0);

                    char buf[8192];
                    size_t r;
                    while ((r = fread(buf, 1, sizeof(buf), fp)) > 0) {
                        send(client_fd, buf, r, 0);
                    }
                    fclose(fp);
                } else {
                    const char *nf = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<h1>404 Not Found</h1>";
                    send(client_fd, nf, strlen(nf), 0);
                }
            }
        }
        close(client_fd);
    }
    return NULL;
}

static void trigger_live_web_server(void) {
    save_file();

    if (!web_server_running) {
        for (int p = 8080; p <= 8090; p++) {
            int sfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sfd < 0) continue;

            int opt = 1;
            setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
            addr.sin_port = htons(p);

            if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) == 0 && listen(sfd, 5) == 0) {
                web_server_sock = sfd;
                web_server_port = p;
                web_server_running = 1;
                pthread_create(&web_server_thread, NULL, web_server_worker, NULL);
                break;
            }
            close(sfd);
        }
    }

    if (web_server_running) {
        snprintf(notify_status, sizeof(notify_status), "[🌐 Server: http://localhost:%d/]", web_server_port);

        char open_cmd[256];
        if (system("which termux-open-url >/dev/null 2>&1") == 0) {
            snprintf(open_cmd, sizeof(open_cmd), "termux-open-url http://localhost:%d/ >/dev/null 2>&1 &", web_server_port);
            (void)!system(open_cmd);
        } else if (system("which xdg-open >/dev/null 2>&1") == 0) {
            snprintf(open_cmd, sizeof(open_cmd), "xdg-open http://localhost:%d/ >/dev/null 2>&1 &", web_server_port);
            (void)!system(open_cmd);
        }
    } else {
        snprintf(notify_status, sizeof(notify_status), "[✖ Falha ao abrir porta 8080]");
    }
}

static void run_mini_shell(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    printf("\033[?2004l\033[?1049l\033[?25h\n");

    printf("\033[1;35m╭────────────────────────────────────────────────────────────╮\033[0m\n");
    printf("\033[1;35m│\033[0m  \033[1;36mtedit: Mini-Shell / Terminal de Comandos\033[0m                  \033[1;35m│\033[0m\n");
    printf("\033[1;35m│\033[0m  \033[0;90m• Digite um comando para executar (ex: ls, make, git status)\033[0m\033[1;35m│\033[0m\n");
    printf("\033[1;35m│\033[0m  \033[0;90m• Pressione ENTER sem digitar nada para abrir a shell padrão\033[0m\033[1;35m│\033[0m\n");
    printf("\033[1;35m╰────────────────────────────────────────────────────────────╯\033[0m\n");
    printf("\033[1;33mtedit-cmd> \033[0m");
    fflush(stdout);

    char cmd[512] = "";
    if (fgets(cmd, sizeof(cmd), stdin)) {
        size_t clen = strlen(cmd);
        while (clen > 0 && (cmd[clen-1] == '\r' || cmd[clen-1] == '\n')) cmd[--clen] = '\0';

        if (clen == 0) {
            const char *sh = getenv("SHELL");
            if (!sh) sh = "/bin/sh";
            printf("\n\033[1;32m[Abrindo %s. Digite 'exit' para voltar ao tedit]\033[0m\n\n", sh);
            (void)!system(sh);
        } else {
            printf("\n\033[1;36m[Executando: %s]\033[0m\n------------------------------------------------------------\n", cmd);
            (void)!system(cmd);
            printf("------------------------------------------------------------\n");
            printf("\033[1;32m[Pressione ENTER para voltar ao tedit]\033[0m");
            fflush(stdout);
            int dummy = getchar();
            (void)dummy;
        }
    }

    enable_raw_mode();
    snprintf(notify_status, sizeof(notify_status), "[✔ Retornou da Shell]");
}

static char *base64_encode_mem(const unsigned char *data, size_t input_len) {
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t out_len = 4 * ((input_len + 2) / 3);
    char *out = malloc(out_len + 1);
    if (!out) return NULL;

    size_t i = 0, j = 0;
    while (i < input_len) {
        uint32_t a = i < input_len ? data[i++] : 0;
        uint32_t b = i < input_len ? data[i++] : 0;
        uint32_t c = i < input_len ? data[i++] : 0;
        uint32_t triple = (a << 16) + (b << 8) + c;

        out[j++] = tbl[(triple >> 18) & 0x3F];
        out[j++] = tbl[(triple >> 12) & 0x3F];
        out[j++] = tbl[(triple >> 6) & 0x3F];
        out[j++] = tbl[triple & 0x3F];
    }
    int mod = input_len % 3;
    if (mod == 1) { out[out_len - 1] = '='; out[out_len - 2] = '='; }
    else if (mod == 2) { out[out_len - 1] = '='; }
    out[out_len] = '\0';
    return out;
}

static void copy_all_to_clipboard(void) {
    size_t total_chars = 0;
    for (int i = 0; i < line_count; i++) total_chars += strlen(lines[i]) + 1;

    char *full_text = malloc(total_chars + 1);
    if (!full_text) return;
    full_text[0] = '\0';

    for (int i = 0; i < line_count; i++) {
        strcat(full_text, lines[i]);
        if (i < line_count - 1) strcat(full_text, "\n");
    }

    char *b64 = base64_encode_mem((const unsigned char *)full_text, strlen(full_text));
    if (b64) {
        printf("\033]52;c;%s\a", b64);
        fflush(stdout);
        free(b64);
    }

    FILE *clip_pipe = NULL;
    if (access("/data/data/com.termux/files/usr/bin/termux-clipboard-set", X_OK) == 0 ||
        system("which termux-clipboard-set >/dev/null 2>&1") == 0) {
        clip_pipe = popen("termux-clipboard-set", "w");
    } else if (system("which wl-copy >/dev/null 2>&1") == 0) {
        clip_pipe = popen("wl-copy", "w");
    } else if (system("which xclip >/dev/null 2>&1") == 0) {
        clip_pipe = popen("xclip -selection clipboard", "w");
    }

    if (clip_pipe) {
        fputs(full_text, clip_pipe);
        pclose(clip_pipe);
    }

    free(full_text);
    snprintf(notify_status, sizeof(notify_status), "[✔ Copiado p/ Clipboard!]");
}

static void insert_char(char c) {
    size_t len = strlen(lines[cur_line]);
    if (len < MAX_LINE_LEN - 2) {
        memmove(&lines[cur_line][cur_col + 1], &lines[cur_line][cur_col], len - cur_col + 1);
        lines[cur_line][cur_col] = c;
        cur_col++;
        is_modified = 1;
    }
}

static void insert_newline(void) {
    if (line_count < MAX_LINES - 1) {
        for (int i = line_count; i > cur_line + 1; i--) {
            strcpy(lines[i], lines[i - 1]);
        }
        strcpy(lines[cur_line + 1], &lines[cur_line][cur_col]);
        lines[cur_line][cur_col] = '\0';
        line_count++;
        cur_line++;
        cur_col = 0;
        is_modified = 1;
    }
}

static void insert_bulk_text(const char *text, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (text[i] == '\r') continue;
        if (text[i] == '\n') {
            insert_newline();
        } else if (isprint((unsigned char)text[i]) || (unsigned char)text[i] >= 128 || text[i] == '\t') {
            if (text[i] == '\t') {
                insert_char(' '); insert_char(' '); insert_char(' '); insert_char(' ');
            } else {
                insert_char(text[i]);
            }
        }
    }
}

static void delete_char(void) {
    if (cur_col > 0) {
        size_t len = strlen(lines[cur_line]);
        memmove(&lines[cur_line][cur_col - 1], &lines[cur_line][cur_col], len - cur_col + 1);
        cur_col--;
        is_modified = 1;
    } else if (cur_line > 0) {
        size_t prev_len = strlen(lines[cur_line - 1]);
        size_t cur_len = strlen(lines[cur_line]);
        if (prev_len + cur_len < MAX_LINE_LEN - 2) {
            strcat(lines[cur_line - 1], lines[cur_line]);
            for (int i = cur_line; i < line_count - 1; i++) {
                strcpy(lines[i], lines[i + 1]);
            }
            line_count--;
            cur_line--;
            cur_col = prev_len;
            is_modified = 1;
        }
    }
}

static void render_editor(void) {
    int term_rows, term_cols;
    get_window_size(&term_rows, &term_cols);
    const tedit_theme_t *th = &themes[current_theme_idx];

    int box_w = term_cols - 4;
    if (box_w > 105) box_w = 105;
    if (box_w < 30) box_w = 30;

    int box_h = term_rows - 4;
    if (box_h < 10) box_h = 10;

    int start_x = (term_cols - box_w) / 2;
    int start_y = (term_rows - box_h) / 2;
    int inner_w = box_w - 4;
    int inner_h = box_h - 3;

    if (cur_line < scroll_y) scroll_y = cur_line;
    if (cur_line >= scroll_y + inner_h) scroll_y = cur_line - inner_h + 1;

    printf("\033[H\033[?25l");

    for (int r = 0; r < start_y; r++) printf("\033[K\r\n");

    printf("\033[%d;%dH%s╭", start_y, start_x, th->border);
    for (int i = 0; i < box_w - 2; i++) printf("─");
    printf("╮%s\r\n", COLOR_RESET);

    char title[128];
    snprintf(title, sizeof(title), " tedit: %s%s [%s]%s ",
             filename, is_modified ? " *" : "", th->name, web_server_running ? " 🌐:8080" : "");
    printf("\033[%d;%dH%s│%s%s%-*s%s%s│%s\r\n",
           start_y + 1, start_x,
           th->border, COLOR_RESET,
           th->header, box_w - 2, title, COLOR_RESET,
           th->border, COLOR_RESET);

    for (int row = 0; row < inner_h; row++) {
        int file_line_idx = scroll_y + row;
        printf("\033[%d;%dH%s│%s ", start_y + 2 + row, start_x, th->border, COLOR_RESET);

        if (file_line_idx < line_count) {
            char *line_str = lines[file_line_idx];
            size_t len = strlen(line_str);

            if ((int)len > inner_w) {
                printf("%s%.*s%s", th->text, inner_w, line_str, COLOR_RESET);
            } else {
                printf("%s%s%s%*s", th->text, line_str, COLOR_RESET, (int)(inner_w - len), "");
            }
        } else {
            printf("%s~%*s%s", COLOR_MUTED, inner_w - 1, "", COLOR_RESET);
        }

        printf(" %s│%s\r\n", th->border, COLOR_RESET);
    }

    char status[128];
    if (strlen(notify_status) > 0) {
        snprintf(status, sizeof(status), " %s (L: %d/%d C: %d) ", notify_status, cur_line + 1, line_count, cur_col + 1);
    } else {
        snprintf(status, sizeof(status), " ^X Sair | ^S Salvar | ^K Copiar | ^H Web Server | Alt+S Shell ");
    }

    printf("\033[%d;%dH%s│%s%s%-*s%s%s│%s\r\n",
           start_y + 2 + inner_h, start_x,
           th->border, COLOR_RESET,
           th->footer, box_w - 2, status, COLOR_RESET,
           th->border, COLOR_RESET);

    printf("\033[%d;%dH%s╰", start_y + 3 + inner_h, start_x, th->border);
    for (int i = 0; i < box_w - 2; i++) printf("─");
    printf("╯%s", COLOR_RESET);

    int screen_cursor_y = start_y + 2 + (cur_line - scroll_y);
    int screen_cursor_x = start_x + 2 + cur_col;
    if (screen_cursor_x > start_x + inner_w + 1) screen_cursor_x = start_x + inner_w + 1;

    printf("\033[%d;%dH\033[?25h", screen_cursor_y, screen_cursor_x);
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    utilipc_init();
    utilipc_register_process("tedit");

    const char *target_file = "index.html";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--theme") == 0 || strcmp(argv[i], "-t") == 0) {
            if (i + 1 < argc) {
                const char *th_arg = argv[++i];
                for (size_t t = 0; t < THEME_COUNT; t++) {
                    if (strcasestr(themes[t].name, th_arg)) {
                        current_theme_idx = t;
                        break;
                    }
                }
            }
        } else {
            target_file = argv[i];
        }
    }

    load_file(target_file);
    enable_raw_mode();

    char read_buffer[4096];

    while (1) {
        render_editor();

        ssize_t bytes_read = read(STDIN_FILENO, read_buffer, sizeof(read_buffer) - 1);
        if (bytes_read <= 0) break;
        read_buffer[bytes_read] = '\0';
        notify_status[0] = '\0';

        // 1. Trata Bracketed Paste e Inserções em Lote
        char *paste_start = strstr(read_buffer, "\033[200~");
        if (paste_start) {
            paste_start += 6;
            char *paste_end = strstr(paste_start, "\033[201~");
            if (paste_end) *paste_end = '\0';
            insert_bulk_text(paste_start, strlen(paste_start));
            snprintf(notify_status, sizeof(notify_status), "[✔ Texto Colado em Bloco!]");
            continue;
        }

        if (bytes_read > 4 && read_buffer[0] != 27) {
            insert_bulk_text(read_buffer, bytes_read);
            snprintf(notify_status, sizeof(notify_status), "[✔ Texto Colado em Bloco!]");
            continue;
        }

        // 2. Atalhos com a tecla ALT (Sequência ESC + Letra)
        if (read_buffer[0] == 27 && bytes_read >= 2) {
            if (read_buffer[1] == 's' || read_buffer[1] == 'S') {
                run_mini_shell();
                continue;
            }
            if (read_buffer[1] == 'h' || read_buffer[1] == 'H') {
                trigger_live_web_server();
                continue;
            }
        }

        char c = read_buffer[0];

        // Ctrl + X (Salva e Encerra)
        if (c == 24) {
            save_file();
            break;
        }

        // Ctrl + S (Salva sem sair)
        if (c == 19) {
            save_file();
            continue;
        }

        // Ctrl + H (Live Web Server)
        if (c == 8) {
            trigger_live_web_server();
            continue;
        }

        // Ctrl + K (Copiar tudo para Clipboard)
        if (c == 11) {
            copy_all_to_clipboard();
            continue;
        }

        // Ctrl + T (Alternar Temas)
        if (c == 20) {
            current_theme_idx = (current_theme_idx + 1) % THEME_COUNT;
            snprintf(notify_status, sizeof(notify_status), "[Tema: %s]", themes[current_theme_idx].name);
            continue;
        }

        // Ctrl + Q (Sair sem salvar)
        if (c == 17) {
            break;
        }

        // Enter
        if (c == '\r' || c == '\n') {
            insert_newline();
            continue;
        }

        // Backspace
        if (c == 127) {
            delete_char();
            continue;
        }

        // Setas de Navegação
        if (c == 27 && bytes_read >= 3 && read_buffer[1] == '[') {
            if (read_buffer[2] == 'A') {
                if (cur_line > 0) {
                    cur_line--;
                    size_t len = strlen(lines[cur_line]);
                    if (cur_col > (int)len) cur_col = len;
                }
            } else if (read_buffer[2] == 'B') {
                if (cur_line < line_count - 1) {
                    cur_line++;
                    size_t len = strlen(lines[cur_line]);
                    if (cur_col > (int)len) cur_col = len;
                }
            } else if (read_buffer[2] == 'C') {
                size_t len = strlen(lines[cur_line]);
                if (cur_col < (int)len) cur_col++;
                else if (cur_line < line_count - 1) { cur_line++; cur_col = 0; }
            } else if (read_buffer[2] == 'D') {
                if (cur_col > 0) cur_col--;
                else if (cur_line > 0) { cur_line--; cur_col = strlen(lines[cur_line]); }
            } else if (read_buffer[2] == 'H') {
                cur_col = 0;
            } else if (read_buffer[2] == 'F') {
                cur_col = strlen(lines[cur_line]);
            }
            continue;
        }

        if (isprint((unsigned char)c) || (unsigned char)c >= 128) {
            insert_char(c);
        }
    }

    return 0;
}
