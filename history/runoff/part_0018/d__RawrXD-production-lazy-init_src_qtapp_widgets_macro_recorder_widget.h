/**
 * @file macro_recorder_widget.h
 * @brief Header for MacroRecorderWidget - Macro recording and playback
 */

#pragma once

#include <QWidget>
#include <QString>
#include <QList>

class QVBoxLayout;
class QHBoxLayout;
class QPushButton;
class QLabel;
class QListWidget;
class QLineEdit;
class QSpinBox;
class QCheckBox;

struct MacroCommand {
    QString id;
    QString action; // "keypress", "click", "type"
    QString details;
    int delayMs;
};

class MacroRecorderWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit MacroRecorderWidget(QWidget* parent = nullptr);
    ~MacroRecorderWidget();
    
public slots:
    void onStartRecording();
    void onStopRecording();
    void onPlayMacro();
    void onSaveMacro();
    void onLoadMacro();
    void onClearMacro();
    void onDeleteSelected();
    
signals:
    void recordingStarted();
    void recordingStopped();
    void macroPlayed();
    
private:
    void setupUI();
    void connectSignals();
    
    QVBoxLayout* mMainLayout;
    QPushButton* mRecordButton;
    QPushButton* mStopButton;
    QPushButton* mPlayButton;
    QPushButton* mSaveButton;
    QPushButton* mLoadButton;
    QPushButton* mClearButton;
    QPushButton* mDeleteButton;
    
    QLabel* mStatusLabel;
    QLabel* mCommandCountLabel;
    QLineEdit* mMacroNameEdit;
    QSpinBox* mPlayCountSpinBox;
    QCheckBox* mLoopCheckbox;
    QListWidget* mCommandList;
    
    QList<MacroCommand> mCurrentMacro;
    bool mRecording;
};





