#include "pseudo_console.h"

#include <stdatomic.h>
#include <assert.h>

static HRESULT init_startup_information(HPCON hpc, STARTUPINFOEX *psi) {
    STARTUPINFOEX si;
    ZeroMemory(&si, sizeof(si));
    si.StartupInfo.cb = sizeof(STARTUPINFOEX);

    size_t bytes_required;
    InitializeProcThreadAttributeList(NULL, 1, 0, &bytes_required);

    si.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, bytes_required);
    if (!si.lpAttributeList) {
        return E_OUTOFMEMORY;
    }

    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytes_required)) {
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        return HRESULT_FROM_WIN32(GetLastError());
    }

    if (!UpdateProcThreadAttribute(
            si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hpc, sizeof(hpc), NULL, NULL)) {
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        return HRESULT_FROM_WIN32(GetLastError());
    }

    *psi = si;

    return S_OK;
}

struct PseudoConsole pseudo_console_create(COORD size) {
    HRESULT hr = S_OK;

    // Closed after creating the child process.
    HANDLE input_read, output_write;
    // Used to communicate with the child process.
    HANDLE output_read, input_write;

    if (!CreatePipe(&input_read, &input_write, NULL, 0)) {
        return (struct PseudoConsole){
            HRESULT_FROM_WIN32(GetLastError()),
            NULL,
            NULL,
            NULL,
            NULL,
        };
    }

    if (!CreatePipe(&output_read, &output_write, NULL, 0)) {
        return (struct PseudoConsole){
            HRESULT_FROM_WIN32(GetLastError()),
            NULL,
            NULL,
            NULL,
            NULL,
        };
    }

    HPCON hpc;
    hr = CreatePseudoConsole(size, input_read, output_write, 0, &hpc);
    if (FAILED(hr)) {
        return (struct PseudoConsole){
            hr,
            NULL,
            NULL,
            NULL,
            NULL,
        };
    }

    STARTUPINFOEX si_ex;
    init_startup_information(hpc, &si_ex);

    PCWSTR child_application = L"C:\\Program Files\\PowerShell\\7\\pwsh.exe";

    const size_t child_application_length = wcslen(child_application) + 1;
    PWSTR command = (PWSTR)HeapAlloc(GetProcessHeap(), 0, sizeof(wchar_t) * child_application_length);

    if (!command) {
        return (struct PseudoConsole){
            E_OUTOFMEMORY,
            NULL,
            NULL,
            NULL,
            NULL,
        };
    }

    wcscpy_s(command, child_application_length, child_application);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessW(NULL, command, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL,
            (LPSTARTUPINFOW)(&si_ex.StartupInfo), &pi)) {
        HeapFree(GetProcessHeap(), 0, command);
        return (struct PseudoConsole){
            HRESULT_FROM_WIN32(GetLastError()),
            NULL,
            NULL,
            NULL,
            NULL,
        };
    }

    return (struct PseudoConsole){
        hr,
        hpc,
        pi.hProcess,
        output_read,
        input_write,
    };
}

void pseudo_console_resize(struct PseudoConsole *pseudo_console, size_t width, size_t height) {
    ResizePseudoConsole(pseudo_console->hpc, (COORD){width, height});
}

struct PipeDrainInfo {
    struct PseudoConsole *pseudo_console;
    struct TextBuffer *text_buffer;
    _Atomic(bool) is_pseudo_console_closed;
};

static DWORD WINAPI pipe_drain_thread_start(void *start_info) {
    struct PipeDrainInfo *info = start_info;

    DWORD bytes_available;

    while (true) {
        if (PeekNamedPipe(info->pseudo_console->output, NULL, 0, NULL, &bytes_available, NULL) && bytes_available > 0) {
            ReadFile(info->pseudo_console->output, info->text_buffer->data, TEXT_BUFFER_CAPACITY, &info->text_buffer->length, NULL);
        }

        if (bytes_available <= 0 && info->is_pseudo_console_closed) {
            break;
        }
    }

    return 0;
}

void pseudo_console_destroy(struct PseudoConsole *pseudo_console, struct TextBuffer *text_buffer) {
    struct PipeDrainInfo pipe_drain_info = (struct PipeDrainInfo){
        .pseudo_console = pseudo_console,
        .text_buffer = text_buffer,
    };
    HANDLE pipe_drain_thread = CreateThread(NULL, 0, pipe_drain_thread_start, &pipe_drain_info, 0, NULL);
    assert(pipe_drain_thread);

    // Closing the pseudo console may send console output, and blocks until the output pipe is empty.
    // So before doing this a thread is created to drain that output, and then the thread ends once the
    // output pipe is empty and the console has been closed.
    ClosePseudoConsole(pseudo_console->hpc);
    pipe_drain_info.is_pseudo_console_closed = true;

    WaitForSingleObject(pipe_drain_thread, INFINITE);
    CloseHandle(pipe_drain_thread);
}