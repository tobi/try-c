#ifndef COMMANDS_H
#define COMMANDS_H

void cmd_init(int argc, char **argv);
void cmd_clone(int argc, char **argv, const char *tries_path);
void cmd_worktree(int argc, char **argv, const char *tries_path);
void cmd_cd(int argc, char **argv, const char *tries_path);

#endif // COMMANDS_H
