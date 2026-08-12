#include "kmem.h"
#include "kfixed.h"
#include "kprintf.h"
#include <stdio.h>

static uint8_t kernel_ram_region[1024 * 1024];

/* Standard output wrapper hook for testing */
static void test_putchar(char c) {
    putchar(c);
}

int main(void) {
    /* Bind kprintf output hook to standard test putchar */
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

    /* 3. Formatting Test (kprintf) */
    kprintf("  • kprintf: Pointer: %p | Hex: 0x%X | Binary: %b\n", 
            (void*)page_table, 0xDEADC0DE, 0b10110011);

    kfree(page_table);
    kprintf("  • kmem: Free Memory = %u KB\n", (unsigned int)(kmem_get_free_bytes() / 1024));
    kprintf("==========================================\n");

    return 0;
}
