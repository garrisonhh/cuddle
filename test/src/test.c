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
    // housekeeping stuff
    setlocale(LC_ALL, "en_US.UTF-8");

    kdl_tokenizer_t tzr;
    kdl_tokenizer_make(&tzr);

    FILE *file = fopen("files/website.kdl", "r");

    if (!file) {
        printf("can't open file :(\n");
        exit(-1);
    }

    fwide(file, 1);

    // read file in, small
    const size_t len_buf = 1024;
    wchar_t buf[len_buf];
    size_t n_read = -1;

    do {
        kdl_token_t token;

        n_read = wchar_fread(buf, len_buf, file);
        kdl_tokenizer_feed(&tzr, buf, n_read);

        while (kdl_tokenizer_next_token(&tzr, &token)) {
            ;
        }
    } while (n_read == len_buf);

    fclose(file);

    return 0;
}
