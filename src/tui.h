#ifndef TUI_H
#define TUI_H

#include <time.h>

typedef enum { ACTION_NONE, ACTION_CD, ACTION_MKDIR, ACTION_CANCEL } ActionType;

typedef struct {
  char name[256];
  char path[1024];
  time_t mtime;
  float score;
} TryEntry;

typedef struct {
  ActionType type;
  char *path;
} SelectionResult;

// Run the interactive selector
// base_path: directory to scan for tries
// initial_filter: initial search term (can be NULL)
SelectionResult run_selector(const char *base_path, const char *initial_filter);

#endif // TUI_H
