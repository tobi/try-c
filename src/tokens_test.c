/*
 * Token system unit tests
 * Uses acutest: https://github.com/mity/acutest
 */

#include "acutest.h"
#include "tokens.h"
#include <string.h>

/* Helper to check if a string contains a substring */
static int contains(const char *haystack, const char *needle) {
    return strstr(haystack, needle) != NULL;
}

/* Helper to count occurrences of a substring */
static int count_occurrences(const char *haystack, const char *needle) {
    int count = 0;
    const char *p = haystack;
    size_t needle_len = strlen(needle);
    while ((p = strstr(p, needle)) != NULL) {
        count++;
        p += needle_len;
    }
    return count;
}

/*
 * Test: {b} token should emit just bold, not bold+yellow
 */
void test_b_is_just_bold(void) {
    zstr result = expand_tokens("{b}test{/}");
    const char *s = zstr_cstr(&result);
    
    /* Should contain bold code */
    TEST_CHECK(contains(s, "\033[1m") || contains(s, "\033[0;1m"));
    /* Should NOT contain yellow code 33 */
    TEST_CHECK(!contains(s, ";33m") && !contains(s, "[33m"));
    TEST_CHECK(contains(s, "test"));
    
    zstr_free(&result);
}

/*
 * Test: {highlight} token should emit bold+yellow
 */
void test_highlight_is_bold_yellow(void) {
    zstr result = expand_tokens("{highlight}test{/}");
    const char *s = zstr_cstr(&result);
    
    /* Should contain both bold and yellow */
    TEST_CHECK(contains(s, "1") && contains(s, "33"));
    TEST_CHECK(contains(s, "test"));
    
    zstr_free(&result);
}

/*
 * Test: Redundant tokens should not emit multiple codes
 * "{dim}{dim}{dim}b" should emit only one \033[90m
 */
void test_redundant_tokens_no_duplicate_codes(void) {
    zstr result = expand_tokens("{dim}{dim}{dim}b");
    const char *s = zstr_cstr(&result);
    
    /* Should contain exactly one dim code (37 = white) */
    int count = count_occurrences(s, "\033[37m");
    TEST_CHECK_(count == 1, "Expected 1 dim code, got %d", count);
    TEST_CHECK(contains(s, "b"));
    
    zstr_free(&result);
}

/*
 * Test: Deferred emission - codes only emit when there's a character
 * "{b}{/}x" should emit just "x" with no ANSI codes
 */
void test_deferred_emission_no_unused_codes(void) {
    zstr result = expand_tokens("{b}{/}x");
    const char *s = zstr_cstr(&result);
    
    /* Should just be "x" with no escape codes */
    TEST_CHECK_(!contains(s, "\033"), "Expected no ANSI codes, got: %s", s);
    TEST_CHECK(strcmp(s, "x") == 0);
    
    zstr_free(&result);
}

/*
 * Test: Auto-reset at newlines
 * "{b}bold text\nnormal" should have reset before newline
 */
void test_auto_reset_at_newline(void) {
    zstr result = expand_tokens("{b}bold text\nnormal");
    const char *s = zstr_cstr(&result);
    
    /* Should contain reset code before the newline */
    const char *newline_pos = strchr(s, '\n');
    TEST_CHECK(newline_pos != NULL);
    
    /* The reset (0m) should appear before the newline */
    const char *reset_pos = strstr(s, "\033[0m");
    TEST_CHECK(reset_pos != NULL);
    TEST_CHECK(reset_pos < newline_pos);
    
    /* "normal" should NOT have bold applied (comes after newline) */
    const char *normal_start = newline_pos + 1;
    /* Check that there's no bold code right before "normal" */
    TEST_CHECK(strncmp(normal_start, "normal", 6) == 0);
    
    zstr_free(&result);
}

/*
 * Test: Stack-based nesting works correctly
 */
void test_stack_nesting(void) {
    zstr result = expand_tokens("{bold}{red}both{/}just bold{/}normal");
    const char *s = zstr_cstr(&result);
    
    TEST_CHECK(contains(s, "both"));
    TEST_CHECK(contains(s, "just bold"));
    TEST_CHECK(contains(s, "normal"));
    
    zstr_free(&result);
}

/*
 * Test: {danger} token works (renamed from {strike})
 */
void test_danger_token(void) {
    zstr result = expand_tokens("{danger}warning{/}");
    const char *s = zstr_cstr(&result);
    
    /* Should contain background color 48;5;52 (dark red bg) */
    TEST_CHECK(contains(s, "48;5;52") || contains(s, "48;5"));
    TEST_CHECK(contains(s, "warning"));
    
    zstr_free(&result);
}

/*
 * Test: {strike} is strikethrough text
 */
void test_strike_is_strikethrough(void) {
    zstr result = expand_tokens("{strike}crossed{/}");
    const char *s = zstr_cstr(&result);

    /* Should contain strikethrough code (9) */
    TEST_CHECK(contains(s, "9") || contains(s, ";9"));
    TEST_CHECK(contains(s, "crossed"));

    zstr_free(&result);
}

/*
 * Test: {/name} works as generic pop (e.g., {/highlight})
 */
void test_generic_pop(void) {
    zstr result1 = expand_tokens("{highlight}a{/highlight}b");
    zstr result2 = expand_tokens("{highlight}a{/}b");

    /* Both should produce same output */
    TEST_CHECK(strcmp(zstr_cstr(&result1), zstr_cstr(&result2)) == 0);

    zstr_free(&result1);
    zstr_free(&result2);
}

/*
 * Test: Heading tokens work
 */
void test_heading_tokens(void) {
    zstr h1 = expand_tokens("{h1}H1{/}");
    zstr h2 = expand_tokens("{h2}H2{/}");
    zstr h3 = expand_tokens("{h3}H3{/}");
    
    /* All should contain bold (1) */
    TEST_CHECK(contains(zstr_cstr(&h1), "H1"));
    TEST_CHECK(contains(zstr_cstr(&h2), "H2"));
    TEST_CHECK(contains(zstr_cstr(&h3), "H3"));
    
    /* h1 should have 256-color orange (38;5;214) */
    TEST_CHECK(contains(zstr_cstr(&h1), "38;5;214"));
    
    /* h2 should have blue (34) */
    TEST_CHECK(contains(zstr_cstr(&h2), "34"));
    
    zstr_free(&h1);
    zstr_free(&h2);
    zstr_free(&h3);
}

/*
 * Test: 256-color foreground
 */
void test_256_color_fg(void) {
    zstr result = expand_tokens("{fg:214}orange{/}");
    const char *s = zstr_cstr(&result);
    
    TEST_CHECK(contains(s, "38;5;214"));
    TEST_CHECK(contains(s, "orange"));
    
    zstr_free(&result);
}

/*
 * Test: 256-color background
 */
void test_256_color_bg(void) {
    zstr result = expand_tokens("{bg:52}darkred{/}");
    const char *s = zstr_cstr(&result);
    
    TEST_CHECK(contains(s, "48;5;52"));
    TEST_CHECK(contains(s, "darkred"));
    
    zstr_free(&result);
}

/*
 * Test: Cursor position tracking
 */
void test_cursor_tracking(void) {
    TokenExpansion te = expand_tokens_with_cursor("Hello {cursor}World");

    /* Cursor should be at column 7 (after "Hello "), row 1 */
    TEST_CHECK_(te.cursor_col == 7, "Expected cursor_col at 7, got %d", te.cursor_col);
    TEST_CHECK_(te.cursor_row == 1, "Expected cursor_row at 1, got %d", te.cursor_row);
    TEST_CHECK_(te.has_cursor == true, "Expected has_cursor to be true");
    TEST_CHECK(contains(zstr_cstr(&te.expanded), "Hello"));
    TEST_CHECK(contains(zstr_cstr(&te.expanded), "World"));

    token_expansion_free(&te);
}

/*
 * Test: Control tokens ({clr}, {home}, etc.)
 */
void test_control_tokens(void) {
    zstr clr = expand_tokens("{clr}");
    zstr home = expand_tokens("{home}");
    zstr hide = expand_tokens("{hide}");
    zstr show = expand_tokens("{show}");
    
    TEST_CHECK(strcmp(zstr_cstr(&clr), "\033[K") == 0);
    TEST_CHECK(strcmp(zstr_cstr(&home), "\033[H") == 0);
    TEST_CHECK(strcmp(zstr_cstr(&hide), "\033[?25l") == 0);
    TEST_CHECK(strcmp(zstr_cstr(&show), "\033[?25h") == 0);
    
    zstr_free(&clr);
    zstr_free(&home);
    zstr_free(&hide);
    zstr_free(&show);
}

/*
 * Test: zstr_no_colors flag disables output
 */
void test_no_colors_flag(void) {
    zstr_no_colors = true;
    zstr result = expand_tokens("{b}text{/}");
    
    /* Should just be "text" with no ANSI codes */
    TEST_CHECK(strcmp(zstr_cstr(&result), "text") == 0);
    
    zstr_free(&result);
    zstr_no_colors = false;
}

/*
 * Test: zstr_disable_token_expansion flag
 */
void test_disable_expansion_flag(void) {
    zstr_disable_token_expansion = true;
    zstr result = expand_tokens("{b}text{/}");
    
    /* Should pass through unchanged */
    TEST_CHECK(strcmp(zstr_cstr(&result), "{b}text{/}") == 0);
    
    zstr_free(&result);
    zstr_disable_token_expansion = false;
}

/*
 * Test: {strong} is same as {b}
 */
void test_strong_same_as_b(void) {
    zstr b_result = expand_tokens("{b}x{/}");
    zstr strong_result = expand_tokens("{strong}x{/}");
    
    /* Both should produce same output */
    TEST_CHECK(strcmp(zstr_cstr(&b_result), zstr_cstr(&strong_result)) == 0);
    
    zstr_free(&b_result);
    zstr_free(&strong_result);
}

/*
 * Test: {dim} token
 */
void test_dim_token(void) {
    zstr result = expand_tokens("{dim}dimmed{/}");
    const char *s = zstr_cstr(&result);

    /* Should contain code 37 (white - softer than bright white) */
    TEST_CHECK(contains(s, "37"));
    TEST_CHECK(contains(s, "dimmed"));

    zstr_free(&result);
}

/*
 * Test: ANSI passthrough (existing escape sequences are preserved)
 */
void test_ansi_passthrough(void) {
    zstr result = expand_tokens("hello\033[31mred\033[0mworld");
    const char *s = zstr_cstr(&result);
    
    /* Original ANSI codes should be preserved */
    TEST_CHECK(contains(s, "\033[31m"));
    TEST_CHECK(contains(s, "\033[0m"));
    TEST_CHECK(contains(s, "hello"));
    TEST_CHECK(contains(s, "red"));
    TEST_CHECK(contains(s, "world"));
    
    zstr_free(&result);
}

/*
 * Test: Empty input
 */
void test_empty_input(void) {
    zstr result = expand_tokens("");
    TEST_CHECK(zstr_len(&result) == 0);
    zstr_free(&result);
    
    zstr result2 = expand_tokens(NULL);
    TEST_CHECK(zstr_len(&result2) == 0);
    zstr_free(&result2);
}

/*
 * Test: Unrecognized tokens pass through
 */
void test_unrecognized_tokens(void) {
    zstr result = expand_tokens("{unknown}text");
    const char *s = zstr_cstr(&result);

    /* Unrecognized {unknown} should be passed through as-is */
    TEST_CHECK(contains(s, "{unknown}"));
    TEST_CHECK(contains(s, "text"));

    zstr_free(&result);
}

/*
 * Test: Tokens before newline don't output control sequences
 * "{bold}\n" should just output "\n" with no ANSI codes
 */
void test_tokens_before_newline_no_codes(void) {
    zstr result = expand_tokens("{bold}\n");
    const char *s = zstr_cstr(&result);

    /* Should just be a newline with no escape codes */
    TEST_CHECK_(strcmp(s, "\n") == 0, "Expected just newline, got: %s", s);

    zstr_free(&result);

    /* Multiple tokens before newline */
    zstr result2 = expand_tokens("{red}{blue}{bold}\n");
    const char *s2 = zstr_cstr(&result2);
    TEST_CHECK_(strcmp(s2, "\n") == 0, "Expected just newline, got: %s", s2);

    zstr_free(&result2);
}

/*
 * Test: Pop back to same state doesn't emit redundant codes
 * {green}a{blue}{/}b should emit green once, then output "ab"
 * because after popping blue, we're back to green (already emitted)
 */
void test_pop_to_same_state_no_redundant_codes(void) {
    zstr result = expand_tokens("{green}a{blue}{/}b");
    const char *s = zstr_cstr(&result);

    /* Should contain green code exactly once */
    int green_count = count_occurrences(s, "\033[32m");
    TEST_CHECK_(green_count == 1, "Expected 1 green code, got %d", green_count);

    /* Should NOT contain blue code (no character printed while blue was active) */
    TEST_CHECK_(!contains(s, "\033[34m"), "Should not contain blue code");

    /* Should contain both a and b */
    TEST_CHECK(contains(s, "a"));
    TEST_CHECK(contains(s, "b"));

    zstr_free(&result);
}

/*
 * Test: Multiple pushes with deferred emission
 * Tokens before a character don't emit until the character
 * {blue}{green}{blue}{red}{green}a should emit only one code (green) right before 'a'
 */
void test_multiple_pushes_deferred(void) {
    zstr result = expand_tokens("{blue}{green}{blue}{red}{green}a");
    const char *s = zstr_cstr(&result);

    /* Should contain exactly one escape sequence (green) followed by 'a' */
    /* The format should be \033[32ma or similar */
    TEST_CHECK(contains(s, "32"));  /* green color code */
    TEST_CHECK(contains(s, "a"));

    /* Count total escape sequences - should be exactly 1 */
    int esc_count = count_occurrences(s, "\033[");
    TEST_CHECK_(esc_count == 1, "Expected 1 escape sequence, got %d", esc_count);

    zstr_free(&result);
}

/*
 * Test: Complex push/pop sequence
 * After multiple pops, only emit if state actually changes from last emitted
 */
void test_complex_push_pop_sequence(void) {
    /* {green}a{red}b{/}c - after pop, back to green, same as before red was pushed */
    zstr result = expand_tokens("{green}a{red}b{/}c");
    const char *s = zstr_cstr(&result);

    /* Should have green, red, then green again (because we popped back) */
    TEST_CHECK(contains(s, "a"));
    TEST_CHECK(contains(s, "b"));
    TEST_CHECK(contains(s, "c"));

    /* Count green codes - should be 2 (initial green for 'a', restored green for 'c') */
    int green_count = count_occurrences(s, "\033[32m");
    TEST_CHECK_(green_count == 2, "Expected 2 green codes, got %d", green_count);

    /* Count red codes - should be 1 (for 'b') */
    int red_count = count_occurrences(s, "\033[31m");
    TEST_CHECK_(red_count == 1, "Expected 1 red code, got %d", red_count);

    zstr_free(&result);
}

/*
 * Test list
 */
TEST_LIST = {
    { "b_is_just_bold", test_b_is_just_bold },
    { "highlight_is_bold_yellow", test_highlight_is_bold_yellow },
    { "redundant_tokens_no_duplicate_codes", test_redundant_tokens_no_duplicate_codes },
    { "deferred_emission_no_unused_codes", test_deferred_emission_no_unused_codes },
    { "auto_reset_at_newline", test_auto_reset_at_newline },
    { "stack_nesting", test_stack_nesting },
    { "danger_token", test_danger_token },
    { "strike_is_strikethrough", test_strike_is_strikethrough },
    { "generic_pop", test_generic_pop },
    { "heading_tokens", test_heading_tokens },
    { "256_color_fg", test_256_color_fg },
    { "256_color_bg", test_256_color_bg },
    { "cursor_tracking", test_cursor_tracking },
    { "control_tokens", test_control_tokens },
    { "no_colors_flag", test_no_colors_flag },
    { "disable_expansion_flag", test_disable_expansion_flag },
    { "strong_same_as_b", test_strong_same_as_b },
    { "dim_token", test_dim_token },
    { "ansi_passthrough", test_ansi_passthrough },
    { "empty_input", test_empty_input },
    { "unrecognized_tokens", test_unrecognized_tokens },
    { "tokens_before_newline_no_codes", test_tokens_before_newline_no_codes },
    { "pop_to_same_state_no_redundant_codes", test_pop_to_same_state_no_redundant_codes },
    { "multiple_pushes_deferred", test_multiple_pushes_deferred },
    { "complex_push_pop_sequence", test_complex_push_pop_sequence },
    { NULL, NULL }
};
