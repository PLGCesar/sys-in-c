#include "kvfs.h"
#include "kstring.h"
#include "kmem.h"

static kvfs_node_t vfs_table[KVFS_MAX_NODES];
static size_t vfs_node_count = 0;

static const char etc_hostname[] = "utils-in-c-os\n";
static const char etc_os_release[] = "NAME=\"utils-in-c OS\"\nVERSION=\"2.5\"\nID=utils-os\nPRETTY_NAME=\"utils-in-c OS v2.5\"\n";
static const char etc_passwd[] = "root:x:0:0:root:/root:/bin/sh\nuser:x:1000:1000:User:/home/user:/bin/sh\n";
static const char etc_kernel_config[] = "KERNEL=Multiboot1\nVIDEO=VBE800x600x32\nVFS=Real_Hierarchical_RAM_ATA\nSMP=Enabled\n";
static const char etc_motd[] = "==========================================\n Welcome to utils-in-c OS (Kernel v2.5)\n Real VFS & ATA PIO Disk Active!\n==========================================\n";
static const char readme_root[] = "Welcome to utils-in-c OS VFS Root.\nUse 'cd bin', 'cd etc' or 'cd dev' to explore.\nUse 'cat <file>' to read files.";

static const char dev_null_info[] = "[DEV] Character Device: Null (/dev/null)\n";
static const char dev_zero_info[] = "[DEV] Character Device: Zero Source (/dev/zero)\n";
static const char dev_fb0_info[] = "[DEV] Video Framebuffer: 800x600x32bpp VBE ARGB at 0xFD000000\n";
static const char dev_ps2kbd_info[] = "[DEV] Input Device: PS/2 Keyboard on Port 0x60 (IRQ 1)\n";
static const char dev_ps2mouse_info[] = "[DEV] Input Device: PS/2 Mouse on Port 0x60 (IRQ 12)\n";
static const char dev_ata0_info[] = "[DEV] Block Device: Primary Master ATA PIO Hard Disk on 0x1F0\n";

static const char bin_ls_help[] = "[BIN] ls [dir] - Lista os arquivos e subdiretorios\n";
static const char bin_cd_help[] = "[BIN] cd <dir> - Navega entre os diretorios do sistema\n";
static const char bin_pwd_help[] = "[BIN] pwd - Exibe o diretorio de trabalho atual\n";
static const char bin_cat_help[] = "[BIN] cat <path> - Exibe o conteudo de arquivos ou informacoes de /dev\n";
static const char bin_mem_help[] = "[BIN] mem - Exibe diagnostico da memoria heap KMEM\n";
static const char bin_clear_help[] = "[BIN] clear - Limpa o terminal CLI\n";
static const char bin_echo_help[] = "[BIN] echo <texto> - Imprime texto na tela\n";
static const char bin_ata_help[] = "[BIN] ata [info|read <lba>] - Interage diretamente com o disco rigido ATA PIO\n";
static const char bin_help_help[] = "[BIN] help - Exibe a ajuda geral do sistema\n";
static const char bin_exit_help[] = "[BIN] exit - Retorna ao modo de interface grafica GUI\n";

static void sanitize_path(const char *in, char *out, size_t max_len) {
    size_t j = 0;
    if (!in || in[0] == '\0') {
        out[0] = '/'; out[1] = '\0'; return;
    }
    if (in[0] != '/') out[j++] = '/';
    for (size_t i = 0; in[i] != '\0' && j < max_len - 1; i++) {
        if (in[i] == '/' && j > 0 && out[j - 1] == '/') continue;
        out[j++] = in[i];
    }
    while (j > 1 && out[j - 1] == '/') j--;
    out[j] = '\0';
}

void kvfs_init(void) {
    for (size_t i = 0; i < KVFS_MAX_NODES; i++) {
        vfs_table[i].is_used = 0;
        vfs_table[i].path[0] = '\0';
        vfs_table[i].data = NULL;
        vfs_table[i].size = 0;
        vfs_table[i].type = KVFS_TYPE_FILE;
        vfs_table[i].mode = 0;
    }
    vfs_node_count = 0;

    kvfs_mkdir("/");
    kvfs_mkdir("/bin");
    kvfs_mkdir("/etc");
    kvfs_mkdir("/dev");

    kvfs_create("/readme.txt", readme_root, sizeof(readme_root) - 1, KVFS_TYPE_FILE, 0644);

    kvfs_create("/bin/ls", bin_ls_help, sizeof(bin_ls_help) - 1, KVFS_TYPE_BIN, 0755);
    kvfs_create("/bin/cd", bin_cd_help, sizeof(bin_cd_help) - 1, KVFS_TYPE_BIN, 0755);
    kvfs_create("/bin/pwd", bin_pwd_help, sizeof(bin_pwd_help) - 1, KVFS_TYPE_BIN, 0755);
    kvfs_create("/bin/cat", bin_cat_help, sizeof(bin_cat_help) - 1, KVFS_TYPE_BIN, 0755);
    kvfs_create("/bin/mem", bin_mem_help, sizeof(bin_mem_help) - 1, KVFS_TYPE_BIN, 0755);
    kvfs_create("/bin/clear", bin_clear_help, sizeof(bin_clear_help) - 1, KVFS_TYPE_BIN, 0755);
    kvfs_create("/bin/echo", bin_echo_help, sizeof(bin_echo_help) - 1, KVFS_TYPE_BIN, 0755);
    kvfs_create("/bin/ata", bin_ata_help, sizeof(bin_ata_help) - 1, KVFS_TYPE_BIN, 0755);
    kvfs_create("/bin/help", bin_help_help, sizeof(bin_help_help) - 1, KVFS_TYPE_BIN, 0755);
    kvfs_create("/bin/exit", bin_exit_help, sizeof(bin_exit_help) - 1, KVFS_TYPE_BIN, 0755);

    kvfs_create("/etc/hostname", etc_hostname, sizeof(etc_hostname) - 1, KVFS_TYPE_FILE, 0644);
    kvfs_create("/etc/os-release", etc_os_release, sizeof(etc_os_release) - 1, KVFS_TYPE_FILE, 0644);
    kvfs_create("/etc/passwd", etc_passwd, sizeof(etc_passwd) - 1, KVFS_TYPE_FILE, 0644);
    kvfs_create("/etc/kernel.config", etc_kernel_config, sizeof(etc_kernel_config) - 1, KVFS_TYPE_FILE, 0644);
    kvfs_create("/etc/motd", etc_motd, sizeof(etc_motd) - 1, KVFS_TYPE_FILE, 0644);

    kvfs_create("/dev/null", dev_null_info, sizeof(dev_null_info) - 1, KVFS_TYPE_DEV, 0666);
    kvfs_create("/dev/zero", dev_zero_info, sizeof(dev_zero_info) - 1, KVFS_TYPE_DEV, 0666);
    kvfs_create("/dev/fb0", dev_fb0_info, sizeof(dev_fb0_info) - 1, KVFS_TYPE_DEV, 0660);
    kvfs_create("/dev/ps2kbd", dev_ps2kbd_info, sizeof(dev_ps2kbd_info) - 1, KVFS_TYPE_DEV, 0660);
    kvfs_create("/dev/ps2mouse", dev_ps2mouse_info, sizeof(dev_ps2mouse_info) - 1, KVFS_TYPE_DEV, 0660);
    kvfs_create("/dev/ata0", dev_ata0_info, sizeof(dev_ata0_info) - 1, KVFS_TYPE_DEV, 0660);
}

int kvfs_mkdir(const char *path) {
    return kvfs_create(path, NULL, 0, KVFS_TYPE_DIR, 0755);
}

int kvfs_create(const char *path, const void *data, size_t size, uint8_t type, uint16_t mode) {
    if (!path || vfs_node_count >= KVFS_MAX_NODES) return -1;

    char clean_path[KVFS_MAX_PATH];
    sanitize_path(path, clean_path, sizeof(clean_path));

    for (size_t i = 0; i < KVFS_MAX_NODES; i++) {
        if (!vfs_table[i].is_used) {
            kstrncpy(vfs_table[i].path, clean_path, sizeof(vfs_table[i].path) - 1);
            vfs_table[i].data = (const uint8_t *)data;
            vfs_table[i].size = size;
            vfs_table[i].type = type;
            vfs_table[i].mode = mode;
            vfs_table[i].is_used = 1;
            vfs_node_count++;
            return 0;
        }
    }
    return -1;
}

const kvfs_node_t *kvfs_open(const char *path) {
    if (!path) return NULL;
    char clean_path[KVFS_MAX_PATH];
    sanitize_path(path, clean_path, sizeof(clean_path));

    for (size_t i = 0; i < KVFS_MAX_NODES; i++) {
        if (vfs_table[i].is_used && kstrcmp(vfs_table[i].path, clean_path) == 0) {
            return &vfs_table[i];
        }
    }
    return NULL;
}

int kvfs_read(const kvfs_node_t *node, void *buf, size_t offset, size_t count) {
    if (!node || !buf || !node->data || offset >= node->size) return 0;
    size_t to_copy = count;
    if (offset + to_copy > node->size) to_copy = node->size - offset;
    kmemcpy(buf, node->data + offset, to_copy);
    return (int)to_copy;
}

void kvfs_list_dir(const char *dir_path, kvfs_ls_callback_t callback) {
    if (!callback) return;
    char target_dir[KVFS_MAX_PATH];
    sanitize_path(dir_path ? dir_path : "/", target_dir, sizeof(target_dir));

    size_t dir_len = kstrlen(target_dir);
    int is_root = (kstrcmp(target_dir, "/") == 0);

    for (size_t i = 0; i < KVFS_MAX_NODES; i++) {
        if (!vfs_table[i].is_used) continue;
        if (kstrcmp(vfs_table[i].path, target_dir) == 0) continue;

        const char *p = vfs_table[i].path;
        if (is_root) {
            if (p[0] == '/' && p[1] != '\0') {
                const char *sub = kstrchr(p + 1, '/');
                if (!sub) {
                    callback(p + 1, vfs_table[i].size, vfs_table[i].type, vfs_table[i].mode);
                } else if (vfs_table[i].type == KVFS_TYPE_DIR && sub[1] == '\0') {
                    char dname[32];
                    size_t dlen = sub - (p + 1);
                    if (dlen < sizeof(dname)) {
                        kstrncpy(dname, p + 1, dlen);
                        dname[dlen] = '\0';
                        callback(dname, vfs_table[i].size, vfs_table[i].type, vfs_table[i].mode);
                    }
                }
            }
        } else {
            if (kstrncmp(p, target_dir, dir_len) == 0 && p[dir_len] == '/') {
                const char *item_name = p + dir_len + 1;
                const char *next_slash = kstrchr(item_name, '/');
                if (!next_slash) {
                    callback(item_name, vfs_table[i].size, vfs_table[i].type, vfs_table[i].mode);
                }
            }
        }
    }
}
