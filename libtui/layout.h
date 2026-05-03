#ifndef LIBTUI_LAYOUT_INCLUDE
#define LIBTUI_LAYOUT_INCLUDE

#define PSH_CORE_NO_PREFIX
#include "psh_core/psh_core.h"

#include "style.h"

typedef struct LayoutNode LayoutNode;
list_def(LayoutNode);

typedef enum {
    LAYOUT_NODE_RECT,
    LAYOUT_NODE_TEXT,
} LayoutNodeType;

typedef struct {
    Sizing size;
    Color color;
} LayoutNodeStyle;

struct LayoutNode {
    LayoutNodeType type;

    LayoutNode *parent;
    List(LayoutNode) children;

    LayoutNodeStyle style;

    i32 x, y; // resolved coords
    i32 w, h; // resolved w, h

    void *userdata;
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
    };
} LayoutCommand;

b32 layout_cmd_next(LayoutCommand *out);

#endif

#ifdef LIBTUI_LAYOUT_IMPL

static struct {
    LayoutNode *root;
    Arena tmp;
} Layout = {0};

#endif