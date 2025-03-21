#include "utils.h"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

void copyDirectoryFiles(const std::string& sourceDir, const std::string& destinationDir) {
    try {
        fs::create_directories(destinationDir);

        for (const auto& entry : fs::directory_iterator(sourceDir)) {
            const auto& path = entry.path();
            const auto destPath = fs::path(destinationDir) / path.filename();

            if (fs::is_regular_file(path)) {
                fs::copy_file(path, destPath, fs::copy_options::overwrite_existing);
                // std::cout << "Copied: " << path << " to " << destPath << std::endl;
            }
        }
        // std::cout << "All files copied successfully." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void moveDirectoryFiles(const std::string& sourceDir, const std::string& destinationDir) {
    try {
        // Create destination directory if it doesn't exist
        fs::create_directories(destinationDir);
        
        // Iterate through files in source directory
        for (const auto& entry : fs::directory_iterator(sourceDir)) {
            const auto& path = entry.path();
            const auto destPath = fs::path(destinationDir) / path.filename();
            
            if (fs::is_regular_file(path)) {
                // Check if destination file already exists
                if (fs::exists(destPath)) {
                    fs::remove(destPath);  // Remove existing file to avoid rename failure
                }
                
                fs::rename(path, destPath);
                // std::cout << "Moved: " << path << " to " << destPath << std::endl;
            }
        }
        // std::cout << "All files moved successfully." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

bool createFolder(const std::string& folderPath) {
    try {
        if (!fs::exists(folderPath)) {
            if (fs::create_directories(folderPath)) {
                std::cout << "Folder created: " << folderPath << std::endl;
                return true;
            } else {
                std::cerr << "Failed to create folder: " << folderPath << std::endl;
                return false;
            }
        } else {
            std::cout << "Folder already exists: " << folderPath << std::endl;
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error creating folder: " << e.what() << std::endl;
        return false;
    }
}

void deleteFilesInFolder(const std::string& folderPath) {
    try {
        if (!fs::exists(folderPath)) {
            std::cerr << "Folder does not exist: " << folderPath << std::endl;
            return;
        }

        for (const auto& entry : fs::directory_iterator(folderPath)) {
            if (fs::is_regular_file(entry)) {
                fs::remove(entry.path());
                // std::cout << "Deleted file: " << entry.path() << std::endl;
            }
        }
        // std::cout << "All files in the folder have been deleted." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error deleting files: " << e.what() << std::endl;
    }
}