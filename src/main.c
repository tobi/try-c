// Feature test macros for cross-platform compatibility
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#else
#define _GNU_SOURCE
#endif

#include "commands.h"
#include "config.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Compact help for direct mode
static void print_help(void) {
  Z_CLEANUP(zstr_free) zstr default_path = get_default_tries_path();

  Z_CLEANUP(zstr_free) zstr help = zstr_from(
    "{h1}try{/} v" TRY_VERSION " - ephemeral workspace manager\n\n"
    "{h1}To use try, add to your shell config:{/}\n\n"
    "  {bright:blue}# bash/zsh (~/.bashrc or ~/.zshrc){/}\n"
    "  eval \"$(try init ~/src/tries)\"\n\n"
    "  {bright:blue}# fish (~/.config/fish/config.fish){/}\n"
    "  eval (try init ~/src/tries | string collect)\n\n"
    "{h1}Commands:{/}\n"
    "  {b}try{/} [query|url]      {dim}Interactive selector, or clone if URL{/}\n"
    "  {b}try clone{/} <url>      {dim}Clone repo into dated directory{/}\n"
    "  {b}try worktree{/} <name>  {dim}Create worktree from current git repo{/}\n"
    "  {b}try exec{/} [query]     {dim}Output shell script (for manual eval){/}\n"
    "  {b}try --help{/}           {dim}Show this help{/}\n\n"
    "{h1}Defaults:{/}\n"
    "  Path: {b}~/src/tries{/} (override with {b}--path{/} on init)\n"
    "  Current: {b}");
  zstr_cat(&help, zstr_cstr(&default_path));
  zstr_cat(&help, "{/}\n\n"
    "{h1}Examples:{/}\n"
    "  try clone https://github.com/user/repo.git       {bright:blue}# YYYY-MM-DD-user-repo{/}\n"
    "  try clone https://github.com/user/repo.git foo   {bright:blue}# YYYY-MM-DD-foo{/}\n"
    "  try https://github.com/user/repo.git             {bright:blue}# shorthand for clone{/}\n"
    "  try ./my-project worktree feature                {bright:blue}# YYYY-MM-DD-feature{/}\n");

  Z_CLEANUP(zstr_free) zstr expanded = zstr_expand_tokens(zstr_cstr(&help));
  fprintf(stderr, "%s", zstr_cstr(&expanded));
}

// Parse a --flag=value or --flag value option, returns value or NULL
// Sets *skip to 1 if value was in next arg (so caller can skip it)
static const char *parse_option_value(const char *arg, const char *next_arg,
                                       const char *flag, int *skip) {
  size_t flag_len = strlen(flag);
  *skip = 0;

  // Check --flag=value form
  if (strncmp(arg, flag, flag_len) == 0 && arg[flag_len] == '=') {
    return arg + flag_len + 1;
  }

  // Check --flag value form
  if (strcmp(arg, flag) == 0 && next_arg != NULL) {
    *skip = 1;
    return next_arg;
  }

  return NULL;
}

int main(int argc, char **argv) {
  Z_CLEANUP(zstr_free) zstr tries_path = zstr_init();
  Z_CLEANUP(vec_free_char_ptr) vec_char_ptr cmd_args = vec_init_capacity_char_ptr(argc);

  // Check NO_COLOR environment variable (https://no-color.org/)
  if (getenv("NO_COLOR") != NULL) {
    zstr_no_colors = true;
  }

  // Mode configuration
  Mode mode = {.type = MODE_DIRECT};

  // Parse arguments - options can appear anywhere
  for (int i = 1; i < argc; i++) {
    const char *arg = argv[i];
    const char *next = (i + 1 < argc) ? argv[i + 1] : NULL;
    const char *value;
    int skip = 0;

    // Boolean flags
    if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
      print_help();
      return 0;
    }
    if (strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0) {
      printf("try %s\n", TRY_VERSION);
      return 0;
    }
    if (strcmp(arg, "--no-colors") == 0) {
      zstr_no_colors = true;
      continue;
    }
    if (strcmp(arg, "--no-expand-tokens") == 0) {
      zstr_disable_token_expansion = true;
      continue;
    }
    if (strcmp(arg, "--and-exit") == 0) {
      mode.render_once = true;
      continue;
    }

    // Options with values
    if ((value = parse_option_value(arg, next, "--path", &skip))) {
      zstr_free(&tries_path);
      tries_path = zstr_from(value);
      i += skip;
      continue;
    }
    if ((value = parse_option_value(arg, next, "--and-keys", &skip))) {
      mode.inject_keys = value;
      i += skip;
      continue;
    }

    // Positional argument
    vec_push_char_ptr(&cmd_args, argv[i]);
  }

  // Default tries path
  if (zstr_is_empty(&tries_path)) {
    tries_path = get_default_tries_path();
  }

  if (zstr_is_empty(&tries_path)) {
    fprintf(stderr, "Error: Could not determine tries path. Set HOME or use --path.\n");
    return 1;
  }

  const char *path_cstr = zstr_cstr(&tries_path);

  // Ensure tries directory exists
  if (!dir_exists(path_cstr)) {
    if (mkdir_p(path_cstr) != 0) {
      fprintf(stderr, "Error: Could not create tries directory: %s\n", path_cstr);
      return 1;
    }
  }

  // No command = show help (direct mode)
  if (cmd_args.length == 0) {
    print_help();
    return 0;
  }

  const char *command = *vec_at_char_ptr(&cmd_args, 0);

  // Route commands
  if (strcmp(command, "init") == 0) {
    cmd_init((int)cmd_args.length - 1, cmd_args.data + 1, path_cstr);
    return 0;
  } else if (strcmp(command, "exec") == 0) {
    // Exec mode
    mode.type = MODE_EXEC;
    return cmd_exec((int)cmd_args.length - 1, cmd_args.data + 1, path_cstr, &mode);
  } else if (strcmp(command, "cd") == 0) {
    // Direct mode cd (interactive selector)
    return cmd_selector((int)cmd_args.length - 1, cmd_args.data + 1, path_cstr, &mode);
  } else if (strcmp(command, "clone") == 0) {
    // Direct mode clone
    return cmd_clone((int)cmd_args.length - 1, cmd_args.data + 1, path_cstr, &mode);
  } else if (strcmp(command, "worktree") == 0) {
    // Direct mode worktree
    return cmd_worktree((int)cmd_args.length - 1, cmd_args.data + 1, path_cstr, &mode);
  } else if (strncmp(command, "https://", 8) == 0 ||
             strncmp(command, "http://", 7) == 0 ||
             strncmp(command, "git@", 4) == 0) {
    // URL shorthand for clone: try <url> = try clone <url>
    return cmd_clone((int)cmd_args.length, cmd_args.data, path_cstr, &mode);
  } else {
    // Unknown command - show help
    fprintf(stderr, "Unknown command: %s\n\n", command);
    print_help();
    return 1;
  }
}
