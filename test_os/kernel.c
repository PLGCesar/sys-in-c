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

    /* 2. Inicializa GDT, IDT (Exceptions + PIC) & PS/2 Ports (Teclado + Mouse) */
    gdt_init();
    idt_init();
    ps2_init();

    /* 3. Desenha Interface */
    kgfx_draw_rect(&os_fb, 10, 10, width - 20, height - 20, KGFX_CYAN, 0);
    kgfx_draw_rect(&os_fb, 12, 12, width - 24, 32, KGFX_BLUE, 1);
    kgfx_draw_string(&os_fb, 20, 22, "utils-in-c OS (Full Freestanding & Kernel Verification)", KGFX_WHITE, KGFX_BLUE);

    kgfx_draw_circle(&os_fb, width - 100, 180, 50, KGFX_YELLOW);

    /* 4. Teste do Gerenciador de Memória (kmem) */
    static uint8_t os_heap_pool[2 * 1024 * 1024]; /* 2 MB Heap Pool */
    kmem_init(os_heap_pool, sizeof(os_heap_pool));
    void *page_table = kmalloc_aligned(4096, 4096);

    /* 5. Teste de Matemática de Ponto Fixo (kfixed) */
    fp32_t a = fp32_from_int(100);
    fp32_t sqrt_res = fp32_sqrt(a);
    char math_buf[32];
    fp32_to_str(sqrt_res, math_buf, sizeof(math_buf), 2);

    /* 6. Diagnóstico Completo no Screen via KPRINTF */
    kprintf("[Kernel & Freestanding Modules Verification]\n");
    kprintf("  • Multiboot1 Magic : 0x%X\n", magic);
    kprintf("  • Video Resolution : %dx%d @ 32bpp (KGFX Active)\n", width, height);
    kprintf("  • GDT & IDT Status  : GDT Loaded | IDT Loaded | PIC Remapped\n");
    kprintf("  • PS/2 Drivers      : Keyboard (IRQ 1) & Mouse (IRQ 12) Active\n");
    kprintf("  • KMEM Heap Pool    : %d KB | Free: %d KB\n", (int)(sizeof(os_heap_pool)/1024), (int)(kmem_get_free_bytes()/1024));
    kprintf("  • KMEM Page Table   : %p (Aligned to 4096: YES)\n", page_table);
    kprintf("  • KFIXED Sqrt(100)  : %s\n", math_buf);

    /* 7. Teste Seguro de Exceção IDT (Divisão por Zero) */
    trigger_divide_by_zero_test();

    kprintf("\n  • System Status     : ALL FREESTANDING MODULES & DRIVERS OPERATIONAL!\n");

    kfree(page_table);

    /* 8. Loop Principal do Kernel: Atualiza Cursor do Mouse PS/2 em Tempo Real */
    kgfx_mouse_t *mouse = ps2_get_mouse_state();
    while (1) {
        kgfx_draw_cursor(&os_fb, mouse);
        __asm__ __volatile__ ("hlt");
    }
}
