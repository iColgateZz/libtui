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
#define TEMP_ID_NONE (-1)
#define ROOT_TEMP_ID 0

typedef struct {
    TempID parent;
    Layla_ElementID id;
    i32 x, y; // resolved coords
    i32 w, h; // resolved w, h
    i32 min_w, min_h;
    ChildrenIndices children;
    u32 next_child_id_offset;
    
    LAYLA_PACKED_ENUM {
        NODE_CONTAINER,
        NODE_TEXT,
    } type;
    union {
        struct {
            Layla_ContainerStyle style;
            Layla_Floating floating;
            void *custom;
        } container;
        struct {
            Layla_TextSlice text;
            Layla_TextStyle style;
            void *userdata;
        } text;
    } as;
} Node;

list_def(Layla_Command)
list_def(Layla_Error)
list_def(Layla_ElementID)
list_def(Node)
list_def(TempID)
typedef Node* NodePtr;
list_def(NodePtr)

typedef struct {
    i32 y;
    i32 max_y;
} ScrollState;

typedef struct {
    Layla_ElementData data;
    u32 generation;
} ElementRecord;

hash_map_def(Layla_ElementID, ElementRecord)
hash_map_def(Layla_ElementID, ScrollState)

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
    List(TempID) floating_roots;
    List(Layla_Command) commands;
    List(Layla_Error) errors;
    List(Layla_ElementID) hovered_element_ids;
    HashMap(Layla_ElementID, ElementRecord) element_records;
    HashMap(Layla_ElementID, ScrollState) scroll_states;
    u32 completed_generation;
    i32 width, height;
    Layla_CursorState cursor;
    Layla_TextMeasureFunction text_measure_function;
    void *text_measure_userdata;
    Layla_ErrorHandler error_handler;
    void *error_handler_userdata;
    Arena tmp;
} State;

// Functions

static inline void emit_error(Layla_ErrorType type, Layla_ElementID id, byte const *message);
static inline u64 hash_element_id(Layla_ElementID id);
static inline b32 equal_element_ids(Layla_ElementID a, Layla_ElementID b);
static inline Node *get_node_by_temp_id(TempID id);
static inline Node *get_node_by_element_id(Layla_ElementID id);
static inline TempID get_temp_id_by_child_index(i32 index);
static inline Node *get_node_by_index(i32 index);
static inline TempID push_node(Node node);
static inline void open_node(Node node);
static inline void test_hover(void);
static inline void floating_layout(Node *node);
static inline void sort_floating_roots(void);
static inline void container_intrinsic_size(Node *node, Dimension dim);
static inline void container_fill_size(Node *node, Dimension dim);

static inline void container_intrinsic_width(Node *node);
static inline void container_fill_width(Node *node);
static inline void container_wrap_text(Node *node);
static inline void container_intrinsic_height(Node *node);
static inline void container_fill_height(Node *node);
static inline void container_positions(Node *node);
static inline void container_commands(Node *node, Layla_Rectangle active_clip);

static inline void text_intrinsic_width(Node *node);
static inline void text_wrap_text(Node *node);
static inline TextMeasurement text_process(Node *node, i32 wrap_width, b32 emit_commands, Layla_Rectangle active_clip);
static inline i32 measure_text_slice(Layla_ElementID id, Layla_TextSlice text);

static inline b32 node_hit_test(Node *node, Layla_Rectangle parent_clip, i32 x, i32 y);
static inline b32 rectangle_contains_point(i32 x, i32 y, Layla_Rectangle r);
static inline Layla_Rectangle intersect_rectangles(Layla_Rectangle a, Layla_Rectangle b);
static inline Layla_Rectangle node_get_rectangle(Node *node);
static inline Dimension get_other_dimension(Dimension dim);
static inline Dimension get_main_dimension_from_direction(Layla_Direction direction);
static inline i32 *node_get_pos(Node *node, Dimension dim);
static inline i32 *node_get_size(Node *node, Dimension dim);
static inline i32 *node_get_min_size(Node *node, Dimension dim);
static inline Layla_SizeStyle get_size_style(Layla_ContainerStyle style, Dimension dim);
static inline SizeRange get_size_range(Layla_SizeStyle size);
static inline PaddingSides get_padding_sides_from_container_style(Layla_ContainerStyle style, Dimension dim);
static inline i32 get_children_spacing(ChildrenIndices children, i32 spacing);
static inline b32 node_is_fill(Node *node, Dimension dim);
static inline b32 node_is_percentage(Node *node, Dimension dim);
static inline i32 calculate_percentage_size(Layla_SizeStyle size, i32 available_size);
static inline i32 node_get_fill_max(Node *node, Dimension dim);
static inline i32 node_get_fill_min(Node *node, Dimension dim);
static inline void distribute_space(i32 space, List(NodePtr) nodes, Dimension dim);
static inline i32 calculate_alignment_offset(Layla_Alignment align, i32 parent_size, PaddingSides padding, i32 child_size);
static inline i32 resolve_alignment_position(Layla_Alignment alignment, i32 size);
static inline Layla_Alignment node_get_align_self(Node *node);
static inline b32 node_is_scroll_y(Node *node);
static inline b32 node_is_floating(Node *node);
static inline ScrollState *get_scroll_state_by_id(Layla_ElementID id);
static inline void append_text_command(Node *node, isize line_start_byte, isize line_end_byte, i32 line_x, i32 line_y, i32 line_width, Layla_Rectangle active_clip);
static inline void floating_measure_size(Node *node, Node *attached, Dimension dim);

#endif
