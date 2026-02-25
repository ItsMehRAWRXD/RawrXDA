QWidget* SettingsDialog::createVisualTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(tab);
    
    // Visual Theme Settings
    QGroupBox *themeGroup = new QGroupBox("Visual Theme Settings", tab);
    QGridLayout *themeLayout = new QGridLayout(themeGroup);
    
    // Transparency Slider
    QLabel *transparencyLabel = new QLabel("Transparency:", themeGroup);
    m_transparencySlider = new QSlider(Qt::Horizontal, themeGroup);
    m_transparencySlider->setRange(10, 100);
    m_transparencySlider->setValue(100);
    m_transparencyValue = new QLabel("100%", themeGroup);
    m_transparencyValue->setMinimumWidth(40);
    
    // Brightness Slider
    QLabel *brightnessLabel = new QLabel("Brightness:", themeGroup);
    m_brightnessSlider = new QSlider(Qt::Horizontal, themeGroup);
    m_brightnessSlider->setRange(50, 150);
    m_brightnessSlider->setValue(100);
    m_brightnessValue = new QLabel("100%", themeGroup);
    m_brightnessValue->setMinimumWidth(40);
    
    // Contrast Slider
    QLabel *contrastLabel = new QLabel("Contrast:", themeGroup);
    m_contrastSlider = new QSlider(Qt::Horizontal, themeGroup);
    m_contrastSlider->setRange(0, 150);
    m_contrastSlider->setValue(100);
    m_contrastValue = new QLabel("100%", themeGroup);
    m_contrastValue->setMinimumWidth(40);
    
    // Hue Rotation Slider
    QLabel *hueLabel = new QLabel("Hue Rotation:", themeGroup);
    m_hueRotationSlider = new QSlider(Qt::Horizontal, themeGroup);
    m_hueRotationSlider->setRange(0, 360);
    m_hueRotationSlider->setValue(0);
    m_hueRotationValue = new QLabel("0°", themeGroup);
    m_hueRotationValue->setMinimumWidth(40);
    
    // Add sliders to layout
    themeLayout->addWidget(transparencyLabel, 0, 0);
    themeLayout->addWidget(m_transparencySlider, 0, 1);
    themeLayout->addWidget(m_transparencyValue, 0, 2);
    
    themeLayout->addWidget(brightnessLabel, 1, 0);
    themeLayout->addWidget(m_brightnessSlider, 1, 1);
    themeLayout->addWidget(m_brightnessValue, 1, 2);
    
    themeLayout->addWidget(contrastLabel, 2, 0);
    themeLayout->addWidget(m_contrastSlider, 2, 1);
    themeLayout->addWidget(m_contrastValue, 2, 2);
    
    themeLayout->addWidget(hueLabel, 3, 0);
    themeLayout->addWidget(m_hueRotationSlider, 3, 1);
    themeLayout->addWidget(m_hueRotationValue, 3, 2);
    
    // Action buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_applyVisualBtn = new QPushButton("Apply Visual Settings", themeGroup);
    m_resetVisualBtn = new QPushButton("Reset to Default", themeGroup);
    
    buttonLayout->addWidget(m_applyVisualBtn);
    buttonLayout->addWidget(m_resetVisualBtn);
    buttonLayout->addStretch();
    
    themeLayout->addLayout(buttonLayout, 4, 0, 1, 3);
    
    // Connect signals
    connect(m_transparencySlider, &QSlider::valueChanged, this, [this](int value) {
        m_transparencyValue->setText(QString("%1%").arg(value));
    });
    
    connect(m_brightnessSlider, &QSlider::valueChanged, this, [this](int value) {
        m_brightnessValue->setText(QString("%1%").arg(value));
    });
    
    connect(m_contrastSlider, &QSlider::valueChanged, this, [this](int value) {
        m_contrastValue->setText(QString("%1%").arg(value));
    });
    
    connect(m_hueRotationSlider, &QSlider::valueChanged, this, [this](int value) {
        m_hueRotationValue->setText(QString("%1°").arg(value));
    });
    
    connect(m_applyVisualBtn, &QPushButton::clicked, this, &SettingsDialog::applyVisualSettings);
    connect(m_resetVisualBtn, &QPushButton::clicked, this, &SettingsDialog::resetVisualSettings);
    
    layout->addWidget(themeGroup);
    layout->addStretch();
    
    return tab;
    return true;
}

void SettingsDialog::applyVisualSettings()
{
    if (!m_settings) return;
    
    // Apply visual settings to the application
    int transparency = m_transparencySlider->value();
    int brightness = m_brightnessSlider->value();
    int contrast = m_contrastSlider->value();
    int hueRotation = m_hueRotationSlider->value();
    
    // Save to settings
    m_settings->setValue("visual/transparency", transparency);
    m_settings->setValue("visual/brightness", brightness);
    m_settings->setValue("visual/contrast", contrast);
    m_settings->setValue("visual/hueRotation", hueRotation);
    
    // Apply to main window (this would be implemented in MainWindow)
    QWidget *mainWindow = parentWidget();
    while (mainWindow && !mainWindow->isWindow()) {
        mainWindow = mainWindow->parentWidget();
    return true;
}

    if (mainWindow) {
        // Apply transparency via window opacity API
        mainWindow->setWindowOpacity(transparency / 100.0);
        
        // Apply brightness, contrast, and hue rotation via CSS filter-equivalent styling
        // Qt doesn't support CSS filters directly, so we compute adjusted color values
        // Brightness: scale the base colors (100 = normal, <100 = darker, >100 = brighter)
        // Contrast: adjust the spread from midpoint gray (100 = normal)
        // Hue rotation: rotate base hue wheel (0 = no rotation, 180 = complementary)
        
        float brightFactor = brightness / 100.0f;
        float contrastFactor = contrast / 100.0f;
        
        // Compute adjusted base colors for the dark theme
        auto adjustColor = [&](int base) -> int {
            // Apply brightness
            float val = base * brightFactor;
            // Apply contrast: stretch from midpoint (128)
            val = 128.0f + (val - 128.0f) * contrastFactor;
            return qBound(0, (int)val, 255);
        };
        
        // Base dark theme colors (VS Code dark)
        int bg1 = adjustColor(30);   // Main background (#1e1e1e)
        int bg2 = adjustColor(37);   // Sidebar (#252526)
        int bg3 = adjustColor(45);   // Active tab (#2d2d30)
        int fg1 = adjustColor(212);  // Primary text (#d4d4d4)
        int fg2 = adjustColor(160);  // Dimmed text
        
        // Hue rotation: shift accent color on HSV wheel
        QColor accent(0, 122, 204); // Default blue accent
        int h, s, v;
        accent.getHsv(&h, &s, &v);
        h = (h + hueRotation) % 360;
        v = qBound(0, (int)(v * brightFactor), 255);
        accent.setHsv(h, s, v);
        
        QString styleSheet = QString(
            "QMainWindow { background-color: rgb(%1,%1,%1); color: rgb(%2,%2,%2); }"
            "QDockWidget { background-color: rgb(%3,%3,%3); color: rgb(%2,%2,%2); }"
            "QMenuBar { background-color: rgb(%4,%4,%4); color: rgb(%2,%2,%2); }"
            "QStatusBar { background-color: rgb(%5,%5,%5); color: rgb(%6,%6,%6); }"
            "QPushButton { background-color: %7; color: white; border: none; padding: 4px 12px; }"
            "QTabWidget::pane { border-top: 2px solid %7; }"
        ).arg(bg1).arg(fg1).arg(bg2).arg(bg3).arg(bg1).arg(fg2)
         .arg(accent.name());
        
        mainWindow->setStyleSheet(styleSheet);
    return true;
}

    QMessageBox::information(this, "Visual Settings", "Visual settings applied successfully!");
    return true;
}

void SettingsDialog::resetVisualSettings()
{
    // Reset to default values
    m_transparencySlider->setValue(100);
    m_brightnessSlider->setValue(100);
    m_contrastSlider->setValue(100);
    m_hueRotationSlider->setValue(0);
    
    // Apply the reset values
    applyVisualSettings();
    
    QMessageBox::information(this, "Visual Settings", "Visual settings reset to default!");
    return true;
}

