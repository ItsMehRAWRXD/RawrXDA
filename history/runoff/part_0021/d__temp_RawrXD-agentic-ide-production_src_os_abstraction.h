#ifndef OS_ABSTRACTION_H
#define OS_ABSTRACTION_H

#include <string>
#include <vector>

// Platform detection
#if defined(_WIN32)
#define PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
#define PLATFORM_MACOS 1
#elif defined(__linux__)
#define PLATFORM_LINUX 1
#else
#define PLATFORM_UNKNOWN 1
#endif

class OSAbstraction {
public:
    // File operations
    static std::string getHomeDirectory();
    static std::string getCurrentDirectory();
    static bool setCurrentDirectory(const std::string& path);
    static std::vector<std::string> listDirectory(const std::string& path);
    static bool createDirectory(const std::string& path);
    static bool fileExists(const std::string& path);
    
    // File dialogs
    static std::string openFileDialog(const std::string& title, const std::string& filter);
    static std::string saveFileDialog(const std::string& title, const std::string& filter);
    static std::string selectDirectoryDialog(const std::string& title);
    
    // Terminal/Shell
    static std::string getDefaultShell();
    static std::vector<std::string> getAvailableShells();
    static bool executeCommand(const std::string& command, std::string& output, std::string& error);
    
    // GUI Window creation
    static void* createWindow(const std::string& title, int width, int height);
    static void destroyWindow(void* window);
    static void showWindow(void* window);
    static void hideWindow(void* window);
    
    // System information
    static std::string getOSName();
    static std::string getOSVersion();
    static std::string getArchitecture();
    
    // Path utilities
    static std::string pathJoin(const std::string& path1, const std::string& path2);
    static std::string pathBaseName(const std::string& path);
    static std::string pathDirName(const std::string& path);
    static bool pathIsAbsolute(const std::string& path);
    
private:
    // Platform-specific implementations
#ifdef PLATFORM_WINDOWS
    static std::string windowsGetHomeDirectory();
    static std::string windowsOpenFileDialog(const std::string& title, const std::string& filter);
    static std::string windowsGetDefaultShell();
#elif defined(PLATFORM_MACOS)
    static std::string macosGetHomeDirectory();
    static std::string macosOpenFileDialog(const std::string& title, const std::string& filter);
    static std::string macosGetDefaultShell();
#elif defined(PLATFORM_LINUX)
    static std::string linuxGetHomeDirectory();
    static std::string linuxOpenFileDialog(const std::string& title, const std::string& filter);
    static std::string linuxGetDefaultShell();
#endif
};

#endif // OS_ABSTRACTION_H