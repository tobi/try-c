# zlist.h

`zlist.h` is a header-only C library providing generic doubly linked lists for C projects using C11 `_Generic` selection and X-Macros for type safety. It includes a C++11 wrapper for mixed codebases.

## Key Features

- **Type Safety**: Compile-time type checking prevents mixing incompatible types
- **O(1) Operations**: Constant-time insertions/deletions at both ends and known positions
- **Safe Iteration**: `list_foreach_safe` allows node removal during traversal
- **C++ Support**: Full class wrapper with RAII and standard iterators
- **Automated Setup**: Z-Scanner tool generates type registrations automatically
- **Header-Only**: No compilation or linking required
- **Custom Allocators**: Supports arenas, pools, and debug allocators
- **Zero Dependencies**: Uses only standard C headers

## Installation

1. Copy `zlist.h` (and `zcommon.h` if separated) to your project's include folder
2. Optionally add the z-core tools via git submodule for scanner support

## C Usage Example

Define structures and register list types using `DEFINE_LIST_TYPE(type, Name)`. Operations include `list_init()`, `list_push_back()`, `list_head()`, and `list_clear()`.

```c
#include <stdio.h>
#include "zlist.h"

// Request the list type you need.
DEFINE_LIST_TYPE(int, Int)

int main(void)
{
    // Initialize (Standard C style).
    list_Int numbers = list_init(Int);

    list_push_back(&numbers, 10);
    list_push_back(&numbers, 20);
    list_push_back(&numbers, 30);

    // Iterate over elements
    list_foreach(Int, &numbers, node) {
        printf("Value: %d\n", node->data);
    }

    // Cleanup.
    list_clear(&numbers);
    return 0;
}
```

## C++ Usage Example

The `z_list::list<T>` template class supports RAII, range-based for loops, and standard STL-compatible iterators. Constructor accepts initializer lists.

```cpp
#include <iostream>
#include "zlist.h"

DEFINE_LIST_TYPE(int, Int)

int main()
{
    // RAII handles memory automatically.
    z_list::list<int> numbers = {10, 20, 30};

    // Range-based for loop.
    for (auto& val : numbers) {
        std::cout << "Value: " << val << "\n";
    }

    return 0;
}
```

## API Reference - Core Operations

### Initialization & Management

| Macro | Description |
| :--- | :--- |
| `list_init(Name)` | Returns an empty list struct. |
| `list_clear(l)` | Frees all nodes and resets the list to empty. |
| `list_size(l)` | Returns the number of elements (`size_t`). |

### Data Access

| Macro | Description |
| :--- | :--- |
| `list_head(l)` | Returns pointer to the first node, or `NULL` if empty. |
| `list_tail(l)` | Returns pointer to the last node, or `NULL` if empty. |
| `list_at(l, index)` | Returns pointer to node at index (O(n) traversal). |

### Modification

| Macro | Description |
| :--- | :--- |
| `list_push_back(l, val)` | Appends element to end. Returns `Z_OK` or `Z_ERR`. |
| `list_push_front(l, val)` | Prepends element to front. Returns `Z_OK` or `Z_ERR`. |
| `list_pop_back(l)` | Removes and returns the last element. |
| `list_pop_front(l)` | Removes and returns the first element. |
| `list_insert_after(l, node, val)` | Inserts value after the specified node. |
| `list_remove_node(l, node)` | Removes the specified node from the list. |

### Iteration

| Macro | Description |
| :--- | :--- |
| `list_foreach(Name, l, node)` | Iterates over all nodes. `node` is the loop variable. |
| `list_foreach_safe(Name, l, node, tmp)` | Safe iteration allowing node removal during traversal. |

**Example:**
```c
// Standard iteration
list_foreach(Int, &numbers, node) {
    printf("%d\n", node->data);
}

// Safe removal during iteration
list_foreach_safe(Int, &numbers, node, tmp) {
    if (node->data < 0) {
        list_remove_node(&numbers, node);
    }
}
```

### Advanced Operations

| Macro | Description |
| :--- | :--- |
| `list_splice(dst, src)` | Moves all nodes from `src` to end of `dst` in O(1) time. |

## API Reference (C++)

The C++ wrapper lives in the **`z_list`** namespace.

### `class z_list::list<T>`

**Constructors & Management**

| Method | Description |
| :--- | :--- |
| `list()` | Default constructor, creates empty list. |
| `list(initializer_list)` | Constructs list from initializer list. |
| `~list()` | Destructor. Automatically calls `list_clear`. |
| `size()` | Returns current number of elements. |
| `empty()` | Returns `true` if size is 0. |
| `clear()` | Removes all elements. |

**Access**

| Method | Description |
| :--- | :--- |
| `front()` | Returns reference to first element. |
| `back()` | Returns reference to last element. |

**Modification**

| Method | Description |
| :--- | :--- |
| `push_back(val)` | Appends element to end. |
| `push_front(val)` | Prepends element to front. |
| `pop_back()` | Removes last element. |
| `pop_front()` | Removes first element. |
| `insert_after(iter, val)` | Inserts value after iterator position. |
| `erase(iter)` | Removes element at iterator position. |
| `splice(other)` | Moves all elements from other list. |

**Iterators**

| Method | Description |
| :--- | :--- |
| `begin()`, `end()` | Bidirectional iterators for range-based loops. |
| `cbegin()`, `cend()` | Const iterators for read-only access. |

## Memory Management

By default, `zlist.h` uses the standard C library functions (`malloc`, `free`).

Override via `Z_MALLOC`, `Z_CALLOC`, `Z_REALLOC`, `Z_FREE` macros in your registry header before including the library.

```c
#ifndef MY_LISTS_H
#define MY_LISTS_H

// Define your custom memory macros **HERE**.
#define Z_MALLOC(sz)      my_custom_alloc(sz)
#define Z_CALLOC(n, sz)   my_custom_calloc(n, sz)
#define Z_REALLOC(p, sz)  my_custom_realloc(p, sz)
#define Z_FREE(p)         my_custom_free(p)

// Then include the library.
#include "zlist.h"

#endif
```

> **Note:** Override all four macros together for consistency.

Library-specific variants (`Z_LIST_MALLOC`, etc.) allow different allocators per container type.

## Extensions (Experimental)

The `list_autofree(Type)` macro (requires GCC/Clang `__attribute__((cleanup))`) automatically frees lists when variables leave scope.

```c
void process_data()
{
    // 'items' will be automatically freed when this function returns.
    list_autofree(Int) items = list_init(Int);
    list_push_back(&items, 100);
}
```

> **Disable Extensions:** Define `Z_NO_EXTENSIONS` before including the library for standard compliance.

## Manual Registry Setup

Without the scanner, create a registry header:

```c
#ifndef MY_LISTS_H
#define MY_LISTS_H

// Register Types.
#define REGISTER_LIST_TYPES(X) \
    X(int, Int)                \
    X(float, Float)

// Include AFTER the macro definition.
#include "zlist.h"

#endif
```
