#ifndef KDL_HTABLE_H
#define KDL_HTABLE_H

#include <stdbool.h>

#ifndef KDL_HTABLE_SIZE
#define KDL_HTABLE_SIZE ((unsigned short)4096)
#endif

/*
 * a combined handle table and allocator
 */
typedef struct kdl_htable {
    // memory is separated into equally sized blocks
    char *blocks;
    size_t block_size, num_blocks;

    // counts represents the generation of each block if it is allocated
    unsigned short counts[KDL_HTABLE_SIZE];

    // reusable is a stack of freed blocks that can be reused
    // if num_reusable is 0, just allocate max_used (the next block up)
    unsigned short reusable[KDL_HTABLE_SIZE];
    size_t num_reusable, max_used;
} kdl_htable_t;

typedef struct kdl_href {
    unsigned short index, count;
} kdl_href_t;

void kdl_htable_make(
    kdl_htable_t *, void *blocks, size_t block_size, size_t num_blocks
);

void *kdl_htable_alloc(kdl_htable_t *, kdl_href_t *, size_t size);

static inline void kdl_htable_free(kdl_htable_t *table, kdl_href_t *ref) {
    ++table->counts[ref->index];
    table->reusable[table->num_reusable++] = ref->index;
}

static inline void *kdl_htable_get(kdl_htable_t *table, kdl_href_t *ref) {
    return (table->counts[ref->index] == ref->count)
        ? table->blocks + ref->index * table->block_size
        : NULL;
}

#endif
