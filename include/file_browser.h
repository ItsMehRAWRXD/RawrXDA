#pragma once

// C++20 / Win32. File browser; no Qt. std::filesystem + callback.

#include <string>
#include <functional>

class FileBrowser
{
public:
    using FileSelectedFn = std::function<void(const std::string& filepath)>;

    FileBrowser() = default;
    void initialize();
    void setOnFileSelected(FileSelectedFn f) { m_onFileSelected = std::move(f); }

    void loadDirectory(const std::string& dirpath);
    void loadDrives();

    void* getWidgetHandle() const { return m_handle; }

private:
    void handleItemClicked(const std::string& path);
    void handleItemExpanded(const std::string& path);

    void* m_handle = nullptr;
    FileSelectedFn m_onFileSelected;
};
