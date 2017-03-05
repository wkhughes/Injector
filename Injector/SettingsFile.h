#pragma once

#include <windows.h>
#include "Process.h"

typedef struct
{
    BOOL rememberTarget;
    BOOL rememberDll;
    WORD injectHotkey;
    WORD ejectHotkey;
} Settings;

void initializeSettingsPath();
void saveSettings(Settings* settings);
BOOL loadSettings(Settings* settings);
void saveLastTarget(Process* target);
void saveLastDllPath(char* target);
void clearLastTarget();
void clearLastDllPath();
BOOL loadLastTarget(Process* process);
BOOL loadLastDllPath(char* dllPath, size_t dllPathSize);