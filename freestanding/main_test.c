#include "kmem.h"
#include "kfixed.h"
#include "kprintf.h"
#include "kgfx.h"
#include <stdio.h>

static uint8_t kernel_ram_region[1024 * 1024];
static uint32_t video_buffer[320 * 200]; /* 320x200 pixel framebuffer */

static void test_putchar(char c) {
    putchar(c);
}

int main(void) {
    kset_putchar(test_putchar);

    kprintf("==========================================\n");
    kprintf("[ OS Kernel Freestanding Subsystem Test ]\n");
    kprintf("==========================================\n");

    /* 1. Memory Test (kmem) */
    kmem_init(kernel_ram_region, sizeof(kernel_ram_region));
    uint8_t *page_table = (uint8_t *)kmalloc_aligned(4096, 4096);
    kprintf("  • kmem: Initialized 1 MB Heap | Page Table: %p\n", (void*)page_table);

    /* 2. Math Test (kfixed) */
    fp32_t a = fp32_from_int(10);
    fp32_t b = fp32_from_int(3);
    fp32_t div_res = fp32_div(a, b);
    char math_buf[32];
    fp32_to_str(div_res, math_buf, sizeof(math_buf), 4);
    kprintf("  • kfixed: Fixed-point 10 / 3 = %s\n", math_buf);

    /* 3. Graphics Test (kgfx) */
    kgfx_fb_t fb;
    kgfx_init(&fb, video_buffer, 320, 200);
    kgfx_clear(&fb, KGFX_DARKGRAY);
    kgfx_draw_rect(&fb, 5, 5, 310, 190, KGFX_CYAN, 0);
    kgfx_draw_circle(&fb, 160, 100, 30, KGFX_YELLOW);
    kgfx_draw_string(&fb, 20, 20, "Bare-Metal Kernel OS", KGFX_WHITE, 0);
    kprintf("  • kgfx: Rendered 320x200 Framebuffer (Shapes & Text)\n");

    /* 4. Formatting Test (kprintf) */
    kprintf("  • kprintf: Pointer: %p | Hex: 0x%X | Binary: %b\n", 
            (void*)page_table, 0xDEADC0DE, 0b10110011);

    kfree(page_table);
    kprintf("  • kmem: Free Memory = %u KB\n", (unsigned int)(kmem_get_free_bytes() / 1024));
    kprintf("==========================================\n");

    return 0;
}
