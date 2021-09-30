#include <cuddle/meta.h>
#include "handler.h"

void htable_make(
    htable_t *table, void *blocks, size_t block_size, size_t num_blocks
) {
    *table = (htable_t){
        .blocks = (char *)blocks,
        .block_size = block_size,
        .num_blocks = num_blocks
    };
}

void *htable_alloc(htable_t *table, href_t *ref) {
    if (table->num_reusable) {
        // reuse old blocks
        ref->index = table->reusable[--table->num_reusable];
    } else {
        // use a new block
        if (table->max_used == table->num_blocks)
            return NULL;

        ref->index = table->max_used++;
    }

    ref->count = ++table->counts[ref->index];

    return table->blocks + ref->index * table->block_size;
}

