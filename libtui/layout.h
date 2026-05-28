#ifndef LIBTUI_LAYOUT_INCLUDE
#define LIBTUI_LAYOUT_INCLUDE

#define PSH_CORE_NO_PREFIX
#include "psh_core/psh_core.h"

#include "style.h"
#include "unicode.h"

typedef struct LayoutNode LayoutNode;
list_def(LayoutNode);

typedef enum {
    LAYOUT_NODE_CONTAINER,
    LAYOUT_NODE_TEXT,
} LayoutNodeType;

typedef struct {
    Sizing size;
    Color color;
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

// it is meant to run the whole pipeline
// currently, the impl is just for testing
// void layout_nodes(LayoutNode* root) {
//     root->w = root->style.size.w.value;
//     root->h = root->style.size.h.value;

//     list_append(
//         &Layout.cmds, 
//         ((LayoutCommand) {
//             .type = LAYOUT_CMD_CLIP_START,
//             .clip = {.x = root->x, .y = root->y, .w = root->w, .h = root->h}, 
//         })
//     );

//     list_append(
//         &Layout.cmds, 
//         ((LayoutCommand) {
//             .type = LAYOUT_CMD_RECT,
//             .rect = {.x = root->x, .y = root->y, .w = root->w, .h = root->h, .color = root->style.color}
//         })
//     );

//     for (usize i = 0; i < root->children.count; ++i) {
//         layout_nodes(&root->children.items[i]);
//     }

//     list_append(
//         &Layout.cmds, 
//         (LayoutCommand) {.type = LAYOUT_CMD_CLIP_END}
//     );
// }

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
    if (node->type == LAYOUT_NODE_TEXT) {

    } else {
        for (usize i = 0; i < node->container.children.count; ++i) {
            layout_intrinsic_width(&node->container.children.items[i]);
        }


    }
}

#endif