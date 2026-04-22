#ifndef LIBTUI_INCLUDE
#define PSH_CORE_NO_PREFIX
#include "psh_build/psh_build.h"

// public

//TODO: make more memory efficient by encoding
//      raw_len and d_width in 1 byte
typedef struct {
    byte raw[4];
    u8 raw_len;
    u8 display_width;
} CodePoint;

list_def(CodePoint);

#define cp(s)      cp_from_utf8(s, sizeof(s) - 1)
CodePoint cp_from_utf8(byte *s, u8 len);
CodePoint cp_from_raw(byte *raw, u8 raw_len, u8 display_width);
CodePoint cp_from_byte(byte b);
b32 cp_equal(CodePoint a, CodePoint b);

#define ctrl(x) cp_from_byte((x) & 0x1F)

typedef struct {
    u8 r, g, b;
} RGB;

enum {
    EFFECT_FG               = 1 << 0,
    EFFECT_BG               = 1 << 1,
    EFFECT_BOLD             = 1 << 2,
    EFFECT_DIM              = 1 << 3,
    EFFECT_ITALIC           = 1 << 4,
    EFFECT_UNDERLINE        = 1 << 5,
    EFFECT_INVERSE          = 1 << 6,
    EFFECT_STRIKETHROUGH    = 1 << 7,
};

typedef struct {
    RGB fg;
    RGB bg;
    u8 flags;
} Effect;

b32 effect_equal(Effect a, Effect b) {
    return memcmp(&a, &b, sizeof a) == 0;
}

#define CELL_REGULAR        0x00
#define CELL_CONTINUATION   0x01
#define CELL_WIDE_LEAD      0x02

typedef struct {
    CodePoint cp;
    u8 flags;
    Effect effect;
} Cell;

Cell cell(CodePoint cp, Effect e);
Cell cell_cont(Effect e);
Cell cell_empty();
b32 cell_equal(Cell a, Cell b);

typedef enum {
    TERMKEY_BACKSPACE = 8,
    TERMKEY_TAB       = 9,
    TERMKEY_ENTER     = 13,
    TERMKEY_ESCAPE    = 27,

    TERMKEY_INSERT    = -1,
    TERMKEY_DELETE    = -2,
    TERMKEY_HOME      = -3,
    TERMKEY_END       = -4,
    TERMKEY_PAGEUP    = -5,
    TERMKEY_PAGEDOWN  = -6,
    TERMKEY_UP        = -7,
    TERMKEY_DOWN      = -8,
    TERMKEY_LEFT      = -9,
    TERMKEY_RIGHT     = -10,
} TermKey;

typedef enum {
    ENone = 0,
    ETermKey,
    ECodePoint,
    EWinch,
    EMouseLeft,
    EMouseRight,
    EMouseMiddle,
    EScrollUp,
    EScrollDown,
    EMouseDrag,
} EventType;

typedef struct {
    EventType type;
    b32 handled;
    union {
        struct {
            i32 x, y;
            b32 pressed;
        } mouse;
        CodePoint parsed_cp;
        TermKey term_key;
    };
} Event;

b32 is_event(EventType e);
i32 get_mouse_x();
i32 get_mouse_y();
b32 is_mouse_pressed();
b32 is_mouse_released();
b32 is_term_key(TermKey k);
b32 is_codepoint(CodePoint cp);
b32 is_event_consumed();
void event_consume();
CodePoint get_codepoint();

typedef struct {
    i32 x, y, w, h;
} Rectangle;

b32 point_in_rect(i32 x, i32 y, Rectangle r);
Rectangle rect_intersect(Rectangle a, Rectangle b);
Rectangle rect_union(Rectangle a, Rectangle b);

typedef struct {
    Rectangle rect;
} Clip;

void clip_push(i32 x, i32 y, i32 w, i32 h);
void clip_push_rect(Rectangle r);
Clip clip_pop();
Clip clip_peek();

void init_terminal();
void set_max_timeout_ms(i32 timeout);
void begin_frame();
void end_frame();
u64 get_delta_time();
u32 get_terminal_width();
u32 get_terminal_height();

typedef u32 Unicode;
u8 unicode_width(Unicode ch);

void put_cp_(i32 x, i32 y, CodePoint cp, Effect e);
#define put_cp(...)                 putx_(__VA_ARGS__, put2_, put1_)(__VA_ARGS__)
#define putx_(a, b, c, d, e, ...)   e
#define put1_(x, y, cp)             put_cp_(x, y, cp, (Effect) {0})
#define put2_(x, y, cp, ef)         put_cp_(x, y, cp, ef)

void put_str_(i32 x, i32 y, byte *str, usize len, Effect e);
#define put_str(...)                 putstrx_(__VA_ARGS__, putstr2_, putstr1_)(__VA_ARGS__)
#define putstrx_(a, b, c, d, e, f, ...) f
#define putstr1_(x, y, s, len)       put_str_(x, y, s, len, (Effect){0})
#define putstr2_(x, y, s, len, ef)   put_str_(x, y, s, len, ef)

void draw_line_(i32 x0, i32 y0, i32 x1, i32 y1, CodePoint cp, Effect e);
#define draw_line(...)               drawlinex_(__VA_ARGS__, drawline2_, drawline1_)(__VA_ARGS__)
#define drawlinex_(a,b,c,d,e,f,g,...) g
#define drawline1_(x0,y0,x1,y1,cp)   draw_line_(x0,y0,x1,y1,cp,(Effect){0})
#define drawline2_(x0,y0,x1,y1,cp,ef) draw_line_(x0,y0,x1,y1,cp,ef)

void draw_box_(Rectangle r, Effect e);
#define draw_box(...)                drawboxx_(__VA_ARGS__, drawbox2_, drawbox1_)(__VA_ARGS__)
#define drawboxx_(a,b,c,...)         c
#define drawbox1_(r)                 draw_box_(r, (Effect){0})
#define drawbox2_(r, ef)             draw_box_(r, ef)

void fill_box(Rectangle r, Effect e);

byte *vfmt(byte *p, byte *end, byte *f, va_list args);
byte *fmt(byte *p, byte *end, byte *f, ...);
byte *fmt_uint(byte *p, byte *end, u64 v, u8 base);
byte *fmt_cstr(byte *p, byte *end, byte *s);
byte *fmt_s8(byte *p, byte *end, s8 s);

typedef struct {
    byte *start;
    byte *p;
    byte *end;
} Stream;

Stream stream_start(byte *buffer, usize size);
void stream_fmt(Stream *s, byte *f, ...);
s8 stream_end(Stream s);
Stream arena_stream_start(Arena *arena, usize size);

void debug(i32 x, i32 y, byte *fmt, ...);

// TUI

typedef struct {
    i32 x;
    i32 y;
} Transform;

list_def(Transform);

void push_transform(i32 dx, i32 dy);
Transform pop_transform();
Transform peek_transform();

typedef struct {
    i32 y_offset;
    i32 content_start;
    i32 content_size;
    i32 viewport_size;
} Scrollable;

void scroll_apply(Scrollable *s) {
    push_transform(0, -s->y_offset);
}

void scroll_pop() {
    pop_transform();
}

i32 scroll_min(Scrollable *s) {
    return s->content_start;
}

i32 scroll_max(Scrollable *s) {
    return s->content_start + s->content_size - s->viewport_size;
}

void scroll_incr(Scrollable *s) {
    if (s->y_offset < scroll_max(s))
        s->y_offset++;
}

void scroll_decr(Scrollable *s) {
    if (s->y_offset > scroll_min(s))
        s->y_offset--;
}

typedef struct {
    i32 x;
    i32 y;
} Position;

typedef struct {
    i32 w;
    i32 h;
} Size;

typedef struct {
    i32 max_w;
    i32 max_h;
} LayoutConstraint;

enum {
    ALIGN_START,
    ALIGN_CENTER,
    ALIGN_END,
};
typedef u8 Align;

typedef struct BorderStyle BorderStyle;
struct BorderStyle {
    u8 width;
    void (*draw)(i32 w, i32 h, BorderStyle *self);
    Effect effect;
};

void default_border(i32 w, i32 h, BorderStyle *self);

//TODO: use less memory
typedef struct {
    i32 w, h; // fixed size
    Effect effect;
    Align align_self;
    u8 padding, margin;
    BorderStyle border;
} WidgetStyle;

typedef struct Widget Widget;

typedef struct {
    void (*layout)(Widget *self, LayoutConstraint constraint);
    Widget *(*hit_test)(Widget *self);
    void (*event)(Widget *self);
    void (*update)(Widget *self);
    void (*draw)(Widget *self);
} WidgetVTable;

//TODO: now it makes sense to add a constructor for widget
struct Widget {
    WidgetStyle style;
    Position offset;
    Size size;
    Widget *parent;
    WidgetVTable *vtable;
    u8 metadata;
};

enum {
    WIDGET_CONTAINER = 1,
};

typedef Widget * WidgetPtr;
list_def(WidgetPtr);

enum {
    LAYOUT_COLUMN,
    LAYOUT_ROW,
};
typedef u8 LayoutDirection;

enum {
    OVERFLOW_VISIBLE_Y,
    OVERFLOW_CLIP,
    OVERFLOW_SCROLL_Y,
};
typedef u8 Overflow;

typedef struct {
    LayoutDirection direction;
    Align align_children;
    u8 spacing;
    Overflow overflow;
} ContainerStyle;

//TODO: rename to Div. Refactor into Div widget + Container interface.
typedef struct {
    Widget widget;
    List(WidgetPtr) children;
    Scrollable scroll;
    ContainerStyle container_style;
} ContainerWidget;

// something like this
// typedef struct {
//     List(WidgetPtr) children;
//     Scrollable scroll;
//     ContainerStyle style;
// } Container;

// Children widgets meausure their sizes
// Parents set children's relative coordinates
void widget_layout(Widget *w, LayoutConstraint c);
// Test if widget was clicked
Widget *widget_hit_test(Widget *w);
// Handle event (Only called on widgets
// that were clicked or focused)
void widget_event(Widget *w);
// Can be used for animations and stuff
// that needs to be updated every frame
void widget_update(Widget *w);
// Draw widget
void widget_draw(Widget *w);

b32 widget_is_container(Widget *w);

Widget *default_hit_test(Widget *w);
void default_update(Widget *w);

void container_layout(Widget *w, LayoutConstraint c);
void container_add(Widget *c, Widget *w);

Rectangle content_rect(Widget *w);
Rectangle content_bp_rect(Widget *w);

void ui_register_root(Widget *w);
void ui_set_focus(Widget *w);
b32 ui_is_focused(Widget *w);
void ui_dispatch_event(Widget *hit);
void ui_run();

void ui_put_cp_(i32 x, i32 y, CodePoint cp, Effect e);
#define ui_put_cp(...)                  uiputx_(__VA_ARGS__, uiput2_, uiput1_)(__VA_ARGS__)
#define uiputx_(a,b,c,d,e,...)          e
#define uiput1_(x, y, cp)               ui_put_cp_(x, y, cp, (Effect){0})
#define uiput2_(x, y, cp, ef)           ui_put_cp_(x, y, cp, ef)

void ui_put_str_(i32 x, i32 y, byte *s, usize len, Effect e);
#define ui_put_str(...)                 uiputstrx_(__VA_ARGS__, uiputstr2_, uiputstr1_)(__VA_ARGS__)
#define uiputstrx_(a,b,c,d,e,f,...)     f
#define uiputstr1_(x,y,s,len)           ui_put_str_(x,y,s,len,(Effect){0})
#define uiputstr2_(x,y,s,len,ef)        ui_put_str_(x,y,s,len,ef)

void ui_draw_line_(i32 x0, i32 y0, i32 x1, i32 y1, CodePoint cp, Effect e);
#define ui_draw_line(...)               uidrawlinex_(__VA_ARGS__, uidrawline2_, uidrawline1_)(__VA_ARGS__)
#define uidrawlinex_(a,b,c,d,e,f,g,...) g
#define uidrawline1_(x0,y0,x1,y1,cp)    ui_draw_line_(x0,y0,x1,y1,cp,(Effect){0})
#define uidrawline2_(x0,y0,x1,y1,cp,ef) ui_draw_line_(x0,y0,x1,y1,cp,ef)

void ui_draw_box_(i32 w, i32 h, Effect e);
#define ui_draw_box(...)                uidrawboxx_(__VA_ARGS__, uidrawbox2_, uidrawbox1_)(__VA_ARGS__)
#define uidrawboxx_(a,b,c,d,...)        d
#define uidrawbox1_(w,h)                ui_draw_box_(w,h,(Effect){0})
#define uidrawbox2_(w,h,ef)             ui_draw_box_(w,h,ef)

void ui_fill_box(i32 w, i32 h, Effect e);

typedef enum {
    // widget style
    STYLE_WIDTH,
    STYLE_HEIGHT,
    STYLE_PADDING,
    STYLE_MARGIN,
    STYLE_ALIGN_SELF,
    STYLE_BG,

    /// container style
    STYLE_DIRECTION,
    STYLE_ALIGN_CHILDREN,
    STYLE_SPACING,
    STYLE_OVERFLOW,

    // border style
    STYLE_BORDER_WIDTH,
    STYLE_BORDER_FN,
    STYLE_BORDER_FG,
    STYLE_BORDER_BG,
    STYLE_BORDER_FLAG,
} StyleProp;

typedef struct {
    StyleProp prop;
    union {
        i32 i;
        u32 u;
        b32 b;
        Align align;
        LayoutDirection direction;
        Overflow overflow;
        RGB rgb;
        void *p;
        struct {
            u8 bit;
            b32 enabled;
        } flag;
    };
} StyleArg;

typedef struct {
    StyleArg *items;
    usize count;
} StyleArgs;

#define width(v)            ((StyleArg){ .prop = STYLE_WIDTH,          .i = (v) })
#define height(v)           ((StyleArg){ .prop = STYLE_HEIGHT,         .i = (v) })
#define padding(v)          ((StyleArg){ .prop = STYLE_PADDING,        .u = (v) })
#define margin(v)           ((StyleArg){ .prop = STYLE_MARGIN,         .u = (v) })
#define align_self(v)       ((StyleArg){ .prop = STYLE_ALIGN_SELF,     .align = (v) })
#define bg(r, g, b)         ((StyleArg){ .prop = STYLE_BG,             .rgb = { (r), (g), (b) } })

#define direction(v)        ((StyleArg){ .prop = STYLE_DIRECTION,      .direction = (v) })
#define align_children(v)   ((StyleArg){ .prop = STYLE_ALIGN_CHILDREN, .align = (v) })
#define spacing(v)          ((StyleArg){ .prop = STYLE_SPACING,        .u = (v) })
#define overflow(v)         ((StyleArg){ .prop = STYLE_OVERFLOW,       .overflow = (v) })

#define border_width(v)           ((StyleArg){ .prop = STYLE_BORDER_WIDTH,    .u = (v) })
#define border_fn(v)              ((StyleArg){ .prop = STYLE_BORDER_FN,       .p = (v) })
#define border_fg(r, g, b)        ((StyleArg){ .prop = STYLE_BORDER_FG,       .rgb = { (r), (g), (b) } })
#define border_bg(r, g, b)        ((StyleArg){ .prop = STYLE_BORDER_BG,       .rgb = { (r), (g), (b) } })

#define border_flag(bit_, enabled_) \
    ((StyleArg){ .prop = STYLE_BORDER_FLAG, .flag = {.bit = bit_, .enabled = enabled_} })
#define border_bold(v)             border_flag(EFFECT_BOLD, v)
#define border_dim(v)              border_flag(EFFECT_DIM, v)
#define border_italic(v)           border_flag(EFFECT_ITALIC, v)
#define border_underline(v)        border_flag(EFFECT_UNDERLINE, v)
#define border_inverse(v)          border_flag(EFFECT_INVERSE, v)
#define border_strikethrough(v)    border_flag(EFFECT_STRIKETHROUGH, v)

#define MAX(a, b)     ((a) > (b) ? (a) : (b))
#define MIN(a, b)     ((a) < (b) ? (a) : (b))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

void style_apply(Widget *w, StyleArg *args, usize n);
#define style(w, ...) \
    do { \
        StyleArg _style_args[] = { __VA_ARGS__ }; \
        style_apply((Widget *)(w), _style_args, ARRAY_SIZE(_style_args)); \
    } while (0)

#define style_new(...) \
    ((StyleArgs){ \
        .items = (StyleArg[]){ __VA_ARGS__ }, \
        .count = sizeof((StyleArg[]){ __VA_ARGS__ }) / sizeof(StyleArg) \
    })

#define style_args(w, args, ...) \
    do { \
        style_apply((Widget *)w, args.items, args.count); \
        StyleArg _style_args[] = { __VA_ARGS__ }; \
        style_apply((Widget *)(w), _style_args, ARRAY_SIZE(_style_args)); \
    } while (0)

typedef enum {
    STYLE_TEXT_FG,
    STYLE_TEXT_BG,
    STYLE_TEXT_FLAG,
    STYLE_TEXT_ALIGN_X,
} TextStyleProp;

typedef struct {
    TextStyleProp prop;
    union {
        RGB rgb;
        Align align;
        void *p;
        struct {
            u8 bit;
            b32 enabled;
        } flag;
    };
} TextStyleArg;

#define text_fg(r, g, b)    ((TextStyleArg){ .prop = STYLE_TEXT_FG,        .rgb = { (r), (g), (b) } })
#define text_bg(r, g, b)    ((TextStyleArg){ .prop = STYLE_TEXT_BG,        .rgb = { (r), (g), (b) } })
#define text_align(v)       ((TextStyleArg){ .prop = STYLE_TEXT_ALIGN_X,   .align = (v) })

#define text_flag(bit_, enabled_) \
    ((TextStyleArg){ .prop = STYLE_TEXT_FLAG, .flag = {.bit = bit_, .enabled = enabled_} })
#define text_bold(v)             text_flag(EFFECT_BOLD, v)
#define text_dim(v)              text_flag(EFFECT_DIM, v)
#define text_italic(v)           text_flag(EFFECT_ITALIC, v)
#define text_underline(v)        text_flag(EFFECT_UNDERLINE, v)
#define text_inverse(v)          text_flag(EFFECT_INVERSE, v)
#define text_strikethrough(v)    text_flag(EFFECT_STRIKETHROUGH, v)

typedef struct {
    Effect effect;
    Align align_x;
} TextStyle;

typedef struct Text Text;

typedef struct {
    void (*layout)(Text *self, LayoutConstraint c);
    void (*draw)(Text *self, Rectangle bounds);
} TextVTable;

struct Text {
    List(CodePoint) text;
    TextStyle style;
    Size measured;
    TextVTable *vtable;
};

void text_style_apply(TextStyle *s, TextStyleArg *args, usize count);
#define text_style(t, ...) \
    do { \
        TextStyleArg _args[] = { __VA_ARGS__ }; \
        text_style_apply(&(t)->style, _args, ARRAY_SIZE(_args)); \
    } while (0)

void text_layout(Text *self, LayoutConstraint c);
void text_draw(Text *self, Rectangle bounds);

void default_text_layout(Text *self, LayoutConstraint c);
void default_text_draw(Text *self, Rectangle bounds);

static TextVTable text_methods = {
    .layout = default_text_layout,
    .draw = default_text_draw,
};

typedef struct {
    Widget widget;
    s8 label;
    b32 state;
} Button;

void button_layout(Widget *w, LayoutConstraint c);
void button_event(Widget *w);
void button_draw(Widget *w);
Button *button_new(s8 label);

static WidgetVTable button_methods = {
    .layout = button_layout,
    .hit_test = default_hit_test,
    .event = button_event,
    .update = default_update,
    .draw = button_draw,
};

typedef ContainerWidget Div;

Widget *div_hit_test(Widget *w);
void div_event(Widget *w);
void div_update(Widget *w);
void div_draw(Widget *w);
Div *div_new();

static WidgetVTable div_methods = {
    .layout = container_layout,
    .hit_test = div_hit_test,
    .event = div_event,
    .update = div_update,
    .draw = div_draw,
};

typedef struct {
    Widget widget;
    Text input;
} TextInput;

void text_input_layout(Widget *w, LayoutConstraint c);
void text_input_event(Widget *w);
void text_input_draw(Widget *w);
TextInput *text_input_new();

static WidgetVTable text_input_methods = {
    .layout = text_input_layout,
    .hit_test = default_hit_test,
    .event = text_input_event,
    .update = default_update,
    .draw = text_input_draw,
};

typedef struct {
    CodePoint *start;
    CodePoint *end;
    i32 max_width;
} WrapIter;

typedef struct {
    CodePoint *ptr;
    usize len;
} WrapSlice;

WrapIter wrap_iter_new(List(CodePoint) *text, i32 width);
WrapSlice wrap_iter_next(WrapIter *it);

#endif //LIBTUI_INCLUDE

#ifdef LIBTUI_IMPL

#include <unistd.h>
#include <termios.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

list_def(Cell);
list_def(Clip);
list_def(byte);

struct {
    struct termios orig_term;
    Unix_Pipe pipe;
    i32 timeout;
    Event event;
    u64 saved_time, dt;
    List(Cell) frontbuffer;
    List(Cell) backbuffer;
    List(byte) frame_cmds;
    Arena tmp;
    List(Clip) clips;
    u32 width, height;
} Terminal = {0};

u64 get_delta_time() { return Terminal.dt; }
u32 get_terminal_width() { return Terminal.width; }
u32 get_terminal_height() { return Terminal.height; }
b32 is_event(EventType e) { return Terminal.event.type == e; }
i32 get_mouse_x() { return Terminal.event.mouse.x; }
i32 get_mouse_y() { return Terminal.event.mouse.y; }
b32 is_mouse_pressed() { return Terminal.event.mouse.pressed; }
b32 is_mouse_released() { return !is_mouse_pressed(); }
b32 is_term_key(TermKey k) { return Terminal.event.term_key == k && Terminal.event.type == ETermKey; }
b32 is_codepoint(CodePoint cp) { return cp_equal(cp, Terminal.event.parsed_cp); }
CodePoint get_codepoint() { return Terminal.event.parsed_cp; }

b32 is_event_consumed() { return Terminal.event.handled; }
void event_consume() { Terminal.event.handled = true; }
b32 is_mouse_event() { return Terminal.event.type == EScrollUp ||
                              Terminal.event.type == EScrollDown ||
                              Terminal.event.type == EMouseDrag || 
                              Terminal.event.type == EMouseLeft ||
                              Terminal.event.type == EMouseMiddle ||
                              Terminal.event.type == EMouseRight; }

// private
void restore_term();
void update_screen_dimensions();
void handle_sigwinch(i32 signo);
i64  time_ms();
void save_timestamp();
void calculate_dt();
void poll_event(Event *e);
void parse_input(byte *str, isize n, Event *e);
void write_str_len(byte *str, usize len);
void write_strf_impl(byte *fmt, ...);
#define write_str(s)        write_str_len(s, sizeof(s) - 1)
#define write_strf(...)     write_strf_impl(__VA_ARGS__)
void emit_absolute_cursor_move(List(byte) *a, u32 row, u32 col);
void render();
void update_root_scope();

b32 try_parse_mouse(byte *str, isize n, Event *e);
b32 try_parse_term_key(byte *str, isize n, Event *e);
b32 try_parse_text(byte *str, isize n, Event *e);
void fix_wide_char(i32 x, i32 y);
void emit_cells(List(byte) *out, Cell *cells, usize start, usize len);
void emit_effect(List(byte) *out, Effect e);
void reset_effect(List(byte) *out);

CodePoint utf8_next(byte **start, byte *end);

static CodePoint UTF8_REPLACEMENT = {
    .raw = {0xEF, 0xBF, 0xBD},
    .raw_len = 3,
    .display_width = 1,
};

void write_str_len(byte *str, usize len) {
    write(STDOUT_FILENO, str, len);
}

void write_strf_impl(byte *fmt, ...) {
    static char buf[1024];
    va_list args;

    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        if (len > (int)sizeof(buf))
            len = sizeof(buf);
        write(STDOUT_FILENO, buf, len);
    }
}

void init_terminal() {
    assert(tcgetattr(STDIN_FILENO, &Terminal.orig_term) == 0);
    
    struct termios raw = Terminal.orig_term;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    assert(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0);
    //TODO: maybe add something to deal with the cursor on the user side
    //      so that the user can manipulate the cursor position maybe
    //TODO: handle pasting into the terminal?

    write_str("\33[?2004l");                 // disable bracketed paste mode
    write_str("\33[?1049h");                 // use alternate buffer
    write_str("\33[?25l");                   // hide cursor
    write_str("\33[?1000h");                 // enable mouse press/release
    write_str("\33[?1002h");                 // enable mouse press/release + drag
    // write_str("\33[?1003h");                 // enable mouse press/release + drag + hover
    write_str("\33[?1006h");                 // use mouse sgr protocol
    write_str("\33[0m");                     // reset text attributes
    write_str("\33[2J");                     // clear screen
    write_str("\33[H");                     // move cursor to home position

    update_screen_dimensions();
    atexit(restore_term);
    assert(pipe_open(&Terminal.pipe));

    struct sigaction sa = {0};
    sa.sa_handler = handle_sigwinch;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sa, NULL);

    list_resize(&Terminal.backbuffer, Terminal.width * Terminal.height);
    list_resize(&Terminal.frontbuffer, Terminal.width * Terminal.height);
    list_resize(&Terminal.frame_cmds, Terminal.width * Terminal.height);

    // manually add the terminal scope
    Rectangle r = {.w = Terminal.width, .h = Terminal.height};
    list_append(&Terminal.clips, (Clip) {.rect = r});

    Terminal.tmp = arena_init(MB(16));
}

void update_screen_dimensions() {
    struct winsize ws = {0};
    assert(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0);

    Terminal.width = ws.ws_col;
    Terminal.height = ws.ws_row;
}

void update_root_scope() {
    Rectangle r = {.w = Terminal.width, .h = Terminal.height};
    Terminal.clips.items[0] = (Clip) {.rect = r};
}

void restore_term() {
    assert(tcsetattr(STDIN_FILENO, TCSAFLUSH, &Terminal.orig_term) == 0);

    write_str("\33[?1000l");                     // disable mouse
    write_str("\33[?1002l");                     // disable mouse
    write_str("\33[0m");                         // reset text attributes
    write_str("\33[?25h");                       // show cursor
    write_str("\33[?1049l");                     // exit alternate buffer

    fd_close(Terminal.pipe.read_fd);
    fd_close(Terminal.pipe.write_fd);

    list_free(Terminal.backbuffer);
    list_free(Terminal.frontbuffer);
    list_free(Terminal.frame_cmds);
    list_free(Terminal.clips);

    arena_destroy(Terminal.tmp);
}

void handle_sigwinch(i32 signo) {
    update_screen_dimensions();

    u32 new_size = Terminal.width * Terminal.height;
    list_resize(&Terminal.backbuffer, new_size);
    list_resize(&Terminal.frontbuffer, new_size);

    update_root_scope();

    // trigger full redraw
    for (usize i = 0; i < Terminal.frontbuffer.count; ++i)
        Terminal.frontbuffer.items[i] = cell(cp_from_byte(0xFF), (Effect) {0});

    write(Terminal.pipe.write_fd, &signo, sizeof signo);
}

void set_max_timeout_ms(i32 timeout) { Terminal.timeout = timeout; }

void begin_frame() {
    save_timestamp();

    arena_clear(&Terminal.tmp);
    list_clear(&Terminal.frame_cmds);
    for (usize i = 0; i < Terminal.backbuffer.count; ++i)
        Terminal.backbuffer.items[i] = cell_empty();

    Terminal.event = (Event) {0};
    poll_event(&Terminal.event);
}

void save_timestamp()  { Terminal.saved_time = time_ms(); }
void calculate_dt() { Terminal.dt = time_ms() - Terminal.saved_time; }

i64 time_ms() {
    struct timespec ts = {0};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void end_frame() {
    render();
    write_str_len(Terminal.frame_cmds.items, Terminal.frame_cmds.count);
    calculate_dt();
}

//TODO: maybe hash each row and compare hashes?
void render() {
    Cell *back_items = Terminal.backbuffer.items;
    Cell *front_items = Terminal.frontbuffer.items;
    u32 screen_w = Terminal.width;

    for (u32 row = 0; row < Terminal.height; row++) {
        usize row_start = row * screen_w;
        usize row_end   = row_start + screen_w;

        usize pos = row_start;
        while (pos < row_end) {
            if (cell_equal(back_items[pos], front_items[pos])) {
                pos++;
                continue;
            }

            usize run_start = pos;
            Effect run_effect = back_items[run_start].effect;
            while (pos < row_end && 
                !cell_equal(back_items[pos], front_items[pos]) &&
                effect_equal(back_items[pos].effect, run_effect)) {
                pos++;
            }

            usize run_len = pos - run_start;
            u32 new_row = run_start / screen_w;
            u32 new_col = run_start % screen_w;
            emit_absolute_cursor_move(&Terminal.frame_cmds, new_row, new_col);
            emit_cells(&Terminal.frame_cmds, back_items, run_start, run_len);

            memcpy(
                front_items + run_start,
                back_items + run_start,
                run_len * sizeof(Cell)
            );
        }
    }
}

void emit_cells(List(byte) *out, Cell *cells, usize start, usize len) {
    emit_effect(out, cells[start].effect);

    for (usize i = 0; i < len; i++) {
        Cell c = cells[start + i];
        if (c.flags & CELL_CONTINUATION) continue;

        list_append_many(out, c.cp.raw, c.cp.raw_len);
    }

    reset_effect(out);
}

void emit_effect(List(byte) *out, Effect e) {
    if (e.flags == 0) return;

    Stream s = arena_stream_start(&Terminal.tmp, 64);
    stream_fmt(&s, "\33[");

    if (e.flags & EFFECT_BOLD)          stream_fmt(&s, "1;");
    if (e.flags & EFFECT_DIM)           stream_fmt(&s, "2;");
    if (e.flags & EFFECT_ITALIC)        stream_fmt(&s, "3;");
    if (e.flags & EFFECT_UNDERLINE)     stream_fmt(&s, "4;");
    if (e.flags & EFFECT_INVERSE)       stream_fmt(&s, "7;");
    if (e.flags & EFFECT_STRIKETHROUGH) stream_fmt(&s, "9;");

    if (e.flags & EFFECT_FG)
        stream_fmt(&s, "38;2;%u;%u;%u;", e.fg.r, e.fg.g, e.fg.b);
    if (e.flags & EFFECT_BG)
        stream_fmt(&s, "48;2;%u;%u;%u;", e.bg.r, e.bg.g, e.bg.b);

    // replace ';' with 'm'
    *(s.p - 1) = 'm';
    s8 result = stream_end(s);
    list_append_many(out, result.s, result.len);
}

void reset_effect(List(byte) *out) {
    s8 result = s8("\33[0m");
    list_append_many(out, result.s, result.len);
}

void emit_absolute_cursor_move(List(byte) *a, u32 row, u32 col) {
    Stream s = arena_stream_start(&Terminal.tmp, 64);
    stream_fmt(&s, "\33[%u;%uH", row + 1, col + 1);
    s8 result = stream_end(s);

    list_append_many(a, result.s, result.len);
}

void poll_event(Event *e) {
    #define PFD_SIZE 2
    struct pollfd pfd[PFD_SIZE] = {
        {.fd = Terminal.pipe.read_fd, .events = POLLIN},
        {.fd = STDIN_FILENO,          .events = POLLIN},
    };

    i32 timeout_ms = Terminal.timeout >= 0 ? Terminal.timeout : -1;
    i32 rval = poll(pfd, PFD_SIZE, timeout_ms);

    if (rval < 0) {
        if (errno == EAGAIN || errno == EINTR) {
            poll_event(e);
            return;
        }

        assert(false && "call to 'poll' failed");
    }

    else if (rval == 0) { // timeout
        e->type = ENone;
        return;
    }

    if (pfd[0].revents & POLLIN) { // window resize
        i32 sig;
        read(Terminal.pipe.read_fd, &sig, sizeof sig);
        e->type = EWinch;
        return;
    }

    if (pfd[1].revents & POLLIN) {
        isize n = 0;
        static byte buffer[256];
        n = read(STDIN_FILENO, buffer, sizeof buffer);

        assert(n > 0 && "read non-positive amount of bytes from STDIN");

        parse_input(buffer, n, e);
        return;
    }
}

void parse_input(byte *str, isize n, Event *e) {
    if (str[0] == TERMKEY_ESCAPE) {
        if (try_parse_mouse(str, n, e)) return;
        if (try_parse_term_key(str, n, e)) return;

        // Unknown escape sequence
        e->type = ENone;
        return;
    }

    // special case
    #define DELETE 127
    if (n == 1 && *str == DELETE) {
        e->type = ETermKey;
        e->term_key = TERMKEY_BACKSPACE;
        return;
    }

    if (try_parse_text(str, n, e)) return;

    e->type = ENone;
}

b32 try_parse_mouse(byte *str, isize n, Event *e) {
    if (n < 9 || memcmp(str, "\33[<", 3) != 0) return false;

    byte *p = str;
    u32 btn          = strtol(p + 3, &p, 10);
    e->mouse.x       = strtol(p + 1, &p, 10) - 1;
    e->mouse.y       = strtol(p + 1, &p, 10) - 1;
    e->mouse.pressed = (*p == 'M');

    switch (btn) {
        case 0:  e->type = EMouseLeft;   break;
        case 1:  e->type = EMouseMiddle; break;
        case 2:  e->type = EMouseRight;  break;
        case 32: e->type = EMouseDrag;   break;
        case 64: e->type = EScrollUp;    break;
        case 65: e->type = EScrollDown;  break;
        default: e->type = ENone;
    }

    return true;
}

static struct {byte str[4]; TermKey k;} term_key_table[] = {
    {"[A" , TERMKEY_UP},
    {"[B" , TERMKEY_DOWN},
    {"[C" , TERMKEY_RIGHT},
    {"[D" , TERMKEY_LEFT},
    {"[2~", TERMKEY_INSERT},
    {"[3~", TERMKEY_DELETE},
    {"[H" , TERMKEY_HOME},
    {"[4~", TERMKEY_END},
    {"[5~", TERMKEY_PAGEUP},
    {"[6~", TERMKEY_PAGEDOWN},
};

b32 try_parse_term_key(byte *str, isize n, Event *e) {
    if (n < 3 || n > 4) return false;

    for (usize i = 0; i < ARRAY_SIZE(term_key_table); i++) {
        byte *key = term_key_table[i].str;
        if (memcmp(str + 1, key, n - 1) == 0) {
            e->type = ETermKey;
            e->term_key = term_key_table[i].k;
            return true;
        }
    }

    return false;
}

b32 try_parse_text(byte *str, isize n, Event *e) {
    if (1 <= n && n <= 4) {
        e->type = ECodePoint;
        e->parsed_cp = utf8_next(&str, str + n);
        return true;
    }

    return false;
}

CodePoint utf8_next(byte **p, byte *end) {
    byte *s = *p;

    if (s >= end) {
        return UTF8_REPLACEMENT;
    }

    u8 first = s[0];
    if (first < 0x80) {
        *p += 1;
        return cp_from_byte(first);
    }

    usize len = 0;
    Unicode value = 0;

    if ((first & 0xE0) == 0xC0) {
        len = 2;
        value = first & 0x1F;
    } else if ((first & 0xF0) == 0xE0) {
        len = 3;
        value = first & 0x0F;
    } else if ((first & 0xF8) == 0xF0) {
        len = 4;
        value = first & 0x07;
    } else {
        *p += 1;
        return UTF8_REPLACEMENT;
    }

    if (s + len > end) {
        *p = end; // consume rest
        return UTF8_REPLACEMENT;
    }

    for (usize i = 1; i < len; i++) {
        if ((s[i] & 0xC0) != 0x80) {
            *p += i;
            return UTF8_REPLACEMENT;
        }
        value = (value << 6) | (s[i] & 0x3F);
    }

    CodePoint cp = cp_from_raw(s, len, unicode_width(value));
    *p += len;
    return cp;
}

u8 unicode_width(Unicode ch) {
    if (ch < 32 || (ch >= 0x7F && ch < 0xA0))
        return 0;

    // combining marks
    if ((ch >= 0x0300 && ch <= 0x036F) ||
        (ch >= 0x1AB0 && ch <= 0x1AFF) ||
        (ch >= 0x1DC0 && ch <= 0x1DFF) ||
        (ch >= 0x20D0 && ch <= 0x20FF) ||
        (ch >= 0xFE20 && ch <= 0xFE2F))
        return 0;

    // wide characters (CJK + emoji)
    if ((ch >= 0x1100 && ch <= 0x115F) ||
        (ch >= 0x2329 && ch <= 0x232A) ||
        (ch >= 0x2E80 && ch <= 0xA4CF) ||
        (ch >= 0xAC00 && ch <= 0xD7A3) ||
        (ch >= 0xF900 && ch <= 0xFAFF) ||
        (ch >= 0xFE10 && ch <= 0xFE19) ||
        (ch >= 0xFE30 && ch <= 0xFE6F) ||
        (ch >= 0xFF00 && ch <= 0xFF60) ||
        (ch >= 0xFFE0 && ch <= 0xFFE6) ||
        (ch >= 0x1F300 && ch <= 0x1F64F) ||
        (ch >= 0x1F900 && ch <= 0x1F9FF))
        return 2;

    return 1;
}

CodePoint cp_from_utf8(byte *s, u8 len) {
    return utf8_next(&s, s + len);
}

CodePoint cp_from_raw(byte *raw, u8 raw_len, u8 display_width) {
    CodePoint cp = {
        .raw_len = raw_len,
        .display_width = display_width,
    };

    memcpy(cp.raw, raw, raw_len);
    return cp;
}

CodePoint cp_from_byte(byte b) {
    return (CodePoint) {
        .raw = {b},
        .raw_len = 1,
        .display_width = 1,
    };
}

b32 cp_equal(CodePoint a, CodePoint b) {
    if (a.raw_len != b.raw_len) return false;
    if (a.display_width != b.display_width) return false;
    return memcmp(a.raw, b.raw, a.raw_len) == 0;
}

Cell cell(CodePoint cp, Effect e) { return (Cell) { .cp = cp, .effect = e }; }
Cell cell_lead(CodePoint cp, Effect e) { return (Cell) { .cp = cp, .flags = CELL_WIDE_LEAD, .effect = e }; }
Cell cell_cont(Effect e) { return (Cell) { .flags = CELL_CONTINUATION, .effect = e }; }
Cell cell_empty() { return (Cell) { .cp = cp_from_byte(' ') }; }
b32 cell_equal(Cell a, Cell b) { return memcmp(&a, &b, sizeof a) == 0; }

void put_str_(i32 x, i32 y, byte *s, usize len, Effect e) {
    byte *p = s;
    byte *end = s + len;

    while (p < end) {
        CodePoint cp = utf8_next(&p, end);
        put_cp(x, y, cp, e);
        x += cp.display_width;
    }
}

void put_cp_(i32 x, i32 y, CodePoint cp, Effect e) {
    Clip parent = clip_peek();

    if (x < 0 || y < 0) return;
    if (!point_in_rect(x, y, parent.rect)) return;

    u32 w = Terminal.width;
    Cell *cells = Terminal.backbuffer.items;

    if (cp.display_width == 1) {
        fix_wide_char(x, y);
        cells[x + y * w] = cell(cp, e);
        return;
    }

    if (cp.display_width == 2) {
        if ((u32)x + 1 >= w) return; // cannot fit

        fix_wide_char(x, y);
        fix_wide_char(x + 1, y);

        cells[x + y * w] = cell_lead(cp, e);
        cells[(x + 1) + y * w] = cell_cont(e);
        return;
    }

    // ignore other widths
    assert(false && "a codepoint with invalid width");
}

void put_effect(i32 x, i32 y, Effect e) {
    Clip parent = clip_peek();

    if (x < 0 || y < 0) return;
    if (!point_in_rect(x, y, parent.rect)) return;

    u32 w = Terminal.width;
    Cell *cells = Terminal.backbuffer.items;
    Cell *cur = &cells[x + y * w];
    
    cur->effect = e;
    if (cur->flags & CELL_WIDE_LEAD) {
        put_effect(x + 1, y, e);
    } else if (cur->flags & CELL_CONTINUATION) {
        put_effect(x - 1, y, e);
    }
}

void fix_wide_char(i32 x, i32 y) {
    u32 w = Terminal.width;
    Cell *cells = Terminal.backbuffer.items;
    Cell c = cells[x + y * w];

    if ((c.flags & CELL_WIDE_LEAD) && (u32)x + 1 < w) {
        cells[(x + 1) + y * w] = cell_empty();
    }

    if ((c.flags & CELL_CONTINUATION) && (u32)x > 0) {
        cells[(x - 1) + y * w] = cell_empty();
    }
}

void clip_push(i32 x, i32 y, i32 w, i32 h) {
    Rectangle r = {x,y,w,h};
    clip_push_rect(r);
}

void clip_push_rect(Rectangle r) {
    Clip parent = clip_peek();
    Rectangle clipped = rect_intersect(parent.rect, r);
    list_append(&Terminal.clips, (Clip){.rect = clipped});
}

Clip clip_pop() { 
    return list_pop(&Terminal.clips);
}

Clip clip_peek() {
    return list_last(&Terminal.clips);
}

b32 point_in_rect(i32 x, i32 y, Rectangle r) {
    return r.x <= x && x < r.x + r.w 
        && r.y <= y && y < r.y + r.h;
}

Rectangle rect_intersect(Rectangle a, Rectangle b) {
    i32 x1 = MAX(a.x, b.x);
    i32 y1 = MAX(a.y, b.y);
    i32 x2 = MIN(a.x + a.w, b.x + b.w);
    i32 y2 = MIN(a.y + a.h, b.y + b.h);

    if (x2 <= x1 || y2 <= y1) {
        return (Rectangle){0,0,0,0}; // fully clipped
    }

    return (Rectangle){ x1, y1, x2 - x1, y2 - y1 };
}

Rectangle rect_union(Rectangle a, Rectangle b) {
    i32 left   = MIN(a.x, b.x);
    i32 top    = MIN(a.y, b.y);
    i32 right  = MAX(a.x + a.w, b.x + b.w);
    i32 bottom = MAX(a.y + a.h, b.y + b.h);

    return (Rectangle) {
        .x = left,
        .y = top,
        .w = right - left,
        .h = bottom - top
    };
}

byte *fmt(byte *p, byte *end, byte *f, ...) {
    va_list args;
    va_start(args, f);
    p = vfmt(p, end, f, args);
    va_end(args);
    return p;
}

// Behaviour is similar to snprintf, in that it does not
// write beyond the *end pointer, but the returned pointer
// may point somewhere beyond the *end. 
byte *vfmt(byte *p, byte *end, byte *f, va_list args) {
    while (*f) {
        if (*f != '%') {
            if (p < end) *p = *f;
            p++;
            f++;
            continue;
        }

        f++;
        switch (*f++) {
            case 'd': {
                i64 v = va_arg(args, i64);

                if (v < 0) {
                    if (p < end) *p = '-';
                    p++;

                    u64 u = (u64)(-v);
                    p = fmt_uint(p, end, u, 10);
                } else {
                    p = fmt_uint(p, end, (u64)v, 10);
                }

            } break;

            case 'u': {
                u64 v = va_arg(args, u64);
                p = fmt_uint(p, end, v, 10);
            } break;

            case 'x': {
                u64 v = va_arg(args, u64);
                p = fmt_uint(p, end, v, 16);
            } break;

            case 's': {
                byte *s = va_arg(args, byte *);
                p = fmt_cstr(p, end, s);
            } break;

            case 'S': {
                s8 s = va_arg(args, s8);
                p = fmt_s8(p, end, s);
            } break;

            case '%': {
                if (p < end) *p = '%';
                p++;
            } break;
        }
    }

    return p;
}

byte *fmt_uint(byte *p, byte *end, u64 v, u8 base) {
    byte tmp[32];
    usize n = 0;

    do {
        u8 d = v % base;
        tmp[n++] = (d < 10) ? '0' + d : 'a' + d - 10;
        v /= base;
    } while (v);

    while (n--) {
        if (p < end) *p = tmp[n];
        p++;
    }

    return p;
}

byte *fmt_cstr(byte *p, byte *end, byte *s) {
    while (*s) {
        if (p < end) *p = *s;
        p++;
        s++;
    }
    return p;
}

byte *fmt_s8(byte *p, byte *end, s8 s) {
    for (usize i = 0; i < s.len; i++) {
        if (p < end) *p = s.s[i];
        p++;
    }
    return p;
}

Stream stream_start(byte *buffer, usize size) {
    return (Stream) {
        .start = buffer,
        .p = buffer,
        .end = buffer + size
    };
}

void stream_fmt(Stream *s, byte *f, ...) {
    va_list args;
    va_start(args, f);
    s->p = vfmt(s->p, s->end, f, args);
    va_end(args);
}

s8 stream_end(Stream s) {
    usize len = MIN(s.p - s.start, s.end - s.start);
    return s8(s.start, len);
}

Stream arena_stream_start(Arena *arena, usize size) {
    byte *buffer = arena_push(arena, byte, size);
    assert(buffer && "arena does not have enough memory");
    return stream_start(buffer, size);
}

void put_cp_debug(i32 x, i32 y, CodePoint cp) {
    u32 w = Terminal.width;
    Cell *cells = Terminal.backbuffer.items;
    cells[x + y * w] = cell(cp, (Effect) {0});
}

void debug(i32 x, i32 y, byte *fmt, ...) {
    Stream s = arena_stream_start(&Terminal.tmp, 256);

    va_list args;
    va_start(args, fmt);
    s.p = vfmt(s.p, s.end, fmt, args);
    va_end(args);

    s8 str = stream_end(s);
    for (usize i = 0; i < str.len; i++)
        put_cp_debug(x + i, y, cp_from_byte(str.s[i]));
}

void draw_line_(i32 x0, i32 y0, i32 x1, i32 y1, CodePoint cp, Effect e) {
    if (x0 == x1) { // vertical
        if (y1 < y0) {
            i32 tmp = y0;
            y0 = y1;
            y1 = tmp;
        }

        for (i32 y = y0; y <= y1; y++) {
            put_cp(x0, y, cp, e);
        }
    }
    else if (y0 == y1) { // horizontal
        if (x1 < x0) {
            i32 tmp = x0;
            x0 = x1;
            x1 = tmp;
        }

        for (i32 x = x0; x <= x1; x++) {
            put_cp(x, y0, cp, e);
        }
    }
}

void draw_box_(Rectangle r, Effect e) {
    if (r.w < 2 || r.h < 2) return;

    i32 x0 = r.x;
    i32 y0 = r.y;
    i32 x1 = r.x + r.w - 1;
    i32 y1 = r.y + r.h - 1;

    put_cp(x0, y0, cp("┌"), e);
    put_cp(x1, y0, cp("┐"), e);
    put_cp(x0, y1, cp("└"), e);
    put_cp(x1, y1, cp("┘"), e);

    draw_line(x0 + 1, y0, x1 - 1, y0, cp("─"), e);
    draw_line(x0 + 1, y1, x1 - 1, y1, cp("─"), e);
    draw_line(x0, y0 + 1, x0, y1 - 1, cp("│"), e);
    draw_line(x1, y0 + 1, x1, y1 - 1, cp("│"), e);
}

void fill_box(Rectangle r, Effect e) {
    for (i32 j = 0; j < r.h; ++j) {
        for (i32 i = 0; i < r.w; ++i) {
            put_effect(r.x + i, r.y + j, e);
        }
    }
}

// TUI

void container_layout_column(Widget *w, LayoutConstraint constraint);
void container_layout_row(Widget *w, LayoutConstraint constraint);
i32 aligned_primary_pos(i32 extra_space, Align align);
i32 aligned_secondary_pos(i32 parent_size, i32 child_size, Align align);

static struct {
    List(Transform) transforms;
    ContainerWidget *root;
    Widget *focus;
    Arena allocator;
} UI = {0};

void ui_init() {
    UI.allocator = arena_init(MB(16));
    list_append(&UI.transforms, ((Transform){0,0}));
}

void ui_register_root(Widget *w) {
    UI.root = div_new();
    style(UI.root, direction(LAYOUT_COLUMN), overflow(OVERFLOW_SCROLL_Y));
    container_add(&UI.root->widget, w);
}

void ui_set_focus(Widget *w) {
    UI.focus = w;
}

b32 ui_is_focused(Widget *w) {
    return w == UI.focus;
}

Widget *ui_get_focus() {
    return UI.focus;
}

void ui_dispatch_event(Widget *hit) {
    if (is_event(EMouseLeft) && is_mouse_pressed()) {
        ui_set_focus(hit);
    }

    Widget *w = is_mouse_event() ? hit : ui_get_focus();

    while (w) {
        widget_event(w);
        if (is_event_consumed()) break;
        w = w->parent;
    }
}

void ui_run() {
    LayoutConstraint c = {
        .max_h = get_terminal_height(),
        .max_w = get_terminal_width(),
    };

    UI.root->widget.style.w = get_terminal_width();
    UI.root->widget.style.h = get_terminal_height();

    widget_layout(&UI.root->widget, c);
    Widget *hit = widget_hit_test(&UI.root->widget);
    ui_dispatch_event(hit);
    widget_update(&UI.root->widget);
    widget_draw(&UI.root->widget);
}

void ui_put_cp_(i32 x, i32 y, CodePoint cp, Effect e) {
    Transform t = peek_transform();
    put_cp(x + t.x, y + t.y, cp, e);
}

void ui_put_str_(i32 x, i32 y, byte *s, usize len, Effect e) {
    Transform t = peek_transform();
    put_str(x + t.x, y + t.y, s, len, e);
}

void ui_draw_line_(i32 x0, i32 y0, i32 x1, i32 y1, CodePoint cp, Effect e) {
    Transform t = peek_transform();
    draw_line(x0 + t.x, y0 + t.y, x1 + t.x, y1 + t.y, cp, e);
}

void ui_draw_box_(i32 w, i32 h, Effect e) {
    Transform t = peek_transform();
    Rectangle r = {t.x, t.y, w, h};
    draw_box(r, e);
}

void ui_fill_box(i32 w, i32 h, Effect e) {
    Transform t = peek_transform();
    Rectangle r = {t.x, t.y, w, h};
    fill_box(r, e);
}

void push_transform(i32 dx, i32 dy) {
    Transform parent = list_last(&UI.transforms);
    Transform t = {
        parent.x + dx,
        parent.y + dy
    };

    list_append(&UI.transforms, t);
}

Transform pop_transform() {
    return list_pop(&UI.transforms);
}

Transform peek_transform() {
    return list_last(&UI.transforms);
}

Rectangle content_rect(Widget *w) {
    Transform t = peek_transform();

    return (Rectangle) {
        .x = t.x,
        .y = t.y,
        .w = w->size.w,
        .h = w->size.h,
    };
}

Rectangle content_bp_rect(Widget *w) {
    Rectangle r = content_rect(w);
    u8 pad = w->style.padding + w->style.border.width;
    r.x -= pad;
    r.y -= pad;
    r.w += 2 * pad;
    r.h += 2 * pad;
    return r;
}

b32 is_hit(Widget *w) {
    Rectangle r = content_bp_rect(w);
    return point_in_rect(get_mouse_x(), get_mouse_y(), r);
}

void widget_layout(Widget *w, LayoutConstraint c) {
    w->vtable->layout(w, c);
}

Widget *widget_hit_test(Widget *w) {
    push_transform(w->offset.x, w->offset.y);
    u8 m = w->style.margin;
    u8 b = w->style.border.width;
    u8 p = w->style.padding;
    push_transform(m , m);
    push_transform(b, b);
    push_transform(p, p);
    Widget *res = w->vtable->hit_test(w);
    pop_transform(); // p
    pop_transform(); // b
    pop_transform(); // m
    pop_transform();
    return res;
}

void widget_event(Widget *w) {
    // push_transform(w->offset.x, w->offset.y);
    w->vtable->event(w);
    // pop_transform();
}

void widget_update(Widget *w) { 
    // push_transform(w->offset.x, w->offset.y);
    w->vtable->update(w);
    // pop_transform();
}

void widget_draw(Widget *w) { 
    push_transform(w->offset.x, w->offset.y);

    u8 m = w->style.margin;
    u8 b = w->style.border.width;
    u8 p = w->style.padding;

    push_transform(m , m);

    if (b) {
        assert(w->style.border.draw && "border function not set");
        w->style.border.draw(
            w->size.w + 2 * (b + p),
            w->size.h + 2 * (b + p),
            &w->style.border
        );
    }

    push_transform(b, b);

    if (w->style.effect.flags & EFFECT_BG) {
        ui_fill_box(w->size.w + 2 * p, w->size.h + 2 * p, w->style.effect);
    }

    push_transform(p, p);

    clip_push_rect(content_rect(w));

    w->vtable->draw(w);

    clip_pop();
    pop_transform(); // p
    pop_transform(); // b
    pop_transform(); // m
    pop_transform(); // offset
}

b32 widget_is_container(Widget *w) { return w->metadata & WIDGET_CONTAINER; }

void u8_flag(u8 *flags, u8 bit, b32 enabled) {
    if (enabled) *flags |= bit;
    else *flags &= ~bit;
}

void style_apply(Widget *w, StyleArg *args, usize count) {
    assert(w);
    WidgetStyle *ws = &w->style;

    ContainerStyle *cs = NULL;
    if (widget_is_container(w)) {
        ContainerWidget *container = container_of(w, ContainerWidget, widget);
        cs = &container->container_style;
    }

    for (usize i = 0; i < count; i++) {
        StyleArg arg = args[i];
        switch (arg.prop) {
            // WidgetStyle properties
            case STYLE_WIDTH: ws->w = arg.i; break;
            case STYLE_HEIGHT: ws->h = arg.i; break;
            case STYLE_PADDING: ws->padding = arg.u; break;
            case STYLE_MARGIN: ws->margin = arg.u; break;
            case STYLE_ALIGN_SELF: ws->align_self = arg.align; break;
            case STYLE_BG:
                ws->effect.bg = arg.rgb;
                ws->effect.flags |= EFFECT_BG;
                break;

            // ContainerStyle properties
            case STYLE_DIRECTION:
                assert(cs && "STYLE_DIRECTION applied to non-container widget");
                cs->direction = arg.direction;
                break;
            case STYLE_ALIGN_CHILDREN:
                assert(cs && "STYLE_ALIGN_CHILDREN applied to non-container widget");
                cs->align_children = arg.align;
                break;
            case STYLE_SPACING:
                assert(cs && "STYLE_SPACING applied to non-container widget");
                cs->spacing = (u8)arg.u;
                break;
            case STYLE_OVERFLOW:
                assert(cs && "STYLE_OVERFLOW applied to non-container widget");
                cs->overflow = arg.overflow;
                break;

            // Border properties
            case STYLE_BORDER_WIDTH: ws->border.width = arg.u; break;
            case STYLE_BORDER_FN: ws->border.draw = arg.p; break;
            case STYLE_BORDER_FG:
                ws->border.effect.fg = arg.rgb;
                ws->border.effect.flags |= EFFECT_FG;
                break;
            case STYLE_BORDER_BG:
                ws->border.effect.bg = arg.rgb;
                ws->border.effect.flags |= EFFECT_BG;
                break;
            case STYLE_BORDER_FLAG: u8_flag(&ws->border.effect.flags, arg.flag.bit, arg.flag.enabled); break;

            default: assert(false && "unknown style property"); break;
        }
    }
}

void text_style_apply(TextStyle *s, TextStyleArg *args, usize count) {
    for (usize i = 0; i < count; i++) {
        TextStyleArg arg = args[i];
        switch (arg.prop) {
            case STYLE_TEXT_FG:
                s->effect.fg = arg.rgb;
                s->effect.flags |= EFFECT_FG;
                break;
            case STYLE_TEXT_BG:
                s->effect.bg = arg.rgb;
                s->effect.flags |= EFFECT_BG;
                break;
            case STYLE_TEXT_FLAG: u8_flag(&s->effect.flags, arg.flag.bit, arg.flag.enabled); break;
            case STYLE_TEXT_ALIGN_X: s->align_x = arg.align; break;
            default: assert(false && "unknown text style property");
        }
    }
}

void text_layout(Text *self, LayoutConstraint c) { self->vtable->layout(self, c); }
void text_draw(Text *self, Rectangle bounds) { self->vtable->draw(self, bounds); }

void default_text_layout(Text *self, LayoutConstraint c) {
    WrapIter it = wrap_iter_new(&self->text, c.max_w);

    i32 lines = 0;
    while (wrap_iter_next(&it).len > 0) lines++;

    self->measured.w = c.max_w;
    self->measured.h = MAX(1, lines);
}

void default_text_draw(Text *self, Rectangle bounds) {
    WrapIter it = wrap_iter_new(&self->text, self->measured.w);

    i32 y = bounds.y;
    while (1) {
        WrapSlice slice = wrap_iter_next(&it);
        if (slice.len == 0) break;

        i32 x = bounds.x + aligned_secondary_pos(bounds.w, self->measured.w, self->style.align_x);

        for (usize i = 0; i < slice.len; i++) {
            CodePoint cp = slice.ptr[i];
            ui_put_cp(x, y, cp, self->style.effect);
            x += cp.display_width;
        }

        y++;
        // here could be some overflow policies
        // if (y >= bounds.y + bounds.h) break;
    }
}

Widget *default_hit_test(Widget *w) {
    return is_hit(w) ? w : NULL;
}

void default_update(Widget *w) { UNUSED(w); }

void default_border(i32 w, i32 h, BorderStyle *self) {
    ui_draw_box(w, h, self->effect);
}

i32 apply_min_wh_constraint(i32 a, i32 b) {
    if (a == 0) return b;
    return MIN(a, b);
}

i32 widget_mbp(Widget *w) { return w->style.margin + w->style.border.width + w->style.padding; }
i32 widget_total_width(Widget *w) { return w->size.w + 2 * widget_mbp(w); }
i32 widget_total_height(Widget *w) { return w->size.h + 2 * widget_mbp(w); }

void container_layout(Widget *w, LayoutConstraint c) {
    assert(widget_is_container(w));
    ContainerWidget *container = container_of(w, ContainerWidget, widget);

    i32 mbp = widget_mbp(w);

    LayoutConstraint constraint = {
        .max_w = apply_min_wh_constraint(container->widget.style.w, c.max_w - mbp * 2),
        .max_h = apply_min_wh_constraint(container->widget.style.h, c.max_h - mbp * 2),
    };

    for (usize i = 0; i < container->children.count; i++)
        widget_layout(container->children.items[i], constraint);

    if (container->container_style.direction == LAYOUT_COLUMN) {
        container_layout_column(w, constraint);
    } else {
        container_layout_row(w, constraint);
    }
}

void container_layout_column(Widget *w, LayoutConstraint constraint) {
    ContainerWidget *container = container_of(w, ContainerWidget, widget);

    i32 primary_axis = 0;
    i32 secondary_axis_max = 0;
    for (usize i = 0; i < container->children.count; i++) {
        Widget *child = container->children.items[i];

        primary_axis += widget_total_height(child);
        secondary_axis_max = MAX(secondary_axis_max, widget_total_width(child));

        if (i < container->children.count - 1) {
            primary_axis += container->container_style.spacing;
        }
    }

    i32 content_w = MAX(secondary_axis_max, w->style.w);
    i32 content_h = MAX(primary_axis, w->style.h);

    // Store how much space is needed for content
    w->size.w = MIN(content_w, constraint.max_w);
    if (container->container_style.overflow == OVERFLOW_VISIBLE_Y) w->size.h = content_h;
    else w->size.h = MIN(content_h, constraint.max_h);

    i32 extra = w->size.h - primary_axis;
    i32 start = aligned_primary_pos(extra, container->container_style.align_children);

    container->scroll.content_size = primary_axis;
    container->scroll.content_start = start;
    container->scroll.viewport_size = w->size.h;

    for (usize i = 0; i < container->children.count; i++) {
        Widget *child = container->children.items[i];
        Align align = child->style.align_self;

        child->offset.x = aligned_secondary_pos(w->size.w, widget_total_width(child), align);
        child->offset.y = start;

        start += widget_total_height(child) + container->container_style.spacing;
    }
}

void container_layout_row(Widget *w, LayoutConstraint constraint) {
    ContainerWidget *container = container_of(w, ContainerWidget, widget);

    i32 primary_axis = 0;
    i32 secondary_axis_max = 0;
    for (usize i = 0; i < container->children.count; i++) {
        Widget *child = container->children.items[i];

        primary_axis += widget_total_width(child);
        secondary_axis_max = MAX(secondary_axis_max, widget_total_height(child));

        if (i < container->children.count - 1) {
            primary_axis += container->container_style.spacing;
        }
    }

    i32 content_w = MAX(primary_axis, w->style.w);
    i32 content_h = MAX(secondary_axis_max, w->style.h);

    w->size.w = MIN(content_w, constraint.max_w);
    w->size.h = MIN(content_h, constraint.max_h);

    i32 extra = w->size.w - primary_axis;
    i32 start = aligned_primary_pos(extra, container->container_style.align_children);

    for (usize i = 0; i < container->children.count; i++) {
        Widget *child = container->children.items[i];
        Align align = child->style.align_self;

        child->offset.x = start;
        child->offset.y = aligned_secondary_pos(w->size.h, widget_total_height(child), align);

        start += widget_total_width(child) + container->container_style.spacing;
    }
}

i32 aligned_primary_pos(i32 extra_space, Align align) {
    switch (align) {
        case ALIGN_CENTER: return extra_space / 2;
        case ALIGN_END: return extra_space;
        case ALIGN_START: return 0;
        default: assert(false && "unknown align value");
    }
}

i32 aligned_secondary_pos(i32 parent_size, i32 child_size, Align align) {
    switch (align) {
        case ALIGN_CENTER: return (parent_size - child_size) / 2;
        case ALIGN_END: return parent_size -  child_size;
        case ALIGN_START: return 0;
        default: assert(false && "unknown align value");
    }
}

void container_add(Widget *c, Widget *w) {
    assert(widget_is_container(c));
    ContainerWidget *container = container_of(c, ContainerWidget, widget);
    w->parent = c;
    list_append(&container->children, w);
}

void button_layout(Widget *w, LayoutConstraint c) {
    Button *b = container_of(w, Button, widget);

    w->size.h = 1;
    w->size.w = MIN(b->label.len, (u32)c.max_w);
}

void button_event(Widget *w) {
    Button *b = container_of(w, Button, widget);

    if (is_event(EMouseLeft) && is_mouse_pressed()) {
        b->state = !b->state;
        event_consume();
    }
}

void button_draw(Widget *w) {
    Button *b = container_of(w, Button, widget);

    if (b->state) ui_put_str(0, 0, b->label.s, b->label.len);
}

Button *button_new(s8 label) {
    Button *b = arena_push(&UI.allocator, Button);
    assert(b);

    b->label = label;
    b->widget.vtable = &button_methods;
    b->widget.style.border.draw = default_border;

    return b;
}

Widget *div_hit_test(Widget *w) {
    Div *div = container_of(w, Div, widget);

    scroll_apply(&div->scroll);
    for (i32 i = div->children.count - 1; i >= 0; i--) {
        Widget *child = div->children.items[i];
        Widget *hit = widget_hit_test(child);

        if (hit) {
            scroll_pop();
            return hit;
        }
    }
    scroll_pop();

    return default_hit_test(w);
}

void div_event(Widget *w) {
    Div *div = container_of(w, Div, widget);
    if (div->container_style.overflow != OVERFLOW_SCROLL_Y) return;

    if (is_event(EScrollDown)) {
        scroll_incr(&div->scroll);
        event_consume();
    }

    if (is_event(EScrollUp)) {
        scroll_decr(&div->scroll);
        event_consume();
    }
}

void div_update(Widget *w) {
    Div *div = container_of(w, Div, widget);

    // scroll_apply(&div->scroll);
    for (usize i = 0; i < div->children.count; i++) {
        Widget *child = div->children.items[i];
        widget_update(child);
    }
    // scroll_pop();
}

void div_draw(Widget *w) {
    Div *div = container_of(w, Div, widget);

    scroll_apply(&div->scroll);
    for (usize i = 0; i < div->children.count; i++) {
        Widget *child = div->children.items[i];
        widget_draw(child);
    }
    scroll_pop();
}

Div *div_new(void) {
    Div *b = arena_push(&UI.allocator, Div);
    assert(b);
    b->widget.vtable = &div_methods;
    b->widget.style.border.draw = default_border;
    u8_flag(&b->widget.metadata, WIDGET_CONTAINER, true);
    return b;
}

void text_input_layout(Widget *w, LayoutConstraint c) {
    TextInput *t = container_of(w, TextInput, widget);

    text_layout(&t->input, (LayoutConstraint) {
        .max_w = MIN(20, c.max_w),
        .max_h = c.max_h,
    });

    w->size.w = MAX(t->input.measured.w, t->widget.style.w);
    w->size.h = t->input.measured.h;
}

void text_input_event(Widget *w) {
    TextInput *t = container_of(w, TextInput, widget);

    if (!ui_is_focused(w)) return;

    if (is_event(ECodePoint)) {
        list_append(&t->input.text, get_codepoint());
        event_consume();
    }

    if (is_term_key(TERMKEY_BACKSPACE)) {
        if (t->input.text.count > 0)
            t->input.text.count--;
        event_consume();
    }
}

void text_input_draw(Widget *w) {
    TextInput *t = container_of(w, TextInput, widget);

    text_draw(&t->input, (Rectangle){0, 0, w->size.w, w->size.h});

    // if (ui_is_focused(w)) {
    //     ui_put_cp(x, y, cp("_"));
    // }
}

TextInput *text_input_new() {
    TextInput *t = arena_push(&UI.allocator, TextInput);
    t->widget.vtable = &text_input_methods;
    t->widget.style.border.draw = default_border;
    t->input.vtable = &text_methods;
    return t;
}

WrapIter wrap_iter_new(List(CodePoint) *text, i32 width) {
    return (WrapIter) {
        .start = text->items,
        .end = text->items + text->count,
        .max_width = width,
    };
}

WrapSlice wrap_iter_next(WrapIter *it) {
    if (it->start >= it->end) {
        return (WrapSlice) {0};
    }

    CodePoint *line_start = it->start;
    CodePoint *p = it->start;

    i32 width = 0;
    while (p < it->end) {
        CodePoint cp = *p;

        if (width + cp.display_width > it->max_width)
            break;

        width += cp.display_width;
        p++;
    }

    CodePoint *line_end = p;
    it->start = p;

    return (WrapSlice) {
        .ptr = line_start,
        .len = line_end - line_start,
    };
}

//TODO: flex layout
//TODO: stack layout
//TODO: list widget with scrollbar?
//TODO: keyboard focus navigation
//TODO: other css-like properties
//TODO: widget tree debugger

#endif //LIBTUI_IMPL
