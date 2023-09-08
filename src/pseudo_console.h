#ifndef PSEUDO_CONSOLE_H
#define PSEUDO_CONSOLE_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct PseudoConsole {
    HRESULT result;
    HPCON hpc;
    HANDLE h_process;
    HANDLE output, input;
};

struct PseudoConsole pseudo_console_create(COORD size);
void pseudo_console_resize(struct PseudoConsole *pseudo_console, size_t width, size_t height);
void pseudo_console_destroy(struct PseudoConsole *pseudo_console);

#endif