#include <windows.h>
#include <stdio.h>
#include <tlhelp32.h>
#include "Injection.h"

DWORD injectDll(DWORD processId, const char* dllPath)
{
    DWORD injectedDllBase = -1;    
    HANDLE processHandle;

    // Get access to the process
    processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);

    if (processHandle != NULL)
    {
        // Get the address of the LoadLibraryA function in kernel32.dll
        HMODULE kernel32Handle = GetModuleHandle("kernel32.dll");

        if (kernel32Handle != NULL)
        {
            FARPROC loadLibrary = GetProcAddress(kernel32Handle, "LoadLibraryA");

            if (loadLibrary != NULL)
            {
                DWORD bytesWritten;
                void* remotePathAddress;
                unsigned int dllPathLength = strlen(dllPath) + 1;

                // Alocate memory of the dll path size in the remote process 
                // and store the address where dllPath will be written to in remotePathAddress
                remotePathAddress = VirtualAllocEx(processHandle, NULL, dllPathLength, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

                if (remotePathAddress != NULL)
                {
                    // Write the dll path to the allocated memory
                    WriteProcessMemory(processHandle, remotePathAddress, dllPath, dllPathLength, &bytesWritten);

                    if (bytesWritten != 0)
                    {
                        HANDLE remoteThreadHandle;

                        // Create a thread in the remote process that starts at the LoadLibrary function,
                        // and passes in the dllPath string as the argument, making the remote process call LoadLibrary on our dll
                        remoteThreadHandle = CreateRemoteThread(processHandle, NULL, 0, (LPTHREAD_START_ROUTINE)loadLibrary, remotePathAddress, 0, NULL);

                        if (remoteThreadHandle != NULL)
                        {
                            // Wait for LoadLibrary in the remote process to finish then store the thread exit code
                            // which will be the return value of LoadLibrary, the module handle
                            if (WaitForSingleObject(remoteThreadHandle, INFINITE) != WAIT_FAILED)
                            {
                                GetExitCodeThread(remoteThreadHandle, &injectedDllBase);
                            }

                            CloseHandle(remoteThreadHandle);
                        }
                    }

                    // Free the allocated memory
                    VirtualFreeEx(processHandle, remotePathAddress, 0, MEM_RELEASE);
                }
            }
        }

        CloseHandle(processHandle);
    }

    return injectedDllBase;
}

BOOL ejectDll(DWORD processId, DWORD dllBase)
{
    BOOL ejected = FALSE;    
    HANDLE processHandle;

    processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);

    if (processHandle != NULL)
    {
        HMODULE kernel32Handle = GetModuleHandle("kernel32.dll");

        if (kernel32Handle != NULL)
        {
            FARPROC freeLibrary = GetProcAddress(kernel32Handle, "FreeLibrary");

            if (freeLibrary != NULL)
            {
                HANDLE remoteThreadHandle = CreateRemoteThread(processHandle, NULL, 0, (LPTHREAD_START_ROUTINE)freeLibrary, (void*)dllBase, 0, NULL);

                if (remoteThreadHandle != NULL)
                {
                    if (WaitForSingleObject(remoteThreadHandle, INFINITE) != WAIT_FAILED)
                    {
                        GetExitCodeThread(remoteThreadHandle, &ejected);
                    }

                    CloseHandle(remoteThreadHandle);
                }
            }
        }

        CloseHandle(processHandle);
    }

    return ejected;
}

BOOL enableDebugPrivileges()
{
    BOOL success = FALSE;
    HANDLE tokenHandle;
    LUID luid;
    TOKEN_PRIVILEGES newPrivileges;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &tokenHandle))
    {
        if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid))
        {
            newPrivileges.PrivilegeCount = 1;
            newPrivileges.Privileges[0].Luid = luid;
            newPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

            success = AdjustTokenPrivileges(tokenHandle, FALSE, &newPrivileges, sizeof(newPrivileges), NULL, NULL);
        }

        CloseHandle(tokenHandle);
    }

    return success;
}

BOOL getProcessList(PROCESSENTRY32* processList, size_t processListSize, unsigned int* processCount)
{
    HANDLE snapshot;
    PROCESSENTRY32 processEntry;
    BOOL processExists;
    unsigned int count = 0;

    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }

    processEntry.dwSize = sizeof(PROCESSENTRY32);
    processExists = Process32First(snapshot, &processEntry);

    while (processExists && count < processListSize)
    {
        processList[count++] = processEntry;
        processExists = Process32Next(snapshot, &processEntry);        
    }

    if (processCount != NULL)
    {
        *processCount = count;
    }

    CloseHandle(snapshot);
    return TRUE;
}

BOOL getProcessModuleBase(const char* moduleName, DWORD processId, DWORD* base)
{
    HANDLE snapshot;
    MODULEENTRY32 moduleEntry;
    BOOL moduleExists;
    unsigned int count = 0;

    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId);

    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }

    moduleEntry.dwSize = sizeof(MODULEENTRY32);
    moduleExists = Module32First(snapshot, &moduleEntry);

    while (moduleExists)
    {
        if (_stricmp(moduleEntry.szModule, moduleName) == 0)
        {
            if (base != NULL)
            {
                *base = (DWORD)moduleEntry.hModule;
            }

            CloseHandle(snapshot);
            return TRUE;
        }

        moduleExists = Module32Next(snapshot, &moduleEntry);        
    }

    CloseHandle(snapshot);
    return FALSE;
}

BOOL isProcessActive(DWORD processId)
{
    DWORD exitCode;
    GetExitCodeProcess(OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId), &exitCode);

    return exitCode == STILL_ACTIVE;
}

HWND getProcessWindow(DWORD processId)
{
    ProcessWindow processWindow = {processId, NULL};
    EnumWindows(getProcessWindowCallback, (LPARAM)&processWindow);

    return processWindow.window;
}

BOOL CALLBACK getProcessWindowCallback(HWND window, LPARAM processWindow)
{
    if ((GetWindowLong(window, GWL_STYLE) & WS_VISIBLE) != 0)
    {
        DWORD windowProcessId;

        GetWindowThreadProcessId(window, &windowProcessId);

        if (windowProcessId == ((ProcessWindow*)processWindow)->processId)
        {
            ((ProcessWindow*)processWindow)->window = window;
            return FALSE;
        }
    }

    return TRUE;
}