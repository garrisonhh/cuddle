// TODO can I do this without stdio.h?
#include <stdio.h>

#include <cuddle/serialize.h>

// TODO I don't think this will handle multibyte chars properly
void kdl_serialize_string(char *buf, size_t buf_size, char *string) {
    *buf++ = '"';

    while (*string) {
        switch (*string) {
#define ESC_CASE(escape, write)\
    case escape: *buf++ = '\\'; *buf++ = write; break
        ESC_CASE('\n', 'n');
        ESC_CASE('\r', 'r');
        ESC_CASE('\t', 't');
        ESC_CASE('\b', 'b');
        ESC_CASE('\f', 'f');
        ESC_CASE('\\', '\\');
        ESC_CASE('"', '\"');
#undef ESC_CASE
        default:
            *buf++ = *string;

            break;
        }

        ++string;
    }

    *buf++ = '"';
    *buf = 0;
}

void kdl_serialize_number(char *buf, size_t buf_size, double number) {
    if ((double)(long)number == number) {
        // integer representable
        snprintf(buf, buf_size, "%ld", (long)number);
    } else {
        snprintf(buf, buf_size, "%f", number);
    }
}

void kdl_serialize_bool(char *buf, size_t buf_size, bool boolean) {
    snprintf(buf, buf_size, boolean ? "true" : "false");
}

void kdl_serialize_null(char *buf, size_t buf_size) {
    snprintf(buf, buf_size, "null");
}
