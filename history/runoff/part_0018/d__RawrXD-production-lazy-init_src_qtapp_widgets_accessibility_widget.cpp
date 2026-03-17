/**
 * @file accessibility_widget.cpp
 * @brief Implementation of AccessibilityWidget - Accessibility settings
 */

#include "accessibility_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QSlider>
#include <QGroupBox>
#include <QApplication>
#include <QSettings>
#include <QPalette>
#include <QFile>
#include <QDebug>

AccessibilityWidget::AccessibilityWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    setWindowTitle("Accessibility Settings");
}

AccessibilityWidget::~AccessibilityWidget() = default;

void AccessibilityWidget::setupUI()
{
    mMainLayout = new QVBoxLayout(this);
    
    QGroupBox* visualGroup = new QGroupBox("Visual", this);
    QVBoxLayout* visualLayout = new QVBoxLayout(visualGroup);
    
    mHighContrastCheckbox = new QCheckBox("High Contrast Mode", this);
    visualLayout->addWidget(mHighContrastCheckbox);
    
    mLargeTextCheckbox = new QCheckBox("Large Text", this);
    visualLayout->addWidget(mLargeTextCheckbox);
    
    visualLayout->addWidget(new QLabel("Theme:", this));
    mThemeCombo = new QComboBox(this);
    mThemeCombo->addItems({"Light", "Dark", "High Contrast"});
    visualLayout->addWidget(mThemeCombo);
    
    visualLayout->addWidget(new QLabel("Font Size:", this));
    mFontSizeSlider = new QSlider(Qt::Horizontal, this);
    mFontSizeSlider->setRange(8, 24);
    mFontSizeSlider->setValue(12);
    visualLayout->addWidget(mFontSizeSlider);
    
    mMainLayout->addWidget(visualGroup);
    
    QGroupBox* audioGroup = new QGroupBox("Audio", this);
    QVBoxLayout* audioLayout = new QVBoxLayout(audioGroup);
    
    mScreenReaderCheckbox = new QCheckBox("Enable Screen Reader", this);
    audioLayout->addWidget(mScreenReaderCheckbox);
    
    mMainLayout->addWidget(audioGroup);
    
    mPreviewLabel = new QLabel("Preview text with selected settings", this);
    mMainLayout->addWidget(new QLabel("Preview:", this));
    mMainLayout->addWidget(mPreviewLabel);
    
    mMainLayout->addStretch();
}

void AccessibilityWidget::onHighContrastToggled()
{
    // Apply high contrast mode to the entire application
    bool enabled = mHighContrastCheckbox->isChecked();
    
    QPalette highContrastPalette;
    if (enabled) {
        // High contrast color scheme: white on black
        highContrastPalette.setColor(QPalette::Window, Qt::black);
        highContrastPalette.setColor(QPalette::WindowText, Qt::white);
        highContrastPalette.setColor(QPalette::Base, Qt::black);
        highContrastPalette.setColor(QPalette::AlternateBase, Qt::darkGray);
        highContrastPalette.setColor(QPalette::ToolTipBase, Qt::black);
        highContrastPalette.setColor(QPalette::ToolTipText, Qt::white);
        highContrastPalette.setColor(QPalette::Text, Qt::white);
        highContrastPalette.setColor(QPalette::Button, Qt::black);
        highContrastPalette.setColor(QPalette::ButtonText, Qt::white);
        highContrastPalette.setColor(QPalette::Link, Qt::cyan);
        highContrastPalette.setColor(QPalette::Highlight, Qt::white);
        highContrastPalette.setColor(QPalette::HighlightedText, Qt::black);
    } else {
        // Reset to default system palette
        highContrastPalette = style()->standardPalette();
    }
    
    QApplication::setPalette(highContrastPalette);
    
    // Update preview
    mPreviewLabel->setText(enabled ? "High Contrast Mode: ON" : "High Contrast Mode: OFF");
    mPreviewLabel->setStyleSheet(enabled ? "color: white; background-color: black; padding: 10px;" : "");
    
    // Save setting
    QSettings settings("RawrXD", "Accessibility");
    settings.setValue("highContrast", enabled);
}

void AccessibilityWidget::onFontSizeChanged()
{
    // Get font size value from slider (range: 8-20 pt)
    int fontSize = mFontSizeSlider->value();
    
    // Create new font with adjusted point size
    QFont appFont = QApplication::font();
    appFont.setPointSize(fontSize);
    
    // Apply to entire application
    QApplication::setFont(appFont);
    
    // Update preview label
    mPreviewLabel->setText(QString("Font Size: %1 pt").arg(fontSize));
    mPreviewLabel->setFont(appFont);
    
    // Save setting
    QSettings settings("RawrXD", "Accessibility");
    settings.setValue("fontSize", fontSize);
}

void AccessibilityWidget::onThemeChanged()
{
    // Get selected theme from combo box
    QString theme = mThemeCombo->currentText();
    
    QString qssFileName;
    if (theme == "Dark") {
        qssFileName = ":/styles/dark.qss";
    } else if (theme == "Light") {
        qssFileName = ":/styles/light.qss";
    } else if (theme == "High Contrast") {
        qssFileName = ":/styles/high_contrast.qss";
    } else {
        qssFileName = ":/styles/default.qss";
    }
    
    // Load QSS stylesheet
    QFile styleFile(qssFileName);
    if (styleFile.open(QFile::ReadOnly)) {
        QString style = QLatin1String(styleFile.readAll());
        qApp->setStyleSheet(style);
        styleFile.close();
        
        // Update preview
        mPreviewLabel->setText(QString("Theme: %1").arg(theme));
    } else {
        qWarning() << "Could not load theme file:" << qssFileName;
    }
    
    // Save setting
    QSettings settings("RawrXD", "Accessibility");
    settings.setValue("theme", theme);
}
