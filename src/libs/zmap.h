/*
 * GENERATED FILE - DO NOT EDIT DIRECTLY
 * Source: zmap.c
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


#ifndef ZMAP_H
#define ZMAP_H
// [Bundled] "zcommon.h" is included inline in this same file
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
#include <stdexcept>
#include <iterator>
#include <utility>
#include <type_traits>

namespace z_map
{
    // Forward declarations.
    template <typename K, typename V> struct map;
    template <typename K, typename V> class map_iterator;

    template <typename K, typename V>
    struct traits
    {
        static_assert(sizeof(K) == 0, "No zmap implementation registered for this key/value pair.");
    };

    // Forward iterator for the map.
    // Iterates over buckets, skipping EMPTY and DELETED states.
    template <typename K, typename V>
    class map_iterator
    {
    public:
        // Detect const-ness
        static constexpr bool is_const = std::is_const<K>::value || std::is_const<V>::value;

        // Strip const for trait lookup (we always look up the raw C type)
        using KeyT = typename std::remove_const<K>::type;
        using ValT = typename std::remove_const<V>::type;
        
        using MapTraits = traits<KeyT, ValT>;
        using CMap = typename MapTraits::map_type;
        using CBucket = typename MapTraits::bucket_type;

        using iterator_category = std::forward_iterator_tag;
        using value_type = CBucket;
        using difference_type = ptrdiff_t;
        
        using reference = typename std::conditional<is_const, const CBucket&, CBucket&>::type;
        using pointer   = typename std::conditional<is_const, const CBucket*, CBucket*>::type;

        map_iterator(CMap* m, size_t idx) : map_ptr(m), index(idx) 
        {
            // If we start at a non-valid index (or empty), scan to first valid.
            if (map_ptr && index < map_ptr->capacity) 
            {
                if (map_ptr->buckets[index].state != 1) // 1 is ZMAP_OCCUPIED
                {
                    advance();
                }
            }
        }

        // Accessors: Returns reference to the bucket (containing .key and .value).
        reference operator*() const { return map_ptr->buckets[index]; }
        pointer operator->() const { return &map_ptr->buckets[index]; }

        bool operator==(const map_iterator& other) const 
        { 
            return map_ptr == other.map_ptr && index == other.index; 
        }
        bool operator!=(const map_iterator& other) const 
        { 
            return !(*this == other); 
        }

        map_iterator& operator++() 
        {
            if (index < map_ptr->capacity) 
            {
                index++;
                advance();
            }
            return *this;
        }

        map_iterator operator++(int) 
        {
            map_iterator temp = *this;
            ++(*this);
            return temp;
        }

    private:
        void advance() 
        {
            while (index < map_ptr->capacity && map_ptr->buckets[index].state != 1) 
            {
                index++;
            }
        }

        CMap* map_ptr;
        size_t index;
    };

    template <typename K, typename V>
    struct map
    {
        using Traits = traits<K, V>;
        using c_map = typename Traits::map_type;
        using c_bucket = typename Traits::bucket_type;

        using iterator = map_iterator<K, V>;
        using const_iterator = map_iterator<const K, const V>;

        c_map inner;

        // Hash and Compare function types matching C signatures.
        using HashFunc = uint32_t (*)(K);
        using CmpFunc = int (*)(K, K);

        // Constructor: requires hash and compare functions.
        map(HashFunc h, CmpFunc c, float load_factor = 0.75f) 
            : inner(Traits::init(h, c, load_factor)) {}

        // Move constructor.
        map(map&& other) noexcept : inner(other.inner) 
        {
            other.inner = (c_map){0}; // Zero out source
        }

        // Destructor.
        ~map() { Traits::free(&inner); }

        // Move assignment.
        map& operator=(map&& other) noexcept 
        {
            if (this != &other) 
            {
                Traits::free(&inner);
                inner = other.inner;
                other.inner = (c_map){0};
            }
            return *this;
        }

        // Deleted copy.
        map(const map&) = delete;
        map& operator=(const map&) = delete;

        /* Modifiers. */

        void put(const K& key, const V& val) 
        {
            if (Traits::put(&inner, key, val) != 0) 
            {
                throw std::bad_alloc();
            }
        }

        void insert_or_assign(const K& key, const V& val) { put(key, val); }

        V* get(const K& key) { return Traits::get(&inner, key); }
        const V* get(const K& key) const { return Traits::get((c_map*)&inner, key); }

        bool contains(const K& key) const { return Traits::contains((c_map*)&inner, key); }

        void erase(const K& key) { Traits::remove(&inner, key); }

        void clear() { Traits::clear(&inner); }

        /* Capacity. */
        size_t size() const { return inner.count; }
        bool empty() const { return inner.count == 0; }

        /* Iterators. */
        iterator begin() { return iterator(&inner, 0); }
        iterator end() { return iterator(&inner, inner.capacity); }
        
        // Const Iterators.
        const_iterator begin() const { return const_iterator((c_map*)&inner, 0); }
        const_iterator end() const { return const_iterator((c_map*)&inner, inner.capacity); }
        const_iterator cbegin() const { return begin(); }
        const_iterator cend() const { return end(); }
    };
}

extern "C" {
#endif // __cplusplus

/* Configuration & allocators. */

#ifndef Z_MAP_MALLOC
    #define Z_MAP_MALLOC(sz)      Z_MALLOC(sz)
#endif

#ifndef Z_MAP_CALLOC
    #define Z_MAP_CALLOC(n, sz)   Z_CALLOC(n, sz)
#endif

#ifndef Z_MAP_REALLOC
    #define Z_MAP_REALLOC(p, sz)  Z_REALLOC(p, sz)
#endif

#ifndef Z_MAP_FREE
    #define Z_MAP_FREE(p)         Z_FREE(p)
#endif

// Default load factor (0.75 is standard for open addressing).
// Can be tuned per-map using map_init_chk.
#define ZMAP_DEFAULT_LOAD 0.75f


/* Hashing helper. */

// FNV-1a Hash implementation (32-bit).
static inline uint32_t zmap_default_hash(const void *key, size_t len)
{
    uint32_t hash = 2166136261u;
    const uint8_t *data = (const uint8_t *)key;
    for (size_t i = 0; i < len; i++)
    {
        hash ^= data[i];
        hash *= 16777619;
    }
    return hash;
}

// Helper macros for hashing scalars and C-strings.
#define ZMAP_HASH_SCALAR(k) zmap_default_hash(&(k), sizeof(k))
#define ZMAP_HASH_STR(k)    zmap_default_hash((k), strlen(k))


/* Internal data structures. */

typedef enum 
{
    ZMAP_EMPTY = 0,
    ZMAP_OCCUPIED,
    ZMAP_DELETED
} zmap_state;


/* * The generator macro. */
#define Z_MAP_GENERATE_IMPL(KeyT, ValT, Name)                                                                   \
                                                                                                                \
/* The bucket struct holding individual entries. */                                                             \
typedef struct                                                                                                  \
{                                                                                                               \
    KeyT key;                                                                                                   \
    ValT value;                                                                                                 \
    zmap_state state;                                                                                           \
} zmap_bucket_##Name;                                                                                           \
                                                                                                                \
/* The main map struct. */                                                                                      \
typedef struct                                                                                                  \
{                                                                                                               \
    zmap_bucket_##Name *buckets;                                                                                \
    size_t capacity;                                                                                            \
    size_t count;       /* Number of active items. */                                                           \
    size_t occupied;    /* Active + Deleted items (for load factor). */                                         \
    size_t threshold;   /* Grow when occupied >= threshold. */                                                  \
    float  load_factor;                                                                                         \
    uint32_t (*hash_func)(KeyT);                                                                                \
    int      (*cmp_func)(KeyT, KeyT);                                                                           \
} map_##Name;                                                                                                   \
                                                                                                                \
/* Iterator state struct. */                                                                                    \
typedef struct                                                                                                  \
{                                                                                                               \
    map_##Name *map;                                                                                            \
    size_t index;                                                                                               \
} zmap_iter_##Name;                                                                                             \
                                                                                                                \
/* Frees the internal bucket memory. Does NOT free the map pointer itself. */                                   \
static inline void map_free_##Name(map_##Name *m)                                                               \
{                                                                                                               \
    Z_MAP_FREE(m->buckets);                                                                                     \
    *m = (map_##Name){0};                                                                                       \
}                                                                                                               \
                                                                                                                \
/* Initializes a map with a custom load factor. */                                                              \
static inline map_##Name map_init_ext_##Name(uint32_t (*h)(KeyT), int (*c)(KeyT, KeyT), float load)             \
{                                                                                                               \
    return (map_##Name){                                                                                        \
        .threshold = 0,                                                                                         \
        .load_factor = (load <= 0.1f || load > 0.95f) ? ZMAP_DEFAULT_LOAD : load,                               \
        .hash_func = h,                                                                                         \
        .cmp_func = c                                                                                           \
    };                                                                                                          \
}                                                                                                               \
                                                                                                                \
/* Initializes a map with default settings (Load Factor 0.75). */                                               \
static inline map_##Name map_init_##Name(uint32_t (*h)(KeyT), int (*c)(KeyT, KeyT))                             \
{                                                                                                               \
    return map_init_ext_##Name(h, c, ZMAP_DEFAULT_LOAD);                                                        \
}                                                                                                               \
                                                                                                                \
/* Internal: Resizes the bucket array and rehashes all active entries. */                                       \
static inline int map_resize_##Name(map_##Name *m, size_t new_cap)                                              \
{                                                                                                               \
    zmap_bucket_##Name *new_buckets = (zmap_bucket_##Name*)Z_MAP_CALLOC(new_cap, sizeof(zmap_bucket_##Name));   \
    if (!new_buckets) return Z_ERR;                                                                             \
                                                                                                                \
    for (size_t i = 0; i < m->capacity; i++)                                                                    \
    {                                                                                                           \
        if (m->buckets[i].state == ZMAP_OCCUPIED)                                                               \
        {                                                                                                       \
            uint32_t hash = m->hash_func(m->buckets[i].key);                                                    \
            size_t idx = hash % new_cap;                                                                        \
            while (new_buckets[idx].state == ZMAP_OCCUPIED)                                                     \
            {                                                                                                   \
                idx = (idx + 1) % new_cap;                                                                      \
            }                                                                                                   \
            new_buckets[idx] = m->buckets[i];                                                                   \
        }                                                                                                       \
    }                                                                                                           \
    Z_MAP_FREE(m->buckets);                                                                                     \
    m->buckets = new_buckets;                                                                                   \
    m->capacity = new_cap;                                                                                      \
    m->occupied = m->count; /* Deleted items are purged during resize. */                                       \
    m->threshold = (size_t)(new_cap * m->load_factor);                                                          \
    return Z_OK;                                                                                                \
}                                                                                                               \
                                                                                                                \
/* Inserts or updates a value. Returns Z_OK on success, Z_ERR on allocation failure. */                         \
static inline int map_put_##Name(map_##Name *m, KeyT key, ValT val)                                             \
{                                                                                                               \
    if (m->occupied >= m->threshold)                                                                            \
    {                                                                                                           \
        size_t new_cap = (m->capacity == 0) ? 16 : (m->capacity * 2);                                           \
        if (map_resize_##Name(m, new_cap) != Z_OK) return Z_ERR;                                                \
    }                                                                                                           \
    uint32_t hash = m->hash_func(key);                                                                          \
    size_t idx = hash % m->capacity;                                                                            \
    size_t deleted_idx = SIZE_MAX;                                                                              \
                                                                                                                \
    for (size_t i = 0; i < m->capacity; i++)                                                                    \
    {                                                                                                           \
        zmap_state s = m->buckets[idx].state;                                                                   \
        if (s == ZMAP_EMPTY)                                                                                    \
        {                                                                                                       \
            if (deleted_idx != SIZE_MAX) idx = deleted_idx;                                                     \
            else m->occupied++;                                                                                 \
            m->buckets[idx] = (zmap_bucket_##Name){ .key = key, .value = val, .state = ZMAP_OCCUPIED };         \
            m->count++;                                                                                         \
            return Z_OK;                                                                                        \
        }                                                                                                       \
        if (s == ZMAP_DELETED)                                                                                  \
        {                                                                                                       \
            if (deleted_idx == SIZE_MAX) deleted_idx = idx;                                                     \
        }                                                                                                       \
        else if (m->cmp_func(m->buckets[idx].key, key) == 0)                                                    \
        {                                                                                                       \
            m->buckets[idx].value = val;                                                                        \
            return Z_OK;                                                                                        \
        }                                                                                                       \
        idx = (idx + 1) % m->capacity;                                                                          \
    }                                                                                                           \
    return Z_ERR;                                                                                               \
}                                                                                                               \
                                                                                                                \
/* Returns a pointer to the value, or NULL if not found. */                                                     \
static inline ValT* map_get_##Name(map_##Name *m, KeyT key)                                                     \
{                                                                                                               \
    if (m->count == 0) return NULL;                                                                             \
    uint32_t hash = m->hash_func(key);                                                                          \
    size_t idx = hash % m->capacity;                                                                            \
                                                                                                                \
    for (size_t i = 0; i < m->capacity; i++)                                                                    \
    {                                                                                                           \
        zmap_state s = m->buckets[idx].state;                                                                   \
        if (s == ZMAP_EMPTY) return NULL;                                                                       \
        if (s == ZMAP_OCCUPIED && m->cmp_func(m->buckets[idx].key, key) == 0)                                   \
        {                                                                                                       \
            return &m->buckets[idx].value;                                                                      \
        }                                                                                                       \
        idx = (idx + 1) % m->capacity;                                                                          \
    }                                                                                                           \
    return NULL;                                                                                                \
}                                                                                                               \
                                                                                                                \
/* Returns true if the key exists in the map. */                                                                \
static inline bool map_contains_##Name(map_##Name *m, KeyT key)                                                 \
{                                                                                                               \
    return map_get_##Name(m, key) != NULL;                                                                      \
}                                                                                                               \
                                                                                                                \
/* Removes an entry by marking it as DELETED (lazy deletion). */                                                \
static inline void map_remove_##Name(map_##Name *m, KeyT key)                                                   \
{                                                                                                               \
    if (m->count == 0) return;                                                                                  \
    uint32_t hash = m->hash_func(key);                                                                          \
    size_t idx = hash % m->capacity;                                                                            \
                                                                                                                \
    for (size_t i = 0; i < m->capacity; i++)                                                                    \
    {                                                                                                           \
        zmap_state s = m->buckets[idx].state;                                                                   \
        if (s == ZMAP_EMPTY) return;                                                                            \
        if (s == ZMAP_OCCUPIED && m->cmp_func(m->buckets[idx].key, key) == 0)                                   \
        {                                                                                                       \
            m->buckets[idx].state = ZMAP_DELETED;                                                               \
            m->count--;                                                                                         \
            return;                                                                                             \
        }                                                                                                       \
        idx = (idx + 1) % m->capacity;                                                                          \
    }                                                                                                           \
}                                                                                                               \
                                                                                                                \
/* Creates a new iterator for the map. */                                                                       \
static inline zmap_iter_##Name map_iter_init_##Name(map_##Name *m)                                              \
{                                                                                                               \
    return (zmap_iter_##Name){ .map = m, .index = 0 };                                                          \
}                                                                                                               \
                                                                                                                \
/* Advances the iterator. Returns true and writes key/val if a next item exists. */                             \
static inline bool map_iter_next_##Name(zmap_iter_##Name *it, KeyT *out_k, ValT *out_v)                         \
{                                                                                                               \
    if (!it->map || !it->map->buckets) return false;                                                            \
    while (it->index < it->map->capacity)                                                                       \
    {                                                                                                           \
        size_t i = it->index++;                                                                                 \
        if (it->map->buckets[i].state == ZMAP_OCCUPIED)                                                         \
        {                                                                                                       \
            if (out_k) *out_k = it->map->buckets[i].key;                                                        \
            if (out_v) *out_v = it->map->buckets[i].value;                                                      \
            return true;                                                                                        \
        }                                                                                                       \
    }                                                                                                           \
    return false;                                                                                               \
}                                                                                                               \
                                                                                                                \
static inline size_t map_size_##Name(map_##Name *m) { return m->count; }                                        \
                                                                                                                \
/* Clears all items (sets count to 0) but keeps memory allocated. */                                            \
static inline void map_clear_##Name(map_##Name *m)                                                              \
{                                                                                                               \
    if (m->capacity > 0)                                                                                        \
    {                                                                                                           \
            memset(m->buckets, 0, m->capacity * sizeof(zmap_bucket_##Name));                                    \
    }                                                                                                           \
    m->count = 0;                                                                                               \
    m->occupied = 0;                                                                                            \
}

// X-Macro entry points.
#define M_PUT_ENTRY(K, V, N)     map_##N*: map_put_##N,
#define M_GET_ENTRY(K, V, N)     map_##N*: map_get_##N,
#define M_HAS_ENTRY(K, V, N)     map_##N*: map_contains_##Name,
#define M_REM_ENTRY(K, V, N)     map_##N*: map_remove_##N,
#define M_FREE_ENTRY(K, V, N)    map_##N*: map_free_##N,
#define M_SIZE_ENTRY(K, V, N)    map_##N*: map_size_##N,
#define M_CLEAR_ENTRY(K, V, N)   map_##N*: map_clear_##N,
#define M_ITER_NEXT_ENTRY(K,V,N) zmap_iter_##N*: map_iter_next_##Name,


/* Generic Accessors */

#if defined(__has_include) && __has_include("z_registry.h")
    #include "z_registry.h"
#endif

#ifndef Z_AUTOGEN_MAPS
    #define Z_AUTOGEN_MAPS(X)
#endif

#ifndef REGISTER_MAP_TYPES
    #define REGISTER_MAP_TYPES(X)
#endif

#define Z_ALL_MAPS(X) \
    Z_AUTOGEN_MAPS(X) \
    REGISTER_MAP_TYPES(X)

Z_ALL_MAPS(Z_MAP_GENERATE_IMPL)

// Standard Init (Load Factor = 0.75).
#define map_init(Name, h_func, c_func)    map_init_##Name(h_func, c_func)

// Advanced Init (Custom Load Factor).
#define map_init_chk(Name, h, c, load)    map_init_ext_##Name(h, c, load)

// Auto-Cleanup Extension (GCC/Clang).
#if defined(Z_HAS_CLEANUP) && Z_HAS_CLEANUP
    #define map_autofree(Name)  Z_CLEANUP(map_free_##Name) map_##Name
#endif

// Generic API Methods.
#define map_put(m, k, v)      _Generic((m), Z_ALL_MAPS(M_PUT_ENTRY)   default: 0) (m, k, v)
#define map_get(m, k)         _Generic((m), Z_ALL_MAPS(M_GET_ENTRY)   default: (void*)0) (m, k)
#define map_contains(m, k)    _Generic((m), Z_ALL_MAPS(M_HAS_ENTRY)   default: false) (m, k)
#define map_remove(m, k)      _Generic((m), Z_ALL_MAPS(M_REM_ENTRY)   default: (void)0) (m, k)
#define map_free(m)           _Generic((m), Z_ALL_MAPS(M_FREE_ENTRY)  default: (void)0) (m)
#define map_size(m)           _Generic((m), Z_ALL_MAPS(M_SIZE_ENTRY)  default: 0) (m)
#define map_clear(m)          _Generic((m), Z_ALL_MAPS(M_CLEAR_ENTRY) default: (void)0) (m)
#define map_iter_next(it, k, v) _Generic((it), Z_ALL_MAPS(M_ITER_NEXT_ENTRY) default: false) (it, k, v)

/* Iteration Macros */

// usage: zmap_iter_Name it = map_iter_init(Name, &my_map);
#define map_iter_init(Name, m) map_iter_init_##Name(m)
// usage: while (map_iter_next(&it, &key, &val)) { ... }
#define map_iter_next(it, k, v) _Generic((it), Z_ALL_MAPS(M_ITER_NEXT_ENTRY) default: false) (it, k, v)

#ifdef __cplusplus
} // extern "C"

// C++ Traits Generation
namespace z_map
{
    #define ZMAP_CPP_TRAITS(Key, Val, Name)                                 \
        template<> struct traits<Key, Val>                                  \
        {                                                                   \
            using map_type = map_##Name;                                    \
            using bucket_type = zmap_bucket_##Name;                         \
            static constexpr auto init = map_init_ext_##Name;               \
            static constexpr auto put = map_put_##Name;                     \
            static constexpr auto get = map_get_##Name;                     \
            static constexpr auto contains = map_contains_##Name;           \
            static constexpr auto remove = map_remove_##Name;               \
            static constexpr auto clear = map_clear_##Name;                 \
            static constexpr auto free = map_free_##Name;                   \
        };

    // Generate traits for all registered types
    Z_ALL_MAPS(ZMAP_CPP_TRAITS)
}
#endif // __cplusplus

#endif // ZMAP_H