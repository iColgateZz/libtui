#include "arena.h"

Arena arena_init( isize alloc_size )
{
    void *buffer = malloc( alloc_size );

    Arena arena = {
        .buffer = buffer,
        .cur = buffer,
        .end = buffer + alloc_size
    };

    return arena;
}

void arena_free( Arena *arena )
{ free( arena->buffer ); }

void arena_clear( Arena *arena )
{
    memset( arena->buffer, 0, (isize)(arena->end - arena->buffer) );
    arena->cur = arena->buffer;
}

void *arena_alloc_( Arena *arena, isize size, isize align, isize n )
{
    isize padding = -(uptr)arena->cur & (align - 1);

    isize available = arena->end - arena->cur - padding;
    if (available < 0 || n > available / size) {
        fprintf(stderr, "arena_alloc: no mem\n");
        return NULL;
    }

    void *p = arena->cur + padding;
    arena->cur += padding + n * size;

    return p;
}

b32 arena_mv_ptr_fw_( Arena *arena, isize size, isize n )
{
    isize available = arena->end - arena->cur;
    if (available < 0 || n > available / size)
    {
        fprintf(stderr, "arena_mv_ptr_fw: no mem\n");
        return 0;
    }

    arena->cur += n * size;
    return 1;
}

void *arena_next_p( Arena *arena, isize align )
{
    isize padding = -(uptr)arena->cur & (align - 1);
    return arena->cur + padding;
}

Scratch scratch_init( Arena *arena )
{
    Scratch scratch = {
        .arena = arena,
        .cur = arena->cur
    };

    return scratch;
}

void *scratch_alloc_( Scratch *scratch, isize size, isize align, isize n)
{ return arena_alloc_( scratch->arena, size, align, n); }

b32 scratch_mv_ptr_fw_( Scratch *scratch, isize size, isize n)
{ return arena_mv_ptr_fw_( scratch->arena, size, n); }

void scratch_free( Scratch *scratch )
{ scratch->arena->cur = scratch->cur; }