/*
 * GENERATED FILE - DO NOT EDIT DIRECTLY
 * Source: zlist.c
 *
 * This file is part of the z-libs collection: https://github.com/z-libs
 * Licensed under the MIT License.
 */


/* ============================================================================
   z-libs Common Definitions (Bundled)
   This block is auto-generated. It is guarded so that if you include multiple
   z-libs it is only defined once.
   ============================================================================ */
#ifndef Z_COMMON_BUNDLED
#define Z_COMMON_BUNDLED


#ifndef ZCOMMON_H
#define ZCOMMON_H

#include <stddef.h>

// Return Codes.
#define Z_OK     0
#define Z_ERR   -1
#define Z_FOUND  1

// Memory Macros.
// If the user hasn't defined their own allocator, use the standard one.
#ifndef Z_MALLOC
    #include <stdlib.h>
    #define Z_MALLOC(sz)       malloc(sz)
    #define Z_CALLOC(n, sz)    calloc(n, sz)
    #define Z_REALLOC(p, sz)   realloc(p, sz)
    #define Z_FREE(p)          free(p)
#endif

// Compiler Extensions (Optional).
// We check for GCC/Clang features to enable RAII-style cleanup.
// Define Z_NO_EXTENSIONS to disable this manually.
#if !defined(Z_NO_EXTENSIONS) && (defined(__GNUC__) || defined(__clang__))
    #define Z_HAS_CLEANUP 1
    #define Z_CLEANUP(func) __attribute__((cleanup(func)))
#else
    #define Z_HAS_CLEANUP 0
    #define Z_CLEANUP(func) 
#endif

// Metaprogramming Markers.
// These macros are used by the Z-Scanner tool to find type definitions.
// For the C compiler, they are no-ops (they compile to nothing).
#define DEFINE_VEC_TYPE(T, Name)
#define DEFINE_LIST_TYPE(T, Name)
#define DEFINE_MAP_TYPE(Key, Val, Name)

//Growth Strategy.
// Determines how containers expand when full.
// Default is 2.0x (Geometric Growth).
//
// Alternatives:
// #define Z_GROWTH_FACTOR(c)  ((c) * 3 / 2) // 1.5x (Better for fragmentation)
// #define Z_GROWTH_FACTOR(c)  ((c) + 16)    // Linear (Not recommended)
#ifndef Z_GROWTH_FACTOR
    #define Z_GROWTH_FACTOR(cap) ((cap) == 0 ? 32 : (cap) * 2)
#endif

#endif


#endif // Z_COMMON_BUNDLED
/* ============================================================================ */


#ifndef ZLIST_H
#define ZLIST_H
// [Bundled] "zcommon.h" is included inline in this same file
#include <assert.h>

#ifndef Z_LIST_MALLOC
    #define Z_LIST_MALLOC(sz)      Z_MALLOC(sz)
#endif

#ifndef Z_LIST_CALLOC
    #define Z_LIST_CALLOC(n, sz)   Z_CALLOC(n, sz)
#endif

#ifndef Z_LIST_REALLOC
    #define Z_LIST_REALLOC(p, sz)  Z_REALLOC(p, sz)
#endif

#ifndef Z_LIST_FREE
    #define Z_LIST_FREE(p)         Z_FREE(p)
#endif

#define Z_LIST_GENERATE_IMPL(T, Name)                                               \
                                                                                    \
typedef struct zlist_node_##Name {                                                  \
    struct zlist_node_##Name *prev;                                                 \
    struct zlist_node_##Name *next;                                                 \
    T value;                                                                        \
} zlist_node_##Name;                                                                \
                                                                                    \
typedef struct {                                                                    \
    zlist_node_##Name *head;                                                        \
    zlist_node_##Name *tail;                                                        \
    size_t length;                                                                  \
} list_##Name;                                                                      \
                                                                                    \
static inline list_##Name list_init_##Name(void) {                                  \
    return (list_##Name){0};                                                        \
}                                                                                   \
                                                                                    \
static inline int list_push_back_##Name(list_##Name *l, T val) {                    \
    zlist_node_##Name *n = Z_LIST_MALLOC(sizeof(zlist_node_##Name));                \
    if (!n) return Z_ERR;                                                           \
    n->value = val;                                                                 \
    n->next = NULL;                                                                 \
    n->prev = l->tail;                                                              \
    if (l->tail) l->tail->next = n;                                                 \
    l->tail = n;                                                                    \
    if (!l->head) l->head = n;                                                      \
    l->length++;                                                                    \
    return Z_OK;                                                                    \
}                                                                                   \
                                                                                    \
static inline int list_push_front_##Name(list_##Name *l, T val) {                   \
    zlist_node_##Name *n = Z_LIST_MALLOC(sizeof(zlist_node_##Name));                \
    if (!n) return Z_ERR;                                                           \
    n->value = val;                                                                 \
    n->next = l->head;                                                              \
    n->prev = NULL;                                                                 \
    if (l->head) l->head->prev = n;                                                 \
    l->head = n;                                                                    \
    if (!l->tail) l->tail = n;                                                      \
    l->length++;                                                                    \
    return Z_OK;                                                                    \
}                                                                                   \
                                                                                    \
static inline int list_insert_after_##Name(list_##Name *l,                          \
    zlist_node_##Name *prev_node, T val) {                                          \
    if (!prev_node) return list_push_front_##Name(l, val);                          \
    zlist_node_##Name *n = Z_LIST_MALLOC(sizeof(zlist_node_##Name));                \
    if (!n) return Z_ERR;                                                           \
    n->value = val;                                                                 \
    n->prev = prev_node;                                                            \
    n->next = prev_node->next;                                                      \
    if (prev_node->next) prev_node->next->prev = n;                                 \
    else l->tail = n;                                                               \
    prev_node->next = n;                                                            \
    l->length++;                                                                    \
    return Z_OK;                                                                    \
}                                                                                   \
                                                                                    \
static inline void list_pop_back_##Name(list_##Name *l) {                           \
    if (!l->tail) return;                                                           \
    zlist_node_##Name *old_tail = l->tail;                                          \
    l->tail = old_tail->prev;                                                       \
    if (l->tail) l->tail->next = NULL;                                              \
    else l->head = NULL;                                                            \
    Z_LIST_FREE(old_tail);                                                          \
    l->length--;                                                                    \
}                                                                                   \
                                                                                    \
static inline void list_pop_front_##Name(list_##Name *l) {                          \
    if (!l->head) return;                                                           \
    zlist_node_##Name *old_head = l->head;                                          \
    l->head = old_head->next;                                                       \
    if (l->head) l->head->prev = NULL;                                              \
    else l->tail = NULL;                                                            \
    Z_LIST_FREE(old_head);                                                          \
    l->length--;                                                                    \
}                                                                                   \
                                                                                    \
static inline void list_remove_node_##Name(list_##Name *l, zlist_node_##Name *n) {  \
    if (!n) return;                                                                 \
    if (n->prev) n->prev->next = n->next;                                           \
    else l->head = n->next;                                                         \
    if (n->next) n->next->prev = n->prev;                                           \
    else l->tail = n->prev;                                                         \
    Z_LIST_FREE(n);                                                                 \
    l->length--;                                                                    \
}                                                                                   \
                                                                                    \
static inline void list_clear_##Name(list_##Name *l) {                              \
    zlist_node_##Name *curr = l->head;                                              \
    while (curr) {                                                                  \
        zlist_node_##Name *next = curr->next;                                       \
        Z_LIST_FREE(curr);                                                          \
        curr = next;                                                                \
    }                                                                               \
    l->head = l->tail = NULL;                                                       \
    l->length = 0;                                                                  \
}                                                                                   \
                                                                                    \
static inline void list_splice_##Name(list_##Name *dest, list_##Name *src) {        \
    if (dest == src) return;                                                        \
    if (!src->head) return;                                                         \
    if (!dest->head) {                                                              \
        *dest = *src;                                                               \
    } else {                                                                        \
        dest->tail->next = src->head;                                               \
        src->head->prev = dest->tail;                                               \
        dest->tail = src->tail;                                                     \
        dest->length += src->length;                                                \
    }                                                                               \
    src->head = src->tail = NULL;                                                   \
    src->length = 0;                                                                \
}                                                                                   \
                                                                                    \
static inline zlist_node_##Name *list_at_##Name(list_##Name *l, size_t index) {     \
    if (index >= l->length) return NULL;                                            \
    zlist_node_##Name *curr = l->head;                                              \
    while (index-- > 0) curr = curr->next;                                          \
    return curr;                                                                    \
}                                                                                   \
                                                                                    \
static inline zlist_node_##Name *list_head_##Name(list_##Name *l) {                 \
    return l->head;                                                                 \
}                                                                                   \
                                                                                    \
static inline zlist_node_##Name *list_tail_##Name(list_##Name *l) {                 \
    return l->tail;                                                                 \
}

#define L_PUSH_B_ENTRY(T, Name)   list_##Name*: list_push_back_##Name,
#define L_PUSH_F_ENTRY(T, Name)   list_##Name*: list_push_front_##Name,
#define L_INS_A_ENTRY(T, Name)    list_##Name*: list_insert_after_##Name,
#define L_POP_B_ENTRY(T, Name)    list_##Name*: list_pop_back_##Name,
#define L_POP_F_ENTRY(T, Name)    list_##Name*: list_pop_front_##Name,
#define L_REM_N_ENTRY(T, Name)    list_##Name*: list_remove_node_##Name,
#define L_CLEAR_ENTRY(T, Name)    list_##Name*: list_clear_##Name,
#define L_SPLICE_ENTRY(T, Name)   list_##Name*: list_splice_##Name,
#define L_HEAD_ENTRY(T, Name)     list_##Name*: list_head_##Name,
#define L_TAIL_ENTRY(T, Name)     list_##Name*: list_tail_##Name,
#define L_AT_ENTRY(T, Name)       list_##Name*: list_at_##Name,

#if defined(__has_include) && __has_include("z_registry.h")
    #include "z_registry.h"
#endif

#ifndef Z_AUTOGEN_LISTS
    #define Z_AUTOGEN_LISTS(X)
#endif

#ifndef REGISTER_TYPES
    #define REGISTER_TYPES(X)
#endif

#define Z_ALL_LISTS(X) \
    Z_AUTOGEN_LISTS(X) \
    REGISTER_TYPES(X)

Z_ALL_LISTS(Z_LIST_GENERATE_IMPL)


#define list_init(Name)           list_init_##Name()

#if Z_HAS_CLEANUP
    #define list_autofree(Name)  Z_CLEANUP(list_clear_##Name) list_##Name
#endif

#define list_push_back(l, val)     _Generic((l),    Z_ALL_LISTS(L_PUSH_B_ENTRY)  default: 0)         (l, val)
#define list_push_front(l, val)    _Generic((l),    Z_ALL_LISTS(L_PUSH_F_ENTRY)  default: 0)         (l, val)
#define list_insert_after(l, n, v) _Generic((l),    Z_ALL_LISTS(L_INS_A_ENTRY)   default: 0)         (l, n, v)
#define list_pop_back(l)           _Generic((l),    Z_ALL_LISTS(L_POP_B_ENTRY)   default: (void)0)   (l)
#define list_pop_front(l)          _Generic((l),    Z_ALL_LISTS(L_POP_F_ENTRY)   default: (void)0)   (l)
#define list_remove_node(l, n)     _Generic((l),    Z_ALL_LISTS(L_REM_N_ENTRY)   default: (void)0)   (l, n)
#define list_clear(l)              _Generic((l),    Z_ALL_LISTS(L_CLEAR_ENTRY)   default: (void)0)   (l)
#define list_splice(dst, src)      _Generic((dst),  Z_ALL_LISTS(L_SPLICE_ENTRY)  default: (void)0)   (dst, src)
#define list_head(l)               _Generic((l),    Z_ALL_LISTS(L_HEAD_ENTRY)    default: (void*)0)  (l)
#define list_tail(l)               _Generic((l),    Z_ALL_LISTS(L_TAIL_ENTRY)    default: (void*)0)  (l)
#define list_at(l, idx)            _Generic((l),    Z_ALL_LISTS(L_AT_ENTRY)      default: (void*)0)  (l, idx)

#define list_foreach(l, iter) \
    for ((iter) = (l)->head; (iter) != NULL; (iter) = (iter)->next)

#define list_foreach_safe(l, iter, safe_iter) \
    for ((iter) = (l)->head, (safe_iter) = (iter) ? (iter)->next : NULL; \
         (iter) != NULL; \
         (iter) = (safe_iter), (safe_iter) = (iter) ? (iter)->next : NULL)

#endif