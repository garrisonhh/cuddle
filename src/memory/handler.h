#ifndef KDL_MEMORY_HANDLER_H
#define KDL_MEMORY_HANDLER_H

#include <stdbool.h>

#define HTABLE_SIZE ((unsigned short)-1)

/*
 * a combined handle table and allocator
 */
typedef struct htable {
    // memory is separated into equally sized blocks
    char *blocks;
    size_t block_size, num_blocks;

    // counts represents the generation of each block if it is allocated
    unsigned short counts[HTABLE_SIZE];

    // reusable is a stack of freed blocks that can be reused
    // if num_reusable is 0, just allocate max_used (the next block up)
    unsigned short reusable[HTABLE_SIZE];
    size_t num_reusable, max_used;
} htable_t;

typedef struct href {
    unsigned short index, count;
} href_t;

void htable_make(
    htable_t *, void *blocks, size_t block_size, size_t num_blocks
);

void *htable_alloc(htable_t *, href_t *);

static inline void htable_free(htable_t *table, href_t *ref) {
    ++table->counts[ref->index];
    table->reusable[table->num_reusable++] = ref->index;
}

static inline void *htable_get(htable_t *table, href_t *ref) {
    return (table->counts[ref->index] == ref->count)
        ? table->blocks + ref->index * table->block_size
        : NULL;
}

#endif
