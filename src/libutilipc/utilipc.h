#ifndef UTILIPC_H
#define UTILIPC_H

#include <time.h>
#include <stddef.h>
#include <sys/types.h>

#define UTILIPC_SHM_NAME     "/utils_ipc_shm"
#define UTILIPC_MAX_MSG      256
#define UTILIPC_HISTORY_SIZE 16
#define UTILIPC_MAX_PROCS    16

// Entrada do Flight-Recorder de Ações
typedef struct {
    char tool[32];
    char action[UTILIPC_MAX_MSG];
    time_t timestamp;
} utilipc_log_entry_t;

// Registro de Processos Ativos
typedef struct {
    pid_t pid;
    char tool[32];
    time_t start_time;
    int is_active;
} utilipc_proc_entry_t;

// Estrutura Completa de Memória Compartilhada
typedef struct {
    double ram_used_mb;
    double ram_total_mb;
    double cpu_load1;
    char last_action[UTILIPC_MAX_MSG];
    time_t last_updated;
    unsigned int total_ipc_calls;

    // Histórico circular
    utilipc_log_entry_t history[UTILIPC_HISTORY_SIZE];
    unsigned int history_head;
    unsigned int history_count;

    // Processos ativos
    utilipc_proc_entry_t active_procs[UTILIPC_MAX_PROCS];
    unsigned int active_proc_count;
} utilipc_data_t;

// Funções de Ciclo de Vida
int utilipc_init(void);
void utilipc_close(void);

// API Legada (100% Retrocompatível)
int utilipc_write_status(double ram_used, double ram_total, double load1, const char *action);
int utilipc_read_status(utilipc_data_t *out_data);

// Novas Funções Avançadas da .SO
int utilipc_log(const char *tool, const char *action);
int utilipc_register_process(const char *tool_name);
int utilipc_unregister_process(void);
int utilipc_get_history(utilipc_log_entry_t *out_entries, size_t max_count, size_t *out_actual);

#endif
