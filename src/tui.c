#define _POSIX_C_SOURCE 200809L
#include "tui.h"
#include "fuzzy.h"
#include "terminal.h"
#include "utils.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MAX_TRIES 1000

typedef struct {
  TryEntry *entries;
  int count;
  int capacity;
} TryList;

static TryList all_tries = {0};
static TryList filtered_tries = {0};
static char filter_buffer[256] = {0};
static int selected_index = 0;
static int scroll_offset = 0;

static void free_try_list(TryList *list) {
  free(list->entries);
  list->entries = NULL;
  list->count = 0;
  list->capacity = 0;
}

static int compare_tries_by_score(const void *a, const void *b) {
  const TryEntry *ta = (const TryEntry *)a;
  const TryEntry *tb = (const TryEntry *)b;
  if (ta->score > tb->score)
    return -1;
  if (ta->score < tb->score)
    return 1;
  return 0;
}

static void scan_tries(const char *base_path) {
  free_try_list(&all_tries);

  DIR *d = opendir(base_path);
  if (!d)
    return;

  all_tries.capacity = 128;
  all_tries.entries = malloc(sizeof(TryEntry) * all_tries.capacity);
  all_tries.count = 0;

  struct dirent *dir;
  while ((dir = readdir(d)) != NULL) {
    if (dir->d_name[0] == '.')
      continue;

    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", base_path, dir->d_name);

    struct stat sb;
    if (stat(full_path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
      if (all_tries.count >= all_tries.capacity) {
        all_tries.capacity *= 2;
        all_tries.entries =
            realloc(all_tries.entries, sizeof(TryEntry) * all_tries.capacity);
      }

      TryEntry *entry = &all_tries.entries[all_tries.count];
      strncpy(entry->name, dir->d_name, sizeof(entry->name) - 1);
      strncpy(entry->path, full_path, sizeof(entry->path) - 1);
      entry->mtime = sb.st_mtime;
      entry->score = calculate_score(entry->name, "", entry->mtime);
      all_tries.count++;
    }
  }
  closedir(d);
}

static void filter_tries() {
  free_try_list(&filtered_tries);

  filtered_tries.capacity = all_tries.count + 1;
  filtered_tries.entries = malloc(sizeof(TryEntry) * filtered_tries.capacity);
  filtered_tries.count = 0;

  for (int i = 0; i < all_tries.count; i++) {
    TryEntry *entry = &all_tries.entries[i];
    entry->score = calculate_score(entry->name, filter_buffer, entry->mtime);

    if (strlen(filter_buffer) > 0 && entry->score <= 0.0) {
      continue;
    }

    filtered_tries.entries[filtered_tries.count++] = *entry;
  }

  qsort(filtered_tries.entries, filtered_tries.count, sizeof(TryEntry),
        compare_tries_by_score);

  if (selected_index >= filtered_tries.count) {
    selected_index = 0;
  }
}

static void render(const char *base_path) {
  int rows, cols;
  get_window_size(&rows, &cols);

  write(STDERR_FILENO, "\x1b[?25l", 6); // Hide cursor
  write(STDERR_FILENO, "\x1b[H", 3);    // Home

  // Separator line
  char sep_line[512] = {0};
  for (int i = 0; i < cols && i < 300; i++)
    strcat(sep_line, "─");

  // Header
  char header[1024];
  snprintf(header, sizeof(header),
           "{h1}📁 Try Directory "
           "Selection{reset}\x1b[K\r\n{dim_text}%s{reset}\x1b[K\r\n",
           sep_line);
  char *exp = expand_tokens(header);
  write(STDERR_FILENO, exp, strlen(exp));
  free(exp);

  // Search bar
  char search[1024];
  snprintf(
      search, sizeof(search),
      "{highlight}Search:{reset} %s\x1b[K\r\n{dim_text}%s{reset}\x1b[K\r\n",
      filter_buffer, sep_line);
  exp = expand_tokens(search);
  write(STDERR_FILENO, exp, strlen(exp));
  free(exp);

  // List
  int list_height = rows - 8;
  if (list_height < 1)
    list_height = 1;

  if (selected_index < scroll_offset)
    scroll_offset = selected_index;
  if (selected_index >= scroll_offset + list_height)
    scroll_offset = selected_index - list_height + 1;

  for (int i = 0; i < list_height; i++) {
    int idx = scroll_offset + i;

    if (idx < filtered_tries.count) {
      TryEntry *entry = &filtered_tries.entries[idx];
      int is_selected = (idx == selected_index);

      char *rel_time = format_relative_time(entry->mtime);
      char meta[64];
      snprintf(meta, sizeof(meta), "%s, %.1f", rel_time, entry->score);
      free(rel_time);

      // Check for date prefix
      int has_date = (strlen(entry->name) >= 11 && isdigit(entry->name[0]) &&
                      isdigit(entry->name[3]) && entry->name[4] == '-' &&
                      entry->name[7] == '-' && entry->name[10] == '-');

      // Calculate padding based on ORIGINAL name length (not highlighted)
      int plain_name_len = strlen(entry->name);
      int meta_len = strlen(meta);
      int padding_len = cols - 5 - plain_name_len - meta_len;
      if (padding_len < 1)
        padding_len = 1;

      char padding[256];
      memset(padding, ' ', padding_len < 255 ? padding_len : 255);
      padding[padding_len < 255 ? padding_len : 255] = '\0';

      char line[2048];

      // Render based on selection and date status
      if (is_selected) {
        if (has_date) {
          // Selected with date: dim date part, highlight separator
          // Apply highlighting to the name part only (after date)
          char *name_part = entry->name + 11;
          if (strlen(filter_buffer) > 0) {
            char *highlighted_name =
                highlight_matches(name_part, filter_buffer);
            snprintf(line, sizeof(line),
                     "{highlight}→ {reset}📁 "
                     "{start_selected}{dim_text}%.10s{reset_fg}{highlight}-{"
                     "reset_fg}%s{end_selected}%s{dim_text}%s{reset}\x1b[K\r\n",
                     entry->name, highlighted_name, padding, meta);
            free(highlighted_name);
          } else {
            snprintf(line, sizeof(line),
                     "{highlight}→ {reset}📁 "
                     "{start_selected}{dim_text}%.10s{reset_fg}{highlight}-{"
                     "reset_fg}%s{end_selected}%s{dim_text}%s{reset}\x1b[K\r\n",
                     entry->name, name_part, padding, meta);
          }
        } else {
          // Selected without date: highlight entire name
          if (strlen(filter_buffer) > 0) {
            char *highlighted = highlight_matches(entry->name, filter_buffer);
            snprintf(line, sizeof(line),
                     "{highlight}→ {reset}📁 "
                     "{start_selected}%s{end_selected}%s{dim_text}%s{reset}"
                     "\x1b[K\r\n",
                     highlighted, padding, meta);
            free(highlighted);
          } else {
            snprintf(line, sizeof(line),
                     "{highlight}→ {reset}📁 "
                     "{start_selected}%s{end_selected}%s{dim_text}%s{reset}"
                     "\x1b[K\r\n",
                     entry->name, padding, meta);
          }
        }
      } else {
        if (has_date) {
          // Not selected with date: dim date and separator
          char *name_part = entry->name + 11;
          if (strlen(filter_buffer) > 0) {
            char *highlighted_name =
                highlight_matches(name_part, filter_buffer);
            snprintf(
                line, sizeof(line),
                "  📁 "
                "{dim_text}%.10s-{reset_fg}%s%s{dim_text}%s{reset}\x1b[K\r\n",
                entry->name, highlighted_name, padding, meta);
            free(highlighted_name);
          } else {
            snprintf(
                line, sizeof(line),
                "  📁 "
                "{dim_text}%.10s-{reset_fg}%s%s{dim_text}%s{reset}\x1b[K\r\n",
                entry->name, name_part, padding, meta);
          }
        } else {
          // Not selected without date: regular rendering
          if (strlen(filter_buffer) > 0) {
            char *highlighted = highlight_matches(entry->name, filter_buffer);
            snprintf(line, sizeof(line),
                     "  📁 %s%s{dim_text}%s{reset}\x1b[K\r\n", highlighted,
                     padding, meta);
            free(highlighted);
          } else {
            snprintf(line, sizeof(line),
                     "  📁 %s%s{dim_text}%s{reset}\x1b[K\r\n", entry->name,
                     padding, meta);
          }
        }
      }

      exp = expand_tokens(line);
      write(STDERR_FILENO, exp, strlen(exp));
      free(exp);

    } else if (idx == filtered_tries.count && strlen(filter_buffer) > 0) {
      if (idx == selected_index) {
        char line[512];
        snprintf(line, sizeof(line),
                 "{highlight}→ {reset}+ Create new: %s\x1b[K\r\n",
                 filter_buffer);
        exp = expand_tokens(line);
        write(STDERR_FILENO, exp, strlen(exp));
        free(exp);
      } else {
        fprintf(stderr, "  + Create new: %s\x1b[K\r\n", filter_buffer);
      }
    } else {
      write(STDERR_FILENO, "\x1b[K\r\n", 5);
    }
  }

  write(STDERR_FILENO, "\x1b[J", 3); // Clear rest

  // Footer
  char footer[1024];
  snprintf(footer, sizeof(footer),
           "{dim_text}%s{reset}\x1b[K\r\n{dim_text}↑/↓: Navigate  Enter: "
           "Select  ESC: Cancel{reset}\x1b[K\r\n",
           sep_line);
  exp = expand_tokens(footer);
  write(STDERR_FILENO, exp, strlen(exp));
  free(exp);

  (void)base_path; // Unused
}

SelectionResult run_selector(const char *base_path,
                             const char *initial_filter) {
  if (initial_filter) {
    strncpy(filter_buffer, initial_filter, sizeof(filter_buffer) - 1);
  }

  scan_tries(base_path);
  filter_tries();

  enable_raw_mode();
  clear_screen();

  SelectionResult result = {.type = ACTION_CANCEL, .path = NULL};

  while (1) {
    render(base_path);

    int c = read_key();
    if (c == -1)
      break;

    if (c == ESC_KEY || c == 3) {
      break;
    } else if (c == ENTER_KEY) {
      if (selected_index < filtered_tries.count) {
        result.type = ACTION_CD;
        result.path = strdup(filtered_tries.entries[selected_index].path);
      } else {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char date_prefix[20];
        strftime(date_prefix, sizeof(date_prefix), "%Y-%m-%d", t);

        char new_name[512];
        snprintf(new_name, sizeof(new_name), "%s-%s", date_prefix,
                 filter_buffer);

        for (int i = 0; new_name[i]; i++) {
          if (isspace(new_name[i]))
            new_name[i] = '-';
        }

        result.type = ACTION_MKDIR;
        result.path = join_path(base_path, new_name);
      }
      break;
    } else if (c == ARROW_UP) {
      if (selected_index > 0)
        selected_index--;
    } else if (c == ARROW_DOWN) {
      int max_idx = filtered_tries.count;
      if (strlen(filter_buffer) > 0)
        max_idx++;
      if (selected_index < max_idx - 1)
        selected_index++;
    } else if (c == BACKSPACE || c == 127) {
      size_t len = strlen(filter_buffer);
      if (len > 0) {
        filter_buffer[len - 1] = '\0';
        filter_tries();
      }
    } else if (!iscntrl(c) && c < 128) {
      size_t len = strlen(filter_buffer);
      if (len < sizeof(filter_buffer) - 1) {
        filter_buffer[len] = c;
        filter_buffer[len + 1] = '\0';
        filter_tries();
      }
    }
  }

  disable_raw_mode();
  fprintf(stderr, "\n");

  free_try_list(&all_tries);
  free_try_list(&filtered_tries);

  return result;
}
