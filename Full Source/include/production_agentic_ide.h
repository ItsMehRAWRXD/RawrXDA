#pragma once

// ============================================================================
// ProductionAgenticIDE — C++20, Win32. No Qt. (QMainWindow, QWidget removed)
// ============================================================================

#include <string>

class ProductionAgenticIDE {
public:
    explicit ProductionAgenticIDE(void* parent = nullptr);
    ~ProductionAgenticIDE();

    void onNewPaint();
    void onNewCode();
    void onNewChat();
    void onOpen();
    void onSave();
    void onSaveAs();
    void onExportImage();
    void onExit();
    void onUndo();
    void onRedo();
    void onCut();
    void onCopy();
    void onPaste();
    void onTogglePaintPanel();
    void onToggleCodePanel();
    void onToggleChatPanel();
    void onToggleFeaturesPanel();
    void onResetLayout();
    void onFeatureToggled(const std::string& featureId, bool enabled);
    void onFeatureClicked(const std::string& featureId);
};
