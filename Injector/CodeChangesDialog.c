#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <commctrl.h>
#include "CodeChangesDialog.h"
#include "InjectorDialog.h"
#include "Interface.h"
#include "Injection.h"
#include "Process.h"
#include "Clipboard.h"
#include "resource.h"

#define CHANGED_BYTES_MAX_SIZE 3072

typedef struct ModuleCode_t
{
    char moduleName[MAX_PATH];
    DWORD start;
    DWORD size;
    BYTE* code;
    struct ModuleCode_t* next;
} ModuleCode;

static const ListViewColumns MODULE_LIST_COLUMNS[] =
{
    {"Module", 30},
    {"Base", 15},
    {"Path", 55}
};

static const ListViewColumns CODE_CHANGE_LIST_COLUMNS[] =
{
    {"Module", 30},
    {"Address", 15},
    {"Size", 15},
    {"Bytes", 40}
};

static Process* g_targetProcess;
static ModuleCode* g_moduleCodeHead = NULL;

static ModuleCode* insertModuleCodeNode()
{
    ModuleCode* newNode = (ModuleCode*)malloc(sizeof(ModuleCode));

    if (newNode == NULL)
    {
        return NULL;
    }

    newNode->next = g_moduleCodeHead;
    g_moduleCodeHead = newNode;

    return g_moduleCodeHead;
}

static void deleteModuleCodeList()
{
    ModuleCode* node = g_moduleCodeHead;

    while (node != NULL)
    {
        ModuleCode* next = node->next;

        free(node->code);
        free(node);

        node = next;
    }

    g_moduleCodeHead = NULL;
}

static void formatChangedBytes(char* destination, size_t destinationSize, BYTE* changedCode, DWORD changeSize)
{
    const unsigned int TEXT_BYTE_SIZE = 3;
    DWORD i = 0;
    char* destinationEnd = destination + destinationSize - 1;
    BYTE* changedCodeEnd = changedCode + changeSize - 1;

    while (destination + TEXT_BYTE_SIZE - 1 <= destinationEnd && changedCode <= changedCodeEnd)
    {
        char byte[4];

        sprintf_s(byte, sizeof(byte), "%02X ", *changedCode);
        memcpy_s(destination, destinationSize, byte, sizeof(byte) - 1);

        changedCode++;
        destination += TEXT_BYTE_SIZE;
    }

    // If at least one 3 character byte written add the null terminator over the trailing space
    if (destinationSize >= TEXT_BYTE_SIZE)
    {
        *(destination - 1) = '\0';
    }
}

static DWORD insertCodeChange(HWND listView, unsigned int index, const char* moduleName, DWORD changeAddress, DWORD changeSize, BYTE* changedCode)
{
    LVITEM item;
    char moduleNameNew[MAX_PATH];
    char changeAddressText[9];
    char changeSizeText[9];
    char changedBytes[CHANGED_BYTES_MAX_SIZE];

    // Insert item
    memset(&item, 0, sizeof(item));

    item.mask = LVIF_TEXT;
    item.state = 0;
    item.stateMask = 0;
    item.iItem = index;

    item.iSubItem = 0;
    strcpy_s(moduleNameNew, sizeof(moduleNameNew), moduleName);
    item.pszText = moduleNameNew;
    item.cchTextMax = sizeof(moduleNameNew);

    if (ListView_InsertItem(listView, &item) == -1)
    {
        return FALSE;
    }
        
    // Insert sub items
    item.iSubItem = 1;
    sprintf_s(changeAddressText, sizeof(changeAddressText), "%08X", changeAddress);
    item.pszText = changeAddressText;
    item.cchTextMax = sizeof(changeAddressText);
        
    if (ListView_SetItem(listView, &item) == -1)
    {
        return FALSE;
    }

    item.iSubItem = 2;
    sprintf_s(changeSizeText, sizeof(changeSizeText), "%08X", changeSize);
    item.pszText = changeSizeText;
    item.cchTextMax = sizeof(changeSizeText);
        
    if (ListView_SetItem(listView, &item) == -1)
    {
        return FALSE;
    }

    item.iSubItem = 3;
    formatChangedBytes(changedBytes, sizeof(changedBytes), changedCode, changeSize);
    item.pszText = changedBytes;
    item.cchTextMax = sizeof(changedBytes);
        
    if (ListView_SetItem(listView, &item) == -1)
    {
        return FALSE;
    }

    return TRUE;
}

static DWORD findCodeChanges(HWND window, DWORD processId)
{
    HANDLE processHandle = processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    ModuleCode* moduleCode = g_moduleCodeHead;
    DWORD changesCount = 0;

    if (processHandle == NULL)
    {
        MessageBox(window, "Failed to open process", "Error", MB_OK | MB_ICONWARNING);
        return FALSE;
    }

    while (moduleCode != NULL)
    {
        BYTE* currentCode;
        DWORD i;

        currentCode = (BYTE*)malloc(moduleCode->size);
        ReadProcessMemory(processHandle, (void*)moduleCode->start, currentCode, moduleCode->size, NULL);

        for (i = 0; i < moduleCode->size; i++)
        {
            if (moduleCode->code[i] != currentCode[i])
            {    
                DWORD changedSize = 1;

                while (moduleCode->code[i + changedSize] != currentCode[i + changedSize])
                {
                    changedSize++;
                }

                insertCodeChange(GetDlgItem(window, IDC_CODE_CHANGE_LIST), changesCount, moduleCode->moduleName,  moduleCode->start + i, changedSize, currentCode + i);
                changesCount++;
                i += changedSize;
            }
        }

        free(currentCode);
        moduleCode = moduleCode->next;
    }

    return changesCount;
}

static BOOL populateModuleList(HWND listView, DWORD processId)
{
    MODULEENTRY32 moduleEntry;
    BOOL moduleExists;
    unsigned int i = 0;
    HANDLE snapshot;
    
    SendMessage(listView, WM_SETREDRAW, FALSE, 0);

    if (!ListView_DeleteAllItems(listView))
    {
        return FALSE;
    }

    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId);

    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }

    moduleEntry.dwSize = sizeof(MODULEENTRY32);
    moduleExists = Module32First(snapshot, &moduleEntry);

    while (moduleExists)
    {
        LVITEM item;
        char moduleName[MAX_PATH];
        char moduleBase[9];
        char modulePath[MAX_PATH];

        // Insert item
        memset(&item, 0, sizeof(item));

        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.state = 0;
        item.stateMask = 0;
        item.iItem = i;
        item.lParam = (LPARAM)moduleEntry.hModule;

        item.iSubItem = 0;
        strcpy_s(moduleName, sizeof(moduleName), moduleEntry.szModule);
        item.pszText = moduleName;
        item.cchTextMax = sizeof(moduleName);

        if (ListView_InsertItem(listView, &item) == -1)
        {
            return FALSE;
        }
        
        // Insert sub items
        item.mask = LVIF_TEXT;

        item.iSubItem = 1;
        sprintf_s(moduleBase, sizeof(moduleBase), "%08X", (DWORD)moduleEntry.modBaseAddr);
        item.pszText = moduleBase;
        item.cchTextMax = sizeof(moduleBase);
        
        if (ListView_SetItem(listView, &item) == -1)
        {
            return FALSE;
        }

        item.iSubItem = 2;
        strcpy_s(modulePath, sizeof(modulePath), moduleEntry.szExePath);
        item.pszText = modulePath;
        item.cchTextMax = sizeof(modulePath);
        
        if (ListView_SetItem(listView, &item) == -1)
        {
            return FALSE;
        }
            
        i++;
        moduleExists = Module32Next(snapshot, &moduleEntry);        
    }

    CloseHandle(snapshot);

    SendMessage(listView, WM_SETREDRAW, TRUE, 0);

    return TRUE;
}

static BOOL recordModuleCode(HANDLE processHandle, HMODULE moduleHandle, const char* moduleName)
{
    IMAGE_DOS_HEADER dosHeader;
    IMAGE_NT_HEADERS ntHeaders;
    ModuleCode* moduleCode = insertModuleCodeNode();

    ReadProcessMemory(processHandle, (void*)moduleHandle, &dosHeader, sizeof(dosHeader), NULL);
    ReadProcessMemory(processHandle, (void*)((DWORD)moduleHandle + dosHeader.e_lfanew), &ntHeaders, sizeof(ntHeaders), NULL);

    strcpy_s(moduleCode->moduleName, sizeof(moduleCode->moduleName), moduleName);
    moduleCode->start = (DWORD)moduleHandle + ntHeaders.OptionalHeader.BaseOfCode;
    moduleCode->size = ntHeaders.OptionalHeader.SizeOfCode;
    moduleCode->code = (BYTE*)malloc(moduleCode->size);
    ReadProcessMemory(processHandle, (void*)moduleCode->start, moduleCode->code, moduleCode->size, NULL);

    return TRUE;
}
/*
static BOOL recordModuleIat(HANDLE processHandle, HMODULE moduleHandle, const char* moduleName)
{
    IMAGE_DOS_HEADER dosHeader;
    IMAGE_NT_HEADERS ntHeaders;
    ModuleCode* moduleCode = insertModuleCodeNode();

    ReadProcessMemory(processHandle, (void*)moduleHandle, &dosHeader, sizeof(dosHeader), NULL);
    ReadProcessMemory(processHandle, (void*)((DWORD)moduleHandle + dosHeader.e_lfanew), &ntHeaders, sizeof(ntHeaders), NULL);

    DWORD importDirectoryOffset = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;

    for (IMAGE_IMPORT_DESCRIPTOR* importDescriptor = (IMAGE_IMPORT_DESCRIPTOR*)((DWORD)&dosHeader + importDirectoryOffset); importDescriptor->FirstThunk != 0; importDescriptor++)        
    {
        if (_strcmpi(exportingModuleName, (char*)((DWORD)dosHeader + importDescriptor->Name)) == 0)
        {
            for(IMAGE_THUNK_DATA* thunkData = (IMAGE_THUNK_DATA*)((DWORD)dosHeader + importDescriptor->FirstThunk); thunkData->u1.Function != 0; thunkData++)
            {
                if (thunkData->u1.Function == functionAddress)
                {
                    return (DWORD)&thunkData->u1.Function;
                }
            }
        }
    }

    return TRUE;
}
*/
static BOOL recordModules(HWND window, DWORD processId)
{
    HANDLE processHandle;
    HWND moduleList = GetDlgItem(window, IDC_MODULE_LIST);
    unsigned int itemCount;
    unsigned int i;

    itemCount = ListView_GetItemCount(moduleList);

    if (itemCount == 0)
    {
        MessageBox(window, "At least 1 module must be selected", "Error", MB_OK | MB_ICONWARNING);
        return FALSE;
    }
    
    //SetWindowLong(moduleList, GWL_STYLE, ES_READONLY);

    processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);

    if (processHandle == NULL)
    {
        MessageBox(window, "Failed to open process", "Error", MB_OK | MB_ICONWARNING);
        return FALSE;
    }

    for (i = 0; i < itemCount; i++)
    {
        if (ListView_GetCheckState(moduleList, i))
        {
            LVITEM item;
            char moduleName[MAX_PATH];

            memset(&item, 0, sizeof(item));
            item.mask = LVIF_TEXT | LVIF_PARAM;
            item.state = 0;
            item.stateMask = 0;
            item.iItem = i;
            item.pszText = moduleName;
            item.cchTextMax = sizeof(moduleName);

            ListView_GetItem(moduleList, &item);

            if (item.lParam != 0)
            {
                recordModuleCode(processHandle, (HMODULE)item.lParam, item.pszText);
            }
            else
            {
                MessageBox(window, "Module handle is null", "Error", MB_OK | MB_ICONWARNING);
                CloseHandle(processHandle);
                return FALSE;
            }
        }
    }

    CloseHandle(processHandle);

    return TRUE;
}

void copySelectedCodeChanges(HWND codeChangesList)
{
    char changes[0x4000];

    getListViewSelectedText(codeChangesList, changes, sizeof(changes));
    setClipboardText(changes);
}

BOOL CALLBACK codeChangesDialogProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
        {
            HWND moduleList = GetDlgItem(window, IDC_MODULE_LIST);
            HWND codeChangeList = GetDlgItem(window, IDC_CODE_CHANGE_LIST);
            g_targetProcess = (Process*)lParam;

            EnumChildWindows(window, applyDefaultFont, 0);

            SendDlgItemMessage(window, IDC_PROCESS, WM_SETTEXT, 0, (LPARAM)g_targetProcess->name);

            // Module list
            ListView_SetExtendedListViewStyle(moduleList, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
            initializeListView(moduleList, MODULE_LIST_COLUMNS, ARRAYSIZE(MODULE_LIST_COLUMNS));
            populateModuleList(moduleList, g_targetProcess->processId);    
            resizeListViewColumns(moduleList, MODULE_LIST_COLUMNS, ARRAYSIZE(MODULE_LIST_COLUMNS));

            // Code changes list
            ListView_SetExtendedListViewStyle(codeChangeList, LVS_EX_FULLROWSELECT);
            initializeListView(codeChangeList, CODE_CHANGE_LIST_COLUMNS, ARRAYSIZE(CODE_CHANGE_LIST_COLUMNS));
            resizeListViewColumns(codeChangeList, CODE_CHANGE_LIST_COLUMNS, ARRAYSIZE(CODE_CHANGE_LIST_COLUMNS));

            break;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDC_RECORD_CODE:
                {
                    HWND operationStatus = GetDlgItem(window, IDC_OPERATION_STATUS);
                    HWND findChangesButton = GetDlgItem(window, IDC_FIND_CHANGES);

                    SetWindowText(operationStatus, "");

                    deleteModuleCodeList();
                    if (recordModules(window, g_targetProcess->processId))
                    {
                        EnableWindow(findChangesButton, TRUE);
                        SetWindowText(operationStatus, "Recorded module code");
                    }
                    else
                    {
                        MessageBox(window, "Module code recording failed", "Error", MB_OK | MB_ICONWARNING);
                    }
                    
                    break;
                }

                case IDC_FIND_CHANGES:
                {
                    HWND operationStatus = GetDlgItem(window, IDC_OPERATION_STATUS);
                    char operationText[256] = "";
                    DWORD changesFound = findCodeChanges(window, g_targetProcess->processId);

                    sprintf_s(operationText, sizeof(operationText), "Found %i code change(s)", changesFound);
                    SetWindowText(operationStatus, operationText);
                    resizeListViewColumns(GetDlgItem(window, IDC_CODE_CHANGE_LIST), CODE_CHANGE_LIST_COLUMNS, ARRAYSIZE(CODE_CHANGE_LIST_COLUMNS));                    
                    break;
                }

                case IDCANCEL:
                    deleteModuleCodeList();
                    EndDialog(window, 0);
                    break;

                case IDM_COPY:
                    copySelectedCodeChanges(GetDlgItem(window, IDC_CODE_CHANGE_LIST));
                    break;
            }

            break;

        case WM_NOTIFY:
        {
            NMHDR* notification = (NMHDR*)lParam;

            if (notification->code == LVN_KEYDOWN && notification->idFrom == IDC_CODE_CHANGE_LIST)
            {
                if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 && ((NMLVKEYDOWN*)lParam)->wVKey == 'C')
                {
                    copySelectedCodeChanges(notification->hwndFrom);
                }
            }
        }

        case WM_CONTEXTMENU:
        {
            HWND contextWindow = (HWND)wParam;

            if (contextWindow == GetDlgItem(window, IDC_CODE_CHANGE_LIST))
            {
                HMENU menu = LoadMenu(g_instance, MAKEINTRESOURCE(IDR_CONTEXT));
                HMENU trackMenu = GetSubMenu(menu, 0);

                TrackPopupMenu(trackMenu, TPM_RIGHTBUTTON, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), 0, window, NULL);
                DestroyMenu(menu);
            }
            
            break;
        }

        default:
            return FALSE;
    }

    return TRUE;
}