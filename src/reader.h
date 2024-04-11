#ifndef READER_H
#define READER_H

#include "graphics/renderer.h"
#include "pseudo_console.h"
#include "window.h"
#include "grid.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct ReadThreadData {
    struct Grid *grid;
    struct Renderer *renderer;
    struct Window *window;
    HANDLE mutex;
    HANDLE event;
};

struct Reader {
    HANDLE read_thread;
};

struct Reader reader_create(struct ReadThreadData *read_thread_data);
void reader_destroy(struct Reader *reader, struct ReadThreadData *read_thread_data);

#endif