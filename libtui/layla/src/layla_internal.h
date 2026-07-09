#ifndef LIBTUI_LAYLA_INTERNAL_INCLUDE
#define LIBTUI_LAYLA_INTERNAL_INCLUDE

#include "layla.h"

#define PSH_CORE_NO_PREFIX
#include "psh_core.h"

typedef struct {
    i32 offset;
    i32 count;
} ChildrenIndices;

typedef i32 TempID;
#define LAYLA_TEMP_ID_NONE (-1)
#define LAYLA_ROOT_TEMP_ID 0

typedef struct {
    TempID parent;
    Layla_PersistentID id;
    i32 x, y; // resolved coords
    i32 w, h; // resolved w, h
    i32 min_w, min_h;
    ChildrenIndices children;
    
    LAYLA_PACKED_ENUM {
        LAYLA_NODE_CONTAINER,
        LAYLA_NODE_TEXT,
    } type;
    union {
        struct {
            Layla_ContainerConfig config;
        } container;
        struct {
            Layla_TextConfig config;
        } text;
    } as;
} Node;

list_def(Layla_Command)
list_def(Node)
list_def(TempID)
typedef Node* NodePtr;
list_def(NodePtr)

typedef struct {
    Layla_PersistentID id;
    i32 y;
    i32 max_y;
} ScrollState;

list_def(ScrollState)

typedef enum {
    DIM_X,
    DIM_Y,
} Dimension;

typedef struct {
    i32 min;
    i32 max;
} SizeRange;

typedef struct {
    i32 start;
    i32 end;
} PaddingSides;

typedef struct {
    i32 natural_width;
    i32 minimum_width;
    i32 line_count;
} TextMeasurement;

typedef struct {
    List(Node) nodes;
    List(TempID) open_node_stack;
    List(TempID) temporary_child_stack;
    List(TempID) frame_children;
    List(Layla_Command) commands;
    List(ScrollState) scroll_states;
    i32 width, height;
    i32 cursor_x, cursor_y;
    TempID hovered_temp_id;
    Layla_PersistentID hovered_persistent_id;
    Layla_TextMeasureFunction text_measure_function;
    void *text_measure_userdata;
    Arena tmp;
} State;

// Functions

static inline Node *node_from_temp_id(TempID id);
static inline TempID temp_id_from_child_index(i32 index);
static inline Node *node_from_index(i32 index);
static inline TempID node_push(Node node);
static inline void hover_test(void);
//TODO: maybe add a layout pass that can check for common errors
//      it may be configured by some macro and not appear in production code
//      that would also need an error reporting mechanism
static inline void node_layout(Node *node);
static inline void container_intrinsic_size(Node *node, Dimension dim);
static inline void container_fill_size(Node *node, Dimension dim);

static inline void container_intrinsic_width(Node *node);
static inline void container_fill_width(Node *node);
static inline void container_wrap_text(Node *node);
static inline void container_intrinsic_height(Node *node);
static inline void container_fill_height(Node *node);
static inline void container_positions(Node *node);
static inline void container_commands(Node *node);

static inline void text_intrinsic_width(Node *node);
static inline void text_wrap_text(Node *node);
static inline TextMeasurement text_process(Node *node, i32 wrap_width, b32 emit_commands);
static inline i32 text_slice_measure(Layla_TextSlice text);

static inline TempID node_hit_test(Node *node, Layla_Rectangle parent_clip, i32 x, i32 y);
static inline b32 rect_contains_point(i32 x, i32 y, Layla_Rectangle r);
static inline Layla_Rectangle rect_intersect(Layla_Rectangle a, Layla_Rectangle b);
static inline Layla_Rectangle rect_from_node(Node *node);
static inline Dimension dimension_get_other(Dimension dim);
static inline Dimension direction_get_main_dimension(Layla_Direction direction);
static inline i32 *node_get_pos(Node *node, Dimension dim);
static inline i32 *node_get_size(Node *node, Dimension dim);
static inline i32 *node_get_min_size(Node *node, Dimension dim);
static inline Layla_SizeStyle get_size_style(Layla_ContainerStyle style, Dimension dim);
static inline SizeRange get_size_range(Layla_SizeStyle size);
static inline PaddingSides padding_sides_from_dimension(Layla_Padding padding, Dimension dim);
static inline i32 get_children_spacing(ChildrenIndices children, i32 spacing);
static inline b32 node_is_fill(Node *node, Dimension dim);
static inline i32 node_get_fill_max(Node *node, Dimension dim);
static inline i32 node_get_fill_min(Node *node, Dimension dim);
static inline void space_distribute(i32 space, List(NodePtr) nodes, Dimension dim);
static inline i32 align_offset(Layla_Alignment align, i32 parent_size, PaddingSides padding, i32 child_size);
static inline Layla_Alignment node_get_align_self(Node *node);
static inline b32 node_is_scroll_y(Node *node);
static inline ScrollState *scroll_state_get_by_id(Layla_PersistentID id);
static inline void append_text_command(
    Layla_TextConfig config,
    isize line_start_byte,
    isize line_end_byte,
    i32 line_x,
    i32 line_y
);

#endif
