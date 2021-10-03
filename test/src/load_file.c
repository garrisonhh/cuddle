#include <stdio.h>
#include <stdlib.h>

#include <cuddle/cuddle.h>

int main(int argc, char **argv) {
    // file stuff
    if (argc != 2) {
        fprintf(stderr, "please supply a file path as first argument.\n");
        exit(-1);
    }

    FILE *file = fopen(argv[1], "r");

    if (!file) {
        fprintf(stderr, "can't open file :(\n");
        exit(-1);
    }

    // tokenizing stuff
    kdl_tokenizer_t tzr;
    kdl_token_t token;

    const size_t megabyte = 1024 * 1024;
    const size_t len_buf = megabyte / sizeof(kdl_u8ch_t);
    kdl_u8ch_t tzr_buf[len_buf];
    char tok_buf[megabyte];

    kdl_tokenizer_make(&tzr, tzr_buf, sizeof(tzr_buf));
    kdl_token_make(&token, tok_buf);

    // read and feed
    char read_buf[megabyte], ser_buf[megabyte];
    int level = 0;

    while (fread(read_buf, sizeof(read_buf[0]), len_buf, file)) {
        kdl_tok_feed(&tzr, read_buf, megabyte);

        while (kdl_tok_next(&tzr, &token)) {
#if 1
            if (token.node) {
                putchar('\n');

                for (int i = 0; i < level; ++i)
                    printf("  ");
            }

            switch (token.type) {
            case KDL_TOK_NULL:
                printf("null");

                break;
            case KDL_TOK_BOOL:
                printf(token.boolean ? "true" : "false");

                break;
            case KDL_TOK_NUMBER:
                kdl_serialize_number(ser_buf, megabyte, token.number);
                printf(ser_buf);

                break;
            case KDL_TOK_IDENTIFIER:
                printf("%s", token.string);

                break;
            case KDL_TOK_STRING:
                kdl_serialize_string(ser_buf, megabyte, token.string);
                printf("\"%s\"", ser_buf);

                break;
            case KDL_TOK_CHILD_BEGIN:
                putchar('{');
                ++level;

                break;
            case KDL_TOK_CHILD_END:
                --level;

                printf("\n");

                for (int i = 0; i < level; ++i)
                    printf("  ");

                putchar('}');

                break;
            }

            putchar(token.property ? '=' : ' ');
#endif
        }
    }

    fclose(file);

    return 0;
}
