#ifndef HS_VEC_H
#define HS_VEC_H

#include <stdlib.h>
#include <string.h>

#define HS_VEC_INIT_CAP 8

#define hs_vec(T) struct { T *data; size_t len; size_t cap; }

typedef hs_vec(char *) hs_strvec;
typedef hs_vec(size_t) hs_sizevec;

#define hs_vec_init(v) do { \
    (v)->data = NULL; \
    (v)->len = 0; \
    (v)->cap = 0; \
} while (0)

#define hs_vec_push(v, item) do { \
    if ((v)->len >= (v)->cap) { \
        (v)->cap = (v)->cap ? (v)->cap * 2 : HS_VEC_INIT_CAP; \
        (v)->data = realloc((v)->data, (v)->cap * sizeof(*(v)->data)); \
    } \
    (v)->data[(v)->len++] = (item); \
} while (0)

#define hs_vec_free(v) do { \
    free((v)->data); \
    (v)->data = NULL; \
    (v)->len = 0; \
    (v)->cap = 0; \
} while (0)

#define hs_vec_clear(v) do { \
    (v)->len = 0; \
} while (0)

#define hs_vec_last(v) ((v)->data[(v)->len - 1])

#define hs_vec_pop(v) ((v)->data[--(v)->len])

#define hs_vec_remove(v, i) do { \
    if ((i) < (v)->len - 1) { \
        memmove(&(v)->data[(i)], &(v)->data[(i) + 1], \
                ((v)->len - (i) - 1) * sizeof(*(v)->data)); \
    } \
    (v)->len--; \
} while (0)

static inline void hs_strvec_free(hs_strvec *v) {
    for (size_t i = 0; i < v->len; i++)
        free(v->data[i]);
    free(v->data);
    v->data = NULL;
    v->len = 0;
    v->cap = 0;
}

static inline hs_strvec hs_strvec_clone(const hs_strvec *src) {
    hs_strvec dst;
    hs_vec_init(&dst);
    for (size_t i = 0; i < src->len; i++) {
        hs_vec_push(&dst, strdup(src->data[i]));
    }
    return dst;
}

static inline int hs_strvec_contains(const hs_strvec *v, const char *s) {
    for (size_t i = 0; i < v->len; i++) {
        if (strcmp(v->data[i], s) == 0) return 1;
    }
    return 0;
}

#endif
