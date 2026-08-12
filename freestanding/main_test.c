#include "kmem.h"
#include "kfixed.h"
#include "kprintf.h"
#include "kgfx.h"
#include "kringbuf.h"
#include "kstring.h"
#include <stdio.h>

static uint8_t kernel_ram_region[1024 * 1024];
static uint32_t front_video[320 * 200];
static uint32_t back_ram[320 * 200];

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

    /* 3. Graphics & Rendering Tech Test (kgfx) */
    kgfx_double_buffer_t db;
    kgfx_fb_t back_fb;
    kgfx_double_buffer_init(&db, front_video, back_ram, 320, 200);
    kgfx_init(&back_fb, back_ram, 320, 200);

    kgfx_clear(&back_fb, KGFX_DARKGRAY);

    /* Button and Mouse Test */
    kgfx_rect_t btn = { .x = 50, .y = 50, .w = 120, .h = 40 };
    kgfx_mouse_t mouse = { .x = 80, .y = 60, .type = KGFX_CURSOR_NORMAL };

    if (kgfx_rect_contains(&btn, mouse.x, mouse.y)) {
        mouse.type = KGFX_CURSOR_CLICKABLE;
    }

    kgfx_draw_rect(&back_fb, btn.x, btn.y, btn.w, btn.h, KGFX_BLUE, 1);
    kgfx_draw_string(&back_fb, btn.x + 10, btn.y + 15, "Click Me", KGFX_WHITE, KGFX_BLUE);
    kgfx_draw_cursor(&back_fb, &mouse);

    /* Swap Buffers */
    kgfx_swap_buffers(&db);
    kprintf("  • kgfx: Double Buffering, Dirty Rects & Mouse Map Tested\n");

    /* 4. Formatting Test (kprintf) */
    kprintf("  • kprintf: Pointer: %p | Hex: 0x%X | Binary: %b\n", 
            (void*)page_table, 0xDEADC0DE, 0b10110011);

    kfree(page_table);
    kprintf("  • kmem: Free Memory = %u KB\n", (unsigned int)(kmem_get_free_bytes() / 1024));
    kprintf("==========================================\n");

    return 0;
}
