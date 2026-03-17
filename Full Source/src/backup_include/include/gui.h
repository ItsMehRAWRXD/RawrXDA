#pragma once
#include <string>
#include <cstdint>

// Forward declarations
struct AppState;

// GUI class for RawrXD (headless/console mode - no actual graphics)
class GUI {
public:
    GUI();
    ~GUI();
    
    // Initialize GUI (no-op for headless build)
    bool Initialize(uint32_t width = 1200, uint32_t height = 800);
    
    // Render main frame
    void Render(AppState& state);
    
    // Shutdown GUI
    void Shutdown();
    
    // Check if window should close
    bool ShouldClose() const;
    
    // Rendering functions
    void RenderMainWindow(AppState& state);
    void RenderChatWindow(AppState& state);
    void RenderModelBrowserWindow(AppState& state);
    void RenderSettingsWindow(AppState& state);
    void RenderOverclockPanel(AppState& state);
    void RenderDownloadWindow(AppState& state);
    void RenderSystemStatus(AppState& state);
    
    // Actions
    void ApplyOverclockProfile(AppState& state);
    void ResetOverclockOffsets(AppState& state);
    void DisplayModelInfo(const std::string& model_path);
    void SendMessage(AppState& state, const std::string& message);
    void AddChatMessage(AppState& state, const std::string& role, const std::string& content);
    void ToggleSetting(bool& setting, const char* name, AppState& state);
    
private:
    uint32_t window_width_;
    uint32_t window_height_;
    bool initialized_;
};
