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

typedef enum {
    LAYOUT_NODE_CONTAINER,
    LAYOUT_NODE_TEXT,
} LayoutNodeType;

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

struct LayoutNode {
    LayoutNode *parent;
    LayoutNodeType type;
    i32 x, y; // resolved coords
    i32 w, h; // resolved w, h

    union {
        struct {
            List(LayoutNode) children;
            LayoutNodeStyle style;
            // void *userdata;
        } container;
        struct {
            List(CodePoint) text;
            TextStyle style;
        } text;
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
        } rect;
        struct {
            i32 x, y, w, h;
        } clip;
    };
} LayoutCommand;

b32 layout_cmd_next(LayoutCommand *out);
void layout(LayoutNode *root);
// hit testing, event handling, and state updates
// happen after layout phase

#endif

#ifdef LIBTUI_LAYOUT_IMPL

list_def(LayoutCommand);

static struct {
    LayoutNode *root;
    Arena tmp;
    List(LayoutCommand) cmds;
    usize cmd_idx;
} Layout = {0};

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

void layout(LayoutNode *root) {
    layout_intrinsic_width(root);
    // other nodes in the pipeline go here..
}

void layout_intrinsic_width(LayoutNode *node) {
    if (node->type == LAYOUT_NODE_TEXT)
    {
        i32 text_width = 0;
        List(CodePoint) text = node->text.text;

        for (usize i = 0; i < text.count; ++i)
            text_width += text.items[i].display_width;

        node->w = text_width;
    }
    else if (node->type == LAYOUT_NODE_CONTAINER)
    {
        List(LayoutNode) children = node->container.children;
        for (usize i = 0; i < children.count; ++i)
            layout_intrinsic_width(&children.items[i]);

        LayoutNodeStyle style = node->container.style;
        Size wsize = style.size.w;
        if (wsize.mode == SIZE_FIXED) {
            //TODO: account for border
            node->w = wsize.fixed.value + 2 * style.padding;
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

#endif