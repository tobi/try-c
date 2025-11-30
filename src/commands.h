#ifndef COMMANDS_H
#define COMMANDS_H

#include "tui.h"

// Script header for exec mode output
#define SCRIPT_HEADER "# if you can read this, you didn't launch try from an alias. run try --help.\n"

// Init command - outputs shell function definition
void cmd_init(int argc, char **argv, const char *tries_path);

// Clone command (works in both modes)
int cmd_clone(int argc, char **argv, const char *tries_path, Mode *mode);

// Worktree command (works in both modes)
int cmd_worktree(int argc, char **argv, const char *tries_path, Mode *mode);

// Exec mode entry point (routes to selector or subcommands)
int cmd_exec(int argc, char **argv, const char *tries_path, Mode *mode);

// Interactive selector
int cmd_selector(int argc, char **argv, const char *tries_path, Mode *mode);

#endif // COMMANDS_H
