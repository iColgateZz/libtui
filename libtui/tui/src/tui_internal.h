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

static inline u64 element_id_hash(Layla_ElementID id);
static inline b32 element_id_equal(Layla_ElementID a, Layla_ElementID b);
static inline Brenda_Color color_from_layla(Layla_Color color);
static inline i32 text_measure(Layla_TextSlice text, void *userdata);
static inline Tui_Binding binding_resolve(Tui_Binding binding, Tui_Binding default_binding);
static inline b32 binding_matches_event(Tui_Binding binding, Brenda_Event event);
static inline InteractionRecord *interaction_record_get(Layla_ElementID id);
static inline Layla_ElementID interaction_target_get(u8 required_flags);
static inline Tui_EventSlice events_route(Brenda_EventSlice events);
static inline void interactions_end(void);
static inline void focus_move(i32 direction);
static inline void commands_draw(Layla_CommandSlice commands);

#endif
