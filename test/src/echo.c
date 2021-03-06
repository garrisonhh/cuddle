#include <stdio.h>
#include <stdlib.h>

#include <cuddle/cuddle.h>

int main(int argc, char **argv) {
    // file stuff
    if (argc != 2) {
        fprintf(stderr, "please supply a file path as first argument.\n");
        exit(-1);
    }

    kdl_document_buffers_t doc_bufs = {
        .num_node_blocks = 256,
        .data_block_size = 16 * 1024,
        .num_data_blocks = 1024,

        .node_blocks = calloc(sizeof(kdl_node_t), doc_bufs.num_node_blocks),
        .data_blocks = calloc(
            doc_bufs.data_block_size, doc_bufs.num_data_blocks
        ),
    };

    kdl_document_t doc;
    kdl_document_make(&doc, &doc_bufs);

    kdl_document_load_file(&doc, argv[1]);

    kdl_document_debug(&doc);

    free(doc_bufs.node_blocks);
    free(doc_bufs.data_blocks);

    return 0;
}
