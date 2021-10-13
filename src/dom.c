#include <stdio.h>

#include <cuddle/meta.h>
#include <cuddle/cuddle.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

/*
 * i'm discarding hrefs all over the place in this file, TODO actually keep them
 * around with pointers and stuff, or just attach them to the pointer with fat
 * pointers
 */

void kdl_document_make(kdl_document_t *doc, kdl_document_buffers_t *bufs) {
    *doc = (kdl_document_t){0};

    kdl_htable_make(
        &doc->node_table,
        bufs->node_blocks,
        sizeof(kdl_node_t),
        bufs->num_node_blocks
    );

    kdl_htable_make(
        &doc->data_table,
        bufs->data_blocks,
        bufs->data_block_size,
        bufs->num_data_blocks
    );

    doc->nodes = kdl_htable_alloc(
        &doc->data_table, &doc->nodes_ref, sizeof(*doc->nodes) * 256
    );
}

static char *dup_string(
    kdl_document_t *doc, char *string, size_t length, kdl_href_t *out_ref
) {
    char *ptr = kdl_htable_alloc(&doc->data_table, out_ref, length + 1);
    char *trav = ptr;

    while ((*trav++ = *string++))
        ;

    return ptr;
}

static void extract_token_value(
    kdl_document_t *doc, kdl_value_t *val, kdl_token_t *token
) {
    switch (token->type) {
    case KDL_TOK_STRING:
        val->type = KDL_STRING;
        val->data.string = dup_string(
            doc, token->string, token->str_len, &val->str_ref
        );

        break;
    case KDL_TOK_NUMBER:
        val->type = KDL_NUMBER;
        val->data.number = token->number;

        break;
    case KDL_TOK_BOOL:
        val->type = KDL_BOOL;
        val->data.boolean = token->boolean;

        break;
    case KDL_TOK_NULL:
        val->type = KDL_NULL;

        break;
    default:
        KDL_ERROR(
            "token type %s is not a value!\n",
            KDL_TOKEN_TYPES[token->type]
        );
    }
}

// TODO add some memory knobs to turn
static kdl_node_t *new_node(kdl_document_t *doc, kdl_token_t *token) {
    kdl_href_t self_ref;

    kdl_node_t *node = kdl_htable_alloc(
        &doc->node_table, &self_ref, sizeof(*node)
    );

    node->self_ref = self_ref;

    node->args = kdl_htable_alloc(
        &doc->data_table, &node->args_ref, sizeof(*node->args) * 256
    );
    node->props = kdl_htable_alloc(
        &doc->data_table, &node->props_ref, sizeof(*node->props) * 256
    );
    node->children = kdl_htable_alloc(
        &doc->data_table, &node->children_ref, sizeof(*node->children) * 256
    );

    node->id = dup_string(doc, token->string, token->str_len, &node->id_ref);
    node->id_is_identifier = token->type == KDL_TOK_IDENTIFIER;

    return node;
}

void kdl_document_load_file(kdl_document_t *doc, const char *filename) {
    FILE *fp = fopen(filename, "r");

    if (!fp)
        KDL_ERROR("couldn't load file: \"%s\"\n", filename);

    // memory
    char read_buf[4096], tok_buf[4096];
    kdl_u8ch_t tzr_buf[4096];

    // tokenizing init
    kdl_tokenizer_t tzr;
    kdl_token_t token;

    kdl_tokenizer_make(&tzr, tzr_buf, ARRAY_SIZE(tzr_buf));
    kdl_token_make(&token, tok_buf);

    // document parsing state
    kdl_node_t *node_stack[256]; // parent nodes
    size_t node_stack_size = 0;
    kdl_node_t *cur_node = NULL;

    bool await_prop = false;

    while (fread(read_buf, sizeof(read_buf[0]), ARRAY_SIZE(read_buf), fp)) {
        kdl_tok_feed(&tzr, read_buf, ARRAY_SIZE(read_buf));

        while (kdl_tok_next(&tzr, &token)) {
            if (token.node) {
                // create new node
                cur_node = new_node(doc, &token);

                // save node to tree
                if (node_stack_size) {
                    kdl_node_t *parent = node_stack[node_stack_size - 1];

                    parent->children[parent->num_children++] = cur_node;
                } else {
                    doc->nodes[doc->num_nodes++] = cur_node;
                }
            } else if (token.property) {
                // property id
                kdl_prop_t *prop = &cur_node->props[cur_node->num_props];

                prop->id = dup_string(
                    doc, token.string, token.str_len, &prop->id_ref
                );

                prop->id_is_identifier = token.type == KDL_TOK_IDENTIFIER;
                await_prop = true;
            } else {
                switch (token.type) {
                case KDL_TOK_CHILD_BEGIN:
                    node_stack[node_stack_size++] = cur_node;

                    break;
                case KDL_TOK_CHILD_END:
                    --node_stack_size;

                    break;
                default:;
                    kdl_value_t *value;

                    if (await_prop) {
                        await_prop = false;
                        value = &cur_node->props[cur_node->num_props++].value;
                    } else {
                        value = &cur_node->args[cur_node->num_args++];
                    }

                    extract_token_value(doc, value, &token);

                    break;
                }
            }
        }
    }

    fclose(fp);
}

static inline void print_level(int level) {
    printf("%*s", level * 4, "");
}

static void serialize_value(char *buf, size_t buf_size, kdl_value_t *val) {
    switch (val->type) {
    case KDL_STRING:
        kdl_serialize_string(buf, buf_size, val->data.string);

        break;
    case KDL_NUMBER:
        kdl_serialize_number(buf, buf_size, val->data.number);

        break;
    case KDL_BOOL:
        kdl_serialize_bool(buf, buf_size, val->data.boolean);

        break;
    case KDL_NULL:
        kdl_serialize_null(buf, buf_size);

        break;
    }
}

static void print_node(kdl_node_t *node, int level) {
    char buf[256];

    print_level(level);

    if (node->id_is_identifier) {
        printf("%s ", node->id);
    } else {
        kdl_serialize_string(buf, ARRAY_SIZE(buf), node->id);
        printf("%s ", buf);
    }

    for (size_t i = 0; i < node->num_args; ++i) {
        serialize_value(buf, ARRAY_SIZE(buf), &node->args[i]);
        printf("%s ", buf);
    }

    for (size_t i = 0; i < node->num_props; ++i) {
        kdl_prop_t *prop = &node->props[i];

        if (prop->id_is_identifier) {
            printf("%s=", prop->id);
        } else {
            kdl_serialize_string(buf, ARRAY_SIZE(buf), prop->id);
            printf("%s=", buf);
        }

        serialize_value(buf, ARRAY_SIZE(buf), &prop->value);
        printf("%s ", buf);
    }

    if (node->num_children) {
        printf("{\n");

        for (size_t i = 0; i < node->num_children; ++i)
            print_node(node->children[i], level + 1);

        print_level(level);
        putchar('}');
    }

    putchar('\n');
}

void kdl_document_debug(kdl_document_t *doc) {
    for (size_t i = 0; i < doc->num_nodes; ++i)
        print_node(doc->nodes[i], 0);
}
