#include <windows.h>
#include <commctrl.h>
#include "Interface.h"

#define LIST_VIEW_COPY_MAX_ITEM_LENGTH 3078

BOOL CALLBACK applyDefaultFont(HWND window, LPARAM lParam)
{
    NONCLIENTMETRICS nonClientMetrics;
    HFONT font;

    nonClientMetrics.cbSize = sizeof(NONCLIENTMETRICS);

    if (!SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &nonClientMetrics, 0))
    {
        return FALSE;
    }

    font = CreateFontIndirect(&nonClientMetrics.lfMessageFont);
    SendMessage(window, WM_SETFONT, (WPARAM)font, MAKELPARAM(TRUE, 0));

    return TRUE;
}

BOOL initializeListView(HWND listView, const ListViewColumns* columns, size_t columnsSize)
{
    LVCOLUMN column;
    unsigned int i;

    // Create columns
    column.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    column.fmt = LVCFMT_LEFT;
    column.cx = 0;

    for (i = 0; i < columnsSize; i++)
    {
        column.iSubItem = i;
        column.pszText = columns[i].text;
        
        if (ListView_InsertColumn(listView, i, &column) == -1)
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOL resizeListViewColumns(HWND listView, const ListViewColumns* columns, size_t columnsSize)
{
    RECT listViewRectangle;
    int listViewWidth;
    unsigned int i;
    
    if (!GetClientRect(listView, &listViewRectangle))
    {
        return FALSE;
    }

    listViewWidth = listViewRectangle.right - listViewRectangle.left;

    for (i = 0; i < columnsSize; i++)
    {
        int columnWidth = listViewWidth * columns[i].percentWidth / 100;

        if (ListView_SetColumnWidth(listView, i, columnWidth) == -1)
        {
            return FALSE;
        }
    }

    return TRUE;
}

void getListViewSelectedText(HWND listView, char* destination, size_t destinationSize)
{
    const unsigned int columnCount = Header_GetItemCount(ListView_GetHeader(listView));
    int selectedRow = ListView_GetNextItem(listView, -1, LVNI_SELECTED);

    if (destinationSize == 0)
    {
        return;
    }

    *destination = '\0';

    while (selectedRow != -1)
    {
        unsigned int column;

        if (destination[0] != '\0')
        {
            strcat_s(destination, destinationSize, "\n");
        }

        for (column = 0; column < columnCount; column++)
        {
            int textBufferSize = 1024;

            LVITEM item;
            memset(&item, 0, sizeof(item));
            item.iSubItem = column;
            item.cchTextMax = textBufferSize;
            item.pszText = (char*)calloc(textBufferSize, sizeof(char));

            // No good way to do this as listview doesn't store the text length, keep trying until our buffer size is larger than the returned text
            while (SendMessage(listView, LVM_GETITEMTEXT, (WPARAM)selectedRow, (LPARAM)&item) == textBufferSize - 1)
            {
                free(item.pszText);
                textBufferSize *= 2;
                item.cchTextMax = textBufferSize;
                item.pszText = (char*)calloc(textBufferSize, sizeof(char));                
            }

            strcat_s(destination, destinationSize, item.pszText);
            free(item.pszText);
            
            if (column < columnCount - 1)
            {
                strcat_s(destination, destinationSize, "\t");
            }
        }

        selectedRow = ListView_GetNextItem(listView, selectedRow, LVNI_SELECTED);        
    }
}