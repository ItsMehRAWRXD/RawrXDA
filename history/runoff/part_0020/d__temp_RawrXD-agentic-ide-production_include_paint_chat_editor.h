#pragma once
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

#include "paint/paint_app.h"

// Headless, native paint editor abstractions. These provide the same
// logical behaviour as the former Qt widgets but are platform-agnostic.

class PaintEditorTab {
public:
    explicit PaintEditorTab(const std::string &tabName);
    ~PaintEditorTab();

    PaintCanvas* getCanvas() { return m_canvas.get(); }
    std::string getTabName() const { return m_tabName; }
    bool hasUnsavedChanges() const { return m_unsavedChanges; }
    void setUnsavedChanges(bool changed);
    void exportPNG(const std::string &filepath);
    void exportBMP(const std::string &filepath);
    void clear();

    // Basic callbacks
    std::function<void()> onCanvasModified;
    std::function<void()> onCloseRequested;

private:
    void buildUI();
    void connectCanvasSignals();
    void updateColorButtons();

    std::unique_ptr<PaintCanvas> m_canvas;
    std::string m_tabName;
    bool m_unsavedChanges;
    std::string m_lastSavedPath;
};

class PaintTabbedEditor {
public:
    PaintTabbedEditor();
    ~PaintTabbedEditor();

    void initialize();
    void newPaintTab();
    void closePaintTab(int index);
    void savePaintTab(int index);
    void saveAllPaintTabs();
    void exportCurrentAsImage();
    PaintEditorTab* getCurrentPaintTab() const;
    int getTabCount() const;

    void closeAllTabs();
    void closeAllExcept(int index);

    // Callbacks
    std::function<void(int)> onTabCountChanged;
    std::function<void(PaintEditorTab*)> onCurrentTabChanged;

private:
    void createUI();
    void setupConnections();
    std::string generateNewTabName();
    void updateTabLabel(int index);

    std::unordered_map<int, std::unique_ptr<PaintEditorTab>> m_tabs;
    int m_tabCounter = 0;
    int m_currentIndex = -1;
};

class ChatTabbedInterface {
public:
    ChatTabbedInterface();
    ~ChatTabbedInterface();

    void initialize();
    void newChatTab();
    void closeChatTab(int index);
    int getTabCount() const;
    // Minimal headless chat interface hooks omitted for brevity

private:
    int m_chatTabCounter = 0;
};

class EnhancedCodeEditor {
public:
    EnhancedCodeEditor();
    ~EnhancedCodeEditor();

    void initialize();
    int getTabCount() const;
    void optimizeForMASM();
    void optimizeForStandard();

private:
    // Implementation details omitted for headless build
    int m_maxCachedTabs = 0;
};
