#ifndef UTILIPC_H
#define UTILIPC_H

#include <time.h>
#include <stddef.h>

#define UTILIPC_SHM_NAME "/utils_ipc_shm"
#define UTILIPC_MAX_MSG 256

typedef struct {
    double ram_used_mb;
    double ram_total_mb;
    double cpu_load1;
    char last_action[UTILIPC_MAX_MSG];
    time_t last_updated;
    unsigned int total_ipc_calls;
} utilipc_data_t;

int utilipc_init(void);
int utilipc_write_status(double ram_used, double ram_total, double load1, const char *action);
int utilipc_read_status(utilipc_data_t *out_data);
void utilipc_close(void);

#endif
