#define _POSIX_C_SOURCE 200809L
#include "commands.h"

#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_help() {
  printf("Usage: try [command] [args]\n");
  printf("Commands:\n");
  printf("  init             Initialize shell integration\n");
  printf("  clone <url>      Clone a repo into a new try\n");
  printf("  worktree         Create a worktree (not implemented)\n");
  printf("  cd [query]       Interactive selector or cd to query\n");
  printf("  [query]          Shorthand for cd [query]\n");
}

int main(int argc, char **argv) {
  char *tries_path = NULL;
  char **cmd_argv = NULL;
  int cmd_argc = 0;

  // Simple arg parsing to find --path
  // We'll reconstruct argv for the command
  cmd_argv = malloc(sizeof(char *) * argc);

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--path") == 0 && i + 1 < argc) {
      tries_path = strdup(argv[i + 1]);
      i++;
    } else if (strncmp(argv[i], "--path=", 7) == 0) {
      tries_path = strdup(argv[i] + 7);
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      print_help();
      return 0;
    } else {
      cmd_argv[cmd_argc++] = argv[i];
    }
  }

  if (!tries_path) {
    tries_path = get_default_tries_path();
  }

  if (!tries_path) {
    fprintf(stderr,
            "Error: Could not determine tries path. Set HOME or use --path.\n");
    return 1;
  }

  // Ensure tries directory exists
  if (!dir_exists(tries_path)) {
    if (mkdir_p(tries_path) != 0) {
      fprintf(stderr, "Error: Could not create tries directory: %s\n",
              tries_path);
      return 1;
    }
  }

  char *command = (cmd_argc > 0) ? cmd_argv[0] : "cd";

  if (strcmp(command, "init") == 0) {
    cmd_init(cmd_argc - 1, cmd_argv + 1);
  } else if (strcmp(command, "clone") == 0) {
    cmd_clone(cmd_argc - 1, cmd_argv + 1, tries_path);
  } else if (strcmp(command, "worktree") == 0) {
    cmd_worktree(cmd_argc - 1, cmd_argv + 1, tries_path);
  } else if (strcmp(command, "cd") == 0) {
    cmd_cd(cmd_argc - 1, cmd_argv + 1, tries_path);
  } else {
    // Treat as "cd query" if not a known command, or "clone" if it looks like a
    // URL? Ruby script logic: if is_git_uri(arg) -> clone else -> cd

    // For now, let's just default to cd with the command as the query
    // But we need to pass the command itself as an arg to cmd_cd
    cmd_cd(cmd_argc, cmd_argv, tries_path);
  }

  free(tries_path);
  free(cmd_argv);

  return 0;
}
