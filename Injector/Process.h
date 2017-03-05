#pragma once

#include <windows.h>

#define MAX_WINDOW_TEXT_LENGTH 2048

typedef struct
{
    DWORD processId;
    char windowText[MAX_WINDOW_TEXT_LENGTH];
    char name[MAX_PATH];
    char path[MAX_PATH];
} Process;