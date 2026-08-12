#define _GNU_SOURCE
#define _DEFAULT_SOURCE

#include "utilipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

typedef struct {
    pthread_mutex_t lock;
    utilipc_data_t data;
} utilipc_shm_t;

static int shm_fd = -1;
static utilipc_shm_t *shm_ptr = NULL;

static const char *get_shm_path(void) {
    static char full_path[512];
    const char *tmp = getenv("TMPDIR");
    if (!tmp || strlen(tmp) == 0) tmp = "/tmp";
    snprintf(full_path, sizeof(full_path), "%s/utils_ipc_shm", tmp);
    return full_path;
}

int utilipc_init(void) {
    if (shm_ptr && shm_ptr != MAP_FAILED) return 0;

    int created = 0;
    const char *path = get_shm_path();

    shm_fd = open(path, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (shm_fd >= 0) {
        created = 1;
        if (ftruncate(shm_fd, sizeof(utilipc_shm_t)) < 0) {
            close(shm_fd);
            shm_fd = -1;
            return -1;
        }
    } else {
        shm_fd = open(path, O_RDWR);
        if (shm_fd < 0) return -1;
    }

    shm_ptr = mmap(NULL, sizeof(utilipc_shm_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        close(shm_fd);
        shm_fd = -1;
        return -1;
    }

    if (created) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&shm_ptr->lock, &attr);
        pthread_mutexattr_destroy(&attr);

        memset(&shm_ptr->data, 0, sizeof(utilipc_data_t));
        strncpy(shm_ptr->data.last_action, "SHM Initialized", UTILIPC_MAX_MSG - 1);
        shm_ptr->data.last_updated = time(NULL);
    }

    return 0;
}

int utilipc_write_status(double ram_used, double ram_total, double load1, const char *action) {
    if (!shm_ptr) if (utilipc_init() < 0) return -1;

    pthread_mutex_lock(&shm_ptr->lock);
    if (ram_used > 0) shm_ptr->data.ram_used_mb = ram_used;
    if (ram_total > 0) shm_ptr->data.ram_total_mb = ram_total;
    if (load1 >= 0) shm_ptr->data.cpu_load1 = load1;
    if (action && strlen(action) > 0) {
        strncpy(shm_ptr->data.last_action, action, UTILIPC_MAX_MSG - 1);
        shm_ptr->data.last_action[UTILIPC_MAX_MSG - 1] = '\0';
    }
    shm_ptr->data.last_updated = time(NULL);
    shm_ptr->data.total_ipc_calls++;
    pthread_mutex_unlock(&shm_ptr->lock);

    return 0;
}

int utilipc_read_status(utilipc_data_t *out_data) {
    if (!shm_ptr) if (utilipc_init() < 0) return -1;

    pthread_mutex_lock(&shm_ptr->lock);
    memcpy(out_data, &shm_ptr->data, sizeof(utilipc_data_t));
    pthread_mutex_unlock(&shm_ptr->lock);

    return 0;
}

void utilipc_close(void) {
    if (shm_ptr && shm_ptr != MAP_FAILED) {
        munmap(shm_ptr, sizeof(utilipc_shm_t));
        shm_ptr = NULL;
    }
    if (shm_fd >= 0) {
        close(shm_fd);
        shm_fd = -1;
    }
}
