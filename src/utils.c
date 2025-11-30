#define _POSIX_C_SOURCE 200809L
#include "utils.h"
#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

char *expand_tokens(const char *text) {
  // Simple replacement for now, can be optimized
  // Supported tokens: {h1}, {h2}, {highlight}, {text}, {dim_text}, {reset},
  // {reset_fg} Note: In C, we'll do a simple multi-pass or single-pass
  // replacement. For simplicity/speed in this MVP, let's just return a strdup
  // if no tokens, or implement a basic replacer.

  // Actually, let's implement a dynamic buffer builder
  size_t size = strlen(text) * 2 + 100; // Rough estimate
  char *buffer = malloc(size);
  char *out = buffer;
  const char *in = text;

  while (*in) {
    if (*in == '{') {
      if (strncmp(in, "{h1}", 4) == 0) {
        out += sprintf(out, "%s%s", ANSI_BOLD, ANSI_MAGENTA);
        in += 4;
      } else if (strncmp(in, "{h2}", 4) == 0) {
        out += sprintf(out, "%s%s", ANSI_BOLD, ANSI_BLUE);
        in += 4;
      } else if (strncmp(in, "{highlight}", 11) == 0) {
        out += sprintf(out, "%s%s", ANSI_BOLD, ANSI_YELLOW);
        in += 11;
      } else if (strncmp(in, "{text}", 6) == 0) {
        out += sprintf(out, "%s", ANSI_RESET); // basic reset for now
      } else if (strncmp(in, "{dim_text}", 10) == 0) {
        out += sprintf(out, "%s", ANSI_DIM);
        in += 10;
      } else if (strncmp(in, "{reset}", 7) == 0) {
        out += sprintf(out, "%s", ANSI_RESET);
        in += 7;
      } else if (strncmp(in, "{reset_fg}", 10) == 0) {
        out += sprintf(out, "%s", ANSI_RESET); // Close enough
        in += 10;
      } else if (strncmp(in, "{text}", 6) == 0) {
        out += sprintf(out, "%s", ANSI_RESET);
        in += 6;
      } else if (strncmp(in, "{start_selected}", 16) == 0) {
        out += sprintf(out, "%s", ANSI_BOLD);
        in += 16;
      } else if (strncmp(in, "{end_selected}", 14) == 0) {
        out += sprintf(out, "%s", ANSI_RESET);
        in += 14;
      } else {
        *out++ = *in++;
      }
    } else {
      *out++ = *in++;
    }

    // Check bounds (crude realloc if needed, but for now assume enough)
    if (out - buffer > (long)size - 50) {
      size *= 2;
      long offset = out - buffer;
      buffer = realloc(buffer, size);
      out = buffer + offset;
    }
  }
  *out = '\0';
  return buffer;
}

char *trim(char *str) {
  char *end;
  while (isspace((unsigned char)*str))
    str++;
  if (*str == 0)
    return str;
  end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end))
    end--;
  end[1] = '\0';
  return str;
}

char *get_home_dir() {
  const char *home = getenv("HOME");
  if (!home) {
    struct passwd *pw = getpwuid(getuid());
    if (pw)
      home = pw->pw_dir;
  }
  return home ? strdup(home) : NULL;
}

char *join_path(const char *dir, const char *file) {
  size_t len = strlen(dir) + strlen(file) + 2;
  char *path = malloc(len);
  sprintf(path, "%s/%s", dir, file);
  return path;
}

char *get_default_tries_path() {
  char *home = get_home_dir();
  if (!home)
    return NULL;
  char *path = join_path(home, DEFAULT_TRIES_PATH_SUFFIX);
  free(home);
  return path;
}

bool dir_exists(const char *path) {
  struct stat sb;
  return (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode));
}

bool file_exists(const char *path) {
  struct stat sb;
  return (stat(path, &sb) == 0 && S_ISREG(sb.st_mode));
}

int mkdir_p(const char *path) {
  // System call is easiest for mkdir -p equivalent in portable C without
  // libraries But let's try to do it manually or just use system("mkdir -p
  // ...") for simplicity in this task? The requirement is "dependency free
  // modern c project". calling system() is standard C. However, implementing it
  // is better.

  char tmp[1024];
  char *p = NULL;
  size_t len;

  snprintf(tmp, sizeof(tmp), "%s", path);
  len = strlen(tmp);
  if (tmp[len - 1] == '/')
    tmp[len - 1] = 0;

  for (p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = 0;
      if (mkdir(tmp, S_IRWXU) != 0 && errno != EEXIST) {
        return -1;
      }
      *p = '/';
    }
  }
  if (mkdir(tmp, S_IRWXU) != 0 && errno != EEXIST) {
    return -1;
  }
  return 0;
}

char *format_relative_time(time_t mtime) {
  time_t now = time(NULL);
  double diff = difftime(now, mtime);
  char *buf = malloc(64);

  if (diff < 60) {
    sprintf(buf, "just now");
  } else if (diff < 3600) {
    sprintf(buf, "%dm ago", (int)(diff / 60));
  } else if (diff < 86400) {
    sprintf(buf, "%dh ago", (int)(diff / 3600));
  } else {
    sprintf(buf, "%dd ago", (int)(diff / 86400));
  }
  return buf;
}
