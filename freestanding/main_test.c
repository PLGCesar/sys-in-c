#include "kmem.h"
#include <stdio.h>

// Arena de memória estática de 1 MB (sem libc malloc)
static uint8_t my_freestanding_heap[1024 * 1024];

int main(void) {
    printf("==========================================\n");
    printf("[ Freestanding Memory Manager Test ]\n");
    printf("==========================================\n");

    // 1. Inicializa o heap de 1 MB
    kmem_init(my_freestanding_heap, sizeof(my_freestanding_heap));
    printf("  • Heap Initialized : %zu KB\n", sizeof(my_freestanding_heap) / 1024);
    printf("  • Free Memory      : %zu bytes\n", kmem_get_free_bytes());

    // 2. Teste de kmalloc (malloc)
    int *numbers = (int *)malloc(10 * sizeof(int));
    if (numbers) {
        for (int i = 0; i < 10; i++) numbers[i] = (i + 1) * 10;
        printf("  • kmalloc Allocated 10 ints. First: %d, Last: %d\n", numbers[0], numbers[9]);
    }

    char *text = (char *)malloc(64);
    if (text) {
        kmemcpy(text, "Hello Freestanding C!", 22);
        printf("  • kmalloc String    : %s\n", text);
    }

    printf("  • Used Memory      : %zu bytes\n", kmem_get_used_bytes());

    // 3. Teste de krealloc (realloc)
    numbers = (int *)realloc(numbers, 20 * sizeof(int));
    if (numbers) {
        numbers[19] = 999;
        printf("  • krealloc Expanded : New last element = %d\n", numbers[19]);
    }

    // 4. Teste de kfree (free) + Coalescência
    kfree(numbers);
    kfree(text);
    printf("  • After kfree      : Free Memory = %zu bytes\n", kmem_get_free_bytes());
    printf("==========================================\n");

    return 0;
}
