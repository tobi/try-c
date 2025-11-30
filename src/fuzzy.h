#ifndef FUZZY_H
#define FUZZY_H

#include <time.h>

// Calculate fuzzy match score for a text against a query
// Returns 0.0 if no match, higher score for better matches
float calculate_score(const char *text, const char *query, time_t mtime);

// Highlight matching characters in text with {highlight} tokens
// Caller must free the returned string
char *highlight_matches(const char *text, const char *query);

#endif // FUZZY_H
