#ifndef KDL_DOM_H
#define KDL_DOM_H

#include <stddef.h>
#include <stdbool.h>

#include <cuddle/htable.h>

// a tagged union for arguments and property values
typedef struct kdl_value {
    enum kdl_value_type {
        KDL_STRING,
        KDL_NUMBER,
        KDL_BOOL,
        KDL_NULL
    } type;

    union kdl_value_data {
        char *string;
        double number;
        bool boolean;
    } data;

    kdl_href_t str_ref;
} kdl_value_t;

// a key/value pair for properties
typedef struct kdl_prop {
    char *id;
    kdl_href_t id_ref;
    bool id_is_identifier;

    kdl_value_t value;
} kdl_prop_t;

typedef struct kdl_node {
    char *id;
    kdl_href_t id_ref, self_ref;
    bool id_is_identifier;

    // these arrays are alloc'd in a kdl_document data_table
    kdl_value_t *args;
    kdl_prop_t *props;
    struct kdl_node **children;
    kdl_href_t args_ref, props_ref, children_ref;
    size_t num_args, num_props, num_children;
} kdl_node_t;

typedef struct kdl_document {
    kdl_htable_t node_table, data_table;

    // allocated in data_table
    kdl_node_t **nodes;
    kdl_href_t nodes_ref;
    size_t num_nodes;
} kdl_document_t;

// fill this in and pass to make. you are responsible for freeing the memory.
typedef struct kdl_document_buffers {
    size_t num_node_blocks; // node block size should be sizeof(kdl_node_t)
    size_t num_data_blocks, data_block_size;

    void *node_blocks, *data_blocks;
} kdl_document_buffers_t;

void kdl_document_make(kdl_document_t *, kdl_document_buffers_t *);
void kdl_document_load_file(kdl_document_t *, const char *filename);

void kdl_document_debug(kdl_document_t *);

#endif

