#include <windows.h>

#define MAX_DRIVES 26

const char* getFileNameFromPath(const char* filePath)
{
    const char* fileName = filePath + strlen(filePath);

    while (fileName > filePath)
    {
        if (*fileName == '\\' || *fileName == '/')
        {
            return ++fileName;
        }

        fileName--;
    }

    return fileName;
}

unsigned int getDosPath(char* logicalPath, char* destination, size_t destinationSize)
{
    char drives[MAX_DRIVES][4];
    unsigned int drive;
    DWORD drivesCount;
    DWORD drivesLength;

    if (logicalPath == NULL)
    {
        return 0;
    }
    
    drivesLength = GetLogicalDriveStrings(256, (char*)drives);
    drivesCount = drivesLength / sizeof(drives[0]);

    for (drive = 0; drive < drivesCount; drive++)
    {
        char driveName[3];
        char driveLogicalPath[MAX_PATH] = "";

        strncpy_s(driveName, sizeof(driveName), drives[drive], 2);

        QueryDosDevice(driveName, driveLogicalPath, MAX_PATH);
        
        if (strstr(logicalPath, driveLogicalPath) == logicalPath)
        {
            strcpy_s(destination, destinationSize, driveName);
            strcat_s(destination, destinationSize, logicalPath + strlen(driveLogicalPath));

            return strlen(destination) + 1;
        }
    }

    return 0;
}
