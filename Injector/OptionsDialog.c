#include <windows.h>
#include <commctrl.h>
#include "OptionsDialog.h"
#include "SettingsFile.h"
#include "InjectorDialog.h"
#include "Interface.h"
#include "Hotkey.h"
#include "resource.h"

static Settings* g_settings;

BOOL CALLBACK optionsDialogProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
            EnumChildWindows(window, applyDefaultFont, 0);

            g_settings = (Settings*)lParam;
            loadSettings(g_settings);

            // Set controls
            SendDlgItemMessage(window, IDC_REMEMBER_TARGET, BM_SETCHECK, g_settings->rememberTarget == BST_CHECKED, 0);
            SendDlgItemMessage(window, IDC_REMEMBER_DLL, BM_SETCHECK, g_settings->rememberDll == BST_CHECKED, 0);
            SendDlgItemMessage(window, IDC_HOTKEY_INJECT, HKM_SETHOTKEY, g_settings->injectHotkey, 0);
            SendDlgItemMessage(window, IDC_HOTKEY_EJECT, HKM_SETHOTKEY, g_settings->ejectHotkey, 0);

            break;

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDOK:
                {
                    g_settings->rememberTarget = SendDlgItemMessage(window, IDC_REMEMBER_TARGET, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    g_settings->rememberDll = SendDlgItemMessage(window, IDC_REMEMBER_DLL, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    g_settings->injectHotkey = LOWORD(SendDlgItemMessage(window, IDC_HOTKEY_INJECT, HKM_GETHOTKEY, 0, 0));
                    g_settings->ejectHotkey = LOWORD(SendDlgItemMessage(window, IDC_HOTKEY_EJECT, HKM_GETHOTKEY, 0, 0));

                    setHotkeys(g_settings->injectHotkey, g_settings->ejectHotkey);

                    saveSettings(g_settings);

                    //Fall through
                }

                case IDCANCEL:
                    EndDialog(window, 0);
                    installHotkeyHook();
                    break;
            }

            break;

        default:
            return FALSE;
    }

    return TRUE;
}