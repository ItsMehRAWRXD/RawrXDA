#include <iostream>
#include <memory>
#include <filesystem>
#include <thread>

#include <mutex>

// Forward declarations - minimal Windows types
typedef void* HWND;
typedef void* HINSTANCE;
typedef unsigned long DWORD;
typedef int BOOL;
typedef unsigned int UINT;
typedef long LPARAM;
typedef long WPARAM;

// Stub implementations for now
struct AppState {
    bool running = true;
    std::string model_path;
};

int main() {


    // Keep running
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}
