#ifndef KDL_SERIALIZE_H
#define KDL_SERIALIZE_H

#include <stdbool.h>

/*
 * serialization functions take in a value and output
 *
 * TODO memory safety with all of these functions
 */

void kdl_serialize_string(char *buf, size_t buf_size, char *string);
void kdl_serialize_number(char *buf, size_t buf_size, double number);
void kdl_serialize_bool(char *buf, size_t buf_size, bool boolean);
// just writes 'null' to buf, here for symmetry lol
void kdl_serialize_null(char *buf, size_t buf_size);

#endif
