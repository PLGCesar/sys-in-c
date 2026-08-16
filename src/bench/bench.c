#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "../libutilipc/utilipc.h"

#define COLOR_RESET   "\033[0m"
#define COLOR_TITLE   "\033[1;35m"
#define COLOR_VAL     "\033[1;32m"

static double get_time_sec(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0.0;
    }
    return (double)ts.tv_sec + ((double)ts.tv_nsec / 1000000000.0);
}

static void run_cpu_bench(void) {
    printf("  [1/2] Running CPU Integer & Math Benchmark...\n");
    fflush(stdout);

    double start = get_time_sec();
    if (start == 0.0) {
        printf("    • Error: Failed to retrieve system time.\n");
        return;
    }

    double duration = 2.0;
    unsigned long long operations = 0;
    volatile unsigned long long count = 0;
    volatile double fp_count = 0.0;

    while (get_time_sec() - start < duration) {
        unsigned long long local_count = 0;
        double local_fp = 0.0;

        for (int i = 1; i <= 1000000; i++) {
            local_count += (i * 3) ^ (i >> 2);
            local_count ^= (i << 1);
            local_fp += ((double)i * 1.000001);
        }

        count += local_count;
        fp_count += local_fp;
        operations += 1000000;
    }

    double elapsed = get_time_sec() - start;
    double mops = 0.0;
    
    if (elapsed > 0.0) {
        mops = (double)operations / (elapsed * 1000000.0);
    }

    printf("    • Operations Completed : %llu\n", operations);
    printf("    • Elapsed Time         : %.3f seconds\n", elapsed);
    printf("    • CPU Performance      : %s%.2f MOPs/sec%s\n\n", COLOR_VAL, mops, COLOR_RESET);
}

static void run_mem_bench(void) {
    printf("  [2/2] Running RAM / Storage Throughput Benchmark...\n");
    fflush(stdout);

    size_t buf_size = 32 * 1024 * 1024;
    unsigned char *buf = malloc(buf_size);
    unsigned char *buf_copy = malloc(buf_size);

    if (!buf || !buf_copy) {
        printf("    • Error: Memory allocation failed.\n");
        if (buf) free(buf);
        if (buf_copy) free(buf_copy);
        return;
    }

    double start = get_time_sec();
    if (start == 0.0) {
        printf("    • Error: Failed to retrieve system time.\n");
        free(buf);
        free(buf_copy);
        return;
    }

    int passes = 10;

    for (int p = 0; p < passes; p++) {
        // Sequential Memory Write
        memset(buf, (p & 0xFF), buf_size);

        // Sequential Memory Copy (Read + Write)
        memcpy(buf_copy, buf, buf_size);

        // Sequential Memory Read
        volatile unsigned long sum = 0;
        unsigned long local_sum = 0;
        for (size_t i = 0; i < buf_size; i += 64) {
            local_sum += buf_copy[i];
        }
        sum += local_sum;
    }

    double elapsed = get_time_sec() - start;
    
    // Each pass processes: 
    // memset (1x write), memcpy (1x read, 1x write), read loop (1x read)
    // Total = 4 * buf_size per pass
    double total_mb = (double)(4ULL * buf_size * passes) / (1024.0 * 1024.0);
    double mbps = 0.0;
    
    if (elapsed > 0.0) {
        mbps = total_mb / elapsed;
    }

    printf("    • Memory Processed     : %.1f MB\n", total_mb);
    printf("    • Elapsed Time         : %.3f seconds\n", elapsed);
    printf("    • RAM/Bus Throughput   : %s%.2f MB/sec%s\n", COLOR_VAL, mbps, COLOR_RESET);

    free(buf);
    free(buf_copy);
}

int main(void) {
    if (utilipc_init() != 0) {
        fprintf(stderr, "Warning: Failed to initialize IPC. Continuing without logging.\n");
    }

    printf("%s==========================================%s\n", COLOR_TITLE, COLOR_RESET);
    printf("%s[ bench - Hardware Micro-Benchmark ]%s\n", COLOR_TITLE, COLOR_RESET);
    printf("%s==========================================%s\n", COLOR_TITLE, COLOR_RESET);

    run_cpu_bench();
    run_mem_bench();

    printf("%s==========================================%s\n", COLOR_TITLE, COLOR_RESET);

    char log_msg[UTILIPC_MAX_MSG];
    snprintf(log_msg, sizeof(log_msg), "bench: completed hardware benchmark");
    utilipc_write_status(-1.0, -1.0, -1.0, log_msg);

    utilipc_close();
    return 0;
}
