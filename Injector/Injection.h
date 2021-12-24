#pragma once

#include <windows.h>
#include <tlhelp32.h>

#define MAX_MODULES 1024

typedef struct ProcessWindow_t
{
    DWORD processId;
    HWND window;
} ProcessWindow;

void* injectDll(DWORD ProcessId, const char* dllPath);
int ejectDll(DWORD processId, void* dllBase);
BOOL enableDebugPrivileges();
BOOL getProcessList(PROCESSENTRY32* processList, size_t processListSize, unsigned int* processCount);
BOOL getProcessModuleBase(const char* moduleName, DWORD processId, void** base);
BOOL isProcessActive(DWORD processId);
HWND getProcessWindow(DWORD processId);
BOOL CALLBACK getProcessWindowCallback(HWND window, LPARAM processWindow);