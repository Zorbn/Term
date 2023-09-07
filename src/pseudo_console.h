#ifndef PSEUDO_CONSOLE_H
#define PSEUDO_CONSOLE_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct {
    HRESULT result;
    HPCON hpc;
    HANDLE h_process;
    HANDLE output, input;
} PseudoConsole;

PseudoConsole SetUpPseudoConsole(COORD size);

#endif