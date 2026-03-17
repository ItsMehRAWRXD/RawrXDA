/**
 * @file audio_call_widget.h
 * @brief Header for AudioCallWidget - Audio call interface with controls
 */

#pragma once

#include <QWidget>
// #include <QAudioDecoder>  // Qt Multimedia not available
// #include <QMediaPlayer>

class QAudioDecoder;
class QMediaPlayer;

class QVBoxLayout;
class QHBoxLayout;
class QPushButton;
class QSlider;
class QLabel;
class QComboBox;
class QProgressBar;

class AudioCallWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit AudioCallWidget(QWidget* parent = nullptr);
    ~AudioCallWidget();
    
public slots:
    void onStartCall();
    void onEndCall();
    void onMicToggle();
    void onSpeakerToggle();
    void onVolumeChanged(int volume);
    void onInputDeviceChanged(int index);
    void onOutputDeviceChanged(int index);
    void onShowDeviceSettings();
    void onDurationChanged(qint64 duration);
    void onPositionChanged(qint64 position);
    void onStateChanged(int state);  // Use int instead of QMediaPlayer::PlaybackState
    
signals:
    void callStarted(const QString& contactId);
    void callEnded(const QString& reason);
    void micStateChanged(bool enabled);
    void speakerStateChanged(bool enabled);
    void volumeChanged(int volume);
    
private:
    void setupUI();
    void createDeviceSelectors();
    void connectSignals();
    void updateDeviceList();
    void restoreState();
    void saveState();
    void initializeAudioDevices();
    
    // UI Components
    QVBoxLayout* mMainLayout;
    QHBoxLayout* mControlsLayout;
    QHBoxLayout* mVolumeLayout;
    QHBoxLayout* mDeviceLayout;
    
    // Call controls
    QPushButton* mStartCallButton;
    QPushButton* mEndCallButton;
    QPushButton* mMicButton;
    QPushButton* mSpeakerButton;
    QPushButton* mSettingsButton;
    
    // Volume control
    QLabel* mVolumeLabel;
    QSlider* mVolumeSlider;
    QLabel* mVolumeValueLabel;
    
    // Device selection
    QLabel* mInputDeviceLabel;
    QComboBox* mInputDeviceCombo;
    QLabel* mOutputDeviceLabel;
    QComboBox* mOutputDeviceCombo;
    
    // Status display
    QLabel* mStatusLabel;
    QLabel* mCallDurationLabel;
    QProgressBar* mSignalStrengthBar;
    
    // Audio components
    QMediaPlayer* mMediaPlayer;
    
    // State tracking
    bool mMicEnabled;
    bool mSpeakerEnabled;
    bool mCallActive;
    qint64 mCallStartTime;
};

