#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <locale.h>

#include <cuddle/cuddle.h>

// since I can't get fread to work
size_t wchar_fread(wchar_t *buf, size_t len_buf, FILE *file) {
    size_t i;

    for (i = 0; i < len_buf && (buf[i] = fgetwc(file)) != WEOF; ++i)
        ;

    return i;
}

int main() {
    // file stuff
    setlocale(LC_ALL, "en_US.UTF-8");

    FILE *file = fopen("files/website.kdl", "r");

    if (!file) {
        printf("can't open file :(\n");
        exit(-1);
    }

    fwide(file, 1);

    // tokenizing stuff
    kdl_tokenizer_t tzr;
    kdl_token_t token;

    const size_t megabyte = 1024 * 1024;

    wchar_t *tzr_buf = calloc(1, megabyte);
    wchar_t *tok_buf = calloc(1, megabyte);

    kdl_tokenizer_make(&tzr, tzr_buf);
    kdl_token_make(&token, tok_buf);

    // read file in, small
    const size_t len_buf = 1 << 16;
    wchar_t buf[megabyte / sizeof(wchar_t)];
    size_t n_read = -1;

    for (size_t iter = 0; iter < 100000; ++iter) {
        do {
            n_read = wchar_fread(buf, len_buf, file);
            kdl_tokenizer_feed(&tzr, buf, n_read);

            while (kdl_tokenizer_next_token(&tzr, &token)) {
                ;
            }
        } while (n_read == len_buf);

        rewind(file);
    }

    fclose(file);

    free(tzr_buf);
    free(tok_buf);

    return 0;
}
