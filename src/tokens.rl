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

%%{
    machine token_parser;

    # Character classes
    num_char = [0-9];

    action mark_start { tok_start = p; }
    action mark_end { tok_end = p; }

    # Actions use % to fire AFTER the pattern is complete (leaving action)

    action emit_char {
        zstr_push(parser.out, *p);
        parser.visual_col++;
    }

    action handle_newline {
        zstr_push(parser.out, *p);
        parser.visual_col = 1;
        parser.visual_row++;
    }

    action cursor_mark {
        parser.cursor_col = parser.visual_col;
        parser.cursor_row = parser.visual_row;
        parser.has_cursor = true;
    }

    action emit_goto_cursor {
        if (parser.cursor_row > 0 && parser.cursor_col > 0) {
            emit_ansi_num(&parser, "\033[", parser.cursor_row, ";");
            emit_ansi_num(&parser, "", parser.cursor_col, "H");
        }
    }

    action pop_style { pop_style(&parser); }
    action reset_all { reset_all(&parser); }
    action reset_fg {
        push_attr(&parser, ATTR_FG, parser.fg_color);
        parser.fg_color = 0;
        mark_dirty(&parser);
    }
    action reset_bg {
        push_attr(&parser, ATTR_BG, parser.bg_color);
        parser.bg_color = 0;
        mark_dirty(&parser);
    }

    # Semantic tokens
    action tok_b { apply_token_b(&parser); }
    action tok_highlight { apply_token_highlight(&parser); }
    action tok_h1 { apply_token_h1(&parser); }
    action tok_h2 { apply_token_h2(&parser); }
    action tok_h3 { apply_token_h3(&parser); }
    action tok_h4 { apply_token_h4(&parser); }
    action tok_h5 { apply_token_h5(&parser); }
    action tok_h6 { apply_token_h6(&parser); }
    action tok_strong { apply_token_strong(&parser); }
    action tok_dim { apply_token_dim_style(&parser); }
    action tok_dark { apply_token_dark_style(&parser); }
    action tok_section { apply_token_section(&parser); }
    action tok_danger { apply_token_danger(&parser); }
    action tok_text { apply_token_text(&parser); }

    # Attribute tokens
    action tok_bold { apply_bold(&parser); }
    action tok_italic { apply_italic(&parser); }
    action tok_underline { apply_underline(&parser); }
    action tok_reverse { apply_reverse(&parser); }
    action tok_strikethrough { apply_strikethrough(&parser); }
    action tok_bright { apply_bright(&parser); }

    # Color tokens - standard foreground
    action fg_black { apply_fg_code(&parser, 30); }
    action fg_red { apply_fg_code(&parser, 31); }
    action fg_green { apply_fg_code(&parser, 32); }
    action fg_yellow { apply_fg_code(&parser, 33); }
    action fg_blue { apply_fg_code(&parser, 34); }
    action fg_magenta { apply_fg_code(&parser, 35); }
    action fg_cyan { apply_fg_code(&parser, 36); }
    action fg_white { apply_fg_code(&parser, 37); }
    action fg_gray { apply_fg_code(&parser, 90); }

    # Color tokens - bright foreground
    action fg_bright_black { apply_fg_code(&parser, 90); }
    action fg_bright_red { apply_fg_code(&parser, 91); }
    action fg_bright_green { apply_fg_code(&parser, 92); }
    action fg_bright_yellow { apply_fg_code(&parser, 93); }
    action fg_bright_blue { apply_fg_code(&parser, 94); }
    action fg_bright_magenta { apply_fg_code(&parser, 95); }
    action fg_bright_cyan { apply_fg_code(&parser, 96); }
    action fg_bright_white { apply_fg_code(&parser, 97); }

    # Color tokens - standard background
    action bg_black { apply_bg_code(&parser, 40); }
    action bg_red { apply_bg_code(&parser, 41); }
    action bg_green { apply_bg_code(&parser, 42); }
    action bg_yellow { apply_bg_code(&parser, 43); }
    action bg_blue { apply_bg_code(&parser, 44); }
    action bg_magenta { apply_bg_code(&parser, 45); }
    action bg_cyan { apply_bg_code(&parser, 46); }
    action bg_white { apply_bg_code(&parser, 47); }
    action bg_gray { apply_bg_code(&parser, 100); }

    # 256-color handling
    action fg_256 {
        int n = parse_number(tok_start, tok_end);
        if (n >= 0 && n <= 255) {
            apply_fg_256(&parser, n);
        }
    }

    action bg_256 {
        int n = parse_number(tok_start, tok_end);
        if (n >= 0 && n <= 255) {
            apply_bg_256(&parser, n);
        }
    }

    # Token patterns - use % (leaving action) instead of @ (finishing action)
    number = num_char+ >mark_start %mark_end;

    # Reset/pop tokens
    # {/} or {/name} all pop one level (name is ignored for flexibility)
    tok_pop = "{/" [a-z]* "}" %pop_style;
    tok_reset = "{reset}" %reset_all;
    tok_reset_fg = "{/fg}" %reset_fg;
    tok_reset_bg = "{/bg}" %reset_bg;

    # Semantic tokens
    tok_b_open = "{b}" %tok_b;
    tok_highlight_open = "{highlight}" %tok_highlight;
    tok_h1 = "{h1}" %tok_h1;
    tok_h2 = "{h2}" %tok_h2;
    tok_h3 = "{h3}" %tok_h3;
    tok_h4 = "{h4}" %tok_h4;
    tok_h5 = "{h5}" %tok_h5;
    tok_h6 = "{h6}" %tok_h6;
    tok_strong = "{strong}" %tok_strong;
    tok_dim_style = "{dim}" %tok_dim;
    tok_dark_style = "{dark}" %tok_dark;
    tok_section_open = "{section}" %tok_section;
    tok_danger_open = "{danger}" %tok_danger;
    tok_strike_open = "{strike}" %tok_strikethrough;  # Strikethrough text
    tok_text = "{text}" %tok_text;
    tok_cursor = "{cursor}" %cursor_mark;

    # Attribute tokens
    tok_bold = ("{bold}" | "{B}") %tok_bold;
    tok_italic = ("{italic}" | "{I}" | "{i}") %tok_italic;
    tok_underline = ("{underline}" | "{U}" | "{u}") %tok_underline;
    tok_reverse = "{reverse}" %tok_reverse;
    tok_strikethrough = "{strikethrough}" %tok_strikethrough;
    tok_bright = "{bright}" %tok_bright;

    # Standard foreground colors
    tok_fg_black = "{black}" %fg_black;
    tok_fg_red = "{red}" %fg_red;
    tok_fg_green = "{green}" %fg_green;
    tok_fg_yellow = "{yellow}" %fg_yellow;
    tok_fg_blue = "{blue}" %fg_blue;
    tok_fg_magenta = "{magenta}" %fg_magenta;
    tok_fg_cyan = "{cyan}" %fg_cyan;
    tok_fg_white = "{white}" %fg_white;
    tok_fg_gray = ("{gray}" | "{grey}") %fg_gray;

    # Bright foreground colors
    tok_fg_bright_black = "{bright:black}" %fg_bright_black;
    tok_fg_bright_red = "{bright:red}" %fg_bright_red;
    tok_fg_bright_green = "{bright:green}" %fg_bright_green;
    tok_fg_bright_yellow = "{bright:yellow}" %fg_bright_yellow;
    tok_fg_bright_blue = "{bright:blue}" %fg_bright_blue;
    tok_fg_bright_magenta = "{bright:magenta}" %fg_bright_magenta;
    tok_fg_bright_cyan = "{bright:cyan}" %fg_bright_cyan;
    tok_fg_bright_white = "{bright:white}" %fg_bright_white;

    # Standard background colors
    tok_bg_black = "{bg:black}" %bg_black;
    tok_bg_red = "{bg:red}" %bg_red;
    tok_bg_green = "{bg:green}" %bg_green;
    tok_bg_yellow = "{bg:yellow}" %bg_yellow;
    tok_bg_blue = "{bg:blue}" %bg_blue;
    tok_bg_magenta = "{bg:magenta}" %bg_magenta;
    tok_bg_cyan = "{bg:cyan}" %bg_cyan;
    tok_bg_white = "{bg:white}" %bg_white;
    tok_bg_gray = ("{bg:gray}" | "{bg:grey}") %bg_gray;

    # 256-color tokens
    tok_fg_256 = "{fg:" number "}" %fg_256;
    tok_bg_256 = "{bg:" number "}" %bg_256;

    # Control sequence tokens (for convenience in TUI code)
    action emit_clear_line { emit_ansi(&parser, "\033[K"); }
    action emit_clear_screen { emit_ansi(&parser, "\033[J"); }
    action emit_home { emit_ansi(&parser, "\033[H"); }
    action emit_hide_cursor { emit_ansi(&parser, "\033[?25l"); }
    action emit_show_cursor { emit_ansi(&parser, "\033[?25h"); }

    # Cursor positioning: {goto:row,col}
    action mark_row_start { goto_row_start = p; }
    action mark_row_end { goto_row_end = p; }
    action mark_col_start { goto_col_start = p; }
    action mark_col_end { goto_col_end = p; }
    action emit_goto {
        int row = parse_number(goto_row_start, goto_row_end);
        int col = parse_number(goto_col_start, goto_col_end);
        if (row >= 0 && col >= 0) {
            emit_ansi_num(&parser, "\033[", row, ";");
            emit_ansi_num(&parser, "", col, "H");
        }
    }

    tok_clear_line = "{clr}" %emit_clear_line;
    tok_clear_screen = "{cls}" %emit_clear_screen;
    tok_home = "{home}" %emit_home;
    tok_hide_cursor = "{hide}" %emit_hide_cursor;
    tok_show_cursor = "{show}" %emit_show_cursor;
    tok_goto = "{goto:" (num_char+ >mark_row_start %mark_row_end) "," (num_char+ >mark_col_start %mark_col_end) "}" %emit_goto;
    tok_goto_cursor = "{goto_cursor}" %emit_goto_cursor;

    # All tokens combined
    token = (
        tok_pop |
        tok_reset |
        tok_reset_fg |
        tok_reset_bg |
        tok_b_open |
        tok_highlight_open |
        tok_h1 |
        tok_h2 |
        tok_h3 |
        tok_h4 |
        tok_h5 |
        tok_h6 |
        tok_strong |
        tok_dim_style |
        tok_dark_style |
        tok_section_open |
        tok_danger_open |
        tok_strike_open |
        tok_text |
        tok_cursor |
        tok_bold |
        tok_italic |
        tok_underline |
        tok_reverse |
        tok_strikethrough |
        tok_bright |
        tok_fg_black |
        tok_fg_red |
        tok_fg_green |
        tok_fg_yellow |
        tok_fg_blue |
        tok_fg_magenta |
        tok_fg_cyan |
        tok_fg_white |
        tok_fg_gray |
        tok_fg_bright_black |
        tok_fg_bright_red |
        tok_fg_bright_green |
        tok_fg_bright_yellow |
        tok_fg_bright_blue |
        tok_fg_bright_magenta |
        tok_fg_bright_cyan |
        tok_fg_bright_white |
        tok_bg_black |
        tok_bg_red |
        tok_bg_green |
        tok_bg_yellow |
        tok_bg_blue |
        tok_bg_magenta |
        tok_bg_cyan |
        tok_bg_white |
        tok_bg_gray |
        tok_fg_256 |
        tok_bg_256 |
        tok_clear_line |
        tok_clear_screen |
        tok_home |
        tok_hide_cursor |
        tok_show_cursor |
        tok_goto |
        tok_goto_cursor
    );

    # Helper action to emit the matched token text
    action emit_token {
        for (const char *c = ts; c < te; c++) {
            zstr_push(parser.out, *c);
            if (*c != '\033' && *c != '[' && (*c < '0' || *c > '9') && *c != ';') {
                if (*c != '\n') {
                    parser.visual_col++;
                } else {
                    parser.visual_col = 1;
                }
            }
        }
    }

    action emit_regular_char {
        sync_styles(&parser);  /* Emit any pending style changes */
        zstr_push(parser.out, *ts);
        parser.visual_col++;
    }

    action emit_newline {
        /* Auto-reset all active styles before newline */
        reset_line_styles(&parser);
        zstr_push(parser.out, *ts);
        parser.visual_col = 1;
    }

    action emit_ansi_passthrough {
        for (const char *c = ts; c < te; c++) {
            zstr_push(parser.out, *c);
        }
    }

    # ANSI escape sequence passthrough (don't count as visible)
    ansi_seq = 0x1B '[' (any - alpha)* alpha;

    # Regular character (not start of token, not escape, not newline)
    regular_char = (any - '{' - 0x1B - '\n');

    # Newline handling
    newline = '\n';

    # Unrecognized { - just emit it
    open_brace = '{';

    # Main scanner using |* for proper longest-match scanning
    # Use => for scanner actions (fire on token acceptance)
    main := |*
        token => {};
        ansi_seq => emit_ansi_passthrough;
        newline => emit_newline;
        regular_char => emit_regular_char;
        open_brace => emit_regular_char;
    *|;

    write data;
}%%

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

    %% write init;
    %% write exec;

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
