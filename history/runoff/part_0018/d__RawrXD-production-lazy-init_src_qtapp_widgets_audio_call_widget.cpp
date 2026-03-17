/**
 * @file audio_call_widget.cpp
 * @brief Implementation of AudioCallWidget - Audio call interface
 */

#include "audio_call_widget.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QProgressBar>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>
#include <QDebug>
#include <QDateTime>
#ifdef HAVE_QT_MULTIMEDIA
#include <QtMultimedia/QMediaDevices>
#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QMediaPlayer>
#endif

AudioCallWidget::AudioCallWidget(QWidget* parent)
    : QWidget(parent), mMicEnabled(true), mSpeakerEnabled(true), mCallActive(false),
      mCallStartTime(0)
{
    setupUI();
    createDeviceSelectors();
    initializeAudioDevices();
    connectSignals();
    restoreState();
    
    setWindowTitle("Audio Call");
}

AudioCallWidget::~AudioCallWidget()
{
    saveState();
#ifdef HAVE_QT_MULTIMEDIA
    if (mMediaPlayer) {
        mMediaPlayer->stop();
        delete mMediaPlayer;
    }
#endif
}

void AudioCallWidget::setupUI()
{
    mMainLayout = new QVBoxLayout(this);
    
    // Status display
    mStatusLabel = new QLabel("Ready", this);
    mStatusLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    mMainLayout->addWidget(mStatusLabel);
    
    // Call duration
    mCallDurationLabel = new QLabel("00:00:00", this);
    mCallDurationLabel->setAlignment(Qt::AlignCenter);
    mMainLayout->addWidget(mCallDurationLabel);
    
    // Signal strength
    mSignalStrengthBar = new QProgressBar(this);
    mSignalStrengthBar->setRange(0, 100);
    mSignalStrengthBar->setValue(75);
    mMainLayout->addWidget(new QLabel("Signal Strength:", this));
    mMainLayout->addWidget(mSignalStrengthBar);
    
    mMainLayout->addSpacing(10);
    
    // Call control buttons
    mControlsLayout = new QHBoxLayout();
    
    mStartCallButton = new QPushButton("Start Call", this);
    mStartCallButton->setStyleSheet("background-color: #4CAF50; color: white; padding: 10px; font-weight: bold;");
    mControlsLayout->addWidget(mStartCallButton);
    
    mEndCallButton = new QPushButton("End Call", this);
    mEndCallButton->setStyleSheet("background-color: #f44336; color: white; padding: 10px; font-weight: bold;");
    mEndCallButton->setEnabled(false);
    mControlsLayout->addWidget(mEndCallButton);
    
    mMainLayout->addLayout(mControlsLayout);
    
    // Microphone and Speaker toggles
    QHBoxLayout* toggleLayout = new QHBoxLayout();
    
    mMicButton = new QPushButton("Mic: ON", this);
    mMicButton->setCheckable(true);
    mMicButton->setChecked(true);
    mMicButton->setStyleSheet("background-color: #2196F3; color: white; padding: 8px;");
    toggleLayout->addWidget(mMicButton);
    
    mSpeakerButton = new QPushButton("Speaker: ON", this);
    mSpeakerButton->setCheckable(true);
    mSpeakerButton->setChecked(true);
    mSpeakerButton->setStyleSheet("background-color: #2196F3; color: white; padding: 8px;");
    toggleLayout->addWidget(mSpeakerButton);
    
    mMainLayout->addLayout(toggleLayout);
    
    // Volume control
    mVolumeLayout = new QHBoxLayout();
    mVolumeLabel = new QLabel("Volume:", this);
    mVolumeLayout->addWidget(mVolumeLabel);
    
    mVolumeSlider = new QSlider(Qt::Horizontal, this);
    mVolumeSlider->setRange(0, 100);
    mVolumeSlider->setValue(70);
    mVolumeLayout->addWidget(mVolumeSlider);
    
    mVolumeValueLabel = new QLabel("70%", this);
    mVolumeLayout->addWidget(mVolumeValueLabel);
    
    mMainLayout->addLayout(mVolumeLayout);
    
    mMainLayout->addSpacing(15);
    
    // Device selection
    mDeviceLayout = new QHBoxLayout();
    
    mInputDeviceLabel = new QLabel("Input:", this);
    mDeviceLayout->addWidget(mInputDeviceLabel);
    mInputDeviceCombo = new QComboBox(this);
    mDeviceLayout->addWidget(mInputDeviceCombo);
    
    mDeviceLayout->addSpacing(15);
    
    mOutputDeviceLabel = new QLabel("Output:", this);
    mDeviceLayout->addWidget(mOutputDeviceLabel);
    mOutputDeviceCombo = new QComboBox(this);
    mDeviceLayout->addWidget(mOutputDeviceCombo);
    
    mDeviceLayout->addSpacing(15);
    
    mSettingsButton = new QPushButton("Settings", this);
    mSettingsButton->setStyleSheet("padding: 8px;");
    mDeviceLayout->addWidget(mSettingsButton);
    
    mMainLayout->addLayout(mDeviceLayout);
    
    mMainLayout->addStretch();
}

void AudioCallWidget::createDeviceSelectors()
{
    // Initialize audio devices (currently mock implementation)
    mInputDeviceCombo->addItem("Built-in Microphone");
    mInputDeviceCombo->addItem("Headset Microphone");
    mInputDeviceCombo->addItem("USB Microphone");
    
    mOutputDeviceCombo->addItem("Built-in Speaker");
    mOutputDeviceCombo->addItem("Headphones");
    mOutputDeviceCombo->addItem("USB Speakers");
}

void AudioCallWidget::connectSignals()
{
    connect(mStartCallButton, &QPushButton::clicked, this, &AudioCallWidget::onStartCall);
    connect(mEndCallButton, &QPushButton::clicked, this, &AudioCallWidget::onEndCall);
    connect(mMicButton, &QPushButton::toggled, this, &AudioCallWidget::onMicToggle);
    connect(mSpeakerButton, &QPushButton::toggled, this, &AudioCallWidget::onSpeakerToggle);
    connect(mVolumeSlider, &QSlider::valueChanged, this, &AudioCallWidget::onVolumeChanged);
    connect(mInputDeviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioCallWidget::onInputDeviceChanged);
    connect(mOutputDeviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioCallWidget::onOutputDeviceChanged);
    connect(mSettingsButton, &QPushButton::clicked, this, &AudioCallWidget::onShowDeviceSettings);
}

void AudioCallWidget::onStartCall()
{
    mCallActive = true;
    mCallStartTime = QDateTime::currentMSecsSinceEpoch();
    mStatusLabel->setText("Call in progress...");
    mStatusLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: green;");
    mStartCallButton->setEnabled(false);
    mEndCallButton->setEnabled(true);
    mInputDeviceCombo->setEnabled(false);
    mOutputDeviceCombo->setEnabled(false);
    
    emit callStarted("user@example.com");
}

void AudioCallWidget::onEndCall()
{
    mCallActive = false;
    mStatusLabel->setText("Call ended");
    mStatusLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: red;");
    mStartCallButton->setEnabled(true);
    mEndCallButton->setEnabled(false);
    mInputDeviceCombo->setEnabled(true);
    mOutputDeviceCombo->setEnabled(true);
    mCallDurationLabel->setText("00:00:00");
    
    emit callEnded("User ended call");
}

void AudioCallWidget::onMicToggle()
{
    mMicEnabled = mMicButton->isChecked();
    mMicButton->setText(mMicEnabled ? "Mic: ON" : "Mic: OFF");
    mMicButton->setStyleSheet(mMicEnabled ? "background-color: #2196F3; color: white; padding: 8px;" : 
                                            "background-color: #757575; color: white; padding: 8px;");
    emit micStateChanged(mMicEnabled);
}

void AudioCallWidget::onSpeakerToggle()
{
    mSpeakerEnabled = mSpeakerButton->isChecked();
    mSpeakerButton->setText(mSpeakerEnabled ? "Speaker: ON" : "Speaker: OFF");
    mSpeakerButton->setStyleSheet(mSpeakerEnabled ? "background-color: #2196F3; color: white; padding: 8px;" : 
                                                     "background-color: #757575; color: white; padding: 8px;");
    emit speakerStateChanged(mSpeakerEnabled);
}

void AudioCallWidget::onVolumeChanged(int volume)
{
    mVolumeValueLabel->setText(QString::number(volume) + "%");
    emit volumeChanged(volume);
}

void AudioCallWidget::onInputDeviceChanged(int index)
{
    qDebug() << "Input device changed to:" << mInputDeviceCombo->itemText(index);
}

void AudioCallWidget::onOutputDeviceChanged(int index)
{
    qDebug() << "Output device changed to:" << mOutputDeviceCombo->itemText(index);
}

void AudioCallWidget::onShowDeviceSettings()
{
    QMessageBox::information(this, "Device Settings", 
        "Current Settings:\n\n"
        "Input Device: " + mInputDeviceCombo->currentText() + "\n"
        "Output Device: " + mOutputDeviceCombo->currentText() + "\n"
        "Volume: " + mVolumeValueLabel->text() + "\n"
        "Microphone: " + (mMicEnabled ? "Enabled" : "Disabled") + "\n"
        "Speaker: " + (mSpeakerEnabled ? "Enabled" : "Disabled"));
}

void AudioCallWidget::onDurationChanged(qint64 duration)
{
    // Update UI with audio duration formatted as HH:MM:SS
    int hours = duration / 3600000;        // milliseconds to hours
    int minutes = (duration % 3600000) / 60000;
    int seconds = (duration % 60000) / 1000;
    
    QString durationStr = QString("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
    
    if (mCallDurationLabel) {
        mCallDurationLabel->setText(durationStr);
    }
}

void AudioCallWidget::onPositionChanged(qint64 position)
{
    if (mCallActive) {
        qint64 elapsed = (QDateTime::currentMSecsSinceEpoch() - mCallStartTime) / 1000;
        int hours = elapsed / 3600;
        int minutes = (elapsed % 3600) / 60;
        int seconds = elapsed % 60;
        mCallDurationLabel->setText(QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0')));
    }
}

// Implementation of onStateChanged with int parameter (Qt 6 API uses int instead of PlaybackState enum)
void AudioCallWidget::onStateChanged(int state)
{
    // Update UI based on playback state
    // Note: In Qt 6, PlaybackState enum values are: 0=StoppedState, 1=PlayingState, 2=PausedState
    switch (state) {
        case 1: // PlayingState
            mStatusLabel->setText("Playing...");
            break;
            
        case 2: // PausedState
            mStatusLabel->setText("Paused");
            break;
            
        case 0: // StoppedState
        default:
            mStatusLabel->setText("Stopped");
            break;
    }
}

void AudioCallWidget::updateDeviceList()
{
#ifdef HAVE_QT_MULTIMEDIA
    // Query system for actual audio devices using QMediaDevices
    if (mInputDeviceCombo) {
        mInputDeviceCombo->clear();
        
        // Get default input device
        QAudioDevice defaultInput = QMediaDevices::defaultAudioInput();
        if (!defaultInput.isNull()) {
            mInputDeviceCombo->addItem(defaultInput.description());
        }
        
        // Add all available input devices
        for (const QAudioDevice& device : QMediaDevices::audioInputs()) {
            QString description = device.description();
            if (description != defaultInput.description()) {
                mInputDeviceCombo->addItem(description);
            }
        }
    }
    
    if (mOutputDeviceCombo) {
        mOutputDeviceCombo->clear();
        
        // Get default output device
        QAudioDevice defaultOutput = QMediaDevices::defaultAudioOutput();
        if (!defaultOutput.isNull()) {
            mOutputDeviceCombo->addItem(defaultOutput.description());
        }
        
        // Add all available output devices
        for (const QAudioDevice& device : QMediaDevices::audioOutputs()) {
            QString description = device.description();
            if (description != defaultOutput.description()) {
                mOutputDeviceCombo->addItem(description);
            }
        }
    }
#else
    // Qt Multimedia not available - add placeholder devices
    if (mInputDeviceCombo) {
        mInputDeviceCombo->clear();
        mInputDeviceCombo->addItem("Default Input Device");
    }
    if (mOutputDeviceCombo) {
        mOutputDeviceCombo->clear();
        mOutputDeviceCombo->addItem("Default Output Device");
    }
#endif
}

void AudioCallWidget::restoreState()
{
    QSettings settings("RawrXD", "IDE");
    int volume = settings.value("audio/volume", 70).toInt();
    mVolumeSlider->setValue(volume);
    
    QString inputDevice = settings.value("audio/inputDevice", "Built-in Microphone").toString();
    int inputIndex = mInputDeviceCombo->findText(inputDevice);
    if (inputIndex >= 0) mInputDeviceCombo->setCurrentIndex(inputIndex);
    
    QString outputDevice = settings.value("audio/outputDevice", "Built-in Speaker").toString();
    int outputIndex = mOutputDeviceCombo->findText(outputDevice);
    if (outputIndex >= 0) mOutputDeviceCombo->setCurrentIndex(outputIndex);
}

void AudioCallWidget::saveState()
{
    QSettings settings("RawrXD", "IDE");
    settings.setValue("audio/volume", mVolumeSlider->value());
    settings.setValue("audio/inputDevice", mInputDeviceCombo->currentText());
    settings.setValue("audio/outputDevice", mOutputDeviceCombo->currentText());
}

void AudioCallWidget::initializeAudioDevices()
{
    // Initialize QAudioDeviceInfo enumeration for real device detection
    updateDeviceList();
    
    // Set up default audio format
    // Note: In Qt 6, QAudioFormat is used but we don't store it as a member
    // as this is mainly for device enumeration
}