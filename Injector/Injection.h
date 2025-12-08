#pragma once

#include <windows.h>
#include <tlhelp32.h>

#define MAX_MODULES 1024
#define INJECT_ERR_OPEN_PROCESS -1
#define INJECT_ERR_GET_KERNEL32_DLL -2
#define INJECT_ERR_GET_LOAD_LIBRARY_ADDRESS -3
#define INJECT_ERR_VIRTUAL_ALLOC -4
#define INJECT_ERR_WRITE_PROCESS_MEMORY -5
#define INJECT_ERR_CREATE_REMOTE_THREAD -6

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