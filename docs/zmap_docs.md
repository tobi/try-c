# zmap.h

`zmap.h` provides generic hash maps (dictionaries) for C projects. It uses open addressing with linear probing for high performance and cache efficiency. Unlike typical C map implementations that force `void*` casting or string-only keys, `zmap.h` uses C11 `_Generic` selection to generate fully typed, type-safe implementations for your specific key-value pairs.

It also includes a robust **C++11 wrapper**, allowing you to use it as a lightweight, drop-in map class (`z_map::map`) in mixed codebases while sharing the same underlying C implementation.

## Features

* **Type Safety**: Compiler errors if you try to put a `float` key into an `int` map.
* **Native Performance**: Open addressing with linear probing reduces pointer chasing and malloc overhead compared to chained hashmaps.
* **C++ Support**: Includes a full C++ class wrapper with RAII, iterators, and STL-like API.
* **Zero Boilerplate**: Use the **Z-Scanner** tool to automatically generate type registrations.
* **Header Only**: No linking required.
* **Memory Agnostic**: Supports custom allocators (Arenas, Pools, Debuggers).
* **Zero Dependencies**: Only standard C headers used.

## Installation

`zmap.h` works best when you use the provided scanner script to manage type registrations, though it can be used manually.

1.  Copy `zmap.h` (and `zcommon.h` if separated) to your project's include folder.
2.  Add the `z-core` tools (optional but recommended).
```bash
git submodule add https://github.com/z-libs/z-core.git z-core
```

## Usage: C

For C projects, you define the map types you need using a macro that the scanner detects.

```c
#include <stdio.h>
#include <string.h>
#include "zmap.h"

// Helper functions are required for hashing and comparing keys.
// (You can write your own or use the provided macros).
uint32_t hash_str(char *key)   { return ZMAP_HASH_STR(key); }
int      cmp_str(char *a, char *b) { return strcmp(a, b); }

// Request the map types you need.
// Syntax: DEFINE_MAP_TYPE(KeyType, ValueType, ShortName).
DEFINE_MAP_TYPE(char*, int, StrInt)

int main(void)
{
    // Initialize (Standard C style).
    map_StrInt scores = map_init(StrInt, hash_str, cmp_str);

    map_put(&scores, "Alice", 100);
    map_put(&scores, "Bob", 200);

    // Get returns a pointer to the value, or NULL.
    int *score = map_get(&scores, "Alice");
    if (score)
    {
        printf("Alice: %d\n", *score);
    }

    // Cleanup.
    map_free(&scores);
    return 0;
}
```

## Usage: C++

The library detects C++ compilers automatically. The C++ wrapper lives in the **`z_map`** namespace.

```cpp
#include <iostream>
#include <string>
#include "zmap.h"

// Request types (scanner sees this even in .cpp files).
DEFINE_MAP_TYPE(int, float, IntFloat)

// Helpers
uint32_t hash_int(int k) { return ZMAP_HASH_SCALAR(k); }
int cmp_int(int a, int b) { return a - b; }

int main()
{
    // RAII handles memory automatically.
    // Constructor takes hash and compare functions.
    z_map::map<int, float> data(hash_int, cmp_int);

    // STL-like API.
    data.put(10, 1.5f);
    data.insert_or_assign(20, 2.5f);

    // Check existence.
    if (data.contains(10)) {
        std::cout << "Found 10: " << *data.get(10) << "\n";
    }

    // Range-based for loops.
    for(auto& item : data) {
        std::cout << "Key: " << item.key << ", Val: " << item.value << "\n";
    }

    return 0;
}
```

## Compilation Guide

Since `zmap.h` relies on code generation for its type safety, here you have the guide if you use the scanner.

**[Read the Compilation Guide](examples/README.md)** for detailed instructions on how to use `zscanner.py`.

## Manual Setup

If you cannot use Python or prefer manual control, you can use the **Registry Header** approach.

* Create a file named `my_maps.h`.
* Register your types using X-Macros.

```c
#ifndef MY_MAPS_H
#define MY_MAPS_H

// Register Types.
// Syntax: X(KeyType, ValueType, ShortName).
#define REGISTER_MAP_TYPES(X) \
    X(char*, int, StrInt)     \
    X(int, float, IntFloat)

// **IT HAS TO BE INCLUDED AFTER, NOT BEFORE**.
#include "zmap.h"

#endif
```

* Include `"my_maps.h"` instead of `"zmap.h"` in your C files.

## API Reference (C)

`zmap.h` uses C11 `_Generic` to automatically select the correct function implementation based on the map pointer you pass.

**Initialization & Management**

| Macro | Description |
| :--- | :--- |
| `map_init(Name, hash_fn, cmp_fn)` | Returns an empty map struct with default load factor (0.75). |
| `map_init_chk(Name, h, c, load)` | Returns an empty map with a custom load factor (0.1 to 0.95). |
| `map_free(m)` | Frees the internal bucket array and zeroes the map structure. |
| `map_clear(m)` | Clears all items (memset to 0) but keeps the allocated memory capacity. |
| `map_size(m)` | Returns the number of active items (`size_t`). |

**Data Access**

| Macro | Description |
| :--- | :--- |
| `map_put(m, key, val)` | Inserts the key-value pair. If the key exists, updates the value. Returns `Z_OK` or `Z_ERR`. |
| `map_get(m, key)` | Returns a **pointer** to the value associated with `key`, or `NULL` if not found. |
| `map_contains(m, key)` | Returns `true` if the key exists in the map, otherwise `false`. |
| `map_remove(m, key)` | Removes the item associated with `key`. Does nothing if the key is missing. |

**Iteration**

| Macro | Description |
| :--- | :--- |
| `map_iter_init(Name, m)` | Returns a `zmap_iter_Name` struct initialized to the start of the map. |
| `map_iter_next(it, k, v)` | Advances the iterator. Returns `true` if a pair was retrieved, `false` at the end. |

**Example:**
```c
zmap_iter_int it = map_iter_init(int, &my_map);
zstr key; int val;
while (map_iter_next(&it, &key, &val)) { ... }
```

| Function/Macro | Description |
| :--- | :--- |
| `ZMAP_HASH_STR(char *s)` | FNV-1a hash helper for null-terminated C strings. |
| `ZMAP_HASH_SCALAR(val)` | FNV-1a hash helper for scalar types (`int`, `long`, `float`, etc.). |

### Extensions (Experimental)

If you are using a compiler that supports `__attribute__((cleanup))` (like GCC or Clang), you can use the **Auto-Cleanup** extension to automatically free maps when they go out of scope.

| Macro | Description |
| :--- | :--- |
| `map_autofree(Name)` | Declares a map that automatically calls `map_free` when the variable leaves scope (RAII style). |

**Example:**

```c
void process_data()
{
    // 'cache' will be automatically freed when this function returns.
    map_autofree(StrInt) cache = map_init(StrInt, hash_str, cmp_str);
    map_put(&cache, "Key", 100);
}
```

> **Disable Extensions:** To force standard compliance and disable these extensions, define `Z_NO_EXTENSIONS` before including the library.

## API Reference (C++)

The C++ wrapper lives in the **`z_map`** namespace. It strictly adheres to RAII principles and delegates all logic to the underlying C implementation.

### `class z_map::map<K, V>`

**Constructors & Management**

| Method | Description |
| :--- | :--- |
| `map(hash_fn, cmp_fn)` | Constructs map with specific helpers. |
| `~map()` | Destructor. Automatically calls `map_free`. |
| `operator=` | Move assignment operator. |
| `size()` | Returns current number of elements. |
| `empty()` | Returns `true` if size is 0. |
| `clear()` | Clears items but keeps capacity. |

**Access & Modification**

| Method | Description |
| :--- | :--- |
| `put(k, v)` | Inserts or updates key-value pair. Throws `std::bad_alloc` on failure. |
| `insert_or_assign(k, v)` | Alias for `put`. |
| `get(k)` | Returns `V*` or `const V*`. Returns `nullptr` if not found. |
| `contains(k)` | Returns `true` if key exists. |
| `erase(k)` | Removes the key if present. |

**Iterators**

| Method | Description |
| :--- | :--- |
| `begin()`, `end()` | Forward iterators compatible with STL. Returns reference to bucket (`{key, value}`). |
| `cbegin()`, `cend()` | Const iterators for read-only access. |

## Memory Management

By default, `zmap.h` uses the standard C library functions (`malloc`, `calloc`, `free`).

However, you can override these to use your own memory subsystem (e.g., **Memory Arenas**, **Pools**, or **Debug Allocators**).

### First Option: Global Override (Recommended)

To use a custom allocator, define the `Z_` macros **inside your registry header**, immediately before including `zmap.h`.

```c
#ifndef MY_MAPS_H
#define MY_MAPS_H

// Define your custom memory macros **HERE**.
#include "my_memory_system.h"

// IMPORTANT: Override all four to prevent mixing allocators.
// (Maps use calloc for buckets to detect empty slots).
#define Z_MALLOC(sz)      my_custom_alloc(sz)
#define Z_CALLOC(n, sz)   my_custom_calloc(n, sz)
#define Z_REALLOC(n, sz)  my_custom_realloc(p, sz)
#define Z_FREE(p)         my_custom_free(p)

// Then include the library.
#include "zmap.h"

// ... Register types ...

#endif
```

> **Note:** You **must** override **all four macros** (`MALLOC`, `CALLOC`, `REALLOC`, `FREE`) if you override one, to ensure consistency with all libraries.

### Second Option: Library-Specific Override (Advanced)

If you need different allocators for different containers (e.g., an Arena for Maps but the Heap for Vectors), you can use the library-specific macros. These take priority over the global `Z_` macros.

```c
// Example: Maps use a Dict Arena, everything else uses standard malloc.
#define Z_MAP_CALLOC(n, sz)  arena_alloc_zero(dict_arena, (n) * (sz))
#define Z_MAP_FREE(p)        /* no-op for linear arena */

#include "zmap.h"
#include "zvec.h" // zvec will still use standard malloc!
```

## Notes

### Why do I need to provide a "Short Name"?

In `DEFINE_MAP_TYPE(Key, Val, ShortName)`, you must provide a Short Name.

```c
//              Key    Val   ShortName
DEFINE_MAP_TYPE(char*, int,  StrInt)
```

The reason is that C macros cannot handle special characters (like `*` or space) when generating names. The library tries to create structs and functions by gluing `map_` + `Name`.

If you tried to generate `map_char*`, it would be a syntax error. By passing `StrInt`, it correctly generates `map_StrInt`, `map_put_StrInt`, etc.
