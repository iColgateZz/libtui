#ifndef LIBTUI_LAYLA_INTERNAL_INCLUDE
#define LIBTUI_LAYLA_INTERNAL_INCLUDE

#include "layla.h"

#define PSH_CORE_NO_PREFIX
#include "psh_core.h"

typedef struct {
    i32 offset;
    i32 count;
} Layout__ChildrenIndices;

typedef i32 Layout__TempID;

typedef struct {
    //TODO: using i16 is probably more than enough
    Layout__TempID parent;
    Layout_PersistentID id;
    i32 x, y; // resolved coords
    i32 w, h; // resolved w, h
    i32 min_w, min_h;
    Layout__ChildrenIndices children;
    
    LAYOUT_PACKED_ENUM {
        //TODO: for custom nodes there may be 2 variants:
        //      one that has a vtable with all pipeline methods,
        //      one that only has a custom draw method that emits renderer commands.

        // Maybe instead of adding a new type just emit a custom command that gives
        // user enough context so that they can adapt it for the renderer?

        // Just pass the userdata pointer transparently via a command.
        LAYOUT_NODE_CONTAINER,
        LAYOUT_NODE_TEXT,
    } type;
    union {
        struct {
            Layout_ContainerConfig config;
        } container;
        struct {
            Layout_TextConfig config;
        } text;
    } as;
} Layout__Node;

list_def(Layout_Command);
list_def(Layout__Node);
list_def(Layout__TempID);
typedef Layout__Node* Layout__NodePtr;
list_def(Layout__NodePtr);

#define LAYOUT_TEMP_ID_NONE ((Layout__TempID)-1)
#define LAYOUT_PERSISTENT_ID_NONE 0
#define LAYOUT_ROOT_ID 0

//TODO: externally supplied scroll offsets?
typedef struct {
    Layout_PersistentID id;
    i32 y;
    i32 max_y;
} Layout__ScrollState;

list_def(Layout__ScrollState);

typedef enum {
    DIM_X,
    DIM_Y,
} Layout__Dimension;

typedef struct {
    i32 min;
    i32 max;
} Layout__SizeRange;

typedef struct {
    List(Layout__Node) nodes;
    List(Layout__TempID) open_node_stack;
    List(Layout__TempID) temporary_child_stack;
    List(Layout__TempID) frame_children;
    List(Layout_Command) commands;
    List(Layout__ScrollState) scroll_states;
    i32 width, height;
    i32 cursor_x, cursor_y;
    Layout__TempID hovered_temp_id;
    Layout_PersistentID hovered_persistent_id;
    Arena tmp;
} Layout__State;

// Functions

static inline Layout__Node *layout__node_from_temp_id(Layout__TempID id);
static inline Layout__TempID layout__temp_id_from_child_index(i32 index);
static inline Layout__Node *layout__node_from_index(i32 index);
static inline Layout__TempID layout__node_push(Layout__Node node);
static inline void layout__hover_test(void);
static inline void layout__node_layout(Layout__Node *node);
static inline void layout__node_intrinsic_size(Layout__Node *node, Layout__Dimension dim);
static inline void layout__node_intrinsic_width(Layout__Node *node);
static inline void layout__container_intrinsic_size(Layout__Node *node, Layout__Dimension dim);
static inline void layout__node_fill_width(Layout__Node *node);
static inline void layout__node_fill_size(Layout__Node *node, Layout__Dimension dim);
static inline void layout__container_fill_size(Layout__Node *node, Layout__Dimension dim);
static inline void layout__node_wrap_text(Layout__Node *node);
static inline void layout__node_intrinsic_height(Layout__Node *node);
static inline void layout__node_fill_height(Layout__Node *node);
static inline void layout__node_positions(Layout__Node *node);
static inline void layout__node_commands(Layout__Node *node);

/*
    fn                  | container | *generic  | text
    ---------------------------------------------------
    intrinsic_width     | yes       | yes       | yes
    fill_width          | yes       | yes       | no
    wrap_text           | yes       | no        | yes
    intrinsic_height    | yes       | yes       | no
    fill_height         | yes       | yes       | no
    positions           | yes       | no        | yes
    commands            | yes       | no        | yes
*/

static inline void layout__container_intrinsic_width(Layout__Node *node);
static inline void layout__container_fill_width(Layout__Node *node);
static inline void layout__container_wrap_text(Layout__Node *node);
static inline void layout__container_intrinsic_height(Layout__Node *node);
static inline void layout__container_fill_height(Layout__Node *node);
static inline void layout__container_positions(Layout__Node *node);
static inline void layout__container_commands(Layout__Node *node);

static inline void layout__text_intrinsic_width(Layout__Node *node);
static inline void layout__text_wrap_text(Layout__Node *node);
static inline void layout__text_commands(Layout__Node *node);
static inline void layout__text_fill_width(Layout__Node *node) { UNUSED(node); }
static inline void layout__text_intrinsic_height(Layout__Node *node) { UNUSED(node); }
static inline void layout__text_fill_height(Layout__Node *node) { UNUSED(node); }
static inline void layout__text_positions(Layout__Node *node) { UNUSED(node); }

static Layout__TempID layout__node_hit_test(Layout__Node *node, Layout_Rect parent_clip, i32 x, i32 y);
static inline b32 layout__rect_contains_point(i32 x, i32 y, Layout_Rect r);
static inline Layout_Rect layout__rect_intersect(Layout_Rect a, Layout_Rect b);
static inline Layout_Rect layout__rect_from_node(Layout__Node *node);
static inline Layout__Dimension layout__dimension_get_other(Layout__Dimension dim);
static inline Layout__Dimension layout__direction_get_main_dimension(Layout_Direction direction);
static inline i32 *layout__node_get_pos(Layout__Node *node, Layout__Dimension dim);
static inline i32 *layout__node_get_size(Layout__Node *node, Layout__Dimension dim);
static inline i32 *layout__node_get_min_size(Layout__Node *node, Layout__Dimension dim);
static inline Layout_SizeStyle layout__get_size_style(Layout_ContainerStyle style, Layout__Dimension dim);
static inline Layout__SizeRange layout__get_size_range(Layout_SizeStyle size);
static inline i32 layout__get_children_spacing(Layout__ChildrenIndices children, i32 spacing);
static inline b32 layout__node_is_fill(Layout__Node *node, Layout__Dimension dim);
static inline i32 layout__node_get_fill_max(Layout__Node *node, Layout__Dimension dim);
static inline i32 layout__node_get_fill_min(Layout__Node *node, Layout__Dimension dim);
static inline void layout__space_distribute(i32 space, List(Layout__NodePtr) nodes, Layout__Dimension dim);
static inline i32 layout__align_cross(Layout_Alignment align, i32 parent_size, i32 parent_padding, i32 child_size);
static inline i32 layout__align_along(Layout_Alignment align, i32 parent_size, i32 parent_padding, i32 children_size);
static inline Layout_Alignment layout__node_get_align_self(Layout__Node *node);
static inline b32 layout__node_is_scroll_y(Layout__Node *node);
static inline Layout__ScrollState *layout__scroll_state_from_id(Layout_PersistentID id);
static inline void layout__append_text_command(
    Layout__Node *node,
    Layout_TextStyle style,
    Layout_TextSlice source,
    isize line_start_byte,
    isize line_end_byte,
    i32 line_y
);

#endif
