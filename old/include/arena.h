#ifndef ARENA_INCLUDE
#define ARENA_INCLUDE

#include <stdio.h>
#include <stdlib.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdarg.h>
#include <sys/cdefs.h>
#include <string.h>

#include "types.h"

#define arena_alloc(...)                allocx_(__VA_ARGS__, alloc2_, alloc1_)(__VA_ARGS__)
#define allocx_(a, b, c, d, ...)        d
#define alloc1_(a_ptr, t)               arena_alloc_(a_ptr, sizeof(t), alignof(t), 1)
#define alloc2_(a_ptr, t, n)            arena_alloc_(a_ptr, sizeof(t), alignof(t), n)

#define arena_next(a, type)             arena_next_p(a, alignof(type))

#define arena_mv_ptr_fw(...)            mv_ptr_fw_x(__VA_ARGS__, mv_ptr_fw2, mv_ptr_fw1)(__VA_ARGS__)
#define mv_ptr_fw_x(a, b, c, d, ...)    d
#define mv_ptr_fw1(a_ptr, t)            arena_mv_ptr_fw_(a_ptr, sizeof(t), 1)
#define mv_ptr_fw2(a_ptr, t, n)         arena_mv_ptr_fw_(a_ptr, sizeof(t), n)

#define scratch_alloc(...)              sallocx_(__VA_ARGS__, salloc2_, salloc1_)(__VA_ARGS__)
#define sallocx_(a, b, c, d, ...)       d
#define salloc1_(a_ptr, t)              scratch_alloc_(a_ptr, sizeof(t), alignof(t), 1)
#define salloc2_(a_ptr, t, n)           scratch_alloc_(a_ptr, sizeof(t), alignof(t), n)

#define scratch_mv_ptr_fw(...)          s_mv_ptr_fw_x(__VA_ARGS__, mv_ptr_fw2, mv_ptr_fw1)(__VA_ARGS__)
#define s_mv_ptr_fw_x(a, b, c, d, ...)  d
#define s_mv_ptr_fw1(s_ptr, t)          scratch_mv_ptr_fw_(s_ptr, sizeof(t), 1)
#define s_mv_ptr_fw2(s_ptr, t, n)       scratch_mv_ptr_fw_(s_ptr, sizeof(t), n)

typedef struct {
    void *cur, *end;
    void *buffer;
} Arena;

typedef struct {
    Arena *arena;
    void *cur;
} Scratch;

Arena arena_init( isize alloc_size );
void arena_free( Arena *arena );
void arena_clear( Arena *arena );
void *arena_alloc_( Arena *arena, isize size, isize align, isize n );
b32 arena_mv_ptr_fw_( Arena *arena, isize size, isize n );
void *arena_next_p( Arena *arena, isize align );
Scratch scratch_init( Arena *arena );
void *scratch_alloc_( Scratch *scratch, isize size, isize align, isize n);
b32 scratch_mv_ptr_fw_( Scratch *scratch, isize size, isize n);
void scratch_free( Scratch *scratch );

#endif