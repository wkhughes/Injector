#pragma once

#define HOTKEY_INJECT 1
#define HOTKEY_EJECT 2

void installHotkeyHook();
void removeHotkeyHook();
void setHotkeys(WORD injectHotkey, WORD ejectHotkey);