#include "multiboot.h"
#include "idt.h"
#include "ps2.h"
#include "gdt.h"
#include "../freestanding/kmem.h"
#include "../freestanding/kfixed.h"
#include "../freestanding/kprintf.h"
#include "../freestanding/kgfx.h"
#include "../freestanding/kstring.h"
#include "../freestanding/kvfs.h"
#include "../freestanding/kata.h"

static kgfx_fb_t os_fb;
static int os_mode = 0; /* 0 = GUI Mode, 1 = CLI Mode */

static char cli_input[128] = "";
static size_t cli_pos = 0;

/* Buffer de restauração do mouse (16x16) */
#define MOUSE_BUF_SZ 16
static uint32_t mouse_under_buf[MOUSE_BUF_SZ * MOUSE_BUF_SZ];
static int mouse_under_saved = 0;
static int under_x = 0;
static int under_y = 0;
static int under_w = 0;
static int under_h = 0;

static int prev_mouse_x = 400;
static int prev_mouse_y = 300;

static void save_mouse_under(int x, int y, int w, int h) {
    under_x = x; under_y = y; under_w = w; under_h = h;
    for (int r = 0; r < h; r++) {
        for (int c = 0; c < w; c++) {
            int px = x + c;
            int py = y + r;
            if (px >= 0 && (uint32_t)px < os_fb.width && py >= 0 && (uint32_t)py < os_fb.height) {
                mouse_under_buf[r * MOUSE_BUF_SZ + c] = os_fb.buffer[py * os_fb.pitch + px];
            } else {
                mouse_under_buf[r * MOUSE_BUF_SZ + c] = KGFX_DARKGRAY;
            }
        }
    }
    mouse_under_saved = 1;
}

static void restore_mouse_under(void) {
    if (!mouse_under_saved) return;
    for (int r = 0; r < under_h; r++) {
        for (int c = 0; c < under_w; c++) {
            int px = under_x + c;
            int py = under_y + r;
            if (px >= 0 && (uint32_t)px < os_fb.width && py >= 0 && (uint32_t)py < os_fb.height) {
                os_fb.buffer[py * os_fb.pitch + px] = mouse_under_buf[r * MOUSE_BUF_SZ + c];
            }
        }
    }
    mouse_under_saved = 0;
}

void os_draw_mouse_cursor(void) {
    if (os_mode != 0) return;

    kgfx_mouse_t *mouse = ps2_get_mouse_state();
    if (mouse->x == prev_mouse_x && mouse->y == prev_mouse_y && mouse_under_saved) return;

    restore_mouse_under();
    save_mouse_under(mouse->x, mouse->y, MOUSE_BUF_SZ, MOUSE_BUF_SZ);
    kgfx_draw_cursor(&os_fb, mouse);

    prev_mouse_x = mouse->x;
    prev_mouse_y = mouse->y;
}

static void os_putchar(char c) {
    static int cursor_x = 30;
    static int cursor_y = 120;

    if (c == '\n') {
        cursor_x = 30;
        cursor_y += 14;
    } else if (c == '\b') {
        if (cursor_x >= 38) {
            cursor_x -= 8;
            kgfx_draw_rect(&os_fb, cursor_x, cursor_y, 8, 12, (os_mode == 1) ? KGFX_BLACK : KGFX_DARKGRAY, 1);
        }
    } else {
        kgfx_draw_char(&os_fb, cursor_x, cursor_y, c, KGFX_WHITE, 0);
        cursor_x += 8;
        if (cursor_x > (int)os_fb.width - 40) {
            cursor_x = 30;
            cursor_y += 14;
        }
    }

    if (cursor_y > (int)os_fb.height - 40) {
        kgfx_clear(&os_fb, (os_mode == 1) ? KGFX_BLACK : KGFX_DARKGRAY);
        kgfx_draw_rounded_rect(&os_fb, 10, 10, os_fb.width - 20, os_fb.height - 20, 8, KGFX_CYAN, 0);
        cursor_y = 40;
        cursor_x = 30;
    }
}

static void draw_gui_desktop(void) {
    kgfx_clear(&os_fb, KGFX_DARKGRAY);

    kgfx_draw_rounded_rect(&os_fb, 10, 10, os_fb.width - 20, os_fb.height - 20, 10, KGFX_CYAN, 0);
    kgfx_draw_rect(&os_fb, 12, 12, os_fb.width - 24, 36, KGFX_BLUE, 1);
    kgfx_draw_string_scaled(&os_fb, 20, 20, "utils-in-c OS v2.5", KGFX_WHITE, KGFX_BLUE, 2);

    kgfx_draw_rect_alpha(&os_fb, 20, 60, os_fb.width - 40, 45, kgfx_argb(180, 24, 24, 37));
    kgfx_draw_string(&os_fb, 30, 75, "Pressione [ESC], [F1] ou [TAB] para abrir o Terminal CLI!", KGFX_YELLOW, 0);

    kgfx_draw_filled_circle(&os_fb, os_fb.width - 80, 180, 35, KGFX_PURPLE);
    kgfx_draw_triangle(&os_fb, os_fb.width - 150, 220, os_fb.width - 110, 150, os_fb.width - 70, 220, KGFX_GREEN, 1);

    const kata_drive_info_t *disk = kata_get_info();

    kgfx_draw_string(&os_fb, 30, 130, "[Kernel Core Subsystems Online]", KGFX_WHITE, 0);
    kgfx_draw_string(&os_fb, 30, 150, "  * Hierarchical VFS  : Mounted Root / with /bin, /etc, /dev", KGFX_CYAN, 0);
    if (disk->drive_present) {
        kgfx_draw_string(&os_fb, 30, 170, "  * ATA PIO Storage   : Primary Master Disk Detected (/dev/ata0)", KGFX_GREEN, 0);
    } else {
        kgfx_draw_string(&os_fb, 30, 170, "  * ATA PIO Storage   : Driver Ready (Primary Bus Active)", KGFX_CYAN, 0);
    }
    kgfx_draw_string(&os_fb, 30, 190, "  * PS/2 Controller   : Keyboard & Mouse (IRQ 1 & 12 Active)", KGFX_CYAN, 0);
    kgfx_draw_string(&os_fb, 30, 210, "  * Video System      : VBE 32-bit ARGB + Alpha Blending Active", KGFX_CYAN, 0);
}

static void vfs_ls_callback(const char *name, size_t size, uint8_t type, uint16_t mode) {
    (void)mode;
    if (type == KVFS_TYPE_DIR) {
        kprintf("  \033[1;34m[DIR]\033[0m  %-20s/\n", name);
    } else if (type == KVFS_TYPE_BIN) {
        kprintf("  \033[1;32m[BIN]\033[0m  %-20s (%u bytes)\n", name, (unsigned int)size);
    } else if (type == KVFS_TYPE_DEV) {
        kprintf("  \033[1;33m[DEV]\033[0m  %-20s\n", name);
    } else {
        kprintf("  \033[1;37m[FILE]\033[0m %-20s (%u bytes)\n", name, (unsigned int)size);
    }
}

static void execute_cli_command(const char *cmd) {
    kprintf("\n");

    while (*cmd == ' ') cmd++;
    if (*cmd == '\0') {
        kprintf("test_os> ");
        return;
    }

    if (kstrcmp(cmd, "help") == 0 || kstrcmp(cmd, "?") == 0) {
        kprintf("  [utils-in-c OS - VFS Commands]\n");
        kprintf("    • ls [caminho]  : Listar arquivos (ex: 'ls /', 'ls /bin', 'ls /etc', 'ls /dev')\n");
        kprintf("    • cat <caminho> : Exibir arquivo (ex: 'cat /etc/os-release', 'cat /dev/ata0')\n");
        kprintf("    • ata <info|0>  : Interagir com disco ATA PIO\n");
        kprintf("    • echo <texto>  : Imprimir texto\n");
        kprintf("    • mem           : Estatisticas da heap KMEM\n");
        kprintf("    • clear         : Limpar terminal\n");
        kprintf("    • exit          : Voltar ao modo GUI\n");
    } else if (kstrncmp(cmd, "ls", 2) == 0) {
        const char *target = cmd + 2;
        while (*target == ' ') target++;
        if (*target == '\0') target = "/";

        kprintf("  [Directory Listing of %s]:\n", target);
        kvfs_list_dir(target, vfs_ls_callback);
    } else if (kstrncmp(cmd, "cat ", 4) == 0) {
        const char *fname = cmd + 4;
        while (*fname == ' ') fname++;

        const kvfs_node_t *file = kvfs_open(fname);
        if (file) {
            kprintf("  [Content of %s]:\n", file->path);
            if (file->data) {
                kprintf("%s\n", (const char *)file->data);
            } else {
                kprintf("  (Directory / Empty Node)\n");
            }
        } else {
            kprintf("  Error: '%s' not found in VFS.\n", fname);
        }
    } else if (kstrncmp(cmd, "ata", 3) == 0) {
        const char *arg = cmd + 3;
        while (*arg == ' ') arg++;

        const kata_drive_info_t *d = kata_get_info();
        if (!d->drive_present) {
            kprintf("  ATA Driver: Primary Master drive offline or not attached.\n");
        } else {
            kprintf("  [ATA PIO Primary Master Drive]:\n");
            kprintf("    • Model   : %s\n", d->model);
            kprintf("    • Serial  : %s\n", d->serial);
            kprintf("    • Capacity: %u MB (%u sectors)\n", (unsigned int)d->size_mb, (unsigned int)d->total_sectors);

            if (kstrncmp(arg, "read", 4) == 0 || kstrcmp(arg, "0") == 0) {
                uint8_t sec_buf[512];
                if (kata_read_sector(0, sec_buf) == 0) {
                    kprintf("    • Sector 0 (MBR) read successfully! Signature: 0x%02X 0x%02X\n", sec_buf[510], sec_buf[511]);
                } else {
                    kprintf("    • Error reading sector 0.\n");
                }
            }
        }
    } else if (kstrncmp(cmd, "echo ", 5) == 0) {
        kprintf("  %s\n", cmd + 5);
    } else if (kstrcmp(cmd, "mem") == 0) {
        kprintf("  [KMEM Heap Diagnostics]:\n");
        kprintf("    • Total Memory : %u KB\n", (unsigned int)(kmem_get_total_bytes() / 1024));
        kprintf("    • Free Memory  : %u KB\n", (unsigned int)(kmem_get_free_bytes() / 1024));
        kprintf("    • Used Memory  : %u bytes\n", (unsigned int)kmem_get_used_bytes());
    } else if (kstrcmp(cmd, "clear") == 0) {
        kgfx_clear(&os_fb, KGFX_BLACK);
        kgfx_draw_rounded_rect(&os_fb, 10, 10, os_fb.width - 20, os_fb.height - 20, 8, KGFX_CYAN, 0);
        kprintf("[Interactive Kernel CLI Terminal - Press ESC to Return to GUI]\n\n");
    } else if (kstrcmp(cmd, "exit") == 0) {
        os_mode = 0;
        mouse_under_saved = 0;
        draw_gui_desktop();
        return;
    } else {
        kprintf("  Unknown command '%s'. Type 'help' for command list.\n", cmd);
    }
    kprintf("test_os> ");
}

void os_handle_keypress(char c) {
    if (os_mode == 1) {
        if (c == '\b') {
            if (cli_pos > 0) {
                cli_input[--cli_pos] = '\0';
                kprintf("\b");
            }
        } else if (c == '\n') {
            execute_cli_command(cli_input);
            cli_pos = 0;
            cli_input[0] = '\0';
        } else if (c && cli_pos < sizeof(cli_input) - 1) {
            cli_input[cli_pos++] = c;
            cli_input[cli_pos] = '\0';
            kprintf("%c", c);
        }
    }
}

void os_toggle_cli_mode(void) {
    os_mode = !os_mode;
    if (os_mode == 1) {
        restore_mouse_under();
        kgfx_clear(&os_fb, KGFX_BLACK);
        kgfx_draw_rounded_rect(&os_fb, 10, 10, os_fb.width - 20, os_fb.height - 20, 8, KGFX_CYAN, 0);
        kprintf("\n[Interactive Kernel CLI Terminal Active - Type 'help' or press ESC to exit]\n\n");
        kprintf("test_os> ");
    } else {
        mouse_under_saved = 0;
        draw_gui_desktop();
    }
}

void kernel_main(uint32_t magic, multiboot_info_t *mb_info) {
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC || !mb_info) return;

    uint32_t *fb_ptr = (uint32_t *)(uintptr_t)mb_info->framebuffer_addr;
    uint32_t width = mb_info->framebuffer_width ? mb_info->framebuffer_width : 800;
    uint32_t height = mb_info->framebuffer_height ? mb_info->framebuffer_height : 600;

    if (!fb_ptr) fb_ptr = (uint32_t *)0xFD000000;

    kgfx_init(&os_fb, fb_ptr, width, height);
    kset_putchar(os_putchar);

    kvfs_init();
    gdt_init();
    idt_init();
    ps2_init();
    kata_init();

    static uint8_t os_heap_pool[2 * 1024 * 1024];
    kmem_init(os_heap_pool, sizeof(os_heap_pool));

    draw_gui_desktop();

    while (1) {
        os_draw_mouse_cursor();
        __asm__ __volatile__ ("hlt");
    }
}
