/**
 * @file screen_share_widget.h
 * @brief Header for ScreenShareWidget - Screen capture and sharing controls
 */

#pragma once

#include <QWidget>
#include <QScreen>
#include <QPixmap>

class QVBoxLayout;
class QHBoxLayout;
class QPushButton;
class QComboBox;
class QLabel;
class QCheckBox;
class QSpinBox;

class ScreenShareWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit ScreenShareWidget(QWidget* parent = nullptr);
    ~ScreenShareWidget();
    
public slots:
    void onStartSharing();
    void onStopSharing();
    void onScreenSelected(int index);
    void onResolutionChanged(int index);
    void onFrameRateChanged(int fps);
    void onToggleAudio();
    void onToggleCursor();
    void onSelectRegion();
    void onSettingsClicked();
    void onCaptureScreen();
    
signals:
    void sharingStarted(const QString& screenName);
    void sharingStopped();
    void screenCaptured(const QPixmap& pixmap);
    void settingsChanged();
    
private:
    void setupUI();
    void populateScreens();
    void populateResolutions();
    void connectSignals();
    void updateScreenInfo();
    void restoreState();
    void saveState();
    void initializeScreens();
    
    // UI Components
    QVBoxLayout* mMainLayout;
    QHBoxLayout* mScreenLayout;
    QHBoxLayout* mResolutionLayout;
    QHBoxLayout* mFpsLayout;
    QHBoxLayout* mControlsLayout;
    QHBoxLayout* mOptionsLayout;
    
    // Screen selection
    QLabel* mScreenLabel;
    QComboBox* mScreenCombo;
    QLabel* mScreenInfoLabel;
    
    // Resolution settings
    QLabel* mResolutionLabel;
    QComboBox* mResolutionCombo;
    
    // Frame rate
    QLabel* mFpsLabel;
    QSpinBox* mFpsSpinBox;
    
    // Options
    QCheckBox* mAudioCheckbox;
    QCheckBox* mCursorCheckbox;
    
    // Control buttons
    QPushButton* mStartButton;
    QPushButton* mStopButton;
    QPushButton* mRegionButton;
    QPushButton* mCaptureButton;
    QPushButton* mSettingsButton;
    
    // Status display
    QLabel* mStatusLabel;
    QLabel* mBitrateLabel;
    
    // State tracking
    bool mSharingActive;
    QList<QScreen*> mAvailableScreens;
};

#endif // SCREEN_SHARE_WIDGET_H
