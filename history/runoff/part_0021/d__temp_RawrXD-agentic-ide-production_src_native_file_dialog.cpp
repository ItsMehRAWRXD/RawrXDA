#include "native_file_dialog.h"
#include <windows.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <iostream>

#ifdef _WIN32

std::string NativeFileDialog::getOpenFileName(const std::string& title, 
                                             const std::string& filter, 
                                             const std::string& defaultPath) {
    OPENFILENAME ofn;
    char szFile[260] = {0};
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    
    std::string convertedFilter = convertFilter(filter);
    ofn.lpstrFilter = convertedFilter.c_str();
    ofn.nFilterIndex = 1;
    
    if (!defaultPath.empty()) {
        ofn.lpstrInitialDir = defaultPath.c_str();
    }
    
    ofn.lpstrTitle = title.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    
    if (GetOpenFileName(&ofn)) {
        return std::string(szFile);
    }
    
    return "";
}

std::string NativeFileDialog::getSaveFileName(const std::string& title, 
                                             const std::string& filter, 
                                             const std::string& defaultPath) {
    OPENFILENAME ofn;
    char szFile[260] = {0};
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    
    std::string convertedFilter = convertFilter(filter);
    ofn.lpstrFilter = convertedFilter.c_str();
    ofn.nFilterIndex = 1;
    
    if (!defaultPath.empty()) {
        ofn.lpstrInitialDir = defaultPath.c_str();
    }
    
    ofn.lpstrTitle = title.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    
    if (GetSaveFileName(&ofn)) {
        return std::string(szFile);
    }
    
    return "";
}

std::string NativeFileDialog::getExistingDirectory(const std::string& title) {
    BROWSEINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.lpszTitle = title.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    
    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    if (pidl != nullptr) {
        char path[MAX_PATH];
        if (SHGetPathFromIDList(pidl, path)) {
            CoTaskMemFree(pidl);
            return std::string(path);
        }
        CoTaskMemFree(pidl);
    }
    
    return "";
}

std::string NativeFileDialog::convertFilter(const std::string& filter) {
    std::string result;
    
    // Convert Qt-style filter to Windows format
    // Qt: "Images (*.png *.jpg);;Text files (*.txt)"
    // Win: "Images\\0*.png;*.jpg\\0Text files\\0*.txt\\0"
    
    size_t pos = 0;
    size_t lastPos = 0;
    
    while ((pos = filter.find(";;", lastPos)) != std::string::npos) {
        std::string segment = filter.substr(lastPos, pos - lastPos);
        
        // Find the extension part
        size_t extStart = segment.find('(');
        size_t extEnd = segment.find(')');
        
        if (extStart != std::string::npos && extEnd != std::string::npos) {
            std::string description = segment.substr(0, extStart);
            std::string extensions = segment.substr(extStart + 1, extEnd - extStart - 1);
            
            // Remove spaces and convert to Windows format
            std::string winExtensions;
            size_t extPos = 0;
            size_t lastExtPos = 0;
            
            while ((extPos = extensions.find(' ', lastExtPos)) != std::string::npos) {
                std::string ext = extensions.substr(lastExtPos, extPos - lastExtPos);
                if (!ext.empty()) {
                    winExtensions += ext + ";";
                }
                lastExtPos = extPos + 1;
            }
            
            // Add the last extension
            std::string lastExt = extensions.substr(lastExtPos);
            if (!lastExt.empty()) {
                winExtensions += lastExt + ";";
            }
            
            // Remove trailing semicolon
            if (!winExtensions.empty()) {
                winExtensions.pop_back();
            }
            
            result += description + "\\0" + winExtensions + "\\0";
        }
        
        lastPos = pos + 2;
    }
    
    // Add the last segment
    std::string lastSegment = filter.substr(lastPos);
    if (!lastSegment.empty()) {
        size_t extStart = lastSegment.find('(');
        size_t extEnd = lastSegment.find(')');
        
        if (extStart != std::string::npos && extEnd != std::string::npos) {
            std::string description = lastSegment.substr(0, extStart);
            std::string extensions = lastSegment.substr(extStart + 1, extEnd - extStart - 1);
            
            std::string winExtensions;
            size_t extPos = 0;
            size_t lastExtPos = 0;
            
            while ((extPos = extensions.find(' ', lastExtPos)) != std::string::npos) {
                std::string ext = extensions.substr(lastExtPos, extPos - lastExtPos);
                if (!ext.empty()) {
                    winExtensions += ext + ";";
                }
                lastExtPos = extPos + 1;
            }
            
            std::string lastExt = extensions.substr(lastExtPos);
            if (!lastExt.empty()) {
                winExtensions += lastExt + ";";
            }
            
            if (!winExtensions.empty()) {
                winExtensions.pop_back();
            }
            
            result += description + "\\0" + winExtensions + "\\0";
        }
    }
    
    // Add final null terminator
    result += "\\0";
    
    return result;
}

#endif // _WIN32