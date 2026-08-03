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

typedef struct {
    HashMap(Layla_ElementID, InteractionRecord) interaction_records;
    List(Layla_ElementID) focus_order;
    Tui_Config config;
    Brenda_EventSlice events;
    Layla_ElementID pressed_id;
    Layla_ElementID clicked_id;
    Layla_ElementID focused_id;
    u32 generation;
    isize registered_count;
} State;

static inline u64 element_id_hash(Layla_ElementID id);
static inline b32 element_id_equal(Layla_ElementID a, Layla_ElementID b);
static inline i32 text_measure(Layla_TextSlice text, void *userdata);
static inline InteractionRecord *interaction_record_get(Layla_ElementID id);
static inline Layla_ElementID interaction_target_get(u8 required_flags);
static inline void interactions_begin(Brenda_EventSlice events);
static inline void interactions_end(void);
static inline void focus_move(i32 direction);
static inline b32 event_is_activation(Brenda_Event event);
static inline void commands_draw(Layla_CommandSlice commands);
static inline void text_input_events_handle(Tui_TextInputState *input, Tui_TextInputResult *result);
static inline isize utf8_previous_byte(Tui_TextInputState *input);
static inline isize utf8_next_byte(Tui_TextInputState *input);

#endif
