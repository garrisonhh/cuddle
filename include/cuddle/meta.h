#ifndef KDL_META_H
#define KDL_META_H

#include <stdlib.h>
#include <stdio.h>

/*
 * debugging, errors, etc.
 */

#ifdef KDL_DEBUG_INFO
#define KDL_DEBUG(...) printf(__VA_ARGS__)
#else
#define KDL_DEBUG(...)
#endif

#ifndef NDEBUG
#define KDL_ASSERT(cond, ...) if (!(cond)) KDL_ERROR(__VA_ARGS__)
#else
#define KDL_ASSERT(...)
#endif

#define KDL_ERROR(...)\
    do {\
        fprintf(stderr, "KDL ERROR: ");\
        fprintf(stderr, __VA_ARGS__);\
        exit(-1);\
    } while (0)

#define KDL_UNIMPL(...) KDL_ERROR("unimplemented: " __VA_ARGS__)

#endif
