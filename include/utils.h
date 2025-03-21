#ifndef UTILS_H
#define UTILS_H

#include <string>

void copyDirectoryFiles(const std::string& sourceDir, const std::string& destinationDir);
bool createFolder(const std::string& folderPath);
void deleteFilesInFolder(const std::string& folderPath);
void moveDirectoryFiles(const std::string& sourceDir, const std::string& destinationDir);

#endif // UTILS_H