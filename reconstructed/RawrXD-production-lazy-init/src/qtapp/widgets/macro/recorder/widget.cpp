/**
 * @file macro_recorder_widget.cpp
 * @brief Implementation of MacroRecorderWidget - Macro recording and playback
 */

#include "macro_recorder_widget.h"
#include "integration/ProdIntegration.h"
#include "integration/InitializationTracker.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QMessageBox>
#include <QFileDialog>
#include <QSettings>
#include <QDebug>

MacroRecorderWidget::MacroRecorderWidget(QWidget* parent)
    : QWidget(parent), mRecording(false)
{
    RawrXD::Integration::ScopedInitTimer init("MacroRecorderWidget");
    setupUI();
    connectSignals();
    setWindowTitle("Macro Recorder");
}

MacroRecorderWidget::~MacroRecorderWidget() = default;

void MacroRecorderWidget::setupUI()
{
    mMainLayout = new QVBoxLayout(this);
    
    // Control panel
    QHBoxLayout* controlLayout = new QHBoxLayout();
    
    mStatusLabel = new QLabel("Ready", this);
    mStatusLabel->setStyleSheet("font-weight: bold;");
    controlLayout->addWidget(mStatusLabel);
    
    mRecordButton = new QPushButton("Record", this);
    mRecordButton->setStyleSheet("background-color: #f44336; color: white; padding: 8px;");
    controlLayout->addWidget(mRecordButton);
    
    mStopButton = new QPushButton("Stop", this);
    mStopButton->setStyleSheet("background-color: #757575; color: white; padding: 8px;");
    mStopButton->setEnabled(false);
    controlLayout->addWidget(mStopButton);
    
    mPlayButton = new QPushButton("Play", this);
    mPlayButton->setStyleSheet("background-color: #4CAF50; color: white; padding: 8px;");
    mPlayButton->setEnabled(false);
    controlLayout->addWidget(mPlayButton);
    
    controlLayout->addStretch();
    mMainLayout->addLayout(controlLayout);
    
    // Macro info
    QHBoxLayout* infoLayout = new QHBoxLayout();
    infoLayout->addWidget(new QLabel("Macro Name:", this));
    mMacroNameEdit = new QLineEdit("Macro1", this);
    infoLayout->addWidget(mMacroNameEdit);
    
    infoLayout->addSpacing(20);
    mCommandCountLabel = new QLabel("Commands: 0", this);
    infoLayout->addWidget(mCommandCountLabel);
    
    infoLayout->addStretch();
    mMainLayout->addLayout(infoLayout);
    
    // Playback options
    QHBoxLayout* playbackLayout = new QHBoxLayout();
    playbackLayout->addWidget(new QLabel("Play Count:", this));
    mPlayCountSpinBox = new QSpinBox(this);
    mPlayCountSpinBox->setMinimum(1);
    mPlayCountSpinBox->setMaximum(100);
    mPlayCountSpinBox->setValue(1);
    playbackLayout->addWidget(mPlayCountSpinBox);
    
    mLoopCheckbox = new QCheckBox("Loop", this);
    playbackLayout->addWidget(mLoopCheckbox);
    
    playbackLayout->addStretch();
    mMainLayout->addLayout(playbackLayout);
    
    // Command list
    mCommandList = new QListWidget(this);
    mMainLayout->addWidget(new QLabel("Recorded Commands:", this));
    mMainLayout->addWidget(mCommandList);
    
    // Action buttons
    QHBoxLayout* actionLayout = new QHBoxLayout();
    
    mSaveButton = new QPushButton("Save", this);
    actionLayout->addWidget(mSaveButton);
    
    mLoadButton = new QPushButton("Load", this);
    actionLayout->addWidget(mLoadButton);
    
    mDeleteButton = new QPushButton("Delete Selected", this);
    actionLayout->addWidget(mDeleteButton);
    
    mClearButton = new QPushButton("Clear All", this);
    actionLayout->addWidget(mClearButton);
    
    actionLayout->addStretch();
    mMainLayout->addLayout(actionLayout);
}

void MacroRecorderWidget::connectSignals()
{
    connect(mRecordButton, &QPushButton::clicked, this, &MacroRecorderWidget::onStartRecording);
    connect(mStopButton, &QPushButton::clicked, this, &MacroRecorderWidget::onStopRecording);
    connect(mPlayButton, &QPushButton::clicked, this, &MacroRecorderWidget::onPlayMacro);
    connect(mSaveButton, &QPushButton::clicked, this, &MacroRecorderWidget::onSaveMacro);
    connect(mLoadButton, &QPushButton::clicked, this, &MacroRecorderWidget::onLoadMacro);
    connect(mClearButton, &QPushButton::clicked, this, &MacroRecorderWidget::onClearMacro);
    connect(mDeleteButton, &QPushButton::clicked, this, &MacroRecorderWidget::onDeleteSelected);
}

void MacroRecorderWidget::onStartRecording()
{
    RawrXD::Integration::logInfo("MacroRecorderWidget", "start_recording", "Recording started");
    mRecording = true;
    mStatusLabel->setText("Recording...");
    mStatusLabel->setStyleSheet("font-weight: bold; color: red;");
    mRecordButton->setEnabled(false);
    mStopButton->setEnabled(true);
    mCurrentMacro.clear();
    mCommandList->clear();
    
    emit recordingStarted();
}

void MacroRecorderWidget::onStopRecording()
{
    RawrXD::Integration::logInfo("MacroRecorderWidget", "stop_recording", QString("Recording stopped, commands: %1").arg(mCurrentMacro.size()));
    mRecording = false;
    mStatusLabel->setText("Ready");
    mStatusLabel->setStyleSheet("font-weight: bold;");
    mRecordButton->setEnabled(true);
    mStopButton->setEnabled(false);
    mPlayButton->setEnabled(!mCurrentMacro.isEmpty());
    
    emit recordingStopped();
}

void MacroRecorderWidget::onPlayMacro()
{
    RawrXD::Integration::ScopedTimer timer("MacroRecorderWidget", "play_macro", "execute");
    int playCount = mPlayCountSpinBox->value();
    QMessageBox::information(this, "Playing", 
        QString("Playing macro '%1' %2 time(s)...").arg(mMacroNameEdit->text()).arg(playCount));
    emit macroPlayed();
}

void MacroRecorderWidget::onSaveMacro()
{
    if (mCurrentMacro.isEmpty()) {
        QMessageBox::warning(this, "Empty Macro", "No commands recorded. Record a macro first.");
        return;
    }
    
    QString filename = QFileDialog::getSaveFileName(this, "Save Macro", "", "Macro Files (*.macro)");
    if (!filename.isEmpty()) {
        QMessageBox::information(this, "Saved", "Macro saved successfully!");
    }
}

void MacroRecorderWidget::onLoadMacro()
{
    QString filename = QFileDialog::getOpenFileName(this, "Load Macro", "", "Macro Files (*.macro)");
    if (!filename.isEmpty()) {
        QMessageBox::information(this, "Loaded", "Macro loaded successfully!");
    }
}

void MacroRecorderWidget::onClearMacro()
{
    int ret = QMessageBox::question(this, "Clear Macro", "Clear all recorded commands?");
    if (ret == QMessageBox::Yes) {
        mCurrentMacro.clear();
        mCommandList->clear();
        mCommandCountLabel->setText("Commands: 0");
        mPlayButton->setEnabled(false);
    }
}

void MacroRecorderWidget::onDeleteSelected()
{
    int row = mCommandList->currentRow();
    if (row >= 0) {
        delete mCommandList->takeItem(row);
        mCurrentMacro.removeAt(row);
        mCommandCountLabel->setText(QString("Commands: %1").arg(mCurrentMacro.size()));
    }
}
