/**
 * @file accessibility_widget.h
 * @brief Header for AccessibilityWidget - Accessibility settings
 */

#pragma once

#include <QWidget>

class QVBoxLayout;
class QCheckBox;
class QComboBox;
class QLabel;
class QSlider;

class AccessibilityWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit AccessibilityWidget(QWidget* parent = nullptr);
    ~AccessibilityWidget();
    
private slots:
    void onHighContrastToggled();
    void onFontSizeChanged();
    void onThemeChanged();
    
private:
    void setupUI();
    
    QVBoxLayout* mMainLayout;
    QCheckBox* mHighContrastCheckbox;
    QCheckBox* mLargeTextCheckbox;
    QCheckBox* mScreenReaderCheckbox;
    QComboBox* mThemeCombo;
    QSlider* mFontSizeSlider;
    QLabel* mPreviewLabel;
};

#endif // ACCESSIBILITY_WIDGET_H
