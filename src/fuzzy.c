#include "fuzzy.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

float calculate_score(const char *text, const char *query, time_t mtime) {
  float score = 0.0;

  // Date prefix bonus (YYYY-MM-DD-)
  if (strlen(text) >= 11 && isdigit(text[0]) && isdigit(text[1]) &&
      isdigit(text[2]) && isdigit(text[3]) && text[4] == '-' &&
      isdigit(text[5]) && isdigit(text[6]) && text[7] == '-' &&
      isdigit(text[8]) && isdigit(text[9]) && text[10] == '-') {
    score += 2.0;
  }

  // If there's a search query, calculate match score
  if (query && strlen(query) > 0) {
    char *text_lower = strdup(text);
    char *query_lower = strdup(query);

    for (int i = 0; text_lower[i]; i++)
      text_lower[i] = tolower(text_lower[i]);
    for (int i = 0; query_lower[i]; i++)
      query_lower[i] = tolower(query_lower[i]);

    int query_len = strlen(query_lower);
    int text_len = strlen(text_lower);
    int query_idx = 0;
    int last_pos = -1;

    // Fuzzy match: find each query character in sequence
    for (int pos = 0; pos < text_len; pos++) {
      if (query_idx >= query_len)
        break;

      if (text_lower[pos] == query_lower[query_idx]) {
        // Base point for match
        score += 1.0;

        // Word boundary bonus
        if (pos == 0 || !isalnum(text_lower[pos - 1])) {
          score += 1.0;
        }

        // Proximity bonus: 1/sqrt(distance)
        if (last_pos >= 0) {
          int gap = pos - last_pos - 1;
          score += 1.0 / sqrt(gap + 1);
        }

        last_pos = pos;
        query_idx++;
      }
    }

    free(text_lower);
    free(query_lower);

    // Return 0 if not all query chars matched
    if (query_idx < query_len)
      return 0.0;

    // Density bonus - prefer shorter matches
    if (last_pos >= 0) {
      score *= ((float)query_len / (last_pos + 1));
    }

    // Length penalty - shorter text scores higher for same match
    score *= (10.0 / (text_len + 10.0));
  }

  // Time-based scoring (newer is better)
  time_t now = time(NULL);
  double age = difftime(now, mtime);
  if (age < 3600)
    score += 0.5; // Last hour
  else if (age < 86400)
    score += 0.3; // Last day
  else if (age < 604800)
    score += 0.1; // Last week

  return score;
}

char *highlight_matches(const char *text, const char *query) {
  if (!query || strlen(query) == 0)
    return strdup(text);

  int text_len = strlen(text);
  // Allocate enough space: each char can become "{highlight}X{text}" = 20 chars
  char *result = malloc(text_len * 25 + 1);
  if (!result)
    return strdup(text);

  char *text_lower = strdup(text);
  char *query_lower = strdup(query);
  if (!text_lower || !query_lower) {
    free(result);
    free(text_lower);
    free(query_lower);
    return strdup(text);
  }

  for (int i = 0; text_lower[i]; i++)
    text_lower[i] = tolower(text_lower[i]);
  for (int i = 0; query_lower[i]; i++)
    query_lower[i] = tolower(query_lower[i]);

  int query_len = strlen(query_lower);
  int query_idx = 0;
  int result_pos = 0;

  for (int i = 0; text[i]; i++) {
    if (query_idx < query_len && text_lower[i] == query_lower[query_idx]) {
      // Copy {highlight}
      strcpy(result + result_pos, "{highlight}");
      result_pos += 11;
      // Copy character
      result[result_pos++] = text[i];
      // Copy {text}
      strcpy(result + result_pos, "{text}");
      result_pos += 6;
      query_idx++;
    } else {
      result[result_pos++] = text[i];
    }
  }
  result[result_pos] = '\0';

  free(text_lower);
  free(query_lower);
  return result;
}
