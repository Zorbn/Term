#include "pseudo_console.h"

HRESULT PrepareStartupInformation(HPCON hpc, STARTUPINFOEX *psi) {
    // Prepare Startup Information structure
    STARTUPINFOEX si;
    ZeroMemory(&si, sizeof(si));
    si.StartupInfo.cb = sizeof(STARTUPINFOEX);

    // Discover the size required for the list
    size_t bytesRequired;
    InitializeProcThreadAttributeList(NULL, 1, 0, &bytesRequired);

    // Allocate memory to represent the list
    si.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, bytesRequired);
    if (!si.lpAttributeList) {
        return E_OUTOFMEMORY;
    }

    // Initialize the list memory location
    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytesRequired)) {
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Set the pseudoconsole information into the list
    if (!UpdateProcThreadAttribute(
            si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hpc, sizeof(hpc), NULL, NULL)) {
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        return HRESULT_FROM_WIN32(GetLastError());
    }

    *psi = si;

    return S_OK;
}

PseudoConsole SetUpPseudoConsole(COORD size) {
    HRESULT hr = S_OK;

    // - Close these after CreateProcess of child application with
    // pseudoconsole object.
    HANDLE inputReadSide, outputWriteSide;

    // - Hold onto these and use them for communication with the child
    // through the pseudoconsole.
    HANDLE outputReadSide, inputWriteSide;

    if (!CreatePipe(&inputReadSide, &inputWriteSide, NULL, 0)) {
        return (PseudoConsole){
            HRESULT_FROM_WIN32(GetLastError()),
            NULL,
            NULL,
            NULL,
            NULL,
        };
    }

    if (!CreatePipe(&outputReadSide, &outputWriteSide, NULL, 0)) {
        return (PseudoConsole){
            HRESULT_FROM_WIN32(GetLastError()),
            NULL,
            NULL,
            NULL,
            NULL,
        };
    }

    HPCON hpc;
    hr = CreatePseudoConsole(size, inputReadSide, outputWriteSide, 0, &hpc);
    if (FAILED(hr)) {
        return (PseudoConsole){
            hr,
            NULL,
            NULL,
            NULL,
            NULL,
        };
    }

    STARTUPINFOEX siEx;
    PrepareStartupInformation(hpc, &siEx);

    PCWSTR childApplication = L"C:\\Program Files\\PowerShell\\7\\pwsh.exe";

    // Create mutable text string for CreateProcessW command line string.
    const size_t charsRequired = wcslen(childApplication) + 1; // +1 null terminator
    PWSTR cmdLineMutable = (PWSTR)HeapAlloc(GetProcessHeap(), 0, sizeof(wchar_t) * charsRequired);

    if (!cmdLineMutable) {
        return (PseudoConsole){
            E_OUTOFMEMORY,
            NULL,
            NULL,
            NULL,
            NULL,
        };
    }

    wcscpy_s(cmdLineMutable, charsRequired, childApplication);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    // Call CreateProcess
    if (!CreateProcessW(NULL, cmdLineMutable, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL,
            (LPSTARTUPINFOW)(&siEx.StartupInfo), &pi)) {
        HeapFree(GetProcessHeap(), 0, cmdLineMutable);
        return (PseudoConsole){
            HRESULT_FROM_WIN32(GetLastError()),
            NULL,
            NULL,
            NULL,
            NULL,
        };
    }

    return (PseudoConsole){
        hr,
        hpc,
        pi.hProcess,
        outputReadSide,
        inputWriteSide,
    };
}