# Command Line Specification

## Synopsis

```
try [options] [command] [args...]
try exec [options] [command] [args...]
```

## Description

`try` is an ephemeral workspace manager that helps organize project directories with date-prefixed naming. It provides an interactive selector for navigating between workspaces and commands for creating new ones.

## Global Options

| Option | Description |
|--------|-------------|
| `--help`, `-h` | Show help text |
| `--version`, `-v` | Show version number |
| `--path <dir>` | Override tries directory (default: `~/src/tries`) |

## Commands

### cd (default)

Interactive directory selector with fuzzy search.

```
try cd [query]
try exec cd [query]
try exec [query]        # equivalent to: try exec cd [query]
```

**Arguments:**
- `query` (optional): Initial filter text for fuzzy search

**Behavior:**
- Opens interactive TUI for directory selection
- Filters directories by query if provided
- Returns shell script to cd into selected directory

**Actions:**
- Select existing directory → touch and cd
- Select "[new]" entry → mkdir and cd (creates `YYYY-MM-DD-query`)
- Press Esc → cancel (exit 1)

### clone

Clone a git repository into a dated directory.

```
try clone <url> [name]
try exec clone <url> [name]
```

**Arguments:**
- `url` (required): Git repository URL
- `name` (optional): Custom name suffix (default: extracted from URL)

**Behavior:**
- Creates directory named `YYYY-MM-DD-<name>`
- Clones repository into that directory
- Returns shell script to cd into cloned directory

**Examples:**
```
try clone https://github.com/user/repo
# Creates: 2025-11-30-repo

try clone https://github.com/user/repo myproject
# Creates: 2025-11-30-myproject
```

### worktree

Create a git worktree in a dated directory.

```
try worktree <name>
try exec worktree <name>
```

**Arguments:**
- `name` (required): Branch or worktree name

**Behavior:**
- Must be run from within a git repository
- Creates worktree in `YYYY-MM-DD-<name>`
- Returns shell script to cd into worktree

### init

Output shell function definition for shell integration.

```
try init [path]
```

**Arguments:**
- `path` (optional): Override default tries directory

**Behavior:**
- Detects current shell (bash/zsh or fish)
- Outputs appropriate function definition to stdout
- Function wraps `try exec` and evals output

**Usage:**
```bash
# bash/zsh
eval "$(try init ~/src/tries)"

# fish
eval (try init ~/src/tries | string collect)
```

## Execution Modes

### Direct Mode

When `try` is invoked without `exec`:

- Commands execute immediately
- Cannot change parent shell's directory
- Prints cd hint for user to copy/paste

```
$ try clone https://github.com/user/repo
Cloning into '/home/user/src/tries/2025-11-30-repo'...
cd '/home/user/src/tries/2025-11-30-repo'
```

### Exec Mode

When `try exec` is used (typically via shell alias):

- Returns shell script to stdout
- Exit code 0: alias evals output (performs cd)
- Exit code 1: alias prints output (error/cancel message)

```
$ try exec clone https://github.com/user/repo
# if you can read this, you didn't launch try from an alias. run try --help.
git clone 'https://github.com/user/repo' '/home/user/src/tries/2025-11-30-repo' && \
  cd '/home/user/src/tries/2025-11-30-repo' && \
  true
```

## Script Output Format

All exec mode commands output shell scripts:

```bash
# if you can read this, you didn't launch try from an alias. run try --help.
<command> && \
  cd '<path>' && \
  true
```

The warning comment helps users who accidentally run `try exec` directly.

## Exit Codes

| Code | Meaning | Alias Action |
|------|---------|--------------|
| 0 | Success | Eval output |
| 1 | Error or cancelled | Print output |

## Environment

| Variable | Description |
|----------|-------------|
| `HOME` | Used to resolve default tries path (`$HOME/src/tries`) |
| `SHELL` | Used by `init` to detect shell type |

## Defaults

- **Tries directory**: `~/src/tries`
- **Date format**: `YYYY-MM-DD`
- **Directory naming**: `YYYY-MM-DD-<name>`

---

## Testing and Debugging

> **Note**: The following options are for automated testing and debugging only.
> They are not part of the public interface and may change without notice.

### Test Options

| Option | Description |
|--------|-------------|
| `--and-exit` | Render TUI once and exit immediately (exit code 1) |
| `--and-keys=<keys>` | Inject key sequence into TUI, then exit |
| `--no-expand-tokens` | Disable ANSI token expansion (outputs raw `{b}`, `{dim}`, etc.) |

### Test Option Details

**`--and-exit`**

Renders the TUI once without waiting for input. Useful for testing rendering output.

```bash
./try --path=/tmp/test --and-exit exec 2>&1
```

**`--and-keys=<keys>`**

Injects a sequence of keys as if typed by user. Supports escape sequences:
- `\x1b` - Escape key
- `\r` - Enter key
- `\x1b[A` - Up arrow
- `\x1b[B` - Down arrow

```bash
# Type "beta" then press Enter
./try --path=/tmp/test --and-keys="beta"$'\r' exec

# Press Escape to cancel
./try --path=/tmp/test --and-keys=$'\x1b' exec

# Navigate down then select
./try --path=/tmp/test --and-keys=$'\x1b[B\r' exec
```

**`--no-expand-tokens`**

Outputs formatting tokens as literal text instead of ANSI codes. Useful for testing token placement.

```bash
./try --no-expand-tokens --help
# Output contains: {h1}try{reset} instead of ANSI codes
```

---

## Examples

```bash
# Set up shell integration
eval "$(try init)"

# Interactive selector
try

# Selector with initial filter
try project

# Clone a repository
try clone https://github.com/user/repo

# Clone with custom name
try clone https://github.com/user/repo my-fork

# Create git worktree (from within a repo)
try worktree feature-branch

# Show version
try --version

# Show help
try --help
```
