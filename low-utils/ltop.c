#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <dirent.h>
#include <sys/types.h>
#include <pwd.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include "low.h"

#define COLOR_RESET   "\033[0m"
#define COLOR_HEADER  "\033[1;37;45m"
#define COLOR_FOOTER  "\033[1;37;44m"
#define COLOR_SEL     "\033[1;30;46m"
#define COLOR_PID     "\033[1;33m"
#define COLOR_USER    "\033[0;36m"
#define COLOR_RUN     "\033[1;32m"
#define COLOR_SLEEP   "\033[0;34m"
#define COLOR_ZOMBIE  "\033[1;31m"
#define COLOR_DISK    "\033[1;35m"
#define COLOR_MUTED   "\033[0;90m"

typedef struct {
    pid_t pid;
    pid_t ppid;
    char state;
    char user[32];
    unsigned long long rss_kb;
    unsigned long long vsz_kb;
    int threads;
    int nice_val;
    char cmd[256];
} ProcEntry;

static struct termios orig_termios;
static int sort_by_mem = 1;
static int selected_idx = 0;
static int scroll_offset = 0;
static char status_msg[128] = "";

static void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    printf("\033[?1049l\033[?25h\033[0m");
    fflush(stdout);
}

static void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    printf("\033[?1049h\033[?25l\033[H");
    fflush(stdout);
}

static void sig_handler(int sig) {
    (void)sig;
    disable_raw_mode();
    exit(0);
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

static int compare_procs(const void *a, const void *b) {
    const ProcEntry *pa = (const ProcEntry *)a;
    const ProcEntry *pb = (const ProcEntry *)b;
    if (sort_by_mem) {
        if (pb->rss_kb > pa->rss_kb) return 1;
        if (pb->rss_kb < pa->rss_kb) return -1;
    }
    return (pa->pid > pb->pid) ? 1 : -1;
}

static size_t collect_processes(ProcEntry *procs, size_t max_count, int *out_running, int *out_sleeping, int *out_zombies) {
    DIR *dir = opendir("/proc");
    if (!dir) return 0;

    size_t count = 0;
    struct dirent *de;
    *out_running = 0;
    *out_sleeping = 0;
    *out_zombies = 0;

    while ((de = readdir(dir)) != NULL && count < max_count) {
        if (!isdigit(de->d_name[0])) continue;
        pid_t pid = atoi(de->d_name);

        ProcEntry p;
        memset(&p, 0, sizeof(ProcEntry));
        p.pid = pid;
        strcpy(p.user, "unknown");
        strcpy(p.cmd, "unknown");

        char path[128];
        snprintf(path, sizeof(path), "/proc/%d/stat", pid);
        FILE *fp = fopen(path, "r");
        if (fp) {
            char comm[128] = "";
            long rss_pages = 0;
            unsigned long vsz = 0;
            int ppid = 0, nice_v = 0, threads = 1;
            char state = 'S';

            if (fscanf(fp, "%*d (%127[^)]) %c %d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %*d %*d %*d %d %d %*d %*u %lu %ld",
                       comm, &state, &ppid, &nice_v, &threads, &vsz, &rss_pages) >= 5) {
                p.state = state;
                p.ppid = ppid;
                p.nice_val = nice_v;
                p.threads = threads;
                p.vsz_kb = vsz / 1024;
                p.rss_kb = (rss_pages * sysconf(_SC_PAGESIZE)) / 1024;
                strncpy(p.cmd, comm, sizeof(p.cmd) - 1);

                if (state == 'R') (*out_running)++;
                else if (state == 'Z') (*out_zombies)++;
                else (*out_sleeping)++;
            }
            fclose(fp);
        }

        snprintf(path, sizeof(path), "/proc/%d/status", pid);
        fp = fopen(path, "r");
        if (fp) {
            char line[256];
            while (fgets(line, sizeof(line), fp)) {
                if (strncmp(line, "Uid:", 4) == 0) {
                    uid_t uid = 0;
                    sscanf(line + 4, "%u", &uid);
                    struct passwd *pw = getpwuid(uid);
                    if (pw) strncpy(p.user, pw->pw_name, sizeof(p.user) - 1);
                    else snprintf(p.user, sizeof(p.user), "%u", (unsigned int)uid);
                    break;
                }
            }
            fclose(fp);
        }

        snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
        fp = fopen(path, "r");
        if (fp) {
            char cmdline[256];
            size_t n = fread(cmdline, 1, sizeof(cmdline) - 1, fp);
            if (n > 0) {
                for (size_t i = 0; i < n; i++) if (cmdline[i] == '\0') cmdline[i] = ' ';
                cmdline[n] = '\0';
                strncpy(p.cmd, cmdline, sizeof(p.cmd) - 1);
            }
            fclose(fp);
        }

        procs[count++] = p;
    }
    closedir(dir);
    qsort(procs, count, sizeof(ProcEntry), compare_procs);
    return count;
}

static void render_screen(const ProcEntry *procs, size_t count, int running, int sleeping, int zombies) {
    int rows, cols;
    get_window_size(&rows, &cols);

    printf("\033[H");

    char top_bar[512];
    snprintf(top_bar, sizeof(top_bar),
             " [ ltop - Interactive Process Monitor ]  Tasks: %zu (R:%d S:%d Z:%d)  Sort: %s ",
             count, running, sleeping, zombies, sort_by_mem ? "MEMORIA (RSS)" : "PID");

    printf("%s%-*.*s%s\r\n", COLOR_HEADER, cols, cols, top_bar, COLOR_RESET);
    printf("%s  %-6s %-12s %-5s %-4s %-8s %-9s %s%s\033[K\r\n",
           LOW_COLOR_LABEL, "PID", "USER", "STAT", "THRD", "RSS(MB)", "VSZ(MB)", "COMMAND", COLOR_RESET);

    int visible_rows = rows - 4;
    if (visible_rows < 1) visible_rows = 1;

    if (selected_idx < scroll_offset) scroll_offset = selected_idx;
    if (selected_idx >= scroll_offset + visible_rows) scroll_offset = selected_idx - visible_rows + 1;

    for (int r = 0; r < visible_rows; r++) {
        size_t idx = scroll_offset + r;
        if (idx < count) {
            const ProcEntry *p = &procs[idx];
            int is_sel = ((int)idx == selected_idx);

            const char *st_col = (p->state == 'R') ? COLOR_RUN : (p->state == 'Z') ? COLOR_ZOMBIE : (p->state == 'D') ? COLOR_DISK : COLOR_SLEEP;

            if (is_sel) {
                printf("%s> %-6d %-12.12s [%c]   %-4d %6.1fM %7.1fM %-*.*s%s\033[K\r\n",
                       COLOR_SEL, p->pid, p->user, p->state, p->threads,
                       (double)p->rss_kb / 1024.0, (double)p->vsz_kb / 1024.0,
                       cols - 50, cols - 50, p->cmd, COLOR_RESET);
            } else {
                printf("  %s%-6d%s %s%-12.12s%s %s[%c]%s   %-4d %6.1fM %7.1fM %s%-*.*s%s\033[K\r\n",
                       COLOR_PID, p->pid, COLOR_RESET,
                       COLOR_USER, p->user, COLOR_RESET,
                       st_col, p->state, COLOR_RESET,
                       p->threads,
                       (double)p->rss_kb / 1024.0, (double)p->vsz_kb / 1024.0,
                       COLOR_RESET, cols - 50, cols - 50, p->cmd, COLOR_RESET);
            }
        } else {
            printf("\033[K\r\n");
        }
    }

    char bot_bar[512];
    if (strlen(status_msg) > 0) {
        snprintf(bot_bar, sizeof(bot_bar), " %s ", status_msg);
    } else {
        snprintf(bot_bar, sizeof(bot_bar),
                 " [↑/↓] Navegar | [k] SIGTERM | [9] SIGKILL | [p] Pausar/Continuar | [m] Ordenar | [q] Sair ");
    }
    printf("%s%-*.*s%s", COLOR_FOOTER, cols, cols, bot_bar, COLOR_RESET);
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    if (argc >= 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        low_print_banner("ltop");
        printf("USAGE:\n  ./ltop\n\nInteractive real-time process manager and signal dispatcher.\n");
        return 0;
    }

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    enable_raw_mode();

    ProcEntry *procs = malloc(4096 * sizeof(ProcEntry));
    if (!procs) return 1;

    while (1) {
        int running = 0, sleeping = 0, zombies = 0;
        size_t count = collect_processes(procs, 4096, &running, &sleeping, &zombies);
        if (selected_idx >= (int)count) selected_idx = count > 0 ? count - 1 : 0;

        render_screen(procs, count, running, sleeping, zombies);
        status_msg[0] = '\0';

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };

        int sel = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
        if (sel > 0) {
            char buf[32];
            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
            if (n <= 0) break;
            buf[n] = '\0';

            char c = buf[0];

            if (c == 'q' || c == 'Q' || (c == 27 && n == 1)) break;

            if (c == 'k' || c == 'K') {
                if (count > 0 && selected_idx < (int)count) {
                    pid_t target = procs[selected_idx].pid;
                    if (kill(target, SIGTERM) == 0) {
                        snprintf(status_msg, sizeof(status_msg), "[✔] Enviado SIGTERM (15) para PID %d (%s)", target, procs[selected_idx].cmd);
                    } else {
                        snprintf(status_msg, sizeof(status_msg), "[✖] Erro ao enviar sinal: %s", strerror(errno));
                    }
                }
            } else if (c == '9') {
                if (count > 0 && selected_idx < (int)count) {
                    pid_t target = procs[selected_idx].pid;
                    if (kill(target, SIGKILL) == 0) {
                        snprintf(status_msg, sizeof(status_msg), "[✔] Enviado SIGKILL (9) para PID %d (%s)", target, procs[selected_idx].cmd);
                    } else {
                        snprintf(status_msg, sizeof(status_msg), "[✖] Erro ao enviar SIGKILL: %s", strerror(errno));
                    }
                }
            } else if (c == 'p' || c == 'P') {
                if (count > 0 && selected_idx < (int)count) {
                    pid_t target = procs[selected_idx].pid;
                    int sig = (procs[selected_idx].state == 'T') ? SIGCONT : SIGSTOP;
                    if (kill(target, sig) == 0) {
                        snprintf(status_msg, sizeof(status_msg), "[✔] Processo PID %d %s", target, (sig == SIGSTOP) ? "Pausado (SIGSTOP)" : "Despausado (SIGCONT)");
                    }
                }
            } else if (c == 'm' || c == 'M') {
                sort_by_mem = !sort_by_mem;
                selected_idx = 0;
            } else if (c == 27 && n >= 3 && buf[1] == '[') {
                if (buf[2] == 'A') {
                    if (selected_idx > 0) selected_idx--;
                } else if (buf[2] == 'B') {
                    if (selected_idx + 1 < (int)count) selected_idx++;
                }
            }
        }
    }

    free(procs);
    return 0;
}
