#ifndef KVFS_H
#define KVFS_H

#include <stddef.h>
#include <stdint.h>

#define KVFS_MAX_NODES 64
#define KVFS_MAX_PATH  128

/* Tipos de Nós no VFS */
typedef enum {
    KVFS_TYPE_FILE = 0, /* Arquivo Regular */
    KVFS_TYPE_DIR,      /* Diretório */
    KVFS_TYPE_BIN,      /* Binário / Comando */
    KVFS_TYPE_DEV       /* Dispositivo de Hardware */
} kvfs_node_type_t;

/* Estrutura de Nó no Sistema de Arquivos */
typedef struct {
    char path[KVFS_MAX_PATH];
    const uint8_t *data;
    size_t size;
    uint8_t type;       /* kvfs_node_type_t */
    uint16_t mode;      /* Permissões UNIX (ex: 0755, 0644) */
    int is_used;
} kvfs_node_t;

/* Callback de listagem de diretório */
typedef void (*kvfs_ls_callback_t)(const char *name, size_t size, uint8_t type, uint16_t mode);

/* Inicialização e Formatação da Raiz */
void kvfs_init(void);

/* Criação de Diretórios e Nós */
int kvfs_mkdir(const char *path);
int kvfs_create(const char *path, const void *data, size_t size, uint8_t type, uint16_t mode);

/* Abertura e Leitura */
const kvfs_node_t *kvfs_open(const char *path);
int kvfs_read(const kvfs_node_t *node, void *buf, size_t offset, size_t count);

/* Listagem de Diretório Específico */
void kvfs_list_dir(const char *dir_path, kvfs_ls_callback_t callback);

#endif
