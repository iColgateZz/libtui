#ifndef LIBTUI_TUI_INTERNAL_INCLUDE
#define LIBTUI_TUI_INTERNAL_INCLUDE

#include "tui.h"

#define PSH_CORE_NO_PREFIX
#include "psh_core.h"

typedef struct {
    Tui_ElementConfig config;
    u32 generation;
} InteractionRecord;

hash_map_def(Layla_ElementID, InteractionRecord)
list_def(Layla_ElementID)
list_def(Tui_Event)

typedef struct {
    HashMap(Layla_ElementID, InteractionRecord) interaction_records;
    List(Layla_ElementID) focus_order;
    List(Tui_Event) unhandled_events;
    Tui_Config config;
    Layla_ElementID pressed_id;
    Layla_ElementID clicked_id;
    Layla_ElementID focused_id;
    u32 generation;
    isize registered_count;
} State;

static inline u64 hash_element_id(Layla_ElementID id);
static inline b32 equal_element_ids(Layla_ElementID a, Layla_ElementID b);
static inline Brenda_Color color_from_layla(Layla_Color color);
static inline i32 measure_text(Layla_TextSlice text, void *userdata);
static inline Tui_Binding resolve_binding(Tui_Binding binding, Tui_Binding default_binding);
static inline b32 binding_matches_event(Tui_Binding binding, Brenda_Event event);
static inline InteractionRecord *get_interaction_record_by_id(Layla_ElementID id);
static inline Layla_ElementID get_interaction_target_by_flags(u8 required_flags);
static inline Tui_EventSlice route_events(Brenda_EventSlice events);
static inline void end_interactions(void);
static inline void move_focus(i32 direction);
static inline void draw_commands(Layla_CommandSlice commands);

#endif
