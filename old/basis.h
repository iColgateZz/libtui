#ifndef BASIS_HEADER
#define BASIS_HEADER

#include "types.h"
#include "s8.h"

void clear(void);
void flush(void);
void getTermDimensions(i32 *x, i32 *y);

void mvCursor(i32 x, i32 y);
void mvCursorUp(i32 num_lines);
void mvCursorDown(i32 num_lines);
void mvCursorRight(i32 num_cols);
void mvCursorLeft(i32 num_cols);
void mvCursorStartOfLine(void);

void drawCharXY(byte ch, i32 x, i32 y);
void drawChar(byte ch);
void drawS8XY(s8 s, i32 x, i32 y);
void drawS8(s8 s);

#endif