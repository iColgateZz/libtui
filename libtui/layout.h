#ifndef LIBTUI_LAYOUT_INCLUDE
#define LIBTUI_LAYOUT_INCLUDE

#define PSH_CORE_NO_PREFIX
#include "psh_core/psh_core.h"

#include "unicode.h"

typedef struct LayoutNode LayoutNode;
list_def(LayoutNode);

typedef enum {
    SIZE_FIT,
    SIZE_FILL,
    SIZE_FIXED,
} SizeMode;

typedef struct {
    SizeMode mode;
    union {
        struct {
            i32 value;
        } fixed;
        struct {
            i32 min;
            i32 max;
        } fit;
        struct {
            i32 min;
            i32 max;
        } fill;
    };
} Size;

typedef struct {
    Size w;
    Size h;
} Sizing;

typedef struct {
    u8 r, g, b;
} Color;

typedef enum {
    DIR_ROW,
    DIR_COL,
} Direction;

typedef struct {
    Sizing size;
    Color color;
    u8 padding;
    u8 spacing;
    // u8 margin;
    // BorderStyle border;
    Direction direction;
} LayoutNodeStyle;

typedef struct {

} TextStyle;

typedef enum {
    LAYOUT_NODE_CONTAINER,
    LAYOUT_NODE_TEXT,
} LayoutNodeType;

struct LayoutNode {
    LayoutNode *parent;
    i32 x, y; // resolved coords
    i32 w, h; // resolved w, h
    
    LayoutNodeType type;
    union {
        struct {
            List(LayoutNode) children;
            LayoutNodeStyle style;
            // void *userdata;
        } _LAYOUT_NODE_CONTAINER;
        struct {
            List(CodePoint) text;
            TextStyle style;
        } _LAYOUT_NODE_TEXT;
    };
};

typedef enum {
    LAYOUT_CMD_RECT,
    // LAYOUT_CMD_TEXT,
    LAYOUT_CMD_CLIP_START,
    LAYOUT_CMD_CLIP_END,
    // LAYOUT_CMD_BORDER, // pass concrete codepoints to draw
} LayoutCommandType;

typedef struct {
    LayoutCommandType type;
    union {
        struct {
            i32 x, y, w, h;
            Color color;
        } _LAYOUT_CMD_RECT;
        struct {
            i32 x, y, w, h;
        } _LAYOUT_CMD_CLIP_START;
    };
} LayoutCommand;

b32 layout_cmd_next(LayoutCommand *out);
void layout(LayoutNode *root);
void layout_set_screen_w(u32 w);
void layout_set_screen_h(u32 h);
// hit testing, event handling, and state updates
// happen after layout phase

//TODO: move to psh_core
#define typeof __typeof__
#define match(tu) for (typeof(tu) _tu = tu, *p = &_tu; p; p = NULL) switch ((tu).type)
// may be fixed with __VA_OPT(__VA_ARGS__,) in C23 or
// https://medium.com/@pauljlucas/using-advanced-c-preprocessor-macros-for-a-pre-c23-c-20-va-opt-substitute-bccefde27817
#define case(...)   xcase(__VA_ARGS__, _case2, _case1)(__VA_ARGS__)
#define xcase(a, b, c, ...) c
#define _case2(tag, varname) break; case (tag): \
        for (typeof(_tu._##tag) varname = _tu._##tag, *_once = &varname; _once; _once = NULL)
#define _case1(tag) break; case (tag):
#define otherwise break; default:

#define tag(T, tag, ...) ((T) {.type = tag, ._##tag = __VA_ARGS__})
#define tag0(T, tag) ((T) {.type = tag})

#endif

#ifdef LIBTUI_LAYOUT_IMPL

list_def(LayoutCommand);

static struct {
    LayoutNode *root;
    Arena tmp;
    List(LayoutCommand) cmds;
    usize cmd_idx;
    i32 screen_w, screen_h;
} Layout = {0};

void layout_set_screen_w(u32 w) { Layout.screen_w = w; }
void layout_set_screen_h(u32 h) { Layout.screen_h = h; }

b32 layout_cmd_next(LayoutCommand *out) {
    if (Layout.cmd_idx >= Layout.cmds.count) {
        Layout.cmd_idx = 0;
        Layout.cmds.count = 0;
        return false;
    }

    *out = Layout.cmds.items[Layout.cmd_idx++];
    return true;
}

void layout_intrinsic_width(LayoutNode *node);
void layout_fill_width(LayoutNode *node);
void layout_intrinsic_height(LayoutNode *node);
void layout_fill_height(LayoutNode *node);
void layout_positions(LayoutNode *node);
void layout_commands(LayoutNode *node);

void layout(LayoutNode *root) {
    layout_intrinsic_width(root);
    layout_fill_width(root);

    layout_intrinsic_height(root);
    layout_fill_height(root);

    layout_positions(root);

    layout_commands(root);
    // other nodes in the pipeline go here..
}

void layout_intrinsic_width(LayoutNode *node) {
    if (node->type == LAYOUT_NODE_TEXT)
    {
        i32 text_width = 0;
        List(CodePoint) text = node->_LAYOUT_NODE_TEXT.text;

        for (usize i = 0; i < text.count; ++i)
            text_width += text.items[i].display_width;

        node->w = text_width;
    }
    else if (node->type == LAYOUT_NODE_CONTAINER)
    {
        List(LayoutNode) children = node->_LAYOUT_NODE_CONTAINER.children;
        for (usize i = 0; i < children.count; ++i)
            layout_intrinsic_width(&children.items[i]);

        LayoutNodeStyle style = node->_LAYOUT_NODE_CONTAINER.style;
        Size wsize = style.size.w;
        if (wsize.mode == SIZE_FIXED) {
            //TODO: account for border
            //      do I add padding here?
            node->w = wsize.fixed.value;
        }
        else if (wsize.mode == SIZE_FIT || wsize.mode == SIZE_FILL) 
        {
            if (style.direction == DIR_ROW)
            {
                i32 children_width = 0;
                for (usize i = 0; i < children.count; ++i)
                    children_width += children.items[i].w;

                node->w = children_width + 2 * style.padding + 
                          MAX(children.count - 1, 0) * style.spacing;
            }
            else if (style.direction == DIR_COL)
            {
                i32 max_child_w = 0;
                for (usize i = 0; i < children.count; ++i)
                    max_child_w = MAX(children.items[i].w, max_child_w);
                
                node->w = max_child_w + 2 * style.padding;
            }

            if (wsize.fit.min < wsize.fit.max)
                node->w = CLAMP(node->w, wsize.fit.min, wsize.fit.max);
        }
    }
}

void layout_fill_width(LayoutNode *node) { UNUSED(node); }

void layout_intrinsic_height(LayoutNode *node) {
    if (node->type == LAYOUT_NODE_TEXT)
    {

    }
    else if (node->type == LAYOUT_NODE_CONTAINER)
    {
        List(LayoutNode) children = node->_LAYOUT_NODE_CONTAINER.children;
        for (usize i = 0; i < children.count; ++i)
            layout_intrinsic_height(&children.items[i]);

        LayoutNodeStyle style = node->_LAYOUT_NODE_CONTAINER.style;
        Size hsize = style.size.h;
        if (hsize.mode == SIZE_FIXED) {
            //TODO: account for border
            //      do I add padding here?
            node->h = hsize.fixed.value;
        }
        else if (hsize.mode == SIZE_FIT || hsize.mode == SIZE_FILL) 
        {
            if (style.direction == DIR_ROW)
            {
                i32 max_child_h = 0;
                for (usize i = 0; i < children.count; ++i)
                    max_child_h = MAX(children.items[i].h, max_child_h);
                
                node->h = max_child_h + 2 * style.padding;
            }
            else if (style.direction == DIR_COL)
            {
                i32 children_height = 0;
                for (usize i = 0; i < children.count; ++i)
                    children_height += children.items[i].h;

                node->h = children_height + 2 * style.padding + 
                          MAX(children.count - 1, 0) * style.spacing;
            }

            if (hsize.fit.min < hsize.fit.max)
                node->h = CLAMP(node->h, hsize.fit.min, hsize.fit.max);
        }
    }
}

void layout_fill_height(LayoutNode *node) { UNUSED(node); }

void layout_positions(LayoutNode *node) {
    if (node->type == LAYOUT_NODE_TEXT)
    {

    }
    else if (node->type == LAYOUT_NODE_CONTAINER)
    {
        if (node->parent == NULL)
            node->x = node->y = 0;

        LayoutNodeStyle style = node->_LAYOUT_NODE_CONTAINER.style;
        i32 pos_x = node->x + style.padding;
        i32 pos_y = node->y + style.padding;

        //TODO: alignment across axis goes here
        List(LayoutNode) children = node->_LAYOUT_NODE_CONTAINER.children;
        if (style.direction == DIR_ROW)
        {
            for (usize i = 0; i < children.count; ++i) {
                LayoutNode *child = &children.items[i];
                child->x = pos_x;
                child->y = pos_y;

                pos_x += child->w + style.spacing;
            }
        }
        else if (style.direction == DIR_COL)
        {
            for (usize i = 0; i < children.count; ++i) {
                LayoutNode *child = &children.items[i];
                child->x = pos_x;
                child->y = pos_y;

                pos_y += child->h + style.spacing;
            }
        }

        for (usize i = 0; i < children.count; ++i)
            layout_positions(&children.items[i]);
    }
}

void layout_commands(LayoutNode *node) {
    match(*node) {
        case(LAYOUT_NODE_TEXT, text) {}
        case(LAYOUT_NODE_CONTAINER, container) {
            List(LayoutNode) children = container.children;
            LayoutNodeStyle style = container.style;

            list_append(&Layout.cmds, 
                tag(LayoutCommand, LAYOUT_CMD_CLIP_START, {
                    .x = node->x, .y = node->y, .w = node->w, .h = node->h
            }));

            list_append(&Layout.cmds,
                tag(LayoutCommand, LAYOUT_CMD_RECT, {
                    .x = node->x, .y = node->y, .w = node->w, .h = node->h, .color = style.color
            }));

            for (usize i = 0; i < children.count; ++i)
                layout_commands(&children.items[i]);

            list_append(&Layout.cmds, tag0(LayoutCommand, LAYOUT_CMD_CLIP_END));
        }
    }
}

#endif