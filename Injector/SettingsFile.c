#include <windows.h>
#include <stdio.h>
#include <commctrl.h>
#include "SettingsFile.h"
#include "Process.h"

static const char* FILE_NAME = "Settings.ini";

// Sections
static const char* SECTION_GENERAL = "General";
static const char* SECTION_TARGET = "Target";
static const char* SECTION_DLL = "Dll";

// General
static const char* KEY_SAVE_TARGET = "SaveTarget";
static const char* KEY_SAVE_DLL = "SaveDll";
static const char* KEY_HOTKEY_INJECT = "InjectHotkey";
static const char* KEY_HOTKEY_EJECT = "EjectHotkey";

// Target
static const char* KEY_TARGET_NAME = "Name";
static const char* KEY_TARGET_PATH = "Path";

// Dll
static const char* KEY_DLL_PATH = "Path";

static char g_settingsFilePath[MAX_PATH];

/*
static BOOL g_saveTarget = TRUE;
static BOOL g_saveDll = TRUE;
static int g_injectGlobalHotkey = 0;
static int g_ejectGlobalHotkey = 0;
*/

static void writeBoolean(const char* section, const char* key, BOOL value)
{
    WritePrivateProfileString(section, key, value ? "TRUE" : "FALSE", g_settingsFilePath);
}

static BOOL readBoolean(const char* section, const char* key)
{
    char booleanString[6] = "";
    GetPrivateProfileString(section, key, NULL, booleanString, sizeof(booleanString), g_settingsFilePath);

    if (_stricmp(booleanString, "true") == 0 || _stricmp(booleanString, "1") == 0 || _stricmp(booleanString, "yes") == 0)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

static void writeHotkey(const char* section, const char* key, WORD hotkey)
{
    char keyString[6];

    sprintf_s(keyString, sizeof(keyString), "%i", hotkey);
    WritePrivateProfileString(section, key, keyString, g_settingsFilePath);
}

static WORD readVirtualKey(const char* section, const char* key)
{
    char keyString[6] = "";

    GetPrivateProfileString(section, key, NULL, keyString, sizeof(keyString), g_settingsFilePath);
    return atoi(keyString);
}

void initializeSettingsPath()
{
    GetCurrentDirectory(sizeof(g_settingsFilePath), g_settingsFilePath);
    strcat_s(g_settingsFilePath, sizeof(g_settingsFilePath), "\\");
    strcat_s(g_settingsFilePath, sizeof(g_settingsFilePath), FILE_NAME);
}

void saveSettings(Settings* settings)
{    
    writeBoolean(SECTION_GENERAL, KEY_SAVE_TARGET, settings->rememberTarget);
    writeBoolean(SECTION_GENERAL, KEY_SAVE_DLL, settings->rememberDll);
    writeHotkey(SECTION_GENERAL, KEY_HOTKEY_INJECT, settings->injectHotkey);
    writeHotkey(SECTION_GENERAL, KEY_HOTKEY_EJECT, settings->ejectHotkey);
}

BOOL loadSettings(Settings* settings)
{
    if (GetFileAttributes(g_settingsFilePath) == INVALID_FILE_ATTRIBUTES)
    {
        return FALSE;
    }

    settings->rememberTarget = readBoolean(SECTION_GENERAL, KEY_SAVE_TARGET);
    settings->rememberDll = readBoolean(SECTION_GENERAL, KEY_SAVE_DLL);
    settings->injectHotkey = readVirtualKey(SECTION_GENERAL, KEY_HOTKEY_INJECT);
    settings->ejectHotkey = readVirtualKey(SECTION_GENERAL, KEY_HOTKEY_EJECT);

    return TRUE;
}

void saveLastTarget(Process* target)
{
    WritePrivateProfileString(SECTION_TARGET, KEY_TARGET_NAME, target->name, g_settingsFilePath);
    WritePrivateProfileString(SECTION_TARGET, KEY_TARGET_PATH, target->path, g_settingsFilePath);    
}

void saveLastDllPath(char* dllPath)
{
    WritePrivateProfileString(SECTION_DLL, KEY_DLL_PATH, dllPath, g_settingsFilePath);
}

void clearLastTarget()
{
    WritePrivateProfileString(SECTION_TARGET, KEY_TARGET_NAME, "", g_settingsFilePath);
    WritePrivateProfileString(SECTION_TARGET, KEY_TARGET_PATH, "", g_settingsFilePath);
}

void clearLastDllPath()
{
    WritePrivateProfileString(SECTION_DLL, KEY_DLL_PATH, "", g_settingsFilePath);
}

BOOL loadLastTarget(Process* target)
{
    if (GetFileAttributes(g_settingsFilePath) == INVALID_FILE_ATTRIBUTES)
    {
        return FALSE;
    }

    GetPrivateProfileString(SECTION_TARGET, KEY_TARGET_PATH, NULL, target->path, sizeof(target->path), g_settingsFilePath);    
    return GetPrivateProfileString(SECTION_TARGET, KEY_TARGET_NAME, NULL, target->name, sizeof(target->name), g_settingsFilePath) > 0;
}

BOOL loadLastDllPath(char* dllPath, size_t dllPathSize)
{
    if (GetFileAttributes(g_settingsFilePath) == INVALID_FILE_ATTRIBUTES)
    {
        return FALSE;
    }

    return GetPrivateProfileString(SECTION_DLL, KEY_DLL_PATH, NULL, dllPath, dllPathSize, g_settingsFilePath) > 0;
}