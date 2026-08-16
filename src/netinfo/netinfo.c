#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include "../libutilipc/utilipc.h"

static void get_net_usage(unsigned long long *rx, unsigned long long *tx) {
    FILE *fp = fopen("/proc/net/dev", "r");
    *rx = 0; *tx = 0;
    if (!fp) return;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strchr(line, ':')) {
            unsigned long long r_bytes, t_bytes;
            if (sscanf(strchr(line, ':') + 1, "%llu %*u %*u %*u %*u %*u %*u %*u %llu", &r_bytes, &t_bytes) == 2) {
                *rx += r_bytes;
                *tx += t_bytes;
            }
        }
    }
    fclose(fp);
}

static void run_tui(void) {
    printf("\033[?25l\033[H\033[J"); // Oculta cursor e limpa
    unsigned long long rx_prev = 0, tx_prev = 0;
    get_net_usage(&rx_prev, &tx_prev);

    const char *spark[] = {" ", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
    double rx_hist[20] = {0}, tx_hist[20] = {0};

    while (1) {
        sleep(1);
        unsigned long long rx_now, tx_now;
        get_net_usage(&rx_now, &tx_now);

        double rx_mbps = (double)(rx_now - rx_prev) / (1024.0 * 1024.0);
        double tx_mbps = (double)(tx_now - tx_prev) / (1024.0 * 1024.0);
        rx_prev = rx_now; tx_prev = tx_now;

        for (int i=0; i<19; i++) { rx_hist[i]=rx_hist[i+1]; tx_hist[i]=tx_hist[i+1]; }
        rx_hist[19] = rx_mbps; tx_hist[19] = tx_mbps;

        printf("\033[H\033[1;35m==========================================\n");
        printf("[ netinfo TUI - Live Network Monitor ]\n");
        printf("==========================================\033[0m\n");
        
        printf(" \033[1;32m↓ RX (Download): %.2f MB/s\033[0m\n  ", rx_mbps);
        for(int i=0; i<20; i++) {
            int idx = (int)(rx_hist[i] * 4.0);
            if(idx > 7) idx = 7; if(idx < 0) idx = 0;
            printf("\033[1;32m%s\033[0m", spark[idx]);
        }
        
        printf("\n\n \033[1;36m↑ TX (Upload):   %.2f MB/s\033[0m\n  ", tx_mbps);
        for(int i=0; i<20; i++) {
            int idx = (int)(tx_hist[i] * 4.0);
            if(idx > 7) idx = 7; if(idx < 0) idx = 0;
            printf("\033[1;36m%s\033[0m", spark[idx]);
        }
        printf("\n\033[1;35m==========================================\n");
        printf("[Press Ctrl+C to Exit]\033[0m\033[K\n");
        fflush(stdout);
    }
}

int main(int argc, char *argv[]) {
    if (argc >= 2 && strcmp(argv[1], "-tui") == 0) {
        run_tui();
        return 0;
    }
    printf("Usage:\n  netinfo -tui   (Run Interactive Live Monitor)\n");
    return 0;
}
