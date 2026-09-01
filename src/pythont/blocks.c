#include "pythont.h"

void handle_dedent(int new_indent, int is_else_or_elif) {
    while (block_top > 0 && new_indent < block_indent[block_top - 1]) {
        block_type_t popped = block_type[--block_top];
        if (popped == BLOCK_FUNC) { emit("}\n"); inside_function = 0; }
        else if (!is_else_or_elif || block_top > 0) emit("    }\n");
    }
}
