#include <windows.h>
#include <commctrl.h>
#include "Hotkey.h"
#include "InjectorDialog.h"

#define KEY_STATE_DOWN 0x8000

static HHOOK g_hook;
static WORD g_injectHotkey;
static WORD g_ejectHotkey;

BOOL isHotkeyDown(WORD hotkey, BYTE virtualKey)
{
    BYTE modifiers = 0;

    if ((GetKeyState(VK_SHIFT) & KEY_STATE_DOWN) != 0)
    {
        modifiers |= HOTKEYF_SHIFT;
    }

    if ((GetKeyState(VK_CONTROL) & KEY_STATE_DOWN) != 0)
    {
        modifiers |= HOTKEYF_CONTROL;
    }

    if ((GetKeyState(VK_MENU) & KEY_STATE_DOWN) != 0)
    {
        modifiers |= HOTKEYF_ALT;
    }

    return hotkey == MAKEWORD(virtualKey, modifiers);
}

void setHotkeys(WORD injectHotkey, WORD ejectHotkey)
{
    g_injectHotkey = injectHotkey;
    g_ejectHotkey = ejectHotkey;
}

/*
void reregisterHotkeys(HWND window, WORD injectHotkey, WORD ejectHotkey)
{
    GlobalAddAtom("abcdef");
    UnregisterHotKey(window, HOTKEY_INJECT);
    UnregisterHotKey(window, HOTKEY_EJECT);

    RegisterHotKey(window, HOTKEY_INJECT, getKeyModifier(HIBYTE(injectHotkey)), LOBYTE(injectHotkey));
    RegisterHotKey(window, HOTKEY_EJECT, getKeyModifier(HIBYTE(ejectHotkey)), LOBYTE(ejectHotkey));
}*/

LRESULT CALLBACK keyboardHook(int code, WPARAM wParam, LPARAM lParam)
{
    if (code >= 0)
    {
        KBDLLHOOKSTRUCT* keyboard = (KBDLLHOOKSTRUCT*)lParam;

        if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
        {
            if (isHotkeyDown(g_injectHotkey, LOBYTE(keyboard->vkCode)))
            {
                inject();
            }

            if (isHotkeyDown(g_ejectHotkey, LOBYTE(keyboard->vkCode)))
            {
                eject();
            }
        }
    }

    return CallNextHookEx(g_hook, code, wParam, lParam);
}

void installHotkeyHook()
{
    g_hook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboardHook, NULL, 0);
}

void removeHotkeyHook()
{
    UnhookWindowsHookEx(g_hook);
}