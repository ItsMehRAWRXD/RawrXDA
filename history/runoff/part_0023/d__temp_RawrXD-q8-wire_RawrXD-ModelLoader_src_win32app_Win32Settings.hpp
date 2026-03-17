/**
 * @file Win32Settings.hpp
 * @brief Header for Win32 settings dialog system
 */

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>

namespace RawrXD::Win32 {

enum class SettingType {
    Boolean,
    String,
    Integer,
    Float,
    Choice,
    Color,
    FilePath,
    DirectoryPath
};

struct SettingValue {
    SettingType type;
    bool boolValue = false;
    std::string stringValue;
    int intValue = 0;
    float floatValue = 0.0f;
    std::vector<std::string> choiceOptions;
    int choiceIndex = 0;

    SettingValue() = default;
    SettingValue(bool val) : type(SettingType::Boolean), boolValue(val) {}
    SettingValue(const std::string& val) : type(SettingType::String), stringValue(val) {}
    SettingValue(int val) : type(SettingType::Integer), intValue(val) {}
    SettingValue(float val) : type(SettingType::Float), floatValue(val) {}
};

struct Setting {
    std::string key;
    std::string displayName;
    std::string description;
    SettingValue value;
    std::string category;
    std::function<void(const SettingValue&)> onChange;
};

class Win32Settings {
public:
    Win32Settings();
    ~Win32Settings();

    // Settings management
    void addSetting(const Setting& setting);
    void setValue(const std::string& key, const SettingValue& value);
    SettingValue getValue(const std::string& key) const;
    bool hasSetting(const std::string& key) const;

    // Dialog management
    void showSettingsDialog(HWND parentHwnd);
    void closeSettingsDialog();

    // Persistence
    void loadFromFile(const std::string& filename);
    void saveToFile(const std::string& filename);

    // Categories
    std::vector<std::string> getCategories() const;
    std::vector<Setting> getSettingsInCategory(const std::string& category) const;

private:
    std::unordered_map<std::string, Setting> m_settings;
    std::unordered_map<std::string, std::vector<std::string>> m_categories;
    HWND m_dialogHwnd = nullptr;

    // Dialog procedure
    static INT_PTR CALLBACK SettingsDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Helper methods
    void createSettingsDialog(HWND parentHwnd);
    void populateCategoryList(HWND listBox);
    void populateSettingsList(HWND listBox, const std::string& category);
    void createSettingControl(HWND parent, const Setting& setting, int x, int y, int width, int height);
    void updateSettingControl(HWND control, const Setting& setting);
    void applySettingChange(const std::string& key, const SettingValue& newValue);

    // Control IDs
    enum {
        IDC_CATEGORY_LIST = 1001,
        IDC_SETTINGS_LIST = 1002,
        IDC_SETTING_CONTROL = 1003,
        IDC_OK_BUTTON = 1004,
        IDC_CANCEL_BUTTON = 1005,
        IDC_APPLY_BUTTON = 1006
    };
};

} // namespace RawrXD::Win32