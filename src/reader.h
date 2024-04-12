#ifndef READER_H
#define READER_H

#include "graphics/renderer.h"
#include "pseudo_console.h"
#include "grid.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct ReadThreadData {
    struct Grid *grid;
    struct PseudoConsole *pseudo_console;
    struct Renderer *renderer;

    struct TextBuffer text_buffer;
    struct TitleBuffer title_buffer;

    HANDLE mutex;
    HANDLE event;
};

struct Reader {
    HANDLE read_thread;
};

struct ReadThreadData read_thread_data_create(struct Grid *grid, struct PseudoConsole *pseudo_console, struct Renderer *renderer);
void read_thread_data_destroy(struct ReadThreadData *read_thread_data);
void read_thread_data_lock(struct ReadThreadData *read_thread_data);
void read_thread_data_unlock(struct ReadThreadData *read_thread_data);

struct Reader reader_create(struct ReadThreadData *read_thread_data);
void reader_destroy(struct Reader *reader);

#endif