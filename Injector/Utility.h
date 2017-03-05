#pragma once

const char* getFileNameFromPath(const char* filePath);
unsigned int getDosPath(char* logicalPath, char* destination, size_t destinationSize);