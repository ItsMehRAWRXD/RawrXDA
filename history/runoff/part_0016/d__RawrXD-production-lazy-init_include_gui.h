#pragma once

#include "settings.h"
#include <string>

class GUI {
public:
	GUI();
	~GUI();

	bool Initialize(uint32_t width, uint32_t height);
	void Shutdown();
	bool ShouldClose() const;

	void Render(AppState& state);
	void RenderMainWindow(AppState& state);
	void RenderChatWindow(AppState& state);
	void RenderModelBrowserWindow(AppState& state);
	void RenderSettingsWindow(AppState& state);
	void RenderDownloadWindow(AppState& state);
	void RenderSystemStatus(AppState& state);
	void RenderOverclockPanel(AppState& state);

	void DisplayModelInfo(const std::string& model_path);
	void SendMessage(AppState& state, const std::string& message);
	void AddChatMessage(AppState& state, const std::string& role, const std::string& content);
	void ToggleSetting(bool& setting, const char* name, AppState& state);

	void ApplyOverclockProfile(AppState& state);
	void ResetOverclockOffsets(AppState& state);

private:
	uint32_t window_width_;
	uint32_t window_height_;
	bool initialized_;
};
