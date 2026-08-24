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
#include <errno.h>
#include <signal.h>

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

static void lock_shm(void) {
    if (!shm_ptr) return;
    pthread_mutex_lock(&shm_ptr->lock);
}

static void unlock_shm(void) {
    if (!shm_ptr) return;
    pthread_mutex_unlock(&shm_ptr->lock);
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

        // Auto-redimensiona caso o arquivo no disco seja menor (Evita SIGBUS!)
        struct stat st;
        if (fstat(shm_fd, &st) == 0 && (size_t)st.st_size < sizeof(utilipc_shm_t)) {
            if (ftruncate(shm_fd, sizeof(utilipc_shm_t)) < 0) {
                close(shm_fd);
                shm_fd = -1;
                return -1;
            }
        }
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

int utilipc_log(const char *tool, const char *action) {
    if (!shm_ptr && utilipc_init() < 0) return -1;

    lock_shm();
    time_t now = time(NULL);

    unsigned int idx = (shm_ptr->data.history_head + shm_ptr->data.history_count) % UTILIPC_HISTORY_SIZE;
    strncpy(shm_ptr->data.history[idx].tool, tool ? tool : "unknown", 31);
    shm_ptr->data.history[idx].tool[31] = '\0';
    strncpy(shm_ptr->data.history[idx].action, action ? action : "", UTILIPC_MAX_MSG - 1);
    shm_ptr->data.history[idx].action[UTILIPC_MAX_MSG - 1] = '\0';
    shm_ptr->data.history[idx].timestamp = now;

    if (shm_ptr->data.history_count < UTILIPC_HISTORY_SIZE) {
        shm_ptr->data.history_count++;
    } else {
        shm_ptr->data.history_head = (shm_ptr->data.history_head + 1) % UTILIPC_HISTORY_SIZE;
    }

    if (action) {
        strncpy(shm_ptr->data.last_action, action, UTILIPC_MAX_MSG - 1);
        shm_ptr->data.last_action[UTILIPC_MAX_MSG - 1] = '\0';
    }
    shm_ptr->data.last_updated = now;
    shm_ptr->data.total_ipc_calls++;

    unlock_shm();
    return 0;
}

int utilipc_write_status(double ram_used, double ram_total, double load1, const char *action) {
    if (!shm_ptr && utilipc_init() < 0) return -1;

    lock_shm();
    if (ram_used > 0) shm_ptr->data.ram_used_mb = ram_used;
    if (ram_total > 0) shm_ptr->data.ram_total_mb = ram_total;
    if (load1 >= 0) shm_ptr->data.cpu_load1 = load1;
    unlock_shm();

    if (action && strlen(action) > 0) {
        utilipc_log("system", action);
    }
    return 0;
}

int utilipc_read_status(utilipc_data_t *out_data) {
    if (!shm_ptr && utilipc_init() < 0) return -1;
    if (!out_data) return -1;

    lock_shm();
    memcpy(out_data, &shm_ptr->data, sizeof(utilipc_data_t));
    unlock_shm();

    return 0;
}

int utilipc_register_process(const char *tool_name) {
    if (!shm_ptr && utilipc_init() < 0) return -1;

    lock_shm();
    pid_t my_pid = getpid();
    int slot = -1;

    for (int i = 0; i < UTILIPC_MAX_PROCS; i++) {
        if (shm_ptr->data.active_procs[i].is_active) {
            if (kill(shm_ptr->data.active_procs[i].pid, 0) < 0 && errno == ESRCH) {
                shm_ptr->data.active_procs[i].is_active = 0;
                if (shm_ptr->data.active_proc_count > 0) shm_ptr->data.active_proc_count--;
            }
        }
        if (!shm_ptr->data.active_procs[i].is_active && slot == -1) {
            slot = i;
        }
    }

    if (slot != -1) {
        shm_ptr->data.active_procs[slot].pid = my_pid;
        strncpy(shm_ptr->data.active_procs[slot].tool, tool_name ? tool_name : "proc", 31);
        shm_ptr->data.active_procs[slot].start_time = time(NULL);
        shm_ptr->data.active_procs[slot].is_active = 1;
        shm_ptr->data.active_proc_count++;
    }

    unlock_shm();
    return 0;
}

int utilipc_unregister_process(void) {
    if (!shm_ptr) return 0;

    lock_shm();
    pid_t my_pid = getpid();
    for (int i = 0; i < UTILIPC_MAX_PROCS; i++) {
        if (shm_ptr->data.active_procs[i].is_active && shm_ptr->data.active_procs[i].pid == my_pid) {
            shm_ptr->data.active_procs[i].is_active = 0;
            if (shm_ptr->data.active_proc_count > 0) shm_ptr->data.active_proc_count--;
            break;
        }
    }
    unlock_shm();
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
