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

static kgfx_fb_t os_fb;
static int os_mode = 0; /* 0 = GUI Mode, 1 = CLI Mode */

static char cli_input[128] = "";
static size_t cli_pos = 0;

/* Buffer de restauração de pixels embaixo do mouse (16x16) */
#define MOUSE_BUF_SZ 16
static uint32_t mouse_under_buf[MOUSE_BUF_SZ * MOUSE_BUF_SZ];
static int mouse_under_saved = 0;
static int under_x = 0;
static int under_y = 0;
static int under_w = 0;
static int under_h = 0;

static int prev_mouse_x = 400;
static int prev_mouse_y = 300;

static const char sample_readme[] = "Welcome to utils-in-c OS!\nThis file is read from RAM VFS.";
static const char sample_config[] = "OS_NAME=utils-in-c OS\nVERSION=2.5\nKERNEL=Multiboot1\nVIDEO=VBE800x600";
static const char sample_script[] = "#!/bin/sh\necho 'Running test script inside OS!'";

static void save_mouse_under(int x, int y, int w, int h) {
    under_x = x;
    under_y = y;
    under_w = w;
    under_h = h;

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

    // 1. Restaura os pixels originais onde o mouse estava
    restore_mouse_under();

    // 2. Salva os novos pixels que estão embaixo do mouse
    save_mouse_under(mouse->x, mouse->y, MOUSE_BUF_SZ, MOUSE_BUF_SZ);

    // 3. Desenha o cursor por cima sem apagar nada
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

    // Moldura externa com cantos arredondados
    kgfx_draw_rounded_rect(&os_fb, 10, 10, os_fb.width - 20, os_fb.height - 20, 10, KGFX_CYAN, 0);

    // Barra de Título
    kgfx_draw_rect(&os_fb, 12, 12, os_fb.width - 24, 36, KGFX_BLUE, 1);
    kgfx_draw_string_scaled(&os_fb, 20, 20, "utils-in-c OS v2.5", KGFX_WHITE, KGFX_BLUE, 2);

    // Janela com Alpha Blending (Translúcida com fundo escuro)
    kgfx_draw_rect_alpha(&os_fb, 20, 60, os_fb.width - 40, 45, kgfx_argb(180, 24, 24, 37));
    kgfx_draw_string(&os_fb, 30, 75, "Pressione [ESC], [F1] ou [TAB] para abrir o Terminal Interativo CLI!", KGFX_YELLOW, 0);

    // Formas Geométricas Preenchidas
    kgfx_draw_filled_circle(&os_fb, os_fb.width - 80, 180, 35, KGFX_PURPLE);
    kgfx_draw_triangle(&os_fb, os_fb.width - 150, 220, os_fb.width - 110, 150, os_fb.width - 70, 220, KGFX_GREEN, 1);

    // Textos informativos no desktop
    kgfx_draw_string(&os_fb, 30, 130, "[Kernel Core Subsystems Online]", KGFX_WHITE, 0);
    kgfx_draw_string(&os_fb, 30, 150, "  * GDT & IDT Status  : GDT Loaded | IDT Remapped (IRQ 1 & 12 Active)", KGFX_CYAN, 0);
    kgfx_draw_string(&os_fb, 30, 170, "  * PS/2 Hardware     : Keyboard & Mouse Active with Save/Restore Buffer", KGFX_CYAN, 0);
    kgfx_draw_string(&os_fb, 30, 190, "  * KVFS Filesystem   : 3 RAM Files Mounted (ls / cat ready in CLI)", KGFX_CYAN, 0);
    kgfx_draw_string(&os_fb, 30, 210, "  * Video System      : VBE 32-bit ARGB + Alpha Blending Active", KGFX_CYAN, 0);
}

static void vfs_ls_print_cb(const char *name, size_t size, uint16_t mode) {
    (void)mode;
    kprintf("  • %-20s : %u bytes\n", name, (unsigned int)size);
}

static void execute_cli_command(const char *cmd) {
    kprintf("\n");
    if (kstrcmp(cmd, "help") == 0 || kstrcmp(cmd, "?") == 0) {
        kprintf("  [utils-in-c OS CLI Help]\n");
        kprintf("    • ls / dir      : List VFS files (kls)\n");
        kprintf("    • cat <file>    : Read VFS file contents\n");
        kprintf("    • mem           : Display KMEM heap stats\n");
        kprintf("    • clear         : Clear CLI terminal screen\n");
        kprintf("    • exit          : Switch back to GUI mode\n");
    } else if (kstrcmp(cmd, "ls") == 0 || kstrcmp(cmd, "dir") == 0) {
        kprintf("  [VFS File Directory Listing (kls)]:\n");
        kvfs_list(vfs_ls_print_cb);
    } else if (kstrncmp(cmd, "cat ", 4) == 0) {
        const char *fname = cmd + 4;
        const kvfs_file_t *file = kvfs_open(fname);
        if (file) {
            kprintf("  [Contents of %s]:\n", fname);
            kprintf("    %s\n", (const char *)file->data);
        } else {
            kprintf("  Error: File '%s' not found in VFS.\n", fname);
        }
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
    } else if (kstrlen(cmd) > 0) {
        kprintf("  Unknown command '%s'. Type 'help' for available commands.\n", cmd);
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
    kvfs_create_file("readme.txt", sample_readme, sizeof(sample_readme) - 1, 0644);
    kvfs_create_file("kernel.config", sample_config, sizeof(sample_config) - 1, 0644);
    kvfs_create_file("hello.sh", sample_script, sizeof(sample_script) - 1, 0755);

    gdt_init();
    idt_init();
    ps2_init();

    // Inicializa a Heap
    static uint8_t os_heap_pool[2 * 1024 * 1024];
    kmem_init(os_heap_pool, sizeof(os_heap_pool));

    // Desenha o Desktop Completo
    draw_gui_desktop();

    // Loop Principal
    while (1) {
        os_draw_mouse_cursor();
        __asm__ __volatile__ ("hlt");
    }
}
