
#line 1 "src/tokens.rl"
/*
 * Token expansion state machine - Ragel grammar
 *
 * This provides a stack-based token system for ANSI escape code generation.
 * Tokens like {b}, {dim}, {red} push state, and {/} pops to restore.
 *
 * Generated with: ragel -C -G2 tokens.rl -o tokens.c
 */

#include "tokens.h"
#include "zstr.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Maximum nesting depth for style stack */
#define MAX_STACK_DEPTH 32

/*
 * Global style configuration for semantic tokens.
 * These can be overridden at runtime to customize the appearance.
 * Format: ANSI escape sequence string (without the leading \033[)
 */
const char *token_style_h1 = "1m\033[38;5;214";  /* Bold + orange (256-color 214) */
const char *token_style_h2 = "1;34";              /* Bold + blue */
const char *token_style_h3 = "1;37";              /* Bold + white */
const char *token_style_h4 = "1;37";              /* Bold + white */
const char *token_style_h5 = "1;37";              /* Bold + white */
const char *token_style_h6 = "1;37";              /* Bold + white */
const char *token_style_b = "1";                   /* Bold (same as strong) */
const char *token_style_strong = "1";             /* Bold */
const char *token_style_highlight = "1;33";       /* Bold + yellow */
const char *token_style_dim = "37";               /* White (softer than bright white) */
const char *token_style_danger = "48;5;52";       /* Dark red bg */

/* Style attribute types */
typedef enum {
    ATTR_NONE = 0,
    ATTR_BOLD,
    ATTR_DIM,
    ATTR_ITALIC,
    ATTR_UNDERLINE,
    ATTR_REVERSE,
    ATTR_STRIKETHROUGH,
    ATTR_FG,          /* Foreground color */
    ATTR_BG,          /* Background color */
    ATTR_COMPOSITE,   /* Multiple attributes (for semantic tokens) */
} AttrType;

/* Represents a single attribute value */
typedef struct {
    AttrType type;
    int value;        /* For colors: ANSI code, for bools: 0/1 */
} AttrValue;

/* Stack entry - stores what to restore when popping */
typedef struct {
    AttrType type;
    int prev_value;
    int count;        /* For composite: how many individual attrs were pushed */
} StackEntry;

/* Parser state */
typedef struct {
    zstr *out;              /* Output buffer */
    StackEntry stack[MAX_STACK_DEPTH];
    int stack_depth;
    int cursor_col;         /* Visual column of {cursor} (1-indexed), -1 if no cursor */
    int cursor_row;         /* Visual row of {cursor} (1-indexed), -1 if no cursor */
    bool has_cursor;        /* True if {cursor} was encountered */
    int visual_col;         /* Current visual column */
    int visual_row;         /* Current visual row */
    bool no_colors;         /* If true, don't emit ANSI codes */
    bool disabled;          /* If true, pass through without expansion */

    /* Desired style state (what tokens request) */
    int fg_color;           /* 0 = default, else ANSI code */
    int bg_color;
    bool bold;
    bool dim;
    bool italic;
    bool underline;
    bool reverse;
    bool strikethrough;

    /* Emitted style state (what terminal currently has) */
    int emitted_fg;
    int emitted_bg;
    bool emitted_bold;
    bool emitted_dim;
    bool emitted_italic;
    bool emitted_underline;
    bool emitted_reverse;
    bool emitted_strikethrough;
    bool dirty;             /* True if desired != emitted */
} TokenParser;

/* ANSI escape code helpers */
static void emit_ansi(TokenParser *p, const char *code) {
    if (!p->no_colors) {
        zstr_cat(p->out, code);
    }
}

static void emit_ansi_num(TokenParser *p, const char *prefix, int n, const char *suffix) {
    if (!p->no_colors) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%s%d%s", prefix, n, suffix);
        zstr_cat(p->out, buf);
    }
}

/* Mark state as dirty (deferred emission) */
static void mark_dirty(TokenParser *p) {
    p->dirty = true;
}

/* Sync emitted state with desired state - emit codes only when outputting a character */
static void sync_styles(TokenParser *p) {
    if (!p->dirty || p->no_colors) return;

    /* Check if we need a full reset (going back to default state) */
    bool need_reset = false;

    /* If any attribute is turning OFF, we need reset then re-apply actives */
    if ((p->emitted_bold && !p->bold) ||
        (p->emitted_dim && !p->dim) ||
        (p->emitted_italic && !p->italic) ||
        (p->emitted_underline && !p->underline) ||
        (p->emitted_reverse && !p->reverse) ||
        (p->emitted_strikethrough && !p->strikethrough) ||
        (p->emitted_fg != 0 && p->fg_color == 0) ||
        (p->emitted_bg != 0 && p->bg_color == 0)) {
        need_reset = true;
    }

    if (need_reset) {
        /* Reset and re-apply all active styles */
        zstr_cat(p->out, "\033[0");
        if (p->bold) zstr_cat(p->out, ";1");
        if (p->dim) zstr_cat(p->out, ";2");
        if (p->italic) zstr_cat(p->out, ";3");
        if (p->underline) zstr_cat(p->out, ";4");
        if (p->reverse) zstr_cat(p->out, ";7");
        if (p->strikethrough) zstr_cat(p->out, ";9");
        if (p->fg_color != 0) {
            char buf[20];
            if (p->fg_color >= 1000 && p->fg_color < 2000) {
                /* 256-color foreground */
                snprintf(buf, sizeof(buf), ";38;5;%d", p->fg_color - 1000);
            } else {
                snprintf(buf, sizeof(buf), ";%d", p->fg_color);
            }
            zstr_cat(p->out, buf);
        }
        if (p->bg_color != 0) {
            char buf[20];
            if (p->bg_color >= 2000) {
                /* 256-color background */
                snprintf(buf, sizeof(buf), ";48;5;%d", p->bg_color - 2000);
            } else {
                snprintf(buf, sizeof(buf), ";%d", p->bg_color);
            }
            zstr_cat(p->out, buf);
        }
        zstr_cat(p->out, "m");
    } else {
        /* Apply only changed attributes (turning ON) */
        bool first = true;
        char buf[64];
        buf[0] = '\0';

        if (p->bold && !p->emitted_bold) {
            strcat(buf, first ? "1" : ";1"); first = false;
        }
        if (p->dim && !p->emitted_dim) {
            strcat(buf, first ? "2" : ";2"); first = false;
        }
        if (p->italic && !p->emitted_italic) {
            strcat(buf, first ? "3" : ";3"); first = false;
        }
        if (p->underline && !p->emitted_underline) {
            strcat(buf, first ? "4" : ";4"); first = false;
        }
        if (p->reverse && !p->emitted_reverse) {
            strcat(buf, first ? "7" : ";7"); first = false;
        }
        if (p->strikethrough && !p->emitted_strikethrough) {
            strcat(buf, first ? "9" : ";9"); first = false;
        }
        if (p->fg_color != p->emitted_fg && p->fg_color != 0) {
            char tmp[20];
            if (p->fg_color >= 1000 && p->fg_color < 2000) {
                snprintf(tmp, sizeof(tmp), "%s38;5;%d", first ? "" : ";", p->fg_color - 1000);
            } else {
                snprintf(tmp, sizeof(tmp), "%s%d", first ? "" : ";", p->fg_color);
            }
            strcat(buf, tmp); first = false;
        }
        if (p->bg_color != p->emitted_bg && p->bg_color != 0) {
            char tmp[20];
            if (p->bg_color >= 2000) {
                snprintf(tmp, sizeof(tmp), "%s48;5;%d", first ? "" : ";", p->bg_color - 2000);
            } else {
                snprintf(tmp, sizeof(tmp), "%s%d", first ? "" : ";", p->bg_color);
            }
            strcat(buf, tmp); first = false;
        }

        if (!first) {
            zstr_cat(p->out, "\033[");
            zstr_cat(p->out, buf);
            zstr_cat(p->out, "m");
        }
    }

    /* Update emitted state to match desired */
    p->emitted_bold = p->bold;
    p->emitted_dim = p->dim;
    p->emitted_italic = p->italic;
    p->emitted_underline = p->underline;
    p->emitted_reverse = p->reverse;
    p->emitted_strikethrough = p->strikethrough;
    p->emitted_fg = p->fg_color;
    p->emitted_bg = p->bg_color;
    p->dirty = false;
}

/* Push a restore entry onto the stack */
static void push_attr(TokenParser *p, AttrType type, int prev_value) {
    if (p->stack_depth < MAX_STACK_DEPTH) {
        p->stack[p->stack_depth].type = type;
        p->stack[p->stack_depth].prev_value = prev_value;
        p->stack[p->stack_depth].count = 1;
        p->stack_depth++;
    }
}

/* Push a composite (multiple attrs at once) */
static void push_composite(TokenParser *p, int count) {
    if (p->stack_depth < MAX_STACK_DEPTH) {
        p->stack[p->stack_depth].type = ATTR_COMPOSITE;
        p->stack[p->stack_depth].prev_value = 0;
        p->stack[p->stack_depth].count = count;
        p->stack_depth++;
    }
}

/* Restore an attribute to its previous value (deferred - just updates state) */
static void restore_attr(TokenParser *p, AttrType type, int prev_value) {
    switch (type) {
        case ATTR_BOLD:
            p->bold = prev_value;
            break;
        case ATTR_DIM:
            p->dim = prev_value;
            break;
        case ATTR_ITALIC:
            p->italic = prev_value;
            break;
        case ATTR_UNDERLINE:
            p->underline = prev_value;
            break;
        case ATTR_REVERSE:
            p->reverse = prev_value;
            break;
        case ATTR_STRIKETHROUGH:
            p->strikethrough = prev_value;
            break;
        case ATTR_FG:
            p->fg_color = prev_value;
            break;
        case ATTR_BG:
            p->bg_color = prev_value;
            break;
        default:
            break;
    }
}

/* Pop one entry from the stack and restore */
static void pop_style(TokenParser *p) {
    if (p->stack_depth > 0) {
        p->stack_depth--;
        StackEntry *e = &p->stack[p->stack_depth];

        if (e->type == ATTR_COMPOSITE) {
            /* Pop multiple individual entries */
            for (int i = 0; i < e->count && p->stack_depth > 0; i++) {
                p->stack_depth--;
                StackEntry *inner = &p->stack[p->stack_depth];
                restore_attr(p, inner->type, inner->prev_value);
            }
        } else {
            restore_attr(p, e->type, e->prev_value);
        }
        mark_dirty(p);
    }
}

/* Check if any styles are active */
static bool has_active_styles(TokenParser *p) {
    return p->bold || p->dim || p->italic || p->underline ||
           p->reverse || p->strikethrough || p->fg_color != 0 || p->bg_color != 0;
}

/* Reset all styles and clear stack (deferred - just updates state) */
static void reset_all(TokenParser *p) {
    p->fg_color = 0;
    p->bg_color = 0;
    p->bold = false;
    p->dim = false;
    p->italic = false;
    p->underline = false;
    p->reverse = false;
    p->strikethrough = false;
    p->stack_depth = 0;
    mark_dirty(p);
}

/* Reset all styles at newline - emits immediately since newline follows */
static void reset_line_styles(TokenParser *p) {
    /* Check if emitted state has any active styles */
    if (p->emitted_bold || p->emitted_dim || p->emitted_italic ||
        p->emitted_underline || p->emitted_reverse || p->emitted_strikethrough ||
        p->emitted_fg != 0 || p->emitted_bg != 0) {
        emit_ansi(p, "\033[0m");
        /* Update emitted state */
        p->emitted_bold = false;
        p->emitted_dim = false;
        p->emitted_italic = false;
        p->emitted_underline = false;
        p->emitted_reverse = false;
        p->emitted_strikethrough = false;
        p->emitted_fg = 0;
        p->emitted_bg = 0;
    }
    /* Reset desired state too */
    p->fg_color = 0;
    p->bg_color = 0;
    p->bold = false;
    p->dim = false;
    p->italic = false;
    p->underline = false;
    p->reverse = false;
    p->strikethrough = false;
    p->dirty = false;
    /* Keep stack_depth as-is so {/} still works across lines if needed */
}

/* Style application functions - deferred emission (just update state) */
static void apply_bold(TokenParser *p) {
    push_attr(p, ATTR_BOLD, p->bold);
    p->bold = true;
    mark_dirty(p);
}

static void apply_dim(TokenParser *p) {
    push_attr(p, ATTR_DIM, p->dim);
    p->dim = true;
    mark_dirty(p);
}

static void apply_italic(TokenParser *p) {
    push_attr(p, ATTR_ITALIC, p->italic);
    p->italic = true;
    mark_dirty(p);
}

static void apply_underline(TokenParser *p) {
    push_attr(p, ATTR_UNDERLINE, p->underline);
    p->underline = true;
    mark_dirty(p);
}

static void apply_reverse(TokenParser *p) {
    push_attr(p, ATTR_REVERSE, p->reverse);
    p->reverse = true;
    mark_dirty(p);
}

static void apply_strikethrough(TokenParser *p) {
    push_attr(p, ATTR_STRIKETHROUGH, p->strikethrough);
    p->strikethrough = true;
    mark_dirty(p);
}

static void apply_fg_code(TokenParser *p, int code) {
    push_attr(p, ATTR_FG, p->fg_color);
    p->fg_color = code;
    mark_dirty(p);
}

static void apply_fg_256(TokenParser *p, int n) {
    push_attr(p, ATTR_FG, p->fg_color);
    p->fg_color = 1000 + n; /* Marker for 256-color */
    mark_dirty(p);
}

static void apply_bg_code(TokenParser *p, int code) {
    push_attr(p, ATTR_BG, p->bg_color);
    p->bg_color = code;
    mark_dirty(p);
}

static void apply_bg_256(TokenParser *p, int n) {
    push_attr(p, ATTR_BG, p->bg_color);
    p->bg_color = 2000 + n; /* Marker for 256-color bg */
    mark_dirty(p);
}

/* Semantic token applications - deferred emission (just update state) */
static void apply_token_b(TokenParser *p) {
    /* {b} = bold only (same as strong) */
    push_attr(p, ATTR_BOLD, p->bold);
    p->bold = true;
    mark_dirty(p);
}

static void apply_token_highlight(TokenParser *p) {
    /* {highlight} = bold + yellow */
    push_attr(p, ATTR_BOLD, p->bold);
    push_attr(p, ATTR_FG, p->fg_color);
    push_composite(p, 2);
    p->bold = true;
    p->fg_color = 33; /* Yellow */
    mark_dirty(p);
}

static void apply_token_h1(TokenParser *p) {
    /* {h1} = bold + orange (256-color 214) */
    push_attr(p, ATTR_BOLD, p->bold);
    push_attr(p, ATTR_FG, p->fg_color);
    push_composite(p, 2);
    p->bold = true;
    p->fg_color = 1214; /* 256-color marker for 214 */
    mark_dirty(p);
}

static void apply_token_h2(TokenParser *p) {
    /* {h2} = bold + blue */
    push_attr(p, ATTR_BOLD, p->bold);
    push_attr(p, ATTR_FG, p->fg_color);
    push_composite(p, 2);
    p->bold = true;
    p->fg_color = 34; /* Blue */
    mark_dirty(p);
}

static void apply_token_h3(TokenParser *p) {
    /* {h3} = bold + white */
    push_attr(p, ATTR_BOLD, p->bold);
    push_attr(p, ATTR_FG, p->fg_color);
    push_composite(p, 2);
    p->bold = true;
    p->fg_color = 37; /* White */
    mark_dirty(p);
}

static void apply_token_h4(TokenParser *p) {
    /* {h4} = bold + white */
    push_attr(p, ATTR_BOLD, p->bold);
    push_attr(p, ATTR_FG, p->fg_color);
    push_composite(p, 2);
    p->bold = true;
    p->fg_color = 37;
    mark_dirty(p);
}

static void apply_token_h5(TokenParser *p) {
    /* {h5} = bold + white */
    push_attr(p, ATTR_BOLD, p->bold);
    push_attr(p, ATTR_FG, p->fg_color);
    push_composite(p, 2);
    p->bold = true;
    p->fg_color = 37;
    mark_dirty(p);
}

static void apply_token_h6(TokenParser *p) {
    /* {h6} = bold + white */
    push_attr(p, ATTR_BOLD, p->bold);
    push_attr(p, ATTR_FG, p->fg_color);
    push_composite(p, 2);
    p->bold = true;
    p->fg_color = 37;
    mark_dirty(p);
}

static void apply_token_strong(TokenParser *p) {
    /* {strong} = bold */
    push_attr(p, ATTR_BOLD, p->bold);
    p->bold = true;
    mark_dirty(p);
}

static void apply_token_dim_style(TokenParser *p) {
    /* {dim} = white (standard color, softer than bright white) */
    push_attr(p, ATTR_FG, p->fg_color);
    p->fg_color = 37; /* White */
    mark_dirty(p);
}

static void apply_token_dark_style(TokenParser *p) {
    /* {dark} = 256-color 245 (medium gray, for TUI secondary text) */
    push_attr(p, ATTR_FG, p->fg_color);
    p->fg_color = 1245; /* 256-color marker for 245 */
    mark_dirty(p);
}

static void apply_token_section(TokenParser *p) {
    /* {section} = bold + subtle dark gray background (256-color 237) */
    push_attr(p, ATTR_BOLD, p->bold);
    push_attr(p, ATTR_BG, p->bg_color);
    push_composite(p, 2);
    p->bold = true;
    p->bg_color = 2237; /* 256-color marker for 237 */
    mark_dirty(p);
}

static void apply_token_danger(TokenParser *p) {
    /* {danger} = dark red background (256-color 52) */
    push_attr(p, ATTR_BG, p->bg_color);
    p->bg_color = 2052; /* 256-color marker for 52 */
    mark_dirty(p);
}

static void apply_token_text(TokenParser *p) {
    /* {text} = full reset */
    reset_all(p);
}

/* Brighten current foreground color */
static void apply_bright(TokenParser *p) {
    push_attr(p, ATTR_FG, p->fg_color);
    /* If current color is 30-37 (standard), convert to 90-97 (bright) */
    if (p->fg_color >= 30 && p->fg_color <= 37) {
        p->fg_color = p->fg_color + 60;
    } else {
        /* Already bright or custom - just set bright white */
        p->fg_color = 97;
    }
    mark_dirty(p);
}

/* Parse a number from string, returns -1 on failure */
static int parse_number(const char *start, const char *end) {
    int n = 0;
    for (const char *p = start; p < end; p++) {
        if (*p < '0' || *p > '9') return -1;
        n = n * 10 + (*p - '0');
    }
    return n;
}


#line 560 "src/tokens.c"
static const int token_parser_start = 244;
static const int token_parser_first_final = 244;
static const int token_parser_error = 0;

static const int token_parser_en_main = 244;


#line 897 "src/tokens.rl"


/* Global flags */
bool zstr_disable_token_expansion = false;
bool zstr_no_colors = false;

TokenExpansion expand_tokens_with_cursor(const char *text) {
    TokenExpansion result = {0};
    result.expanded = zstr_init();
    result.cursor_col = -1;
    result.cursor_row = -1;
    result.has_cursor = false;
    result.final_col = 1;
    result.final_row = 1;

    if (!text || !*text) {
        return result;
    }

    /* If expansion is disabled, just copy */
    if (zstr_disable_token_expansion) {
        zstr_cat(&result.expanded, text);
        return result;
    }

    /* Pre-reserve buffer space to avoid reallocations.
     * Estimate: input length + overhead for ANSI codes (~50% extra) */
    size_t input_len = strlen(text);
    zstr_reserve(&result.expanded, input_len + input_len / 2 + 64);

    /* Initialize parser state */
    TokenParser parser = {0};
    parser.out = &result.expanded;
    parser.cursor_col = -1;
    parser.cursor_row = -1;
    parser.visual_col = 1;
    parser.visual_row = 1;
    parser.no_colors = zstr_no_colors;

    /* Ragel variables */
    int cs;
    int act;
    const char *p = text;
    const char *pe = text + strlen(text);
    const char *eof = pe;
    const char *ts = NULL;  /* Token start */
    const char *te = NULL;  /* Token end */
    const char *tok_start = NULL;
    const char *tok_end = NULL;
    const char *goto_row_start = NULL;
    const char *goto_row_end = NULL;
    const char *goto_col_start = NULL;
    const char *goto_col_end = NULL;

    (void)tok_start;
    (void)tok_end;
    (void)goto_row_start;
    (void)goto_row_end;
    (void)goto_col_start;
    (void)goto_col_end;
    (void)eof;
    (void)ts;
    (void)te;
    (void)act;
    (void)token_parser_en_main;

    
#line 636 "src/tokens.c"
	{
	cs = token_parser_start;
	ts = 0;
	te = 0;
	act = 0;
	}

#line 964 "src/tokens.rl"
    
#line 646 "src/tokens.c"
	{
	if ( p == pe )
		goto _test_eof;
	switch ( cs )
	{
tr2:
#line 868 "src/tokens.rl"
	{te = p+1;{
        for (const char *c = ts; c < te; c++) {
            zstr_push(parser.out, *c);
        }
    }}
	goto st244;
tr3:
#line 855 "src/tokens.rl"
	{{p = ((te))-1;}{
        sync_styles(&parser);  /* Emit any pending style changes */
        zstr_push(parser.out, *ts);
        parser.visual_col++;
    }}
	goto st244;
tr294:
#line 855 "src/tokens.rl"
	{te = p+1;{
        sync_styles(&parser);  /* Emit any pending style changes */
        zstr_push(parser.out, *ts);
        parser.visual_col++;
    }}
	goto st244;
tr295:
#line 861 "src/tokens.rl"
	{te = p+1;{
        /* Auto-reset all active styles before newline */
        reset_line_styles(&parser);
        zstr_push(parser.out, *ts);
        parser.visual_col = 1;
    }}
	goto st244;
tr298:
#line 855 "src/tokens.rl"
	{te = p;p--;{
        sync_styles(&parser);  /* Emit any pending style changes */
        zstr_push(parser.out, *ts);
        parser.visual_col++;
    }}
	goto st244;
tr314:
#line 591 "src/tokens.rl"
	{ pop_style(&parser); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr315:
#line 591 "src/tokens.rl"
	{ pop_style(&parser); }
#line 598 "src/tokens.rl"
	{
        push_attr(&parser, ATTR_BG, parser.bg_color);
        parser.bg_color = 0;
        mark_dirty(&parser);
    }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr316:
#line 591 "src/tokens.rl"
	{ pop_style(&parser); }
#line 593 "src/tokens.rl"
	{
        push_attr(&parser, ATTR_FG, parser.fg_color);
        parser.fg_color = 0;
        mark_dirty(&parser);
    }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr317:
#line 621 "src/tokens.rl"
	{ apply_bold(&parser); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr318:
#line 622 "src/tokens.rl"
	{ apply_italic(&parser); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr319:
#line 623 "src/tokens.rl"
	{ apply_underline(&parser); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr320:
#line 668 "src/tokens.rl"
	{
        int n = parse_number(tok_start, tok_end);
        if (n >= 0 && n <= 255) {
            apply_bg_256(&parser, n);
        }
    }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr321:
#line 650 "src/tokens.rl"
	{ apply_bg_code(&parser, 40); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr322:
#line 654 "src/tokens.rl"
	{ apply_bg_code(&parser, 44); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr323:
#line 656 "src/tokens.rl"
	{ apply_bg_code(&parser, 46); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr324:
#line 658 "src/tokens.rl"
	{ apply_bg_code(&parser, 100); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr325:
#line 652 "src/tokens.rl"
	{ apply_bg_code(&parser, 42); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr326:
#line 655 "src/tokens.rl"
	{ apply_bg_code(&parser, 45); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr327:
#line 651 "src/tokens.rl"
	{ apply_bg_code(&parser, 41); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr328:
#line 657 "src/tokens.rl"
	{ apply_bg_code(&parser, 47); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr329:
#line 653 "src/tokens.rl"
	{ apply_bg_code(&parser, 43); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr330:
#line 629 "src/tokens.rl"
	{ apply_fg_code(&parser, 30); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr331:
#line 633 "src/tokens.rl"
	{ apply_fg_code(&parser, 34); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr332:
#line 640 "src/tokens.rl"
	{ apply_fg_code(&parser, 90); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr333:
#line 644 "src/tokens.rl"
	{ apply_fg_code(&parser, 94); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr334:
#line 646 "src/tokens.rl"
	{ apply_fg_code(&parser, 96); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr335:
#line 642 "src/tokens.rl"
	{ apply_fg_code(&parser, 92); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr336:
#line 645 "src/tokens.rl"
	{ apply_fg_code(&parser, 95); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr337:
#line 641 "src/tokens.rl"
	{ apply_fg_code(&parser, 91); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr338:
#line 647 "src/tokens.rl"
	{ apply_fg_code(&parser, 97); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr339:
#line 643 "src/tokens.rl"
	{ apply_fg_code(&parser, 93); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr340:
#line 626 "src/tokens.rl"
	{ apply_bright(&parser); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr341:
#line 605 "src/tokens.rl"
	{ apply_token_b(&parser); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr342:
#line 748 "src/tokens.rl"
	{ emit_ansi(&parser, "\033[K"); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr343:
#line 749 "src/tokens.rl"
	{ emit_ansi(&parser, "\033[J"); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr344:
#line 578 "src/tokens.rl"
	{
        parser.cursor_col = parser.visual_col;
        parser.cursor_row = parser.visual_row;
        parser.has_cursor = true;
    }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr345:
#line 635 "src/tokens.rl"
	{ apply_fg_code(&parser, 36); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr346:
#line 617 "src/tokens.rl"
	{ apply_token_danger(&parser); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr347:
#line 615 "src/tokens.rl"
	{ apply_token_dark_style(&parser); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr348:
#line 614 "src/tokens.rl"
	{ apply_token_dim_style(&parser); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr349:
#line 661 "src/tokens.rl"
	{
        int n = parse_number(tok_start, tok_end);
        if (n >= 0 && n <= 255) {
            apply_fg_256(&parser, n);
        }
    }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr350:
#line 759 "src/tokens.rl"
	{
        int row = parse_number(goto_row_start, goto_row_end);
        int col = parse_number(goto_col_start, goto_col_end);
        if (row >= 0 && col >= 0) {
            emit_ansi_num(&parser, "\033[", row, ";");
            emit_ansi_num(&parser, "", col, "H");
        }
    }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr351:
#line 584 "src/tokens.rl"
	{
        if (parser.cursor_row > 0 && parser.cursor_col > 0) {
            emit_ansi_num(&parser, "\033[", parser.cursor_row, ";");
            emit_ansi_num(&parser, "", parser.cursor_col, "H");
        }
    }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr352:
#line 637 "src/tokens.rl"
	{ apply_fg_code(&parser, 90); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr353:
#line 631 "src/tokens.rl"
	{ apply_fg_code(&parser, 32); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr354:
#line 607 "src/tokens.rl"
	{ apply_token_h1(&parser); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr355:
#line 608 "src/tokens.rl"
	{ apply_token_h2(&parser); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr356:
#line 609 "src/tokens.rl"
	{ apply_token_h3(&parser); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr357:
#line 610 "src/tokens.rl"
	{ apply_token_h4(&parser); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr358:
#line 611 "src/tokens.rl"
	{ apply_token_h5(&parser); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr359:
#line 612 "src/tokens.rl"
	{ apply_token_h6(&parser); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr360:
#line 751 "src/tokens.rl"
	{ emit_ansi(&parser, "\033[?25l"); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr361:
#line 606 "src/tokens.rl"
	{ apply_token_highlight(&parser); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr362:
#line 750 "src/tokens.rl"
	{ emit_ansi(&parser, "\033[H"); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr363:
#line 634 "src/tokens.rl"
	{ apply_fg_code(&parser, 35); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr364:
#line 630 "src/tokens.rl"
	{ apply_fg_code(&parser, 31); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr365:
#line 592 "src/tokens.rl"
	{ reset_all(&parser); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr366:
#line 624 "src/tokens.rl"
	{ apply_reverse(&parser); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr367:
#line 616 "src/tokens.rl"
	{ apply_token_section(&parser); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr368:
#line 752 "src/tokens.rl"
	{ emit_ansi(&parser, "\033[?25h"); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr369:
#line 625 "src/tokens.rl"
	{ apply_strikethrough(&parser); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr370:
#line 613 "src/tokens.rl"
	{ apply_token_strong(&parser); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr371:
#line 618 "src/tokens.rl"
	{ apply_token_text(&parser); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr372:
#line 636 "src/tokens.rl"
	{ apply_fg_code(&parser, 37); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
tr373:
#line 632 "src/tokens.rl"
	{ apply_fg_code(&parser, 33); }
#line 889 "src/tokens.rl"
	{te = p;p--;}
	goto st244;
st244:
#line 1 "NONE"
	{ts = 0;}
	if ( ++p == pe )
		goto _test_eof244;
case 244:
#line 1 "NONE"
	{ts = p;}
#line 1099 "src/tokens.c"
	switch( (*p) ) {
		case 10: goto tr295;
		case 27: goto st1;
		case 123: goto tr297;
	}
	goto tr294;
st1:
	if ( ++p == pe )
		goto _test_eof1;
case 1:
	if ( (*p) == 91 )
		goto st2;
	goto st0;
st0:
cs = 0;
	goto _out;
st2:
	if ( ++p == pe )
		goto _test_eof2;
case 2:
	if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr2;
	} else if ( (*p) >= 65 )
		goto tr2;
	goto st2;
tr297:
#line 1 "NONE"
	{te = p+1;}
	goto st245;
st245:
	if ( ++p == pe )
		goto _test_eof245;
case 245:
#line 1134 "src/tokens.c"
	switch( (*p) ) {
		case 47: goto st3;
		case 66: goto st9;
		case 73: goto st10;
		case 85: goto st11;
		case 98: goto st12;
		case 99: goto st106;
		case 100: goto st118;
		case 102: goto st128;
		case 103: goto st132;
		case 104: goto st153;
		case 105: goto st173;
		case 109: goto st178;
		case 114: goto st185;
		case 115: goto st196;
		case 116: goto st221;
		case 117: goto st225;
		case 119: goto st233;
		case 121: goto st238;
	}
	goto tr298;
st3:
	if ( ++p == pe )
		goto _test_eof3;
case 3:
	switch( (*p) ) {
		case 98: goto st5;
		case 102: goto st7;
		case 125: goto st246;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto st4;
	goto tr3;
st4:
	if ( ++p == pe )
		goto _test_eof4;
case 4:
	if ( (*p) == 125 )
		goto st246;
	if ( 97 <= (*p) && (*p) <= 122 )
		goto st4;
	goto tr3;
st246:
	if ( ++p == pe )
		goto _test_eof246;
case 246:
	goto tr314;
st5:
	if ( ++p == pe )
		goto _test_eof5;
case 5:
	switch( (*p) ) {
		case 103: goto st6;
		case 125: goto st246;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto st4;
	goto tr3;
st6:
	if ( ++p == pe )
		goto _test_eof6;
case 6:
	if ( (*p) == 125 )
		goto st247;
	if ( 97 <= (*p) && (*p) <= 122 )
		goto st4;
	goto tr3;
st247:
	if ( ++p == pe )
		goto _test_eof247;
case 247:
	goto tr315;
st7:
	if ( ++p == pe )
		goto _test_eof7;
case 7:
	switch( (*p) ) {
		case 103: goto st8;
		case 125: goto st246;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto st4;
	goto tr3;
st8:
	if ( ++p == pe )
		goto _test_eof8;
case 8:
	if ( (*p) == 125 )
		goto st248;
	if ( 97 <= (*p) && (*p) <= 122 )
		goto st4;
	goto tr3;
st248:
	if ( ++p == pe )
		goto _test_eof248;
case 248:
	goto tr316;
st9:
	if ( ++p == pe )
		goto _test_eof9;
case 9:
	if ( (*p) == 125 )
		goto st249;
	goto tr3;
st249:
	if ( ++p == pe )
		goto _test_eof249;
case 249:
	goto tr317;
st10:
	if ( ++p == pe )
		goto _test_eof10;
case 10:
	if ( (*p) == 125 )
		goto st250;
	goto tr3;
st250:
	if ( ++p == pe )
		goto _test_eof250;
case 250:
	goto tr318;
st11:
	if ( ++p == pe )
		goto _test_eof11;
case 11:
	if ( (*p) == 125 )
		goto st251;
	goto tr3;
st251:
	if ( ++p == pe )
		goto _test_eof251;
case 251:
	goto tr319;
st12:
	if ( ++p == pe )
		goto _test_eof12;
case 12:
	switch( (*p) ) {
		case 103: goto st13;
		case 108: goto st55;
		case 111: goto st61;
		case 114: goto st63;
		case 125: goto st273;
	}
	goto tr3;
st13:
	if ( ++p == pe )
		goto _test_eof13;
case 13:
	if ( (*p) == 58 )
		goto st14;
	goto tr3;
st14:
	if ( ++p == pe )
		goto _test_eof14;
case 14:
	switch( (*p) ) {
		case 98: goto st16;
		case 99: goto st23;
		case 103: goto st27;
		case 109: goto st34;
		case 114: goto st41;
		case 119: goto st44;
		case 121: goto st49;
	}
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr21;
	goto tr3;
tr21:
#line 562 "src/tokens.rl"
	{ tok_start = p; }
	goto st15;
st15:
	if ( ++p == pe )
		goto _test_eof15;
case 15:
#line 1311 "src/tokens.c"
	if ( (*p) == 125 )
		goto tr30;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st15;
	goto tr3;
tr30:
#line 563 "src/tokens.rl"
	{ tok_end = p; }
	goto st252;
st252:
	if ( ++p == pe )
		goto _test_eof252;
case 252:
#line 1325 "src/tokens.c"
	goto tr320;
st16:
	if ( ++p == pe )
		goto _test_eof16;
case 16:
	if ( (*p) == 108 )
		goto st17;
	goto tr3;
st17:
	if ( ++p == pe )
		goto _test_eof17;
case 17:
	switch( (*p) ) {
		case 97: goto st18;
		case 117: goto st21;
	}
	goto tr3;
st18:
	if ( ++p == pe )
		goto _test_eof18;
case 18:
	if ( (*p) == 99 )
		goto st19;
	goto tr3;
st19:
	if ( ++p == pe )
		goto _test_eof19;
case 19:
	if ( (*p) == 107 )
		goto st20;
	goto tr3;
st20:
	if ( ++p == pe )
		goto _test_eof20;
case 20:
	if ( (*p) == 125 )
		goto st253;
	goto tr3;
st253:
	if ( ++p == pe )
		goto _test_eof253;
case 253:
	goto tr321;
st21:
	if ( ++p == pe )
		goto _test_eof21;
case 21:
	if ( (*p) == 101 )
		goto st22;
	goto tr3;
st22:
	if ( ++p == pe )
		goto _test_eof22;
case 22:
	if ( (*p) == 125 )
		goto st254;
	goto tr3;
st254:
	if ( ++p == pe )
		goto _test_eof254;
case 254:
	goto tr322;
st23:
	if ( ++p == pe )
		goto _test_eof23;
case 23:
	if ( (*p) == 121 )
		goto st24;
	goto tr3;
st24:
	if ( ++p == pe )
		goto _test_eof24;
case 24:
	if ( (*p) == 97 )
		goto st25;
	goto tr3;
st25:
	if ( ++p == pe )
		goto _test_eof25;
case 25:
	if ( (*p) == 110 )
		goto st26;
	goto tr3;
st26:
	if ( ++p == pe )
		goto _test_eof26;
case 26:
	if ( (*p) == 125 )
		goto st255;
	goto tr3;
st255:
	if ( ++p == pe )
		goto _test_eof255;
case 255:
	goto tr323;
st27:
	if ( ++p == pe )
		goto _test_eof27;
case 27:
	if ( (*p) == 114 )
		goto st28;
	goto tr3;
st28:
	if ( ++p == pe )
		goto _test_eof28;
case 28:
	switch( (*p) ) {
		case 97: goto st29;
		case 101: goto st31;
	}
	goto tr3;
st29:
	if ( ++p == pe )
		goto _test_eof29;
case 29:
	if ( (*p) == 121 )
		goto st30;
	goto tr3;
st30:
	if ( ++p == pe )
		goto _test_eof30;
case 30:
	if ( (*p) == 125 )
		goto st256;
	goto tr3;
st256:
	if ( ++p == pe )
		goto _test_eof256;
case 256:
	goto tr324;
st31:
	if ( ++p == pe )
		goto _test_eof31;
case 31:
	switch( (*p) ) {
		case 101: goto st32;
		case 121: goto st30;
	}
	goto tr3;
st32:
	if ( ++p == pe )
		goto _test_eof32;
case 32:
	if ( (*p) == 110 )
		goto st33;
	goto tr3;
st33:
	if ( ++p == pe )
		goto _test_eof33;
case 33:
	if ( (*p) == 125 )
		goto st257;
	goto tr3;
st257:
	if ( ++p == pe )
		goto _test_eof257;
case 257:
	goto tr325;
st34:
	if ( ++p == pe )
		goto _test_eof34;
case 34:
	if ( (*p) == 97 )
		goto st35;
	goto tr3;
st35:
	if ( ++p == pe )
		goto _test_eof35;
case 35:
	if ( (*p) == 103 )
		goto st36;
	goto tr3;
st36:
	if ( ++p == pe )
		goto _test_eof36;
case 36:
	if ( (*p) == 101 )
		goto st37;
	goto tr3;
st37:
	if ( ++p == pe )
		goto _test_eof37;
case 37:
	if ( (*p) == 110 )
		goto st38;
	goto tr3;
st38:
	if ( ++p == pe )
		goto _test_eof38;
case 38:
	if ( (*p) == 116 )
		goto st39;
	goto tr3;
st39:
	if ( ++p == pe )
		goto _test_eof39;
case 39:
	if ( (*p) == 97 )
		goto st40;
	goto tr3;
st40:
	if ( ++p == pe )
		goto _test_eof40;
case 40:
	if ( (*p) == 125 )
		goto st258;
	goto tr3;
st258:
	if ( ++p == pe )
		goto _test_eof258;
case 258:
	goto tr326;
st41:
	if ( ++p == pe )
		goto _test_eof41;
case 41:
	if ( (*p) == 101 )
		goto st42;
	goto tr3;
st42:
	if ( ++p == pe )
		goto _test_eof42;
case 42:
	if ( (*p) == 100 )
		goto st43;
	goto tr3;
st43:
	if ( ++p == pe )
		goto _test_eof43;
case 43:
	if ( (*p) == 125 )
		goto st259;
	goto tr3;
st259:
	if ( ++p == pe )
		goto _test_eof259;
case 259:
	goto tr327;
st44:
	if ( ++p == pe )
		goto _test_eof44;
case 44:
	if ( (*p) == 104 )
		goto st45;
	goto tr3;
st45:
	if ( ++p == pe )
		goto _test_eof45;
case 45:
	if ( (*p) == 105 )
		goto st46;
	goto tr3;
st46:
	if ( ++p == pe )
		goto _test_eof46;
case 46:
	if ( (*p) == 116 )
		goto st47;
	goto tr3;
st47:
	if ( ++p == pe )
		goto _test_eof47;
case 47:
	if ( (*p) == 101 )
		goto st48;
	goto tr3;
st48:
	if ( ++p == pe )
		goto _test_eof48;
case 48:
	if ( (*p) == 125 )
		goto st260;
	goto tr3;
st260:
	if ( ++p == pe )
		goto _test_eof260;
case 260:
	goto tr328;
st49:
	if ( ++p == pe )
		goto _test_eof49;
case 49:
	if ( (*p) == 101 )
		goto st50;
	goto tr3;
st50:
	if ( ++p == pe )
		goto _test_eof50;
case 50:
	if ( (*p) == 108 )
		goto st51;
	goto tr3;
st51:
	if ( ++p == pe )
		goto _test_eof51;
case 51:
	if ( (*p) == 108 )
		goto st52;
	goto tr3;
st52:
	if ( ++p == pe )
		goto _test_eof52;
case 52:
	if ( (*p) == 111 )
		goto st53;
	goto tr3;
st53:
	if ( ++p == pe )
		goto _test_eof53;
case 53:
	if ( (*p) == 119 )
		goto st54;
	goto tr3;
st54:
	if ( ++p == pe )
		goto _test_eof54;
case 54:
	if ( (*p) == 125 )
		goto st261;
	goto tr3;
st261:
	if ( ++p == pe )
		goto _test_eof261;
case 261:
	goto tr329;
st55:
	if ( ++p == pe )
		goto _test_eof55;
case 55:
	switch( (*p) ) {
		case 97: goto st56;
		case 117: goto st59;
	}
	goto tr3;
st56:
	if ( ++p == pe )
		goto _test_eof56;
case 56:
	if ( (*p) == 99 )
		goto st57;
	goto tr3;
st57:
	if ( ++p == pe )
		goto _test_eof57;
case 57:
	if ( (*p) == 107 )
		goto st58;
	goto tr3;
st58:
	if ( ++p == pe )
		goto _test_eof58;
case 58:
	if ( (*p) == 125 )
		goto st262;
	goto tr3;
st262:
	if ( ++p == pe )
		goto _test_eof262;
case 262:
	goto tr330;
st59:
	if ( ++p == pe )
		goto _test_eof59;
case 59:
	if ( (*p) == 101 )
		goto st60;
	goto tr3;
st60:
	if ( ++p == pe )
		goto _test_eof60;
case 60:
	if ( (*p) == 125 )
		goto st263;
	goto tr3;
st263:
	if ( ++p == pe )
		goto _test_eof263;
case 263:
	goto tr331;
st61:
	if ( ++p == pe )
		goto _test_eof61;
case 61:
	if ( (*p) == 108 )
		goto st62;
	goto tr3;
st62:
	if ( ++p == pe )
		goto _test_eof62;
case 62:
	if ( (*p) == 100 )
		goto st9;
	goto tr3;
st63:
	if ( ++p == pe )
		goto _test_eof63;
case 63:
	if ( (*p) == 105 )
		goto st64;
	goto tr3;
st64:
	if ( ++p == pe )
		goto _test_eof64;
case 64:
	if ( (*p) == 103 )
		goto st65;
	goto tr3;
st65:
	if ( ++p == pe )
		goto _test_eof65;
case 65:
	if ( (*p) == 104 )
		goto st66;
	goto tr3;
st66:
	if ( ++p == pe )
		goto _test_eof66;
case 66:
	if ( (*p) == 116 )
		goto st67;
	goto tr3;
st67:
	if ( ++p == pe )
		goto _test_eof67;
case 67:
	switch( (*p) ) {
		case 58: goto st68;
		case 125: goto st272;
	}
	goto tr3;
st68:
	if ( ++p == pe )
		goto _test_eof68;
case 68:
	switch( (*p) ) {
		case 98: goto st69;
		case 99: goto st76;
		case 103: goto st80;
		case 109: goto st85;
		case 114: goto st92;
		case 119: goto st95;
		case 121: goto st100;
	}
	goto tr3;
st69:
	if ( ++p == pe )
		goto _test_eof69;
case 69:
	if ( (*p) == 108 )
		goto st70;
	goto tr3;
st70:
	if ( ++p == pe )
		goto _test_eof70;
case 70:
	switch( (*p) ) {
		case 97: goto st71;
		case 117: goto st74;
	}
	goto tr3;
st71:
	if ( ++p == pe )
		goto _test_eof71;
case 71:
	if ( (*p) == 99 )
		goto st72;
	goto tr3;
st72:
	if ( ++p == pe )
		goto _test_eof72;
case 72:
	if ( (*p) == 107 )
		goto st73;
	goto tr3;
st73:
	if ( ++p == pe )
		goto _test_eof73;
case 73:
	if ( (*p) == 125 )
		goto st264;
	goto tr3;
st264:
	if ( ++p == pe )
		goto _test_eof264;
case 264:
	goto tr332;
st74:
	if ( ++p == pe )
		goto _test_eof74;
case 74:
	if ( (*p) == 101 )
		goto st75;
	goto tr3;
st75:
	if ( ++p == pe )
		goto _test_eof75;
case 75:
	if ( (*p) == 125 )
		goto st265;
	goto tr3;
st265:
	if ( ++p == pe )
		goto _test_eof265;
case 265:
	goto tr333;
st76:
	if ( ++p == pe )
		goto _test_eof76;
case 76:
	if ( (*p) == 121 )
		goto st77;
	goto tr3;
st77:
	if ( ++p == pe )
		goto _test_eof77;
case 77:
	if ( (*p) == 97 )
		goto st78;
	goto tr3;
st78:
	if ( ++p == pe )
		goto _test_eof78;
case 78:
	if ( (*p) == 110 )
		goto st79;
	goto tr3;
st79:
	if ( ++p == pe )
		goto _test_eof79;
case 79:
	if ( (*p) == 125 )
		goto st266;
	goto tr3;
st266:
	if ( ++p == pe )
		goto _test_eof266;
case 266:
	goto tr334;
st80:
	if ( ++p == pe )
		goto _test_eof80;
case 80:
	if ( (*p) == 114 )
		goto st81;
	goto tr3;
st81:
	if ( ++p == pe )
		goto _test_eof81;
case 81:
	if ( (*p) == 101 )
		goto st82;
	goto tr3;
st82:
	if ( ++p == pe )
		goto _test_eof82;
case 82:
	if ( (*p) == 101 )
		goto st83;
	goto tr3;
st83:
	if ( ++p == pe )
		goto _test_eof83;
case 83:
	if ( (*p) == 110 )
		goto st84;
	goto tr3;
st84:
	if ( ++p == pe )
		goto _test_eof84;
case 84:
	if ( (*p) == 125 )
		goto st267;
	goto tr3;
st267:
	if ( ++p == pe )
		goto _test_eof267;
case 267:
	goto tr335;
st85:
	if ( ++p == pe )
		goto _test_eof85;
case 85:
	if ( (*p) == 97 )
		goto st86;
	goto tr3;
st86:
	if ( ++p == pe )
		goto _test_eof86;
case 86:
	if ( (*p) == 103 )
		goto st87;
	goto tr3;
st87:
	if ( ++p == pe )
		goto _test_eof87;
case 87:
	if ( (*p) == 101 )
		goto st88;
	goto tr3;
st88:
	if ( ++p == pe )
		goto _test_eof88;
case 88:
	if ( (*p) == 110 )
		goto st89;
	goto tr3;
st89:
	if ( ++p == pe )
		goto _test_eof89;
case 89:
	if ( (*p) == 116 )
		goto st90;
	goto tr3;
st90:
	if ( ++p == pe )
		goto _test_eof90;
case 90:
	if ( (*p) == 97 )
		goto st91;
	goto tr3;
st91:
	if ( ++p == pe )
		goto _test_eof91;
case 91:
	if ( (*p) == 125 )
		goto st268;
	goto tr3;
st268:
	if ( ++p == pe )
		goto _test_eof268;
case 268:
	goto tr336;
st92:
	if ( ++p == pe )
		goto _test_eof92;
case 92:
	if ( (*p) == 101 )
		goto st93;
	goto tr3;
st93:
	if ( ++p == pe )
		goto _test_eof93;
case 93:
	if ( (*p) == 100 )
		goto st94;
	goto tr3;
st94:
	if ( ++p == pe )
		goto _test_eof94;
case 94:
	if ( (*p) == 125 )
		goto st269;
	goto tr3;
st269:
	if ( ++p == pe )
		goto _test_eof269;
case 269:
	goto tr337;
st95:
	if ( ++p == pe )
		goto _test_eof95;
case 95:
	if ( (*p) == 104 )
		goto st96;
	goto tr3;
st96:
	if ( ++p == pe )
		goto _test_eof96;
case 96:
	if ( (*p) == 105 )
		goto st97;
	goto tr3;
st97:
	if ( ++p == pe )
		goto _test_eof97;
case 97:
	if ( (*p) == 116 )
		goto st98;
	goto tr3;
st98:
	if ( ++p == pe )
		goto _test_eof98;
case 98:
	if ( (*p) == 101 )
		goto st99;
	goto tr3;
st99:
	if ( ++p == pe )
		goto _test_eof99;
case 99:
	if ( (*p) == 125 )
		goto st270;
	goto tr3;
st270:
	if ( ++p == pe )
		goto _test_eof270;
case 270:
	goto tr338;
st100:
	if ( ++p == pe )
		goto _test_eof100;
case 100:
	if ( (*p) == 101 )
		goto st101;
	goto tr3;
st101:
	if ( ++p == pe )
		goto _test_eof101;
case 101:
	if ( (*p) == 108 )
		goto st102;
	goto tr3;
st102:
	if ( ++p == pe )
		goto _test_eof102;
case 102:
	if ( (*p) == 108 )
		goto st103;
	goto tr3;
st103:
	if ( ++p == pe )
		goto _test_eof103;
case 103:
	if ( (*p) == 111 )
		goto st104;
	goto tr3;
st104:
	if ( ++p == pe )
		goto _test_eof104;
case 104:
	if ( (*p) == 119 )
		goto st105;
	goto tr3;
st105:
	if ( ++p == pe )
		goto _test_eof105;
case 105:
	if ( (*p) == 125 )
		goto st271;
	goto tr3;
st271:
	if ( ++p == pe )
		goto _test_eof271;
case 271:
	goto tr339;
st272:
	if ( ++p == pe )
		goto _test_eof272;
case 272:
	goto tr340;
st273:
	if ( ++p == pe )
		goto _test_eof273;
case 273:
	goto tr341;
st106:
	if ( ++p == pe )
		goto _test_eof106;
case 106:
	switch( (*p) ) {
		case 108: goto st107;
		case 117: goto st110;
		case 121: goto st115;
	}
	goto tr3;
st107:
	if ( ++p == pe )
		goto _test_eof107;
case 107:
	switch( (*p) ) {
		case 114: goto st108;
		case 115: goto st109;
	}
	goto tr3;
st108:
	if ( ++p == pe )
		goto _test_eof108;
case 108:
	if ( (*p) == 125 )
		goto st274;
	goto tr3;
st274:
	if ( ++p == pe )
		goto _test_eof274;
case 274:
	goto tr342;
st109:
	if ( ++p == pe )
		goto _test_eof109;
case 109:
	if ( (*p) == 125 )
		goto st275;
	goto tr3;
st275:
	if ( ++p == pe )
		goto _test_eof275;
case 275:
	goto tr343;
st110:
	if ( ++p == pe )
		goto _test_eof110;
case 110:
	if ( (*p) == 114 )
		goto st111;
	goto tr3;
st111:
	if ( ++p == pe )
		goto _test_eof111;
case 111:
	if ( (*p) == 115 )
		goto st112;
	goto tr3;
st112:
	if ( ++p == pe )
		goto _test_eof112;
case 112:
	if ( (*p) == 111 )
		goto st113;
	goto tr3;
st113:
	if ( ++p == pe )
		goto _test_eof113;
case 113:
	if ( (*p) == 114 )
		goto st114;
	goto tr3;
st114:
	if ( ++p == pe )
		goto _test_eof114;
case 114:
	if ( (*p) == 125 )
		goto st276;
	goto tr3;
st276:
	if ( ++p == pe )
		goto _test_eof276;
case 276:
	goto tr344;
st115:
	if ( ++p == pe )
		goto _test_eof115;
case 115:
	if ( (*p) == 97 )
		goto st116;
	goto tr3;
st116:
	if ( ++p == pe )
		goto _test_eof116;
case 116:
	if ( (*p) == 110 )
		goto st117;
	goto tr3;
st117:
	if ( ++p == pe )
		goto _test_eof117;
case 117:
	if ( (*p) == 125 )
		goto st277;
	goto tr3;
st277:
	if ( ++p == pe )
		goto _test_eof277;
case 277:
	goto tr345;
st118:
	if ( ++p == pe )
		goto _test_eof118;
case 118:
	switch( (*p) ) {
		case 97: goto st119;
		case 105: goto st126;
	}
	goto tr3;
st119:
	if ( ++p == pe )
		goto _test_eof119;
case 119:
	switch( (*p) ) {
		case 110: goto st120;
		case 114: goto st124;
	}
	goto tr3;
st120:
	if ( ++p == pe )
		goto _test_eof120;
case 120:
	if ( (*p) == 103 )
		goto st121;
	goto tr3;
st121:
	if ( ++p == pe )
		goto _test_eof121;
case 121:
	if ( (*p) == 101 )
		goto st122;
	goto tr3;
st122:
	if ( ++p == pe )
		goto _test_eof122;
case 122:
	if ( (*p) == 114 )
		goto st123;
	goto tr3;
st123:
	if ( ++p == pe )
		goto _test_eof123;
case 123:
	if ( (*p) == 125 )
		goto st278;
	goto tr3;
st278:
	if ( ++p == pe )
		goto _test_eof278;
case 278:
	goto tr346;
st124:
	if ( ++p == pe )
		goto _test_eof124;
case 124:
	if ( (*p) == 107 )
		goto st125;
	goto tr3;
st125:
	if ( ++p == pe )
		goto _test_eof125;
case 125:
	if ( (*p) == 125 )
		goto st279;
	goto tr3;
st279:
	if ( ++p == pe )
		goto _test_eof279;
case 279:
	goto tr347;
st126:
	if ( ++p == pe )
		goto _test_eof126;
case 126:
	if ( (*p) == 109 )
		goto st127;
	goto tr3;
st127:
	if ( ++p == pe )
		goto _test_eof127;
case 127:
	if ( (*p) == 125 )
		goto st280;
	goto tr3;
st280:
	if ( ++p == pe )
		goto _test_eof280;
case 280:
	goto tr348;
st128:
	if ( ++p == pe )
		goto _test_eof128;
case 128:
	if ( (*p) == 103 )
		goto st129;
	goto tr3;
st129:
	if ( ++p == pe )
		goto _test_eof129;
case 129:
	if ( (*p) == 58 )
		goto st130;
	goto tr3;
st130:
	if ( ++p == pe )
		goto _test_eof130;
case 130:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr161;
	goto tr3;
tr161:
#line 562 "src/tokens.rl"
	{ tok_start = p; }
	goto st131;
st131:
	if ( ++p == pe )
		goto _test_eof131;
case 131:
#line 2308 "src/tokens.c"
	if ( (*p) == 125 )
		goto tr163;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st131;
	goto tr3;
tr163:
#line 563 "src/tokens.rl"
	{ tok_end = p; }
	goto st281;
st281:
	if ( ++p == pe )
		goto _test_eof281;
case 281:
#line 2322 "src/tokens.c"
	goto tr349;
st132:
	if ( ++p == pe )
		goto _test_eof132;
case 132:
	switch( (*p) ) {
		case 111: goto st133;
		case 114: goto st147;
	}
	goto tr3;
st133:
	if ( ++p == pe )
		goto _test_eof133;
case 133:
	if ( (*p) == 116 )
		goto st134;
	goto tr3;
st134:
	if ( ++p == pe )
		goto _test_eof134;
case 134:
	if ( (*p) == 111 )
		goto st135;
	goto tr3;
st135:
	if ( ++p == pe )
		goto _test_eof135;
case 135:
	switch( (*p) ) {
		case 58: goto st136;
		case 95: goto st140;
	}
	goto tr3;
st136:
	if ( ++p == pe )
		goto _test_eof136;
case 136:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr170;
	goto tr3;
tr170:
#line 755 "src/tokens.rl"
	{ goto_row_start = p; }
	goto st137;
st137:
	if ( ++p == pe )
		goto _test_eof137;
case 137:
#line 2371 "src/tokens.c"
	if ( (*p) == 44 )
		goto tr171;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st137;
	goto tr3;
tr171:
#line 756 "src/tokens.rl"
	{ goto_row_end = p; }
	goto st138;
st138:
	if ( ++p == pe )
		goto _test_eof138;
case 138:
#line 2385 "src/tokens.c"
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr173;
	goto tr3;
tr173:
#line 757 "src/tokens.rl"
	{ goto_col_start = p; }
	goto st139;
st139:
	if ( ++p == pe )
		goto _test_eof139;
case 139:
#line 2397 "src/tokens.c"
	if ( (*p) == 125 )
		goto tr175;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st139;
	goto tr3;
tr175:
#line 758 "src/tokens.rl"
	{ goto_col_end = p; }
	goto st282;
st282:
	if ( ++p == pe )
		goto _test_eof282;
case 282:
#line 2411 "src/tokens.c"
	goto tr350;
st140:
	if ( ++p == pe )
		goto _test_eof140;
case 140:
	if ( (*p) == 99 )
		goto st141;
	goto tr3;
st141:
	if ( ++p == pe )
		goto _test_eof141;
case 141:
	if ( (*p) == 117 )
		goto st142;
	goto tr3;
st142:
	if ( ++p == pe )
		goto _test_eof142;
case 142:
	if ( (*p) == 114 )
		goto st143;
	goto tr3;
st143:
	if ( ++p == pe )
		goto _test_eof143;
case 143:
	if ( (*p) == 115 )
		goto st144;
	goto tr3;
st144:
	if ( ++p == pe )
		goto _test_eof144;
case 144:
	if ( (*p) == 111 )
		goto st145;
	goto tr3;
st145:
	if ( ++p == pe )
		goto _test_eof145;
case 145:
	if ( (*p) == 114 )
		goto st146;
	goto tr3;
st146:
	if ( ++p == pe )
		goto _test_eof146;
case 146:
	if ( (*p) == 125 )
		goto st283;
	goto tr3;
st283:
	if ( ++p == pe )
		goto _test_eof283;
case 283:
	goto tr351;
st147:
	if ( ++p == pe )
		goto _test_eof147;
case 147:
	switch( (*p) ) {
		case 97: goto st148;
		case 101: goto st150;
	}
	goto tr3;
st148:
	if ( ++p == pe )
		goto _test_eof148;
case 148:
	if ( (*p) == 121 )
		goto st149;
	goto tr3;
st149:
	if ( ++p == pe )
		goto _test_eof149;
case 149:
	if ( (*p) == 125 )
		goto st284;
	goto tr3;
st284:
	if ( ++p == pe )
		goto _test_eof284;
case 284:
	goto tr352;
st150:
	if ( ++p == pe )
		goto _test_eof150;
case 150:
	switch( (*p) ) {
		case 101: goto st151;
		case 121: goto st149;
	}
	goto tr3;
st151:
	if ( ++p == pe )
		goto _test_eof151;
case 151:
	if ( (*p) == 110 )
		goto st152;
	goto tr3;
st152:
	if ( ++p == pe )
		goto _test_eof152;
case 152:
	if ( (*p) == 125 )
		goto st285;
	goto tr3;
st285:
	if ( ++p == pe )
		goto _test_eof285;
case 285:
	goto tr353;
st153:
	if ( ++p == pe )
		goto _test_eof153;
case 153:
	switch( (*p) ) {
		case 49: goto st154;
		case 50: goto st155;
		case 51: goto st156;
		case 52: goto st157;
		case 53: goto st158;
		case 54: goto st159;
		case 105: goto st160;
		case 111: goto st170;
	}
	goto tr3;
st154:
	if ( ++p == pe )
		goto _test_eof154;
case 154:
	if ( (*p) == 125 )
		goto st286;
	goto tr3;
st286:
	if ( ++p == pe )
		goto _test_eof286;
case 286:
	goto tr354;
st155:
	if ( ++p == pe )
		goto _test_eof155;
case 155:
	if ( (*p) == 125 )
		goto st287;
	goto tr3;
st287:
	if ( ++p == pe )
		goto _test_eof287;
case 287:
	goto tr355;
st156:
	if ( ++p == pe )
		goto _test_eof156;
case 156:
	if ( (*p) == 125 )
		goto st288;
	goto tr3;
st288:
	if ( ++p == pe )
		goto _test_eof288;
case 288:
	goto tr356;
st157:
	if ( ++p == pe )
		goto _test_eof157;
case 157:
	if ( (*p) == 125 )
		goto st289;
	goto tr3;
st289:
	if ( ++p == pe )
		goto _test_eof289;
case 289:
	goto tr357;
st158:
	if ( ++p == pe )
		goto _test_eof158;
case 158:
	if ( (*p) == 125 )
		goto st290;
	goto tr3;
st290:
	if ( ++p == pe )
		goto _test_eof290;
case 290:
	goto tr358;
st159:
	if ( ++p == pe )
		goto _test_eof159;
case 159:
	if ( (*p) == 125 )
		goto st291;
	goto tr3;
st291:
	if ( ++p == pe )
		goto _test_eof291;
case 291:
	goto tr359;
st160:
	if ( ++p == pe )
		goto _test_eof160;
case 160:
	switch( (*p) ) {
		case 100: goto st161;
		case 103: goto st163;
	}
	goto tr3;
st161:
	if ( ++p == pe )
		goto _test_eof161;
case 161:
	if ( (*p) == 101 )
		goto st162;
	goto tr3;
st162:
	if ( ++p == pe )
		goto _test_eof162;
case 162:
	if ( (*p) == 125 )
		goto st292;
	goto tr3;
st292:
	if ( ++p == pe )
		goto _test_eof292;
case 292:
	goto tr360;
st163:
	if ( ++p == pe )
		goto _test_eof163;
case 163:
	if ( (*p) == 104 )
		goto st164;
	goto tr3;
st164:
	if ( ++p == pe )
		goto _test_eof164;
case 164:
	if ( (*p) == 108 )
		goto st165;
	goto tr3;
st165:
	if ( ++p == pe )
		goto _test_eof165;
case 165:
	if ( (*p) == 105 )
		goto st166;
	goto tr3;
st166:
	if ( ++p == pe )
		goto _test_eof166;
case 166:
	if ( (*p) == 103 )
		goto st167;
	goto tr3;
st167:
	if ( ++p == pe )
		goto _test_eof167;
case 167:
	if ( (*p) == 104 )
		goto st168;
	goto tr3;
st168:
	if ( ++p == pe )
		goto _test_eof168;
case 168:
	if ( (*p) == 116 )
		goto st169;
	goto tr3;
st169:
	if ( ++p == pe )
		goto _test_eof169;
case 169:
	if ( (*p) == 125 )
		goto st293;
	goto tr3;
st293:
	if ( ++p == pe )
		goto _test_eof293;
case 293:
	goto tr361;
st170:
	if ( ++p == pe )
		goto _test_eof170;
case 170:
	if ( (*p) == 109 )
		goto st171;
	goto tr3;
st171:
	if ( ++p == pe )
		goto _test_eof171;
case 171:
	if ( (*p) == 101 )
		goto st172;
	goto tr3;
st172:
	if ( ++p == pe )
		goto _test_eof172;
case 172:
	if ( (*p) == 125 )
		goto st294;
	goto tr3;
st294:
	if ( ++p == pe )
		goto _test_eof294;
case 294:
	goto tr362;
st173:
	if ( ++p == pe )
		goto _test_eof173;
case 173:
	switch( (*p) ) {
		case 116: goto st174;
		case 125: goto st250;
	}
	goto tr3;
st174:
	if ( ++p == pe )
		goto _test_eof174;
case 174:
	if ( (*p) == 97 )
		goto st175;
	goto tr3;
st175:
	if ( ++p == pe )
		goto _test_eof175;
case 175:
	if ( (*p) == 108 )
		goto st176;
	goto tr3;
st176:
	if ( ++p == pe )
		goto _test_eof176;
case 176:
	if ( (*p) == 105 )
		goto st177;
	goto tr3;
st177:
	if ( ++p == pe )
		goto _test_eof177;
case 177:
	if ( (*p) == 99 )
		goto st10;
	goto tr3;
st178:
	if ( ++p == pe )
		goto _test_eof178;
case 178:
	if ( (*p) == 97 )
		goto st179;
	goto tr3;
st179:
	if ( ++p == pe )
		goto _test_eof179;
case 179:
	if ( (*p) == 103 )
		goto st180;
	goto tr3;
st180:
	if ( ++p == pe )
		goto _test_eof180;
case 180:
	if ( (*p) == 101 )
		goto st181;
	goto tr3;
st181:
	if ( ++p == pe )
		goto _test_eof181;
case 181:
	if ( (*p) == 110 )
		goto st182;
	goto tr3;
st182:
	if ( ++p == pe )
		goto _test_eof182;
case 182:
	if ( (*p) == 116 )
		goto st183;
	goto tr3;
st183:
	if ( ++p == pe )
		goto _test_eof183;
case 183:
	if ( (*p) == 97 )
		goto st184;
	goto tr3;
st184:
	if ( ++p == pe )
		goto _test_eof184;
case 184:
	if ( (*p) == 125 )
		goto st295;
	goto tr3;
st295:
	if ( ++p == pe )
		goto _test_eof295;
case 295:
	goto tr363;
st185:
	if ( ++p == pe )
		goto _test_eof185;
case 185:
	if ( (*p) == 101 )
		goto st186;
	goto tr3;
st186:
	if ( ++p == pe )
		goto _test_eof186;
case 186:
	switch( (*p) ) {
		case 100: goto st187;
		case 115: goto st188;
		case 118: goto st191;
	}
	goto tr3;
st187:
	if ( ++p == pe )
		goto _test_eof187;
case 187:
	if ( (*p) == 125 )
		goto st296;
	goto tr3;
st296:
	if ( ++p == pe )
		goto _test_eof296;
case 296:
	goto tr364;
st188:
	if ( ++p == pe )
		goto _test_eof188;
case 188:
	if ( (*p) == 101 )
		goto st189;
	goto tr3;
st189:
	if ( ++p == pe )
		goto _test_eof189;
case 189:
	if ( (*p) == 116 )
		goto st190;
	goto tr3;
st190:
	if ( ++p == pe )
		goto _test_eof190;
case 190:
	if ( (*p) == 125 )
		goto st297;
	goto tr3;
st297:
	if ( ++p == pe )
		goto _test_eof297;
case 297:
	goto tr365;
st191:
	if ( ++p == pe )
		goto _test_eof191;
case 191:
	if ( (*p) == 101 )
		goto st192;
	goto tr3;
st192:
	if ( ++p == pe )
		goto _test_eof192;
case 192:
	if ( (*p) == 114 )
		goto st193;
	goto tr3;
st193:
	if ( ++p == pe )
		goto _test_eof193;
case 193:
	if ( (*p) == 115 )
		goto st194;
	goto tr3;
st194:
	if ( ++p == pe )
		goto _test_eof194;
case 194:
	if ( (*p) == 101 )
		goto st195;
	goto tr3;
st195:
	if ( ++p == pe )
		goto _test_eof195;
case 195:
	if ( (*p) == 125 )
		goto st298;
	goto tr3;
st298:
	if ( ++p == pe )
		goto _test_eof298;
case 298:
	goto tr366;
st196:
	if ( ++p == pe )
		goto _test_eof196;
case 196:
	switch( (*p) ) {
		case 101: goto st197;
		case 104: goto st203;
		case 116: goto st206;
	}
	goto tr3;
st197:
	if ( ++p == pe )
		goto _test_eof197;
case 197:
	if ( (*p) == 99 )
		goto st198;
	goto tr3;
st198:
	if ( ++p == pe )
		goto _test_eof198;
case 198:
	if ( (*p) == 116 )
		goto st199;
	goto tr3;
st199:
	if ( ++p == pe )
		goto _test_eof199;
case 199:
	if ( (*p) == 105 )
		goto st200;
	goto tr3;
st200:
	if ( ++p == pe )
		goto _test_eof200;
case 200:
	if ( (*p) == 111 )
		goto st201;
	goto tr3;
st201:
	if ( ++p == pe )
		goto _test_eof201;
case 201:
	if ( (*p) == 110 )
		goto st202;
	goto tr3;
st202:
	if ( ++p == pe )
		goto _test_eof202;
case 202:
	if ( (*p) == 125 )
		goto st299;
	goto tr3;
st299:
	if ( ++p == pe )
		goto _test_eof299;
case 299:
	goto tr367;
st203:
	if ( ++p == pe )
		goto _test_eof203;
case 203:
	if ( (*p) == 111 )
		goto st204;
	goto tr3;
st204:
	if ( ++p == pe )
		goto _test_eof204;
case 204:
	if ( (*p) == 119 )
		goto st205;
	goto tr3;
st205:
	if ( ++p == pe )
		goto _test_eof205;
case 205:
	if ( (*p) == 125 )
		goto st300;
	goto tr3;
st300:
	if ( ++p == pe )
		goto _test_eof300;
case 300:
	goto tr368;
st206:
	if ( ++p == pe )
		goto _test_eof206;
case 206:
	if ( (*p) == 114 )
		goto st207;
	goto tr3;
st207:
	if ( ++p == pe )
		goto _test_eof207;
case 207:
	switch( (*p) ) {
		case 105: goto st208;
		case 111: goto st218;
	}
	goto tr3;
st208:
	if ( ++p == pe )
		goto _test_eof208;
case 208:
	if ( (*p) == 107 )
		goto st209;
	goto tr3;
st209:
	if ( ++p == pe )
		goto _test_eof209;
case 209:
	if ( (*p) == 101 )
		goto st210;
	goto tr3;
st210:
	if ( ++p == pe )
		goto _test_eof210;
case 210:
	switch( (*p) ) {
		case 116: goto st211;
		case 125: goto st301;
	}
	goto tr3;
st211:
	if ( ++p == pe )
		goto _test_eof211;
case 211:
	if ( (*p) == 104 )
		goto st212;
	goto tr3;
st212:
	if ( ++p == pe )
		goto _test_eof212;
case 212:
	if ( (*p) == 114 )
		goto st213;
	goto tr3;
st213:
	if ( ++p == pe )
		goto _test_eof213;
case 213:
	if ( (*p) == 111 )
		goto st214;
	goto tr3;
st214:
	if ( ++p == pe )
		goto _test_eof214;
case 214:
	if ( (*p) == 117 )
		goto st215;
	goto tr3;
st215:
	if ( ++p == pe )
		goto _test_eof215;
case 215:
	if ( (*p) == 103 )
		goto st216;
	goto tr3;
st216:
	if ( ++p == pe )
		goto _test_eof216;
case 216:
	if ( (*p) == 104 )
		goto st217;
	goto tr3;
st217:
	if ( ++p == pe )
		goto _test_eof217;
case 217:
	if ( (*p) == 125 )
		goto st301;
	goto tr3;
st301:
	if ( ++p == pe )
		goto _test_eof301;
case 301:
	goto tr369;
st218:
	if ( ++p == pe )
		goto _test_eof218;
case 218:
	if ( (*p) == 110 )
		goto st219;
	goto tr3;
st219:
	if ( ++p == pe )
		goto _test_eof219;
case 219:
	if ( (*p) == 103 )
		goto st220;
	goto tr3;
st220:
	if ( ++p == pe )
		goto _test_eof220;
case 220:
	if ( (*p) == 125 )
		goto st302;
	goto tr3;
st302:
	if ( ++p == pe )
		goto _test_eof302;
case 302:
	goto tr370;
st221:
	if ( ++p == pe )
		goto _test_eof221;
case 221:
	if ( (*p) == 101 )
		goto st222;
	goto tr3;
st222:
	if ( ++p == pe )
		goto _test_eof222;
case 222:
	if ( (*p) == 120 )
		goto st223;
	goto tr3;
st223:
	if ( ++p == pe )
		goto _test_eof223;
case 223:
	if ( (*p) == 116 )
		goto st224;
	goto tr3;
st224:
	if ( ++p == pe )
		goto _test_eof224;
case 224:
	if ( (*p) == 125 )
		goto st303;
	goto tr3;
st303:
	if ( ++p == pe )
		goto _test_eof303;
case 303:
	goto tr371;
st225:
	if ( ++p == pe )
		goto _test_eof225;
case 225:
	switch( (*p) ) {
		case 110: goto st226;
		case 125: goto st251;
	}
	goto tr3;
st226:
	if ( ++p == pe )
		goto _test_eof226;
case 226:
	if ( (*p) == 100 )
		goto st227;
	goto tr3;
st227:
	if ( ++p == pe )
		goto _test_eof227;
case 227:
	if ( (*p) == 101 )
		goto st228;
	goto tr3;
st228:
	if ( ++p == pe )
		goto _test_eof228;
case 228:
	if ( (*p) == 114 )
		goto st229;
	goto tr3;
st229:
	if ( ++p == pe )
		goto _test_eof229;
case 229:
	if ( (*p) == 108 )
		goto st230;
	goto tr3;
st230:
	if ( ++p == pe )
		goto _test_eof230;
case 230:
	if ( (*p) == 105 )
		goto st231;
	goto tr3;
st231:
	if ( ++p == pe )
		goto _test_eof231;
case 231:
	if ( (*p) == 110 )
		goto st232;
	goto tr3;
st232:
	if ( ++p == pe )
		goto _test_eof232;
case 232:
	if ( (*p) == 101 )
		goto st11;
	goto tr3;
st233:
	if ( ++p == pe )
		goto _test_eof233;
case 233:
	if ( (*p) == 104 )
		goto st234;
	goto tr3;
st234:
	if ( ++p == pe )
		goto _test_eof234;
case 234:
	if ( (*p) == 105 )
		goto st235;
	goto tr3;
st235:
	if ( ++p == pe )
		goto _test_eof235;
case 235:
	if ( (*p) == 116 )
		goto st236;
	goto tr3;
st236:
	if ( ++p == pe )
		goto _test_eof236;
case 236:
	if ( (*p) == 101 )
		goto st237;
	goto tr3;
st237:
	if ( ++p == pe )
		goto _test_eof237;
case 237:
	if ( (*p) == 125 )
		goto st304;
	goto tr3;
st304:
	if ( ++p == pe )
		goto _test_eof304;
case 304:
	goto tr372;
st238:
	if ( ++p == pe )
		goto _test_eof238;
case 238:
	if ( (*p) == 101 )
		goto st239;
	goto tr3;
st239:
	if ( ++p == pe )
		goto _test_eof239;
case 239:
	if ( (*p) == 108 )
		goto st240;
	goto tr3;
st240:
	if ( ++p == pe )
		goto _test_eof240;
case 240:
	if ( (*p) == 108 )
		goto st241;
	goto tr3;
st241:
	if ( ++p == pe )
		goto _test_eof241;
case 241:
	if ( (*p) == 111 )
		goto st242;
	goto tr3;
st242:
	if ( ++p == pe )
		goto _test_eof242;
case 242:
	if ( (*p) == 119 )
		goto st243;
	goto tr3;
st243:
	if ( ++p == pe )
		goto _test_eof243;
case 243:
	if ( (*p) == 125 )
		goto st305;
	goto tr3;
st305:
	if ( ++p == pe )
		goto _test_eof305;
case 305:
	goto tr373;
	}
	_test_eof244: cs = 244; goto _test_eof; 
	_test_eof1: cs = 1; goto _test_eof; 
	_test_eof2: cs = 2; goto _test_eof; 
	_test_eof245: cs = 245; goto _test_eof; 
	_test_eof3: cs = 3; goto _test_eof; 
	_test_eof4: cs = 4; goto _test_eof; 
	_test_eof246: cs = 246; goto _test_eof; 
	_test_eof5: cs = 5; goto _test_eof; 
	_test_eof6: cs = 6; goto _test_eof; 
	_test_eof247: cs = 247; goto _test_eof; 
	_test_eof7: cs = 7; goto _test_eof; 
	_test_eof8: cs = 8; goto _test_eof; 
	_test_eof248: cs = 248; goto _test_eof; 
	_test_eof9: cs = 9; goto _test_eof; 
	_test_eof249: cs = 249; goto _test_eof; 
	_test_eof10: cs = 10; goto _test_eof; 
	_test_eof250: cs = 250; goto _test_eof; 
	_test_eof11: cs = 11; goto _test_eof; 
	_test_eof251: cs = 251; goto _test_eof; 
	_test_eof12: cs = 12; goto _test_eof; 
	_test_eof13: cs = 13; goto _test_eof; 
	_test_eof14: cs = 14; goto _test_eof; 
	_test_eof15: cs = 15; goto _test_eof; 
	_test_eof252: cs = 252; goto _test_eof; 
	_test_eof16: cs = 16; goto _test_eof; 
	_test_eof17: cs = 17; goto _test_eof; 
	_test_eof18: cs = 18; goto _test_eof; 
	_test_eof19: cs = 19; goto _test_eof; 
	_test_eof20: cs = 20; goto _test_eof; 
	_test_eof253: cs = 253; goto _test_eof; 
	_test_eof21: cs = 21; goto _test_eof; 
	_test_eof22: cs = 22; goto _test_eof; 
	_test_eof254: cs = 254; goto _test_eof; 
	_test_eof23: cs = 23; goto _test_eof; 
	_test_eof24: cs = 24; goto _test_eof; 
	_test_eof25: cs = 25; goto _test_eof; 
	_test_eof26: cs = 26; goto _test_eof; 
	_test_eof255: cs = 255; goto _test_eof; 
	_test_eof27: cs = 27; goto _test_eof; 
	_test_eof28: cs = 28; goto _test_eof; 
	_test_eof29: cs = 29; goto _test_eof; 
	_test_eof30: cs = 30; goto _test_eof; 
	_test_eof256: cs = 256; goto _test_eof; 
	_test_eof31: cs = 31; goto _test_eof; 
	_test_eof32: cs = 32; goto _test_eof; 
	_test_eof33: cs = 33; goto _test_eof; 
	_test_eof257: cs = 257; goto _test_eof; 
	_test_eof34: cs = 34; goto _test_eof; 
	_test_eof35: cs = 35; goto _test_eof; 
	_test_eof36: cs = 36; goto _test_eof; 
	_test_eof37: cs = 37; goto _test_eof; 
	_test_eof38: cs = 38; goto _test_eof; 
	_test_eof39: cs = 39; goto _test_eof; 
	_test_eof40: cs = 40; goto _test_eof; 
	_test_eof258: cs = 258; goto _test_eof; 
	_test_eof41: cs = 41; goto _test_eof; 
	_test_eof42: cs = 42; goto _test_eof; 
	_test_eof43: cs = 43; goto _test_eof; 
	_test_eof259: cs = 259; goto _test_eof; 
	_test_eof44: cs = 44; goto _test_eof; 
	_test_eof45: cs = 45; goto _test_eof; 
	_test_eof46: cs = 46; goto _test_eof; 
	_test_eof47: cs = 47; goto _test_eof; 
	_test_eof48: cs = 48; goto _test_eof; 
	_test_eof260: cs = 260; goto _test_eof; 
	_test_eof49: cs = 49; goto _test_eof; 
	_test_eof50: cs = 50; goto _test_eof; 
	_test_eof51: cs = 51; goto _test_eof; 
	_test_eof52: cs = 52; goto _test_eof; 
	_test_eof53: cs = 53; goto _test_eof; 
	_test_eof54: cs = 54; goto _test_eof; 
	_test_eof261: cs = 261; goto _test_eof; 
	_test_eof55: cs = 55; goto _test_eof; 
	_test_eof56: cs = 56; goto _test_eof; 
	_test_eof57: cs = 57; goto _test_eof; 
	_test_eof58: cs = 58; goto _test_eof; 
	_test_eof262: cs = 262; goto _test_eof; 
	_test_eof59: cs = 59; goto _test_eof; 
	_test_eof60: cs = 60; goto _test_eof; 
	_test_eof263: cs = 263; goto _test_eof; 
	_test_eof61: cs = 61; goto _test_eof; 
	_test_eof62: cs = 62; goto _test_eof; 
	_test_eof63: cs = 63; goto _test_eof; 
	_test_eof64: cs = 64; goto _test_eof; 
	_test_eof65: cs = 65; goto _test_eof; 
	_test_eof66: cs = 66; goto _test_eof; 
	_test_eof67: cs = 67; goto _test_eof; 
	_test_eof68: cs = 68; goto _test_eof; 
	_test_eof69: cs = 69; goto _test_eof; 
	_test_eof70: cs = 70; goto _test_eof; 
	_test_eof71: cs = 71; goto _test_eof; 
	_test_eof72: cs = 72; goto _test_eof; 
	_test_eof73: cs = 73; goto _test_eof; 
	_test_eof264: cs = 264; goto _test_eof; 
	_test_eof74: cs = 74; goto _test_eof; 
	_test_eof75: cs = 75; goto _test_eof; 
	_test_eof265: cs = 265; goto _test_eof; 
	_test_eof76: cs = 76; goto _test_eof; 
	_test_eof77: cs = 77; goto _test_eof; 
	_test_eof78: cs = 78; goto _test_eof; 
	_test_eof79: cs = 79; goto _test_eof; 
	_test_eof266: cs = 266; goto _test_eof; 
	_test_eof80: cs = 80; goto _test_eof; 
	_test_eof81: cs = 81; goto _test_eof; 
	_test_eof82: cs = 82; goto _test_eof; 
	_test_eof83: cs = 83; goto _test_eof; 
	_test_eof84: cs = 84; goto _test_eof; 
	_test_eof267: cs = 267; goto _test_eof; 
	_test_eof85: cs = 85; goto _test_eof; 
	_test_eof86: cs = 86; goto _test_eof; 
	_test_eof87: cs = 87; goto _test_eof; 
	_test_eof88: cs = 88; goto _test_eof; 
	_test_eof89: cs = 89; goto _test_eof; 
	_test_eof90: cs = 90; goto _test_eof; 
	_test_eof91: cs = 91; goto _test_eof; 
	_test_eof268: cs = 268; goto _test_eof; 
	_test_eof92: cs = 92; goto _test_eof; 
	_test_eof93: cs = 93; goto _test_eof; 
	_test_eof94: cs = 94; goto _test_eof; 
	_test_eof269: cs = 269; goto _test_eof; 
	_test_eof95: cs = 95; goto _test_eof; 
	_test_eof96: cs = 96; goto _test_eof; 
	_test_eof97: cs = 97; goto _test_eof; 
	_test_eof98: cs = 98; goto _test_eof; 
	_test_eof99: cs = 99; goto _test_eof; 
	_test_eof270: cs = 270; goto _test_eof; 
	_test_eof100: cs = 100; goto _test_eof; 
	_test_eof101: cs = 101; goto _test_eof; 
	_test_eof102: cs = 102; goto _test_eof; 
	_test_eof103: cs = 103; goto _test_eof; 
	_test_eof104: cs = 104; goto _test_eof; 
	_test_eof105: cs = 105; goto _test_eof; 
	_test_eof271: cs = 271; goto _test_eof; 
	_test_eof272: cs = 272; goto _test_eof; 
	_test_eof273: cs = 273; goto _test_eof; 
	_test_eof106: cs = 106; goto _test_eof; 
	_test_eof107: cs = 107; goto _test_eof; 
	_test_eof108: cs = 108; goto _test_eof; 
	_test_eof274: cs = 274; goto _test_eof; 
	_test_eof109: cs = 109; goto _test_eof; 
	_test_eof275: cs = 275; goto _test_eof; 
	_test_eof110: cs = 110; goto _test_eof; 
	_test_eof111: cs = 111; goto _test_eof; 
	_test_eof112: cs = 112; goto _test_eof; 
	_test_eof113: cs = 113; goto _test_eof; 
	_test_eof114: cs = 114; goto _test_eof; 
	_test_eof276: cs = 276; goto _test_eof; 
	_test_eof115: cs = 115; goto _test_eof; 
	_test_eof116: cs = 116; goto _test_eof; 
	_test_eof117: cs = 117; goto _test_eof; 
	_test_eof277: cs = 277; goto _test_eof; 
	_test_eof118: cs = 118; goto _test_eof; 
	_test_eof119: cs = 119; goto _test_eof; 
	_test_eof120: cs = 120; goto _test_eof; 
	_test_eof121: cs = 121; goto _test_eof; 
	_test_eof122: cs = 122; goto _test_eof; 
	_test_eof123: cs = 123; goto _test_eof; 
	_test_eof278: cs = 278; goto _test_eof; 
	_test_eof124: cs = 124; goto _test_eof; 
	_test_eof125: cs = 125; goto _test_eof; 
	_test_eof279: cs = 279; goto _test_eof; 
	_test_eof126: cs = 126; goto _test_eof; 
	_test_eof127: cs = 127; goto _test_eof; 
	_test_eof280: cs = 280; goto _test_eof; 
	_test_eof128: cs = 128; goto _test_eof; 
	_test_eof129: cs = 129; goto _test_eof; 
	_test_eof130: cs = 130; goto _test_eof; 
	_test_eof131: cs = 131; goto _test_eof; 
	_test_eof281: cs = 281; goto _test_eof; 
	_test_eof132: cs = 132; goto _test_eof; 
	_test_eof133: cs = 133; goto _test_eof; 
	_test_eof134: cs = 134; goto _test_eof; 
	_test_eof135: cs = 135; goto _test_eof; 
	_test_eof136: cs = 136; goto _test_eof; 
	_test_eof137: cs = 137; goto _test_eof; 
	_test_eof138: cs = 138; goto _test_eof; 
	_test_eof139: cs = 139; goto _test_eof; 
	_test_eof282: cs = 282; goto _test_eof; 
	_test_eof140: cs = 140; goto _test_eof; 
	_test_eof141: cs = 141; goto _test_eof; 
	_test_eof142: cs = 142; goto _test_eof; 
	_test_eof143: cs = 143; goto _test_eof; 
	_test_eof144: cs = 144; goto _test_eof; 
	_test_eof145: cs = 145; goto _test_eof; 
	_test_eof146: cs = 146; goto _test_eof; 
	_test_eof283: cs = 283; goto _test_eof; 
	_test_eof147: cs = 147; goto _test_eof; 
	_test_eof148: cs = 148; goto _test_eof; 
	_test_eof149: cs = 149; goto _test_eof; 
	_test_eof284: cs = 284; goto _test_eof; 
	_test_eof150: cs = 150; goto _test_eof; 
	_test_eof151: cs = 151; goto _test_eof; 
	_test_eof152: cs = 152; goto _test_eof; 
	_test_eof285: cs = 285; goto _test_eof; 
	_test_eof153: cs = 153; goto _test_eof; 
	_test_eof154: cs = 154; goto _test_eof; 
	_test_eof286: cs = 286; goto _test_eof; 
	_test_eof155: cs = 155; goto _test_eof; 
	_test_eof287: cs = 287; goto _test_eof; 
	_test_eof156: cs = 156; goto _test_eof; 
	_test_eof288: cs = 288; goto _test_eof; 
	_test_eof157: cs = 157; goto _test_eof; 
	_test_eof289: cs = 289; goto _test_eof; 
	_test_eof158: cs = 158; goto _test_eof; 
	_test_eof290: cs = 290; goto _test_eof; 
	_test_eof159: cs = 159; goto _test_eof; 
	_test_eof291: cs = 291; goto _test_eof; 
	_test_eof160: cs = 160; goto _test_eof; 
	_test_eof161: cs = 161; goto _test_eof; 
	_test_eof162: cs = 162; goto _test_eof; 
	_test_eof292: cs = 292; goto _test_eof; 
	_test_eof163: cs = 163; goto _test_eof; 
	_test_eof164: cs = 164; goto _test_eof; 
	_test_eof165: cs = 165; goto _test_eof; 
	_test_eof166: cs = 166; goto _test_eof; 
	_test_eof167: cs = 167; goto _test_eof; 
	_test_eof168: cs = 168; goto _test_eof; 
	_test_eof169: cs = 169; goto _test_eof; 
	_test_eof293: cs = 293; goto _test_eof; 
	_test_eof170: cs = 170; goto _test_eof; 
	_test_eof171: cs = 171; goto _test_eof; 
	_test_eof172: cs = 172; goto _test_eof; 
	_test_eof294: cs = 294; goto _test_eof; 
	_test_eof173: cs = 173; goto _test_eof; 
	_test_eof174: cs = 174; goto _test_eof; 
	_test_eof175: cs = 175; goto _test_eof; 
	_test_eof176: cs = 176; goto _test_eof; 
	_test_eof177: cs = 177; goto _test_eof; 
	_test_eof178: cs = 178; goto _test_eof; 
	_test_eof179: cs = 179; goto _test_eof; 
	_test_eof180: cs = 180; goto _test_eof; 
	_test_eof181: cs = 181; goto _test_eof; 
	_test_eof182: cs = 182; goto _test_eof; 
	_test_eof183: cs = 183; goto _test_eof; 
	_test_eof184: cs = 184; goto _test_eof; 
	_test_eof295: cs = 295; goto _test_eof; 
	_test_eof185: cs = 185; goto _test_eof; 
	_test_eof186: cs = 186; goto _test_eof; 
	_test_eof187: cs = 187; goto _test_eof; 
	_test_eof296: cs = 296; goto _test_eof; 
	_test_eof188: cs = 188; goto _test_eof; 
	_test_eof189: cs = 189; goto _test_eof; 
	_test_eof190: cs = 190; goto _test_eof; 
	_test_eof297: cs = 297; goto _test_eof; 
	_test_eof191: cs = 191; goto _test_eof; 
	_test_eof192: cs = 192; goto _test_eof; 
	_test_eof193: cs = 193; goto _test_eof; 
	_test_eof194: cs = 194; goto _test_eof; 
	_test_eof195: cs = 195; goto _test_eof; 
	_test_eof298: cs = 298; goto _test_eof; 
	_test_eof196: cs = 196; goto _test_eof; 
	_test_eof197: cs = 197; goto _test_eof; 
	_test_eof198: cs = 198; goto _test_eof; 
	_test_eof199: cs = 199; goto _test_eof; 
	_test_eof200: cs = 200; goto _test_eof; 
	_test_eof201: cs = 201; goto _test_eof; 
	_test_eof202: cs = 202; goto _test_eof; 
	_test_eof299: cs = 299; goto _test_eof; 
	_test_eof203: cs = 203; goto _test_eof; 
	_test_eof204: cs = 204; goto _test_eof; 
	_test_eof205: cs = 205; goto _test_eof; 
	_test_eof300: cs = 300; goto _test_eof; 
	_test_eof206: cs = 206; goto _test_eof; 
	_test_eof207: cs = 207; goto _test_eof; 
	_test_eof208: cs = 208; goto _test_eof; 
	_test_eof209: cs = 209; goto _test_eof; 
	_test_eof210: cs = 210; goto _test_eof; 
	_test_eof211: cs = 211; goto _test_eof; 
	_test_eof212: cs = 212; goto _test_eof; 
	_test_eof213: cs = 213; goto _test_eof; 
	_test_eof214: cs = 214; goto _test_eof; 
	_test_eof215: cs = 215; goto _test_eof; 
	_test_eof216: cs = 216; goto _test_eof; 
	_test_eof217: cs = 217; goto _test_eof; 
	_test_eof301: cs = 301; goto _test_eof; 
	_test_eof218: cs = 218; goto _test_eof; 
	_test_eof219: cs = 219; goto _test_eof; 
	_test_eof220: cs = 220; goto _test_eof; 
	_test_eof302: cs = 302; goto _test_eof; 
	_test_eof221: cs = 221; goto _test_eof; 
	_test_eof222: cs = 222; goto _test_eof; 
	_test_eof223: cs = 223; goto _test_eof; 
	_test_eof224: cs = 224; goto _test_eof; 
	_test_eof303: cs = 303; goto _test_eof; 
	_test_eof225: cs = 225; goto _test_eof; 
	_test_eof226: cs = 226; goto _test_eof; 
	_test_eof227: cs = 227; goto _test_eof; 
	_test_eof228: cs = 228; goto _test_eof; 
	_test_eof229: cs = 229; goto _test_eof; 
	_test_eof230: cs = 230; goto _test_eof; 
	_test_eof231: cs = 231; goto _test_eof; 
	_test_eof232: cs = 232; goto _test_eof; 
	_test_eof233: cs = 233; goto _test_eof; 
	_test_eof234: cs = 234; goto _test_eof; 
	_test_eof235: cs = 235; goto _test_eof; 
	_test_eof236: cs = 236; goto _test_eof; 
	_test_eof237: cs = 237; goto _test_eof; 
	_test_eof304: cs = 304; goto _test_eof; 
	_test_eof238: cs = 238; goto _test_eof; 
	_test_eof239: cs = 239; goto _test_eof; 
	_test_eof240: cs = 240; goto _test_eof; 
	_test_eof241: cs = 241; goto _test_eof; 
	_test_eof242: cs = 242; goto _test_eof; 
	_test_eof243: cs = 243; goto _test_eof; 
	_test_eof305: cs = 305; goto _test_eof; 

	_test_eof: {}
	if ( p == eof )
	{
	switch ( cs ) {
	case 245: goto tr298;
	case 3: goto tr3;
	case 4: goto tr3;
	case 246: goto tr314;
	case 5: goto tr3;
	case 6: goto tr3;
	case 247: goto tr315;
	case 7: goto tr3;
	case 8: goto tr3;
	case 248: goto tr316;
	case 9: goto tr3;
	case 249: goto tr317;
	case 10: goto tr3;
	case 250: goto tr318;
	case 11: goto tr3;
	case 251: goto tr319;
	case 12: goto tr3;
	case 13: goto tr3;
	case 14: goto tr3;
	case 15: goto tr3;
	case 252: goto tr320;
	case 16: goto tr3;
	case 17: goto tr3;
	case 18: goto tr3;
	case 19: goto tr3;
	case 20: goto tr3;
	case 253: goto tr321;
	case 21: goto tr3;
	case 22: goto tr3;
	case 254: goto tr322;
	case 23: goto tr3;
	case 24: goto tr3;
	case 25: goto tr3;
	case 26: goto tr3;
	case 255: goto tr323;
	case 27: goto tr3;
	case 28: goto tr3;
	case 29: goto tr3;
	case 30: goto tr3;
	case 256: goto tr324;
	case 31: goto tr3;
	case 32: goto tr3;
	case 33: goto tr3;
	case 257: goto tr325;
	case 34: goto tr3;
	case 35: goto tr3;
	case 36: goto tr3;
	case 37: goto tr3;
	case 38: goto tr3;
	case 39: goto tr3;
	case 40: goto tr3;
	case 258: goto tr326;
	case 41: goto tr3;
	case 42: goto tr3;
	case 43: goto tr3;
	case 259: goto tr327;
	case 44: goto tr3;
	case 45: goto tr3;
	case 46: goto tr3;
	case 47: goto tr3;
	case 48: goto tr3;
	case 260: goto tr328;
	case 49: goto tr3;
	case 50: goto tr3;
	case 51: goto tr3;
	case 52: goto tr3;
	case 53: goto tr3;
	case 54: goto tr3;
	case 261: goto tr329;
	case 55: goto tr3;
	case 56: goto tr3;
	case 57: goto tr3;
	case 58: goto tr3;
	case 262: goto tr330;
	case 59: goto tr3;
	case 60: goto tr3;
	case 263: goto tr331;
	case 61: goto tr3;
	case 62: goto tr3;
	case 63: goto tr3;
	case 64: goto tr3;
	case 65: goto tr3;
	case 66: goto tr3;
	case 67: goto tr3;
	case 68: goto tr3;
	case 69: goto tr3;
	case 70: goto tr3;
	case 71: goto tr3;
	case 72: goto tr3;
	case 73: goto tr3;
	case 264: goto tr332;
	case 74: goto tr3;
	case 75: goto tr3;
	case 265: goto tr333;
	case 76: goto tr3;
	case 77: goto tr3;
	case 78: goto tr3;
	case 79: goto tr3;
	case 266: goto tr334;
	case 80: goto tr3;
	case 81: goto tr3;
	case 82: goto tr3;
	case 83: goto tr3;
	case 84: goto tr3;
	case 267: goto tr335;
	case 85: goto tr3;
	case 86: goto tr3;
	case 87: goto tr3;
	case 88: goto tr3;
	case 89: goto tr3;
	case 90: goto tr3;
	case 91: goto tr3;
	case 268: goto tr336;
	case 92: goto tr3;
	case 93: goto tr3;
	case 94: goto tr3;
	case 269: goto tr337;
	case 95: goto tr3;
	case 96: goto tr3;
	case 97: goto tr3;
	case 98: goto tr3;
	case 99: goto tr3;
	case 270: goto tr338;
	case 100: goto tr3;
	case 101: goto tr3;
	case 102: goto tr3;
	case 103: goto tr3;
	case 104: goto tr3;
	case 105: goto tr3;
	case 271: goto tr339;
	case 272: goto tr340;
	case 273: goto tr341;
	case 106: goto tr3;
	case 107: goto tr3;
	case 108: goto tr3;
	case 274: goto tr342;
	case 109: goto tr3;
	case 275: goto tr343;
	case 110: goto tr3;
	case 111: goto tr3;
	case 112: goto tr3;
	case 113: goto tr3;
	case 114: goto tr3;
	case 276: goto tr344;
	case 115: goto tr3;
	case 116: goto tr3;
	case 117: goto tr3;
	case 277: goto tr345;
	case 118: goto tr3;
	case 119: goto tr3;
	case 120: goto tr3;
	case 121: goto tr3;
	case 122: goto tr3;
	case 123: goto tr3;
	case 278: goto tr346;
	case 124: goto tr3;
	case 125: goto tr3;
	case 279: goto tr347;
	case 126: goto tr3;
	case 127: goto tr3;
	case 280: goto tr348;
	case 128: goto tr3;
	case 129: goto tr3;
	case 130: goto tr3;
	case 131: goto tr3;
	case 281: goto tr349;
	case 132: goto tr3;
	case 133: goto tr3;
	case 134: goto tr3;
	case 135: goto tr3;
	case 136: goto tr3;
	case 137: goto tr3;
	case 138: goto tr3;
	case 139: goto tr3;
	case 282: goto tr350;
	case 140: goto tr3;
	case 141: goto tr3;
	case 142: goto tr3;
	case 143: goto tr3;
	case 144: goto tr3;
	case 145: goto tr3;
	case 146: goto tr3;
	case 283: goto tr351;
	case 147: goto tr3;
	case 148: goto tr3;
	case 149: goto tr3;
	case 284: goto tr352;
	case 150: goto tr3;
	case 151: goto tr3;
	case 152: goto tr3;
	case 285: goto tr353;
	case 153: goto tr3;
	case 154: goto tr3;
	case 286: goto tr354;
	case 155: goto tr3;
	case 287: goto tr355;
	case 156: goto tr3;
	case 288: goto tr356;
	case 157: goto tr3;
	case 289: goto tr357;
	case 158: goto tr3;
	case 290: goto tr358;
	case 159: goto tr3;
	case 291: goto tr359;
	case 160: goto tr3;
	case 161: goto tr3;
	case 162: goto tr3;
	case 292: goto tr360;
	case 163: goto tr3;
	case 164: goto tr3;
	case 165: goto tr3;
	case 166: goto tr3;
	case 167: goto tr3;
	case 168: goto tr3;
	case 169: goto tr3;
	case 293: goto tr361;
	case 170: goto tr3;
	case 171: goto tr3;
	case 172: goto tr3;
	case 294: goto tr362;
	case 173: goto tr3;
	case 174: goto tr3;
	case 175: goto tr3;
	case 176: goto tr3;
	case 177: goto tr3;
	case 178: goto tr3;
	case 179: goto tr3;
	case 180: goto tr3;
	case 181: goto tr3;
	case 182: goto tr3;
	case 183: goto tr3;
	case 184: goto tr3;
	case 295: goto tr363;
	case 185: goto tr3;
	case 186: goto tr3;
	case 187: goto tr3;
	case 296: goto tr364;
	case 188: goto tr3;
	case 189: goto tr3;
	case 190: goto tr3;
	case 297: goto tr365;
	case 191: goto tr3;
	case 192: goto tr3;
	case 193: goto tr3;
	case 194: goto tr3;
	case 195: goto tr3;
	case 298: goto tr366;
	case 196: goto tr3;
	case 197: goto tr3;
	case 198: goto tr3;
	case 199: goto tr3;
	case 200: goto tr3;
	case 201: goto tr3;
	case 202: goto tr3;
	case 299: goto tr367;
	case 203: goto tr3;
	case 204: goto tr3;
	case 205: goto tr3;
	case 300: goto tr368;
	case 206: goto tr3;
	case 207: goto tr3;
	case 208: goto tr3;
	case 209: goto tr3;
	case 210: goto tr3;
	case 211: goto tr3;
	case 212: goto tr3;
	case 213: goto tr3;
	case 214: goto tr3;
	case 215: goto tr3;
	case 216: goto tr3;
	case 217: goto tr3;
	case 301: goto tr369;
	case 218: goto tr3;
	case 219: goto tr3;
	case 220: goto tr3;
	case 302: goto tr370;
	case 221: goto tr3;
	case 222: goto tr3;
	case 223: goto tr3;
	case 224: goto tr3;
	case 303: goto tr371;
	case 225: goto tr3;
	case 226: goto tr3;
	case 227: goto tr3;
	case 228: goto tr3;
	case 229: goto tr3;
	case 230: goto tr3;
	case 231: goto tr3;
	case 232: goto tr3;
	case 233: goto tr3;
	case 234: goto tr3;
	case 235: goto tr3;
	case 236: goto tr3;
	case 237: goto tr3;
	case 304: goto tr372;
	case 238: goto tr3;
	case 239: goto tr3;
	case 240: goto tr3;
	case 241: goto tr3;
	case 242: goto tr3;
	case 243: goto tr3;
	case 305: goto tr373;
	}
	}

	_out: {}
	}

#line 965 "src/tokens.rl"

    result.cursor_col = parser.cursor_col;
    result.cursor_row = parser.cursor_row;
    result.has_cursor = parser.has_cursor;
    result.final_col = parser.visual_col;
    result.final_row = parser.visual_row;
    return result;
}

zstr expand_tokens(const char *text) {
    TokenExpansion result = expand_tokens_with_cursor(text);
    return result.expanded;
}

void zstr_expand_to(FILE *stream, const char *text) {
    if (!text || !*text) return;

    Z_CLEANUP(zstr_free) zstr expanded = expand_tokens(text);
    fwrite(zstr_cstr(&expanded), 1, zstr_len(&expanded), stream);
}

void token_expansion_render(FILE *stream, TokenExpansion *te) {
    if (!te) return;

    /* Write the expanded content */
    fwrite(zstr_cstr(&te->expanded), 1, zstr_len(&te->expanded), stream);

    /* If cursor was marked, clear to end of line and position cursor */
    if (te->has_cursor) {
        /* Clear from current position to end of line (gives cursor room) */
        if (!zstr_no_colors) {
            fprintf(stream, "\033[K");
        }

        /* Position cursor at marked location and show it */
        if (!zstr_no_colors && te->cursor_row > 0 && te->cursor_col > 0) {
            fprintf(stream, "\033[%d;%dH\033[?25h", te->cursor_row, te->cursor_col);
        }
    }
}
