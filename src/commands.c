#define _POSIX_C_SOURCE 200809L
#include "commands.h"
#include "tui.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Helper to emit shell commands
void emit_task(const char *type, const char *arg1, const char *arg2) {
  if (strcmp(type, "mkdir") == 0) {
    printf("mkdir -p '%s' \\\n  && ", arg1);
  } else if (strcmp(type, "cd") == 0) {
    printf("cd '%s' \\\n  && ", arg1);
  } else if (strcmp(type, "touch") == 0) {
    printf("touch '%s' \\\n  && ", arg1);
  } else if (strcmp(type, "git-clone") == 0) {
    printf("git clone '%s' '%s' \\\n  && ", arg1, arg2);
  } else if (strcmp(type, "echo") == 0) {
    // Expand tokens for echo
    char *expanded = expand_tokens(arg1);
    printf("echo '%s' \\\n  && ", expanded);
    free(expanded);
  }
}

void cmd_init(int argc, char **argv) {
  (void)argc;
  (void)argv;
  // Simplified init script emission
  const char *script = "try() {\n"
                       "  if [ \"$1\" = \"init\" ]; then\n"
                       "    /usr/local/bin/try \"$@\"\n"
                       "    return\n"
                       "  fi\n"
                       "  tmp=$(mktemp)\n"
                       "  /usr/local/bin/try \"$@\" > \"$tmp\"\n"
                       "  ret=$?\n"
                       "  if [ $ret -eq 0 ]; then\n"
                       "    . \"$tmp\"\n"
                       "  fi\n"
                       "  rm -f \"$tmp\"\n"
                       "  return $ret\n"
                       "}\n";
  printf("%s", script);
}

void cmd_clone(int argc, char **argv, const char *tries_path) {
  if (argc < 1) {
    fprintf(stderr, "Usage: try clone <url> [name]\n");
    exit(1);
  }

  char *url = argv[0];
  char *name = (argc > 1) ? argv[1] : NULL;

  // Generate name if not provided (simplified)
  char dir_name[256];
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  char date_prefix[20];
  strftime(date_prefix, sizeof(date_prefix), "%Y-%m-%d", t);

  if (name) {
    snprintf(dir_name, sizeof(dir_name), "%s-%s", date_prefix, name);
  } else {
    // Extract repo name from URL
    char *last_slash = strrchr(url, '/');
    char *repo_name = last_slash ? last_slash + 1 : url;
    char *dot_git = strstr(repo_name, ".git");
    if (dot_git)
      *dot_git = '\0';
    snprintf(dir_name, sizeof(dir_name), "%s-%s", date_prefix, repo_name);
  }

  char *full_path = join_path(tries_path, dir_name);

  emit_task("echo", "Cloning into {highlight}new try{reset}...", NULL);
  emit_task("mkdir", full_path, NULL);
  emit_task("git-clone", url, full_path);
  emit_task("touch", full_path, NULL); // Update mtime
  emit_task("cd", full_path, NULL);
  printf("true\n"); // End chain

  free(full_path);
}

void cmd_worktree(int argc, char **argv, const char *tries_path) {
  (void)argc;
  (void)argv;
  (void)tries_path;
  // Simplified worktree implementation
  // try worktree [dir] [name]

  // For now, just a placeholder or basic implementation
  fprintf(stderr, "Worktree command not fully implemented in this MVP.\n");
  exit(1);
}

void cmd_cd(int argc, char **argv, const char *tries_path) {
  // If args provided, try to find match or use as filter
  char *initial_filter = NULL;
  if (argc > 0) {
    // Join args
    // For simplicity, just take first arg
    initial_filter = argv[0];
  }

  SelectionResult result = run_selector(tries_path, initial_filter);

  if (result.type == ACTION_CD) {
    emit_task("touch", result.path, NULL); // Update mtime
    emit_task("cd", result.path, NULL);
    printf("true\n");
  } else if (result.type == ACTION_MKDIR) {
    emit_task("mkdir", result.path, NULL);
    emit_task("cd", result.path, NULL);
    printf("true\n");
  } else {
    // Cancelled
    printf("true\n");
  }

  if (result.path)
    free(result.path);
}
