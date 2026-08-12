#include "multiboot.h"
#include "gdt.h"
#include "idt.h"
#include "ps2.h"
#include "../freestanding/kmem.h"
#include "../freestanding/kfixed.h"
#include "../freestanding/kprintf.h"
#include "../freestanding/kgfx.h"

static kgfx_fb_t os_fb;

static void os_putchar(char c) {
    static int cursor_x = 30;
    static int cursor_y = 100;

    if (c == '\n') {
        cursor_x = 30;
        cursor_y += 14;
    } else {
        kgfx_draw_char(&os_fb, cursor_x, cursor_y, c, KGFX_WHITE, 0);
        cursor_x += 8;
        if (cursor_x > (int)os_fb.width - 40) {
            cursor_x = 30;
            cursor_y += 14;
        }
    }
}

static void trigger_divide_by_zero_test(void) {
    volatile int zero = 0;
    volatile int result = 100 / zero;
    (void)result;
}

void kernel_main(uint32_t magic, multiboot_info_t *mb_info) {
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC || !mb_info) return;

    uint32_t *fb_ptr = (uint32_t *)(uintptr_t)mb_info->framebuffer_addr;
    uint32_t width = mb_info->framebuffer_width ? mb_info->framebuffer_width : 800;
    uint32_t height = mb_info->framebuffer_height ? mb_info->framebuffer_height : 600;

    if (!fb_ptr) fb_ptr = (uint32_t *)0xFD000000;

    /* 1. Inicializa KGFX Framebuffer */
    kgfx_init(&os_fb, fb_ptr, width, height);
    kgfx_clear(&os_fb, KGFX_DARKGRAY);

    kset_putchar(os_putchar);

    /* 2. Inicializa GDT, IDT e PS/2 */
    gdt_init();
    idt_init();
    ps2_init();

    /* 3. Desenha Interface */
    kgfx_draw_rect(&os_fb, 10, 10, width - 20, height - 20, KGFX_CYAN, 0);
    kgfx_draw_rect(&os_fb, 12, 12, width - 24, 32, KGFX_BLUE, 1);
    kgfx_draw_string(&os_fb, 20, 22, "utils-in-c OS (GDT + IDT + PS/2 Mouse & Keyboard Active)", KGFX_WHITE, KGFX_BLUE);

    /* 4. Diagnóstico do Kernel */
    kprintf("[Kernel Core Subsystems Active]\n");
    kprintf("  • GDT & IDT Handlers: GDT Loaded | IDT Loaded | PIC Remapped\n");
    kprintf("  • PS/2 Driver       : Keyboard (IRQ 1) & Mouse (IRQ 12) Initialized\n");

    /* 5. Teste Seguro de Exceção IDT (Divisão por Zero) */
    trigger_divide_by_zero_test();

    /* Loop Principal do Kernel: Atualiza Cursor do Mouse PS/2 */
    kgfx_mouse_t *mouse = ps2_get_mouse_state();
    while (1) {
        kgfx_draw_cursor(&os_fb, mouse);
        __asm__ __volatile__ ("hlt");
    }
}
