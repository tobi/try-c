#ifndef TUI_H
#define TUI_H

#include "zstr.h"
#include <stdbool.h>
#include <time.h>

typedef enum { ACTION_NONE, ACTION_CD, ACTION_MKDIR, ACTION_CANCEL } ActionType;

typedef struct {
  zstr path;     // Full path
  zstr name;     // Directory name
  zstr rendered; // Pre-rendered string with tokens
  time_t mtime;
  float score;
} TryEntry;

typedef struct {
  ActionType type;
  zstr path;
} SelectionResult;

// Execution mode
typedef enum {
  MODE_DIRECT,  // Direct invocation (immediate execution, print cd hint)
  MODE_EXEC     // Via alias (return shell script)
} ModeType;

// Mode configuration
typedef struct {
  ModeType type;
  // Test mode options (orthogonal to type)
  bool render_once;         // Render once and exit (--and-exit)
  const char *inject_keys;  // Keys to inject (--and-keys)
  int key_index;            // Current position in inject_keys
} Mode;

// Run the interactive selector
// base_path: directory to scan for tries
// initial_filter: initial search term (can be NULL)
// mode: execution mode configuration
SelectionResult run_selector(const char *base_path, const char *initial_filter,
                             Mode *mode);

#endif // TUI_H
