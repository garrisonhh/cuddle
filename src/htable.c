#include <cuddle/meta.h>
#include <cuddle/htable.h>

void kdl_htable_make(
    kdl_htable_t *table, void *blocks, size_t block_size, size_t num_blocks
) {
    *table = (kdl_htable_t){
        .blocks = (char *)blocks,
        .block_size = block_size,
        .num_blocks = num_blocks
    };
}

void *kdl_htable_alloc(kdl_htable_t *table, kdl_href_t *ref, size_t size) {
    // assert that this size is 
    // TODO remove this with a compilation flag? 
    if (size > table->block_size) {
        KDL_ERROR(
            "tried to alloc %zu bytes in a htable with blocks of size %zu.\n",
            size, table->block_size
        );
    }

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

