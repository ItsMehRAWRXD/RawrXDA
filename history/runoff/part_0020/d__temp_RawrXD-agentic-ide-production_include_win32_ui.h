#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <map>

class Win32MenuBar {
public:
    Win32MenuBar();
    ~Win32MenuBar();

    bool create(HWND parent);
    void destroy();
    
    HMENU addMenu(const std::string& name);
    void addAction(HMENU menu, const std::string& text, const std::string& shortcut, std::function<void()> callback, int id = -1);
    void addSeparator(HMENU menu);
    
    void setMenu(HWND parent);
    
    bool handleCommand(int commandId);
    
private:
    HWND m_parent = nullptr;
    HMENU m_menuBar = nullptr;
    std::map<int, std::function<void()>> m_commands;
    int m_nextId = 1000;
};

class Win32ToolBar {
public:
    Win32ToolBar();
    ~Win32ToolBar();

    bool create(HWND parent);
    void destroy();
    
    void addAction(const std::string& text, std::function<void()> callback);
    void addSeparator();
    
    void setPosition(int x, int y, int width, int height);
    
    bool handleCommand(int commandId);
    
private:
    HWND m_parent = nullptr;
    HWND m_toolBar = nullptr;
    std::map<int, std::function<void()>> m_commands;
    int m_nextId = 2000;
    int m_buttonCount = 0;
};

class Win32CommandPalette {
public:
    struct Command {
        std::string name;
        std::string description;
        std::function<void()> callback;
    };

    Win32CommandPalette();
    ~Win32CommandPalette();

    bool create(HWND parent);
    void destroy();
    
    void addCommand(const std::string& name, const std::string& description, std::function<void()> callback);
    void show();
    void hide();
    
    bool isVisible() const { return m_visible; }
    void executeCommand(int index);
    
    bool handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
private:
    void updateList();
    void filterCommands(const std::string& filter);
    
    HWND m_parent = nullptr;
    HWND m_dialog = nullptr;
    HWND m_input = nullptr;
    HWND m_list = nullptr;
    
    std::vector<Command> m_commands;
    std::vector<int> m_filteredIndices;
    bool m_visible = false;
};