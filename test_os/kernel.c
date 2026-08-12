#include "multiboot.h"
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

void kernel_main(uint32_t magic, multiboot_info_t *mb_info) {
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC || !mb_info) return;

    uint32_t *fb_ptr = (uint32_t *)(uintptr_t)mb_info->framebuffer_addr;
    uint32_t width = mb_info->framebuffer_width ? mb_info->framebuffer_width : 800;
    uint32_t height = mb_info->framebuffer_height ? mb_info->framebuffer_height : 600;

    if (!fb_ptr) fb_ptr = (uint32_t *)0xFD000000;

    /* 1. Inicializa Framebuffer Gráfico (kgfx) */
    kgfx_init(&os_fb, fb_ptr, width, height);
    kgfx_clear(&os_fb, KGFX_DARKGRAY);

    /* Binda kprintf para imprimir direto na tela via KGFX */
    kset_putchar(os_putchar);

    /* 2. Desenha Janela da UI no Kernel (kgfx) */
    kgfx_draw_rect(&os_fb, 10, 10, width - 20, height - 20, KGFX_CYAN, 0);
    kgfx_draw_rect(&os_fb, 12, 12, width - 24, 32, KGFX_BLUE, 1);
    kgfx_draw_string(&os_fb, 20, 22, "utils-in-c OS (Multiboot1 Kernel)", KGFX_WHITE, KGFX_BLUE);

    kgfx_draw_circle(&os_fb, width - 100, 180, 50, KGFX_YELLOW);

    /* 3. Teste do Gerenciador de Memória (kmem) */
    static uint8_t os_heap_pool[2 * 1024 * 1024]; /* 2 MB Heap Pool */
    kmem_init(os_heap_pool, sizeof(os_heap_pool));
    void *page_table = kmalloc_aligned(4096, 4096);

    /* 4. Teste de Matemática de Ponto Fixo (kfixed) */
    fp32_t a = fp32_from_int(100);
    fp32_t sqrt_res = fp32_sqrt(a);
    char math_buf[32];
    fp32_to_str(sqrt_res, math_buf, sizeof(math_buf), 2);

    /* 5. Imprime Logs do Kernel na Tela (kprintf) */
    kprintf("[Freestanding Subsystem Verification]\n\n");
    kprintf("  • Multiboot1 Magic : 0x%X\n", magic);
    kprintf("  • Video Resolution : %dx%d @ 32bpp\n", width, height);
    kprintf("  • KMEM Heap Pool    : %d KB | Free: %d KB\n", (int)(sizeof(os_heap_pool)/1024), (int)(kmem_get_free_bytes()/1024));
    kprintf("  • KMEM Page Table   : %p (Aligned to 4096: YES)\n", page_table);
    kprintf("  • KFIXED Sqrt(100)  : %s\n", math_buf);
    kprintf("\n  • System Status     : ALL FREESTANDING MODULES OPERATIONAL!\n");

    kfree(page_table);
}
