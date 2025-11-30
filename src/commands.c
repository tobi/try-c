// Feature test macros for cross-platform compatibility
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#else
#define _GNU_SOURCE
#endif

#include "commands.h"
#include "tui.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// ============================================================================
// Script building and execution
// ============================================================================

// Build a script and either execute it (direct mode) or print it (exec mode)
// Returns 0 on success, 1 on failure
static int run_script(const char *script, Mode *mode) {
  if (mode->type == MODE_EXEC) {
    // Exec mode: print script with header for alias to eval
    printf(SCRIPT_HEADER);
    printf("%s", script);
    return 0;
  } else {
    // Direct mode: execute via bash, then print cd hint if present
    // We run everything except cd (which can't work in subprocess)
    // Then print the cd command as a hint

    // Find the cd command in the script (looks for "  cd '" since it's indented)
    const char *cd_line = strstr(script, "\n  cd '");
    if (cd_line) {
      cd_line++; // Skip the newline, point to "  cd '"
    }

    // Build script without cd for execution
    Z_CLEANUP(zstr_free) zstr exec_script = zstr_init();
    if (cd_line && cd_line != script) {
      // Copy everything before the cd line
      zstr_cat_len(&exec_script, script, cd_line - script);
    } else if (!cd_line) {
      // No cd, execute whole script
      zstr_cat(&exec_script, script);
    }
    // If cd is the only line, exec_script stays empty

    // Execute the non-cd part if any
    if (zstr_len(&exec_script) > 0) {
      // Remove trailing newlines/whitespace
      while (zstr_len(&exec_script) > 0) {
        char last = zstr_cstr(&exec_script)[zstr_len(&exec_script) - 1];
        if (last == '\n' || last == ' ' || last == '\\') {
          zstr_pop_char(&exec_script);
        } else {
          break;
        }
      }

      Z_CLEANUP(zstr_free) zstr cmd = zstr_from("/usr/bin/env bash -c '");
      // Escape single quotes in script
      const char *p = zstr_cstr(&exec_script);
      while (*p) {
        if (*p == '\'') {
          zstr_cat(&cmd, "'\\''");
        } else {
          zstr_push(&cmd, *p);
        }
        p++;
      }
      zstr_cat(&cmd, "'");

      int rc = system(zstr_cstr(&cmd));
      if (rc != 0) {
        return 1;
      }
    }

    // Print cd hint (extract path from "  cd '/path' && \" format)
    if (cd_line) {
      // Skip leading spaces
      const char *path_start = cd_line;
      while (*path_start == ' ') path_start++;
      // Now at "cd '/path' && \"
      if (strncmp(path_start, "cd '", 4) == 0) {
        path_start += 4; // Skip "cd '"
        const char *path_end = strchr(path_start, '\'');
        if (path_end) {
          printf("cd '%.*s'\n", (int)(path_end - path_start), path_start);
        }
      }
    }

    return 0;
  }
}

// Helper to generate date-prefixed directory name for clone
static zstr make_clone_dirname(const char *url, const char *name) {
  zstr dir_name = zstr_init();

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  char date_prefix[20];
  strftime(date_prefix, sizeof(date_prefix), "%Y-%m-%d", t);
  zstr_cat(&dir_name, date_prefix);
  zstr_cat(&dir_name, "-");

  if (name) {
    zstr_cat(&dir_name, name);
  } else {
    // Extract repo name from URL
    const char *last_slash = strrchr(url, '/');
    const char *repo_name = last_slash ? last_slash + 1 : url;
    const char *dot_git = strstr(repo_name, ".git");

    if (dot_git) {
      zstr_cat_len(&dir_name, repo_name, dot_git - repo_name);
    } else {
      zstr_cat(&dir_name, repo_name);
    }
  }

  return dir_name;
}

// ============================================================================
// Script builders
// ============================================================================

static zstr build_cd_script(const char *path) {
  zstr script = zstr_init();
  zstr_fmt(&script, "touch '%s' && \\\n", path);
  zstr_fmt(&script, "  cd '%s' && \\\n", path);
  zstr_cat(&script, "  true\n");
  return script;
}

static zstr build_mkdir_script(const char *path) {
  zstr script = zstr_init();
  zstr_fmt(&script, "mkdir -p '%s' && \\\n", path);
  zstr_fmt(&script, "  cd '%s' && \\\n", path);
  zstr_cat(&script, "  true\n");
  return script;
}

static zstr build_clone_script(const char *url, const char *path) {
  zstr script = zstr_init();
  zstr_fmt(&script, "git clone '%s' '%s' && \\\n", url, path);
  zstr_fmt(&script, "  cd '%s' && \\\n", path);
  zstr_cat(&script, "  true\n");
  return script;
}

// ============================================================================
// Init command - outputs shell function definition
// ============================================================================

void cmd_init(int argc, char **argv, const char *tries_path) {
  (void)argc;
  (void)argv;

  // Determine if we're in fish shell
  const char *shell = getenv("SHELL");
  bool is_fish = (shell && strstr(shell, "fish") != NULL);

  // Get the path to this executable
  char self_path[1024];
  ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
  if (len == -1) {
    // Fallback for macOS
    strncpy(self_path, "try", sizeof(self_path) - 1);
  } else {
    self_path[len] = '\0';
  }

  if (is_fish) {
    // Fish shell version
    printf(
      "function try\n"
      "  set -l out ('%s' exec --path '%s' $argv 2>/dev/tty)\n"
      "  if test $status -eq 0\n"
      "    eval $out\n"
      "  else\n"
      "    echo $out\n"
      "  end\n"
      "end\n",
      self_path, tries_path);
  } else {
    // Bash/Zsh version
    printf(
      "try() {\n"
      "  local out\n"
      "  out=$('%s' exec --path '%s' \"$@\" 2>/dev/tty)\n"
      "  if [ $? -eq 0 ]; then\n"
      "    eval \"$out\"\n"
      "  else\n"
      "    echo \"$out\"\n"
      "  fi\n"
      "}\n",
      self_path, tries_path);
  }
}

// ============================================================================
// Clone command
// ============================================================================

int cmd_clone(int argc, char **argv, const char *tries_path, Mode *mode) {
  if (argc < 1) {
    fprintf(stderr, "Usage: try clone <url> [name]\n");
    return 1;
  }

  const char *url = argv[0];
  const char *name = (argc > 1) ? argv[1] : NULL;

  Z_CLEANUP(zstr_free) zstr dir_name = make_clone_dirname(url, name);
  Z_CLEANUP(zstr_free) zstr full_path = join_path(tries_path, zstr_cstr(&dir_name));
  Z_CLEANUP(zstr_free) zstr script = build_clone_script(url, zstr_cstr(&full_path));

  return run_script(zstr_cstr(&script), mode);
}

// ============================================================================
// Worktree command
// ============================================================================

int cmd_worktree(int argc, char **argv, const char *tries_path, Mode *mode) {
  (void)argc;
  (void)argv;
  (void)tries_path;
  (void)mode;
  fprintf(stderr, "Worktree command not yet implemented.\n");
  return 1;
}

// ============================================================================
// Selector command (interactive directory picker)
// ============================================================================

int cmd_selector(int argc, char **argv, const char *tries_path, Mode *mode) {
  const char *initial_filter = (argc > 0) ? argv[0] : NULL;

  SelectionResult result = run_selector(tries_path, initial_filter, mode);

  if (result.type == ACTION_CD) {
    Z_CLEANUP(zstr_free) zstr script = build_cd_script(zstr_cstr(&result.path));
    zstr_free(&result.path);
    return run_script(zstr_cstr(&script), mode);
  } else if (result.type == ACTION_MKDIR) {
    Z_CLEANUP(zstr_free) zstr script = build_mkdir_script(zstr_cstr(&result.path));
    zstr_free(&result.path);
    return run_script(zstr_cstr(&script), mode);
  } else {
    // Cancelled
    zstr_free(&result.path);
    printf("Cancelled.\n");
    return 1;
  }
}

// ============================================================================
// Exec mode entry point
// ============================================================================

int cmd_exec(int argc, char **argv, const char *tries_path, Mode *mode) {
  // No subcommand = interactive selector
  if (argc == 0) {
    return cmd_selector(0, NULL, tries_path, mode);
  }

  const char *subcmd = argv[0];

  if (strcmp(subcmd, "cd") == 0) {
    // Explicit cd command
    return cmd_selector(argc - 1, argv + 1, tries_path, mode);
  } else if (strcmp(subcmd, "clone") == 0) {
    return cmd_clone(argc - 1, argv + 1, tries_path, mode);
  } else if (strcmp(subcmd, "worktree") == 0) {
    return cmd_worktree(argc - 1, argv + 1, tries_path, mode);
  } else {
    // Treat as query for selector (cd is default)
    return cmd_selector(argc, argv, tries_path, mode);
  }
}
