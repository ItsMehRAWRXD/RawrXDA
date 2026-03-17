#ifndef NATIVE_FILE_DIALOG_H
#define NATIVE_FILE_DIALOG_H

#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

class NativeFileDialog {
public:
    static std::string getOpenFileName(const std::string& title = "Open File", 
                                      const std::string& filter = "All Files (*.*)\\0*.*\\0",
                                      const std::string& defaultPath = "");
    
    static std::string getSaveFileName(const std::string& title = "Save File", 
                                      const std::string& filter = "All Files (*.*)\\0*.*\\0",
                                      const std::string& defaultPath = "");
    
    static std::string getExistingDirectory(const std::string& title = "Select Directory");

private:
    static std::string convertFilter(const std::string& filter);
};

#endif // NATIVE_FILE_DIALOG_H