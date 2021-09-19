#ifndef KDL_TOKENS_H
#define KDL_TOKENS_H

#include <stddef.h>
#include <stdbool.h>
#include <wchar.h>

#ifndef KDL_TOKEN_BUFFER
#define KDL_TOKEN_BUFFER (512)
#endif

typedef enum kdl_token_type {
    KDL_TOK_NONE, // unused intentionally for debugging

    // node stuff
    KDL_TOK_NODE,
    KDL_TOK_PROP,
    KDL_TOK_ARG,
    KDL_TOK_CHILD_BEGIN,
    KDL_TOK_CHILD_END,

    // values
    KDL_TOK_STRING,
    KDL_TOK_RAW_STRING,
    KDL_TOK_NUMBER,
    KDL_TOK_TRUE,
    KDL_TOK_FALSE,
    KDL_TOK_NULL
} kdl_token_type_e;

typedef enum kdl_token_sequence_state {
    KDL_SEQ_WHITESPACE,
    KDL_SEQ_NEWLINE,
    KDL_SEQ_EOF,

    KDL_SEQ_C_COMM,
    KDL_SEQ_CPP_COMM,
    KDL_SEQ_SD_COMM,

    KDL_SEQ_CHARACTER,

    KDL_SEQ_RAW_STRING,
    KDL_SEQ_STRING
} kdl_tokenizer_state_e;

typedef struct kdl_tokenizer {
    // tokens are built character by character using this buffer
    wchar_t token[KDL_TOKEN_BUFFER];
    size_t len_token;

    // tokenizer state
    kdl_tokenizer_state_e state;
    unsigned token_break: 1; // whenever a token is completed
    unsigned check_comm: 1; // when a '/' character is found
    unsigned str_escape: 1; // when a '\' is found in a string 
    unsigned save_token: 1; // when state saves tokens
    unsigned reset_index: 1; // reset index on next process_char()
} kdl_tokenizer_t;

// just zero-inits the tokenizer
void kdl_tokenizer_make(kdl_tokenizer_t *);
void kdl_tokenizer_feed(kdl_tokenizer_t *, wchar_t *data, size_t length);

#endif
