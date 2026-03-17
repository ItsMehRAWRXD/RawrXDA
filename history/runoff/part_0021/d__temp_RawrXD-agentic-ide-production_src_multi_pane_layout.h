#ifndef MULTI_PANE_LAYOUT_H
#define MULTI_PANE_LAYOUT_H

#include <string>
#include <vector>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <Cocoa/Cocoa.h>
#else
#include <gtk/gtk.h>
#endif

class MultiPaneLayout {
public:
    enum PaneType {
        FILE_TREE = 0,
        MASM_EDITOR = 1,
        AGENT_CHAT = 2,
        TERMINAL = 3
    };

    struct Pane {
        PaneType type;
        std::string title;
        bool visible;
        int x;
        int y;
        int width;
        int height;
        void* handle;
    };

    MultiPaneLayout();
    ~MultiPaneLayout();

    bool create(void* parentWindow);
    void setPaneVisible(PaneType pane, bool visible);
    void resize(int width, int height);
    
    void* getPaneHandle(PaneType pane) const;
    std::vector<Pane> getPanes() const;
    
    // Terminal pane methods
    bool startTerminalSession(PaneType pane, const std::string& shellType);
    void sendTerminalCommand(PaneType pane, const std::string& command);
    std::string getTerminalOutput(PaneType pane);
    
private:
    void createFileTreePane();
    void createMasmEditorPane();
    void createAgentChatPane();
    void createTerminalPane();
    void layoutPanes();
    
#ifdef _WIN32
    HWND m_parentWindow;
    std::vector<HWND> m_paneWindows;
#elif defined(__APPLE__)
    NSWindow* m_parentWindow;
    std::vector<NSView*> m_paneViews;
#else
    GtkWindow* m_parentWindow;
    std::vector<GtkWidget*> m_paneWidgets;
#endif
    
    std::vector<Pane> m_panes;
    int m_totalWidth;
    int m_totalHeight;
};

#endif // MULTI_PANE_LAYOUT_H