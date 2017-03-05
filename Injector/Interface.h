#pragma once

#include <windows.h>

typedef struct ListViewColumns_t
{
    char* text;
    unsigned int percentWidth;
} ListViewColumns;

BOOL CALLBACK applyDefaultFont(HWND window, LPARAM lParam);
BOOL initializeListView(HWND listView, const ListViewColumns* columns, size_t columnsSize);
BOOL resizeListViewColumns(HWND listView, const ListViewColumns* columns, size_t columnsSize);
void getListViewSelectedText(HWND listView, char* destination, size_t destinationSize);