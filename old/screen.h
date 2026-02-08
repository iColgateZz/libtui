#ifndef SCREEN_HEADER
#define SCREEN_HEADER

#include "types.h"
#include "window.h"

typedef struct Screen Screen;

Screen* initScreen(void);
void addWindow(Screen *screen, Window *window);
void refreshScreen(Screen *);
void destroyScreen(Screen *screen);
// void resize(void); When resize signal is caught, this hook
// can be called to proportionally scale all elements

#endif