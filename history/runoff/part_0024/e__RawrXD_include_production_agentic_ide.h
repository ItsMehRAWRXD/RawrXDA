#pragma once

#include <QMainWindow>
#include <QString>
#include <QObject>

class ProductionAgenticIDE : public QMainWindow {
    Q_OBJECT
public:
    explicit ProductionAgenticIDE(QWidget* parent = nullptr);
    ~ProductionAgenticIDE() override;

public slots:
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
    void onFeatureToggled(const QString& featureId, bool enabled);
    void onFeatureClicked(const QString& featureId);
};
