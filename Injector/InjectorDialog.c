#include <windows.h>
#include <stdio.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <psapi.h>
#include "InjectorDialog.h"
#include "Injection.h"
#include "Process.h"
#include "Utility.h"
#include "Interface.h"
#include "SettingsFile.h"
#include "OptionsDialog.h"
#include "CodeChangesDialog.h"
#include "Hotkey.h"
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "psapi.lib")

static const struct
{
    char* text;
    unsigned int percentWidth;
} PROCESS_LIST_COLUMNS[] =
{
    {"Process", 30},
    {"ID", 10},
    {"Window", 30},
    {"Path", 30}
};

static HWND g_window;
static Settings g_settings = {TRUE, TRUE, MAKEWORD(VK_ADD, 0), MAKEWORD(VK_SUBTRACT, 0)};

static Process g_processes[MAX_PROCESSES];
static Process g_targetProcess;

static void* g_injectedDllBase = -1;
static unsigned int g_processCount = 0;
static BOOL g_hasTargetProcess = FALSE;

HINSTANCE g_instance = NULL;

static BOOL initializeProcessList(HWND listView)
{
    LVCOLUMN column;
    unsigned int i;

    ListView_SetExtendedListViewStyle(listView, LVS_EX_FULLROWSELECT);

    // Create columns
    column.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    column.fmt = LVCFMT_LEFT;
    column.cx = 0;

    for (i = 0; i < ARRAYSIZE(PROCESS_LIST_COLUMNS); i++)
    {
        column.iSubItem = i;
        column.pszText = PROCESS_LIST_COLUMNS[i].text;
        
        if (ListView_InsertColumn(listView, i, &column) == -1)
        {
            return FALSE;
        }
    }

    return TRUE;
}

static BOOL populateProcessList(HWND listView)
{
    PROCESSENTRY32 processList[MAX_PROCESSES];
    HIMAGELIST smallIcons;
    unsigned int i;
    unsigned int row = 0;

    SendMessage(listView, WM_SETREDRAW, FALSE, 0);

    memset(g_processes, 0, sizeof(g_processes));

    if (!ListView_DeleteAllItems(listView))
    {
        return FALSE;
    }

    if (!getProcessList(processList, MAX_PROCESSES, &g_processCount))
    {
        return FALSE;
    }

    smallIcons = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), ILC_MASK | ILC_COLOR32, 1, 1);
    
    for (i = g_processCount; i-- > 0; row++)
    {
        HWND processWindow;
        LVITEM item;        
        HMODULE processHandle;
        HICON smallIcon = NULL;
        int iconsExtracted;
        char processLogicalPath[MAX_PATH] = "";
        char processId[9] = "";

        g_processes[row].processId = processList[i].th32ProcessID;
        strcpy_s(g_processes[row].name, sizeof(g_processes[row].name), processList[i].szExeFile);
        
        // Get process window
        processWindow = getProcessWindow(processList[i].th32ProcessID);

        if (processWindow != NULL)
        {
            GetWindowText(processWindow, g_processes[row].windowText, sizeof(g_processes[row].windowText));
        }

        // Get process path
        processHandle = (HMODULE)OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processList[i].th32ProcessID);

        if (processHandle != NULL)
        {
            GetProcessImageFileName(processHandle, processLogicalPath, MAX_PATH);
            getDosPath(processLogicalPath, g_processes[row].path, MAX_PATH);
            CloseHandle(processHandle);
        }

        // Get process icon and add to image lists
        iconsExtracted = ExtractIconEx(g_processes[row].path, 0, NULL, &smallIcon, 1); // Not documented but can return -1
        ImageList_AddIcon(smallIcons, smallIcon);
        DestroyIcon(smallIcon);
        
        // Insert item
        memset(&item, 0, sizeof(item));

        item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
        item.state = 0;
        item.stateMask = 0;
        item.iItem = row;
        item.iImage = iconsExtracted > 0 ? ImageList_GetImageCount(smallIcons) - 1 : -1;
        item.lParam = processList[i].th32ProcessID;

        item.iSubItem = 0;
        item.pszText = processList[i].szExeFile;
        item.cchTextMax = MAX_PATH;

        if (ListView_InsertItem(listView, &item) == -1)
        {
            return FALSE;
        }

        // Insert sub items
        item.mask = LVIF_TEXT;

        item.iSubItem = 1;
        sprintf_s(processId, 9, "%08X", processList[i].th32ProcessID);
        item.pszText = processId;
        item.cchTextMax = sizeof(processId);
        
        if (ListView_SetItem(listView, &item) == -1)
        {
            return FALSE;
        }
        
        item.iSubItem = 2;
        item.pszText = g_processes[row].windowText;
        item.cchTextMax = sizeof(g_processes[row].windowText);
        
        if (ListView_SetItem(listView, &item) == -1)
        {
            return FALSE;
        }

        item.iSubItem = 3;
        item.pszText = g_processes[row].path;
        item.cchTextMax = sizeof(g_processes[row].path);
        
        if (ListView_SetItem(listView, &item) == -1)
        {
            return FALSE;
        }
    }

    ListView_SetImageList(listView, smallIcons, LVSIL_SMALL);

    // Reselect old selected
    if (g_hasTargetProcess)
    {
        LVFINDINFO findItem;
        int foundIndex;

        memset(&findItem, 0, sizeof(findItem));

        findItem.flags = LVFI_PARAM;
        findItem.lParam = g_targetProcess.processId;

        foundIndex = ListView_FindItem(listView, -1, &findItem);

        if (foundIndex != -1)
        {
            ListView_SetItemState(listView, foundIndex, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
            // TODO set scroll if not visible: http://msdn.microsoft.com/en-us/library/aa452722.aspx
        }
    }

    SendMessage(listView, WM_SETREDRAW, TRUE, 0);

    return TRUE;
}

static BOOL resizeProcessListColumns(HWND listView)
{
    RECT listViewRectangle;
    int listViewWidth;
    unsigned int i;
    
    if (!GetClientRect(listView, &listViewRectangle))
    {
        return FALSE;
    }

    listViewWidth = listViewRectangle.right - listViewRectangle.left;

    for (i = 0; i < ARRAYSIZE(PROCESS_LIST_COLUMNS); i++)
    {
        int columnWidth = listViewWidth * PROCESS_LIST_COLUMNS[i].percentWidth / 100;

        if (ListView_SetColumnWidth(listView, i, columnWidth) == -1)
        {
            return FALSE;
        }
    }

    return TRUE;
}

static void setDllButtonsEnabled(HWND window, BOOL enabled)
{
    HWND injectButton = GetDlgItem(window, IDC_INJECT);
    HWND ejectButton = GetDlgItem(window, IDC_EJECT);

    if (injectButton != NULL && ejectButton != NULL)
    {
        EnableWindow(injectButton, enabled);
        EnableWindow(ejectButton, enabled);
    }
}

static void findActiveProcess()
{
    if (g_hasTargetProcess)
    {
        if (!isProcessActive(g_targetProcess.processId))
        {
            unsigned int i;
            unsigned int j = 0;

            for (i = g_processCount; i-- > 0; j++)
            {
                if (strcmp(g_targetProcess.name, g_processes[j].name) == 0)
                {
                    g_targetProcess = g_processes[j];
                    ListView_SetItemState(GetDlgItem(g_window, IDC_PROCESS_LIST), j, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
                    // TODO set scroll if not visible: http://msdn.microsoft.com/en-us/library/aa452722.aspx

                    break;
                }
            }
        }
    }
}

static void refreshTargetProcess()
{
    if (g_hasTargetProcess)
    {
        HWND name = GetDlgItem(g_window, IDC_TARGET_PROCESS);
        HWND status = GetDlgItem(g_window, IDC_TARGET_STATUS);
        HWND processIcon = GetDlgItem(g_window, IDC_TARGET_ICON);

        SetWindowText(name, g_targetProcess.name);

        if (isProcessActive(g_targetProcess.processId))
        {
            SetWindowText(status, "Active");
        }
        else
        {
            SetWindowText(status, "Process is not running");
        }

        InvalidateRect(processIcon, NULL, TRUE);
    }
}

void refresh()
{
    HWND processList = GetDlgItem(g_window, IDC_PROCESS_LIST);

    populateProcessList(processList);
    resizeProcessListColumns(processList);
    findActiveProcess();
    refreshTargetProcess();
}

BOOL inject()
{
    HWND processList = GetDlgItem(g_window, IDC_PROCESS_LIST);
    HWND selectedDll = GetDlgItem(g_window, IDC_DLL);
    HWND dllStatus = GetDlgItem(g_window, IDC_DLL_STATUS);
    char dllPathText[MAX_PATH] = "";
    void* existingModuleBase;

    // Refresh in case target process wasn't active and now is
    refresh();
    UpdateWindow(processList);

    if (processList == NULL || selectedDll == NULL || dllStatus == NULL)
    {
        return FALSE;
    }

    SetWindowText(dllStatus, "");

    if (GetWindowText(selectedDll, dllPathText, MAX_PATH) == 0)
    {
        MessageBox(g_window, "No DLL selected", "Error", MB_OK | MB_ICONWARNING);
        return FALSE;
    }

    if (!g_hasTargetProcess)
    {
        MessageBox(g_window, "No target process selected", "Error", MB_OK | MB_ICONWARNING);
        return FALSE;
    }

    if (!isProcessActive(g_targetProcess.processId))
    {
        MessageBox(g_window, "Target process is not running", "Error", MB_OK | MB_ICONWARNING);
        return FALSE;
    }

    if (GetFileAttributes(dllPathText) == INVALID_FILE_ATTRIBUTES)
    {
        MessageBox(g_window, "Selected DLL does not exist", "Error", MB_OK | MB_ICONWARNING);
        return FALSE;
    }

    if (getProcessModuleBase(getFileNameFromPath(dllPathText), g_targetProcess.processId, &existingModuleBase))
    {
        char statusText[128];

        g_injectedDllBase = existingModuleBase;
        sprintf_s(statusText, sizeof(statusText), "Module already loaded at base %p", existingModuleBase);
        SetWindowText(dllStatus, statusText);
    }
    else
    {
        void* dllBase;
        
        SetWindowText(dllStatus, "Injecting...");
        UpdateWindow(dllStatus);
        dllBase = injectDll(g_targetProcess.processId, dllPathText);

        if (dllBase < 0)
        {
            char errorText[128];

            sprintf_s(errorText, sizeof(errorText), "Failed to inject, error %p", dllBase);

            SetWindowText(dllStatus, errorText);
            MessageBox(g_window, errorText, "Error", MB_OK | MB_ICONWARNING);
        }
        else if (dllBase == 0)
        {
            SetWindowText(dllStatus, "DLL initialization failed");
        }
        else
        {
            char statusText[128];

            g_injectedDllBase = dllBase;
            sprintf_s(statusText, sizeof(statusText), "Successfully injected at base 0x%p", dllBase);
            SetWindowText(dllStatus, statusText);
        }
    }

    return TRUE;
}

BOOL eject()
{
    HWND selectedDll = GetDlgItem(g_window, IDC_DLL);
    HWND dllStatus = GetDlgItem(g_window, IDC_DLL_STATUS);
    void* existingModuleBase;
    char dllPathText[MAX_PATH];

    if (selectedDll == NULL || dllStatus == NULL)
    {
        return FALSE;
    }

    if (GetWindowText(selectedDll, dllPathText, MAX_PATH) == 0)
    {
        MessageBox(g_window, "No DLL selected", "Error", MB_OK | MB_ICONWARNING);
        return FALSE;
    }

    if (!g_hasTargetProcess)
    {
        MessageBox(g_window, "No target process selected", "Error", MB_OK | MB_ICONWARNING);
        return FALSE;
    }

    if (!isProcessActive(g_targetProcess.processId))
    {
        MessageBox(g_window, "Target process is not running", "Error", MB_OK | MB_ICONWARNING);
        return FALSE;
    }

    if (GetFileAttributes(dllPathText) == INVALID_FILE_ATTRIBUTES)
    {
        MessageBox(g_window, "Selected DLL does not exist", "Error", MB_OK | MB_ICONWARNING);
        return FALSE;
    }

    if (!getProcessModuleBase(getFileNameFromPath(dllPathText), g_targetProcess.processId, &existingModuleBase))
    {
        SetWindowText(dllStatus, "DLL not currently loaded in process");
        return FALSE;
    }
    else
    {
        if (ejectDll(g_targetProcess.processId, g_injectedDllBase))
        {
            SetWindowText(dllStatus, "Successfully ejected");
            return TRUE;
        }
        else
        {
            SetWindowText(dllStatus, "Failed to eject");
            return FALSE;
        }
    }
}

BOOL CALLBACK injectorDialogProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
        {
            HWND processList;

            g_window = window;

            SendMessage(window, WM_SETICON, ICON_SMALL, (LPARAM)LoadIcon(g_instance, MAKEINTRESOURCE(IDI_INJECTOR)));
            SendMessage(window, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(g_instance, MAKEINTRESOURCE(IDI_INJECTOR)));
            EnumChildWindows(window, applyDefaultFont, 0);

            if (!enableDebugPrivileges())
            {
                MessageBox(window, "Failed to enable debug privileges, injection may fail", "Error", MB_OK | MB_ICONEXCLAMATION);
            }
            
            initializeSettingsPath();
            
            loadSettings(&g_settings);
            
            if (g_settings.rememberTarget)
            {
                g_hasTargetProcess = loadLastTarget(&g_targetProcess);
            }

            if (g_settings.rememberDll)
            {
                HWND selectedDll = GetDlgItem(window, IDC_DLL);
                char dllPath[MAX_PATH] = "";

                loadLastDllPath(dllPath, sizeof(dllPath));
                SetWindowText(selectedDll, dllPath);
            }

            processList = GetDlgItem(window, IDC_PROCESS_LIST);
            initializeProcessList(processList);
            ShowWindow(window, SW_SHOW); // Show window here so this process window is listed
            refresh();

            setHotkeys(g_settings.injectHotkey, g_settings.ejectHotkey);
            installHotkeyHook();

            break;
        }

        case WM_NOTIFY:
        {
            NMHDR* notification = (NMHDR*)lParam;

            switch (LOWORD(wParam))
            {
                case IDC_PROCESS_LIST:
                    if (notification->code == NM_CLICK)
                    {
                        HWND processList = GetDlgItem(window, IDC_PROCESS_LIST);

                        if (processList != NULL)
                        {
                            g_hasTargetProcess = TRUE;
                            g_targetProcess = g_processes[ListView_GetNextItem(processList, -1, LVNI_SELECTED)];
                            refreshTargetProcess();
                        }
                    }

                    break;
            }

            break;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDC_DLL:
                    if (HIWORD(wParam) == EN_CHANGE)
                    {
                        HWND selectedDll = GetDlgItem(window, IDC_DLL);
                        HWND injectButton = GetDlgItem(window, IDC_INJECT);
                        HWND ejectButton = GetDlgItem(window, IDC_EJECT);

                        if (selectedDll != NULL && injectButton != NULL && ejectButton != NULL)
                        {
                            if (GetWindowTextLength(selectedDll) > 0)
                            {
                                setDllButtonsEnabled(window, TRUE);
                            }
                            else
                            {
                                setDllButtonsEnabled(window, FALSE);
                            }
                        }
                    }

                    break;

                case IDC_SELECT:
                {
                    HWND selectedDll = GetDlgItem(window, IDC_DLL);
                    char filePath[MAX_PATH] = "";
                    OPENFILENAME openFileDialog;

                    memset(&openFileDialog, 0, sizeof(openFileDialog));

                    openFileDialog.lStructSize = sizeof(openFileDialog);
                    openFileDialog.hwndOwner = window;
                    openFileDialog.Flags = OFN_FILEMUSTEXIST;
                    openFileDialog.lpstrFilter = "Dll Files (*.dll)\0*.dll\0";
                    openFileDialog.lpstrDefExt = "dll";
                    openFileDialog.lpstrFile = filePath;
                    openFileDialog.nMaxFile = MAX_PATH;

                    if (selectedDll != NULL)
                    {
                        char dllPathText[MAX_PATH];

                        if (GetWindowText(selectedDll, dllPathText, MAX_PATH) > 0)
                        {
                            openFileDialog.lpstrInitialDir = dllPathText;
                        }
                    }

                    if (GetOpenFileName(&openFileDialog))
                    {
                        if (selectedDll != NULL)
                        {
                            SetWindowText(selectedDll, filePath);
                        }
                    }

                    break;
                }

                case IDM_PROCESS_REFRESH:
                case IDC_PROCESS_REFRESH:
                    refresh();
                    break;

                case IDC_INJECT:
                case IDM_INJECT:
                    inject();
                    break;

                case IDC_EJECT:
                case IDM_EJECT:
                    eject();
                    break;

                case IDM_CODE_CHANGES:
                {
                    if (!g_hasTargetProcess)
                    {
                        MessageBox(g_window, "No process selected to find code changes", "Error", MB_OK | MB_ICONWARNING);
                        break;
                    }

                    if (!isProcessActive(g_targetProcess.processId))
                    {
                        MessageBox(g_window, "Selected process is not active", "Error", MB_OK | MB_ICONWARNING);
                        break;
                    }

                    CreateDialogParam(g_instance, MAKEINTRESOURCE(IDD_CODE_CHANGES), window, codeChangesDialogProcedure, (LPARAM)&g_targetProcess);
                    break;
                }

                case IDM_OPTIONS:
                    removeHotkeyHook();
                    CreateDialogParam(g_instance, MAKEINTRESOURCE(IDD_OPTIONS), window, optionsDialogProcedure, (LPARAM)&g_settings);
                    break;

                case IDM_EXIT:
                    ExitProcess(0);
                    break;
            }

            break;

        case WM_DRAWITEM:
        {
            DRAWITEMSTRUCT* drawItem = (DRAWITEMSTRUCT*)lParam;

            switch (drawItem->CtlID)
            {
                case IDC_TARGET_ICON:
                {
                    if (g_hasTargetProcess)
                    {
                        HICON largeIcon;

                        FillRect(drawItem->hDC, &drawItem->rcItem, (HBRUSH)COLOR_WINDOW);

                        ExtractIconEx(g_targetProcess.path, 0, &largeIcon, NULL, 1);                        
                        DrawIconEx(drawItem->hDC, drawItem->rcItem.left, drawItem->rcItem.top, largeIcon, drawItem->rcItem.left + GetSystemMetrics(SM_CXICON), drawItem->rcItem.top + GetSystemMetrics(SM_CYICON), 0, NULL, DI_NORMAL);
                        DestroyIcon(largeIcon);
                    }
                }
            }

            break;
        }

        case WM_CLOSE:
        {
            if (g_settings.rememberTarget)
            {
                saveLastTarget(&g_targetProcess);
            }
            else
            {
                clearLastTarget();
            }

            if (g_settings.rememberDll)
            {
                HWND selectedDll = GetDlgItem(window, IDC_DLL);
                char dllPath[MAX_PATH] = "";

                GetWindowText(selectedDll, dllPath, sizeof(dllPath));
                saveLastDllPath(dllPath);
            }
            else
            {
                clearLastDllPath();
            }

            DestroyWindow(window);
            break;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return FALSE;
    }

    return TRUE;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previousInstance, LPSTR commandLine, int showCommand)
{
    MSG message;

    g_instance = instance;
    CreateDialog(g_instance, MAKEINTRESOURCE(IDD_INJECTOR), NULL, injectorDialogProcedure);

    if (g_window == NULL)
    {
        MessageBox(NULL, "Dialog creation failed", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    while (GetMessage(&message, NULL, 0, 0) > 0)
    {
        if (!IsDialogMessage(g_window, &message))
        {            
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
    }

    return message.wParam;
}