#include <windows.h>

BOOL setClipboardText(char* text)
{
    BOOL success = FALSE;

    if (OpenClipboard(NULL))
    {
        if (EmptyClipboard())
        {
            const size_t textLength = strlen(text) + 1;
            HGLOBAL clipboardMemory = GlobalAlloc(GMEM_FIXED, textLength);

            if (clipboardMemory != NULL)
            {
                char* clipboardText;

                clipboardText = (char*)GlobalLock(clipboardMemory);

                if (clipboardText != NULL)
                {
                    strcpy_s(clipboardText, textLength, text);
                    GlobalUnlock(clipboardMemory);
                    success = SetClipboardData(CF_TEXT, clipboardMemory) != NULL;
                }
            }
        }

        CloseClipboard();
    }

    return success;
}