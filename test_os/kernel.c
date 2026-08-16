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

/* 0 = GUI Mode, 1 = CLI Mode */
static int os_mode = 0;

static char cli_input[128] = "";
static size_t cli_pos = 0;

static int prev_mouse_x = 400;
static int prev_mouse_y = 300;

static const char sample_readme[] = "Welcome to utils-in-c OS!\nThis file is read from RAM VFS.";
static const char sample_config[] = "OS_NAME=utils-in-c OS\nVERSION=2.5\nKERNEL=Multiboot1\nVIDEO=VBE800x600";
static const char sample_script[] = "#!/bin/sh\necho 'Running test script inside OS!'";

/* Clean character renderer for KGFX Framebuffer */
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

void os_draw_mouse_cursor(void) {
    if (os_mode != 0) return;

    kgfx_mouse_t *mouse = ps2_get_mouse_state();

    // Apaga a posição anterior do mouse
    kgfx_draw_rect(&os_fb, prev_mouse_x, prev_mouse_y, 16, 16, KGFX_DARKGRAY, 1);

    // Desenha o cursor novo
    kgfx_draw_cursor(&os_fb, mouse);

    prev_mouse_x = mouse->x;
    prev_mouse_y = mouse->y;
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
        kprintf("[Interactive Kernel CLI Terminal - Type 'help' or 'exit']\n\n");
    } else if (kstrcmp(cmd, "exit") == 0) {
        os_mode = 0;
        kgfx_clear(&os_fb, KGFX_DARKGRAY);
        kprintf("Returned to GUI Mode.\n");
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
        kgfx_clear(&os_fb, KGFX_BLACK);
        kgfx_draw_rounded_rect(&os_fb, 10, 10, os_fb.width - 20, os_fb.height - 20, 8, KGFX_CYAN, 0);
        kprintf("\n[Interactive Kernel CLI Terminal Active - Type 'help' or 'exit']\n\n");
        kprintf("test_os> ");
    } else {
        kgfx_clear(&os_fb, KGFX_DARKGRAY);
        kprintf("Returned to GUI Mode.\n");
    }
}

void kernel_main(uint32_t magic, multiboot_info_t *mb_info) {
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC || !mb_info) return;

    uint32_t *fb_ptr = (uint32_t *)(uintptr_t)mb_info->framebuffer_addr;
    uint32_t width = mb_info->framebuffer_width ? mb_info->framebuffer_width : 800;
    uint32_t height = mb_info->framebuffer_height ? mb_info->framebuffer_height : 600;

    if (!fb_ptr) fb_ptr = (uint32_t *)0xFD000000;

    // 1. Inicializa Framebuffer, VFS e Hook de Printf
    kgfx_init(&os_fb, fb_ptr, width, height);
    kgfx_clear(&os_fb, KGFX_DARKGRAY);
    kset_putchar(os_putchar);

    kvfs_init();
    kvfs_create_file("readme.txt", sample_readme, sizeof(sample_readme) - 1, 0644);
    kvfs_create_file("kernel.config", sample_config, sizeof(sample_config) - 1, 0644);
    kvfs_create_file("hello.sh", sample_script, sizeof(sample_script) - 1, 0755);

    // 2. Inicializa GDT, IDT & PS/2 Drivers (Teclado IRQ 1 e Mouse IRQ 12)
    gdt_init();
    idt_init();
    ps2_init();

    // 3. Renderiza Interface Gráfica Avançada com KGFX
    kgfx_draw_rounded_rect(&os_fb, 10, 10, width - 20, height - 20, 10, KGFX_CYAN, 0);
    kgfx_draw_rect(&os_fb, 12, 12, width - 24, 36, KGFX_BLUE, 1);
    kgfx_draw_string_scaled(&os_fb, 20, 20, "utils-in-c OS v2.5", KGFX_WHITE, KGFX_BLUE, 2);

    // Janela com Alpha Blending (Translúcida)
    kgfx_draw_rect_alpha(&os_fb, 20, 60, width - 40, 45, kgfx_argb(180, 24, 24, 37));
    kgfx_draw_string(&os_fb, 30, 75, "Pressione [F1] ou [TAB] a qualquer momento para abrir o Terminal Interativo CLI!", KGFX_YELLOW, 0);

    // Primitivas preenchidas de demonstração
    kgfx_draw_filled_circle(&os_fb, width - 80, 180, 35, KGFX_PURPLE);
    kgfx_draw_triangle(&os_fb, width - 150, 220, width - 110, 150, width - 70, 220, KGFX_GREEN, 1);

    // 4. Diagnóstico Freestanding no Terminal
    static uint8_t os_heap_pool[2 * 1024 * 1024];
    kmem_init(os_heap_pool, sizeof(os_heap_pool));
    void *page_table = kmalloc_aligned(4096, 4096);

    fp32_t a = fp32_from_int(144);
    fp32_t sqrt_res = fp32_sqrt(a);
    char math_buf[32];
    fp32_to_str(sqrt_res, math_buf, sizeof(math_buf), 2);

    kprintf("[Kernel Core Subsystems Online]\n");
    kprintf("  • GDT & IDT Status  : GDT Loaded | IDT Remapped (PIC IRQ 1 & 12 Enabled)\n");
    kprintf("  • PS/2 Hardware     : Keyboard & Mouse Drivers Active & Responsive\n");
    kprintf("  • KVFS Filesystem   : 3 RAM Files Mounted (ls / cat ready)\n");
    kprintf("  • KMEM Heap Pool    : %d KB | Free: %d KB\n", (int)(sizeof(os_heap_pool)/1024), (int)(kmem_get_free_bytes()/1024));
    kprintf("  • KFIXED Sqrt(144)  : %s\n\n", math_buf);

    kfree(page_table);

    // 5. Loop Principal: Mantém Interrupções ativas e renderiza o Cursor do Mouse
    while (1) {
        os_draw_mouse_cursor();
        __asm__ __volatile__ ("hlt");
    }
}
