/**
 * @file color_picker_widget.cpp
 * @brief Full Color Picker Widget implementation for RawrXD IDE
 * @author RawrXD Team
 */

#include "color_picker_widget.h"
#include "integration/ProdIntegration.h"
#include "integration/InitializationTracker.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QMenu>
#include <QAction>
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QClipboard>
#include <QScreen>
#include <QRegularExpression>
#include <QtMath>


// =============================================================================
// ColorWheel Implementation
// =============================================================================

ColorWheel::ColorWheel(QWidget* parent)
    : QWidget(parent)
    , m_color(Qt::red)
{
    setMinimumSize(200, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ColorWheel::setColor(const QColor& color) {
    m_color = color;
    
    // Calculate selector position from color
    int h, s, v;
    color.getHsv(&h, &s, &v);
    
    int cx = width() / 2;
    int cy = height() / 2;
    int radius = qMin(cx, cy) - m_wheelWidth / 2 - 5;
    
    double angle = h * M_PI / 180.0;
    m_selectorPos.setX(cx + static_cast<int>(radius * qCos(angle)));
    m_selectorPos.setY(cy - static_cast<int>(radius * qSin(angle)));
    
    update();
}

void ColorWheel::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    int cx = width() / 2;
    int cy = height() / 2;
    int outerRadius = qMin(cx, cy) - 5;
    int innerRadius = outerRadius - m_wheelWidth;
    
    // Draw color wheel
    if (m_wheelImage.isNull() || m_wheelImage.size() != size()) {
        rebuildWheel();
    }
    painter.drawImage(0, 0, m_wheelImage);
    
    // Draw selector
    painter.setPen(QPen(Qt::white, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(m_selectorPos, 8, 8);
    painter.setPen(QPen(Qt::black, 1));
    painter.drawEllipse(m_selectorPos, 9, 9);
}

void ColorWheel::rebuildWheel() {
    m_wheelImage = QImage(size(), QImage::Format_ARGB32);
    m_wheelImage.fill(Qt::transparent);
    
    int cx = width() / 2;
    int cy = height() / 2;
    int outerRadius = qMin(cx, cy) - 5;
    int innerRadius = outerRadius - m_wheelWidth;
    
    for (int y = 0; y < height(); ++y) {
        for (int x = 0; x < width(); ++x) {
            int dx = x - cx;
            int dy = cy - y;
            double dist = qSqrt(dx * dx + dy * dy);
            
            if (dist >= innerRadius && dist <= outerRadius) {
                double angle = qAtan2(dy, dx);
                if (angle < 0) angle += 2 * M_PI;
                
                int hue = static_cast<int>(angle * 180.0 / M_PI);
                QColor color = QColor::fromHsv(hue, 255, 255);
                m_wheelImage.setPixelColor(x, y, color);
            }
        }
    }
}

void ColorWheel::mousePressEvent(QMouseEvent* event) {
    updateColorFromPosition(event->pos());
}

void ColorWheel::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        updateColorFromPosition(event->pos());
    }
}

void ColorWheel::resizeEvent(QResizeEvent*) {
    rebuildWheel();
}

void ColorWheel::updateColorFromPosition(const QPoint& pos) {
    int cx = width() / 2;
    int cy = height() / 2;
    
    int dx = pos.x() - cx;
    int dy = cy - pos.y();
    
    double angle = qAtan2(dy, dx);
    if (angle < 0) angle += 2 * M_PI;
    
    int hue = static_cast<int>(angle * 180.0 / M_PI);
    
    int h, s, v;
    m_color.getHsv(&h, &s, &v);
    m_color.setHsv(hue, s, v);
    
    m_selectorPos = pos;
    update();
    emit colorChanged(m_color);
}

// =============================================================================
// ColorSquare Implementation
// =============================================================================

ColorSquare::ColorSquare(QWidget* parent)
    : QWidget(parent)
    , m_color(Qt::red)
{
    setMinimumSize(200, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ColorSquare::setHue(int hue) {
    m_hue = hue;
    rebuildSquare();
    update();
}

void ColorSquare::setColor(const QColor& color) {
    int h, s, v;
    color.getHsv(&h, &s, &v);
    
    m_color = color;
    m_hue = h;
    
    m_selectorPos.setX(s * width() / 255);
    m_selectorPos.setY((255 - v) * height() / 255);
    
    rebuildSquare();
    update();
}

void ColorSquare::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    
    if (m_squareImage.isNull() || m_squareImage.size() != size()) {
        rebuildSquare();
    }
    painter.drawImage(0, 0, m_squareImage);
    
    // Draw selector
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::white, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(m_selectorPos, 6, 6);
    painter.setPen(QPen(Qt::black, 1));
    painter.drawEllipse(m_selectorPos, 7, 7);
}

void ColorSquare::rebuildSquare() {
    m_squareImage = QImage(size(), QImage::Format_RGB32);
    
    for (int y = 0; y < height(); ++y) {
        for (int x = 0; x < width(); ++x) {
            int s = x * 255 / width();
            int v = 255 - (y * 255 / height());
            QColor color = QColor::fromHsv(m_hue, s, v);
            m_squareImage.setPixelColor(x, y, color);
        }
    }
}

void ColorSquare::mousePressEvent(QMouseEvent* event) {
    updateColorFromPosition(event->pos());
}

void ColorSquare::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        updateColorFromPosition(event->pos());
    }
}

void ColorSquare::updateColorFromPosition(const QPoint& pos) {
    int x = qBound(0, pos.x(), width() - 1);
    int y = qBound(0, pos.y(), height() - 1);
    
    int s = x * 255 / width();
    int v = 255 - (y * 255 / height());
    
    m_color.setHsv(m_hue, s, v);
    m_selectorPos = QPoint(x, y);
    
    update();
    emit colorChanged(m_color);
}

// =============================================================================
// AlphaSlider Implementation
// =============================================================================

AlphaSlider::AlphaSlider(QWidget* parent)
    : QWidget(parent)
    , m_color(Qt::red)
    , m_alpha(255)
{
    setMinimumHeight(20);
    setMaximumHeight(30);
}

void AlphaSlider::setColor(const QColor& color) {
    m_color = color;
    update();
}

void AlphaSlider::setAlpha(int alpha) {
    m_alpha = qBound(0, alpha, 255);
    update();
}

void AlphaSlider::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    
    // Draw checkerboard background
    int gridSize = 8;
    for (int y = 0; y < height(); y += gridSize) {
        for (int x = 0; x < width(); x += gridSize) {
            bool light = ((x / gridSize) + (y / gridSize)) % 2 == 0;
            painter.fillRect(x, y, gridSize, gridSize, light ? Qt::white : Qt::lightGray);
        }
    }
    
    // Draw gradient
    QLinearGradient gradient(0, 0, width(), 0);
    QColor transparent = m_color;
    transparent.setAlpha(0);
    QColor opaque = m_color;
    opaque.setAlpha(255);
    gradient.setColorAt(0, transparent);
    gradient.setColorAt(1, opaque);
    painter.fillRect(rect(), gradient);
    
    // Draw selector
    int selectorX = m_alpha * width() / 255;
    painter.setPen(QPen(Qt::white, 2));
    painter.drawLine(selectorX, 0, selectorX, height());
    painter.setPen(QPen(Qt::black, 1));
    painter.drawLine(selectorX - 1, 0, selectorX - 1, height());
    painter.drawLine(selectorX + 1, 0, selectorX + 1, height());
}

void AlphaSlider::mousePressEvent(QMouseEvent* event) {
    m_alpha = event->x() * 255 / width();
    m_alpha = qBound(0, m_alpha, 255);
    update();
    emit alphaChanged(m_alpha);
}

void AlphaSlider::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        m_alpha = event->x() * 255 / width();
        m_alpha = qBound(0, m_alpha, 255);
        update();
        emit alphaChanged(m_alpha);
    }
}

// =============================================================================
// EyeDropper Implementation
// =============================================================================

EyeDropper::EyeDropper(QWidget* parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
    , m_updateTimer(new QTimer(this))
{
    setFixedSize(120, 120);
    setCursor(Qt::CrossCursor);
    setMouseTracking(true);
    
    connect(m_updateTimer, &QTimer::timeout, this, &EyeDropper::updatePreview);
}

void EyeDropper::startPicking() {
    m_isPicking = true;
    grabMouse();
    grabKeyboard();
    m_updateTimer->start(50);
    show();
}

void EyeDropper::stopPicking() {
    m_isPicking = false;
    releaseMouse();
    releaseKeyboard();
    m_updateTimer->stop();
    hide();
}

void EyeDropper::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit colorPicked(m_currentColor);
        stopPicking();
    } else if (event->button() == Qt::RightButton) {
        emit pickingCancelled();
        stopPicking();
    }
}

void EyeDropper::mouseMoveEvent(QMouseEvent*) {
    updatePreview();
}

void EyeDropper::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        emit pickingCancelled();
        stopPicking();
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        emit colorPicked(m_currentColor);
        stopPicking();
    }
}

void EyeDropper::updatePreview() {
    QPoint globalPos = QCursor::pos();
    m_currentPos = globalPos;
    
    // Get pixel color
    QScreen* screen = QGuiApplication::screenAt(globalPos);
    if (screen) {
        QPixmap pixmap = screen->grabWindow(0, globalPos.x(), globalPos.y(), 1, 1);
        QImage img = pixmap.toImage();
        m_currentColor = img.pixelColor(0, 0);
        
        // Grab magnified area
        QPixmap areaPixmap = screen->grabWindow(0, globalPos.x() - 5, globalPos.y() - 5, 11, 11);
        m_magnifier = areaPixmap.scaled(110, 110, Qt::KeepAspectRatio, Qt::FastTransformation).toImage();
    }
    
    // Position window near cursor
    move(globalPos.x() + 20, globalPos.y() + 20);
    update();
}

void EyeDropper::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw border
    painter.setPen(QPen(Qt::black, 2));
    painter.setBrush(Qt::white);
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 8, 8);
    
    // Draw magnified preview
    if (!m_magnifier.isNull()) {
        painter.drawImage(5, 5, m_magnifier);
    }
    
    // Draw crosshair
    int cx = 60, cy = 60;
    painter.setPen(QPen(Qt::black, 1));
    painter.drawLine(cx - 15, cy, cx + 15, cy);
    painter.drawLine(cx, cy - 15, cx, cy + 15);
    
    // Draw current color
    painter.setPen(Qt::black);
    painter.setBrush(m_currentColor);
    painter.drawRect(5, height() - 20, 50, 15);
    
    // Draw hex value
    painter.setPen(Qt::black);
    painter.drawText(60, height() - 8, m_currentColor.name().toUpper());
}

// =============================================================================
// ColorPickerWidget Implementation
// =============================================================================

ColorPickerWidget::ColorPickerWidget(QWidget* parent)
    : QWidget(parent)
    , m_settings(new QSettings("RawrXD", "IDE", this))
    , m_currentColor(Qt::red)
{
    RawrXD::Integration::ScopedInitTimer init("ColorPickerWidget");
    setupUI();
    connectSignals();
    loadPalettes();
    loadRecentColors();
    updateAllFromColor(m_currentColor);
}

ColorPickerWidget::~ColorPickerWidget() {
    savePalettes();
    saveRecentColors();
}

void ColorPickerWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    
    setupToolbar();
    mainLayout->addWidget(m_toolbar);
    
    m_tabWidget = new QTabWidget(this);
    
    // Picker tab
    QWidget* pickerTab = new QWidget();
    QVBoxLayout* pickerLayout = new QVBoxLayout(pickerTab);
    setupColorPickers();
    setupSliders();
    setupInputs();
    
    QHBoxLayout* pickersLayout = new QHBoxLayout();
    pickersLayout->addWidget(m_colorWheel);
    pickersLayout->addWidget(m_colorSquare);
    pickerLayout->addLayout(pickersLayout);
    
    pickerLayout->addWidget(m_alphaSlider);
    
    // RGB/HSV sliders
    QGroupBox* slidersGroup = new QGroupBox("Adjustments", pickerTab);
    QGridLayout* slidersLayout = new QGridLayout(slidersGroup);
    
    slidersLayout->addWidget(new QLabel("H:"), 0, 0);
    slidersLayout->addWidget(m_hueSlider, 0, 1);
    slidersLayout->addWidget(m_hueSpin, 0, 2);
    
    slidersLayout->addWidget(new QLabel("S:"), 1, 0);
    slidersLayout->addWidget(m_satSlider, 1, 1);
    slidersLayout->addWidget(m_satSpin, 1, 2);
    
    slidersLayout->addWidget(new QLabel("V:"), 2, 0);
    slidersLayout->addWidget(m_valSlider, 2, 1);
    slidersLayout->addWidget(m_valSpin, 2, 2);
    
    slidersLayout->addWidget(new QLabel("R:"), 3, 0);
    slidersLayout->addWidget(m_redSlider, 3, 1);
    slidersLayout->addWidget(m_redSpin, 3, 2);
    
    slidersLayout->addWidget(new QLabel("G:"), 4, 0);
    slidersLayout->addWidget(m_greenSlider, 4, 1);
    slidersLayout->addWidget(m_greenSpin, 4, 2);
    
    slidersLayout->addWidget(new QLabel("B:"), 5, 0);
    slidersLayout->addWidget(m_blueSlider, 5, 1);
    slidersLayout->addWidget(m_blueSpin, 5, 2);
    
    slidersLayout->addWidget(new QLabel("A:"), 6, 0);
    slidersLayout->addWidget(m_alphaSpin, 6, 2);
    
    pickerLayout->addWidget(slidersGroup);
    
    // Input fields
    QGroupBox* inputGroup = new QGroupBox("Values", pickerTab);
    QGridLayout* inputLayout = new QGridLayout(inputGroup);
    
    inputLayout->addWidget(new QLabel("Format:"), 0, 0);
    inputLayout->addWidget(m_formatCombo, 0, 1);
    
    inputLayout->addWidget(new QLabel("HEX:"), 1, 0);
    inputLayout->addWidget(m_hexInput, 1, 1);
    
    inputLayout->addWidget(new QLabel("RGB:"), 2, 0);
    inputLayout->addWidget(m_rgbInput, 2, 1);
    
    inputLayout->addWidget(new QLabel("HSL:"), 3, 0);
    inputLayout->addWidget(m_hslInput, 3, 1);
    
    pickerLayout->addWidget(inputGroup);
    
    // Preview
    QHBoxLayout* previewLayout = new QHBoxLayout();
    m_colorPreview = new QLabel(pickerTab);
    m_colorPreview->setMinimumSize(80, 40);
    m_colorPreview->setAutoFillBackground(true);
    m_colorPreview->setStyleSheet("border: 1px solid #444;");
    
    m_contrastLabel = new QLabel("Contrast: N/A", pickerTab);
    
    previewLayout->addWidget(m_colorPreview);
    previewLayout->addWidget(m_contrastLabel);
    previewLayout->addStretch();
    pickerLayout->addLayout(previewLayout);
    
    m_tabWidget->addTab(pickerTab, "Picker");
    
    // Palettes tab
    setupPalettes();
    
    // Harmonies tab
    setupHarmonies();
    
    mainLayout->addWidget(m_tabWidget);
}

void ColorPickerWidget::setupToolbar() {
    m_toolbar = new QToolBar("Color Toolbar", this);
    
    QPushButton* eyedropperBtn = new QPushButton("🎯 Pick", this);
    eyedropperBtn->setToolTip("Pick color from screen");
    connect(eyedropperBtn, &QPushButton::clicked, this, &ColorPickerWidget::startEyeDropper);
    m_toolbar->addWidget(eyedropperBtn);
    
    QPushButton* copyBtn = new QPushButton("📋 Copy", this);
    copyBtn->setToolTip("Copy color to clipboard");
    connect(copyBtn, &QPushButton::clicked, this, &ColorPickerWidget::copyToClipboard);
    m_toolbar->addWidget(copyBtn);
    
    QPushButton* pasteBtn = new QPushButton("📥 Paste", this);
    pasteBtn->setToolTip("Paste color from clipboard");
    connect(pasteBtn, &QPushButton::clicked, this, &ColorPickerWidget::pasteFromClipboard);
    m_toolbar->addWidget(pasteBtn);
    
    m_toolbar->addSeparator();
    
    QPushButton* favBtn = new QPushButton("⭐ Favorite", this);
    favBtn->setToolTip("Add to favorites");
    connect(favBtn, &QPushButton::clicked, this, &ColorPickerWidget::addToFavorites);
    m_toolbar->addWidget(favBtn);
}

void ColorPickerWidget::setupColorPickers() {
    m_colorWheel = new ColorWheel(this);
    m_colorSquare = new ColorSquare(this);
    m_alphaSlider = new AlphaSlider(this);
    m_eyeDropper = new EyeDropper(this);
}

void ColorPickerWidget::setupSliders() {
    m_hueSlider = new QSlider(Qt::Horizontal, this);
    m_hueSlider->setRange(0, 359);
    m_hueSpin = new QSpinBox(this);
    m_hueSpin->setRange(0, 359);
    
    m_satSlider = new QSlider(Qt::Horizontal, this);
    m_satSlider->setRange(0, 255);
    m_satSpin = new QSpinBox(this);
    m_satSpin->setRange(0, 255);
    
    m_valSlider = new QSlider(Qt::Horizontal, this);
    m_valSlider->setRange(0, 255);
    m_valSpin = new QSpinBox(this);
    m_valSpin->setRange(0, 255);
    
    m_redSlider = new QSlider(Qt::Horizontal, this);
    m_redSlider->setRange(0, 255);
    m_redSpin = new QSpinBox(this);
    m_redSpin->setRange(0, 255);
    
    m_greenSlider = new QSlider(Qt::Horizontal, this);
    m_greenSlider->setRange(0, 255);
    m_greenSpin = new QSpinBox(this);
    m_greenSpin->setRange(0, 255);
    
    m_blueSlider = new QSlider(Qt::Horizontal, this);
    m_blueSlider->setRange(0, 255);
    m_blueSpin = new QSpinBox(this);
    m_blueSpin->setRange(0, 255);
    
    m_alphaSpin = new QSpinBox(this);
    m_alphaSpin->setRange(0, 255);
    m_alphaSpin->setValue(255);
}

void ColorPickerWidget::setupInputs() {
    m_hexInput = new QLineEdit(this);
    m_hexInput->setPlaceholderText("#RRGGBB");
    
    m_rgbInput = new QLineEdit(this);
    m_rgbInput->setPlaceholderText("rgb(r, g, b)");
    
    m_hslInput = new QLineEdit(this);
    m_hslInput->setPlaceholderText("hsl(h, s%, l%)");
    
    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItem("HEX", static_cast<int>(ColorFormat::Hex));
    m_formatCombo->addItem("RGB", static_cast<int>(ColorFormat::RGB));
    m_formatCombo->addItem("HSL", static_cast<int>(ColorFormat::HSL));
    m_formatCombo->addItem("HSV", static_cast<int>(ColorFormat::HSV));
    m_formatCombo->addItem("CMYK", static_cast<int>(ColorFormat::CMYK));
}

void ColorPickerWidget::setupPalettes() {
    QWidget* paletteTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(paletteTab);
    
    m_paletteCombo = new QComboBox(paletteTab);
    layout->addWidget(m_paletteCombo);
    
    m_paletteList = new QListWidget(paletteTab);
    m_paletteList->setViewMode(QListView::IconMode);
    m_paletteList->setIconSize(QSize(32, 32));
    m_paletteList->setSpacing(4);
    m_paletteList->setStyleSheet("QListWidget { background-color: #252526; }");
    layout->addWidget(m_paletteList);
    
    // Recent colors
    QLabel* recentLabel = new QLabel("Recent Colors:", paletteTab);
    layout->addWidget(recentLabel);
    
    m_recentList = new QListWidget(paletteTab);
    m_recentList->setViewMode(QListView::IconMode);
    m_recentList->setIconSize(QSize(24, 24));
    m_recentList->setMaximumHeight(60);
    m_recentList->setSpacing(2);
    m_recentList->setStyleSheet("QListWidget { background-color: #252526; }");
    layout->addWidget(m_recentList);
    
    // Favorites
    QLabel* favLabel = new QLabel("Favorites:", paletteTab);
    layout->addWidget(favLabel);
    
    m_favoritesList = new QListWidget(paletteTab);
    m_favoritesList->setViewMode(QListView::IconMode);
    m_favoritesList->setIconSize(QSize(24, 24));
    m_favoritesList->setMaximumHeight(60);
    m_favoritesList->setSpacing(2);
    m_favoritesList->setStyleSheet("QListWidget { background-color: #252526; }");
    layout->addWidget(m_favoritesList);
    
    m_tabWidget->addTab(paletteTab, "Palettes");
    
    // Load default palettes
    loadPalettes();
}

void ColorPickerWidget::setupHarmonies() {
    m_harmoniesWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_harmoniesWidget);
    
    QGroupBox* compGroup = new QGroupBox("Complementary", m_harmoniesWidget);
    QHBoxLayout* compLayout = new QHBoxLayout(compGroup);
    m_complementaryLabel = new QLabel(m_harmoniesWidget);
    m_complementaryLabel->setMinimumSize(50, 50);
    m_complementaryLabel->setAutoFillBackground(true);
    compLayout->addWidget(m_complementaryLabel);
    layout->addWidget(compGroup);
    
    QGroupBox* triadGroup = new QGroupBox("Triadic", m_harmoniesWidget);
    QHBoxLayout* triadLayout = new QHBoxLayout(triadGroup);
    m_triadicLabel1 = new QLabel(m_harmoniesWidget);
    m_triadicLabel1->setMinimumSize(50, 50);
    m_triadicLabel1->setAutoFillBackground(true);
    m_triadicLabel2 = new QLabel(m_harmoniesWidget);
    m_triadicLabel2->setMinimumSize(50, 50);
    m_triadicLabel2->setAutoFillBackground(true);
    triadLayout->addWidget(m_triadicLabel1);
    triadLayout->addWidget(m_triadicLabel2);
    layout->addWidget(triadGroup);
    
    QGroupBox* analogGroup = new QGroupBox("Analogous", m_harmoniesWidget);
    QHBoxLayout* analogLayout = new QHBoxLayout(analogGroup);
    m_analogousLabel1 = new QLabel(m_harmoniesWidget);
    m_analogousLabel1->setMinimumSize(50, 50);
    m_analogousLabel1->setAutoFillBackground(true);
    m_analogousLabel2 = new QLabel(m_harmoniesWidget);
    m_analogousLabel2->setMinimumSize(50, 50);
    m_analogousLabel2->setAutoFillBackground(true);
    analogLayout->addWidget(m_analogousLabel1);
    analogLayout->addWidget(m_analogousLabel2);
    layout->addWidget(analogGroup);
    
    layout->addStretch();
    
    m_tabWidget->addTab(m_harmoniesWidget, "Harmonies");
}

void ColorPickerWidget::connectSignals() {
    connect(m_colorWheel, &ColorWheel::colorChanged, this, &ColorPickerWidget::onWheelColorChanged);
    connect(m_colorSquare, &ColorSquare::colorChanged, this, &ColorPickerWidget::onSquareColorChanged);
    connect(m_alphaSlider, &AlphaSlider::alphaChanged, this, &ColorPickerWidget::onAlphaChanged);
    
    // Sliders
    connect(m_hueSlider, &QSlider::valueChanged, m_hueSpin, &QSpinBox::setValue);
    connect(m_hueSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_hueSlider, &QSlider::setValue);
    connect(m_hueSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int h) {
        if (!m_updatingUI) {
            int s, v;
            m_currentColor.getHsv(nullptr, &s, &v);
            m_currentColor.setHsv(h, s, v, m_currentColor.alpha());
            updateAllFromColor(m_currentColor, m_hueSpin);
        }
    });
    
    connect(m_satSlider, &QSlider::valueChanged, m_satSpin, &QSpinBox::setValue);
    connect(m_satSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_satSlider, &QSlider::setValue);
    
    connect(m_valSlider, &QSlider::valueChanged, m_valSpin, &QSpinBox::setValue);
    connect(m_valSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_valSlider, &QSlider::setValue);
    
    connect(m_redSlider, &QSlider::valueChanged, m_redSpin, &QSpinBox::setValue);
    connect(m_redSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_redSlider, &QSlider::setValue);
    connect(m_redSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int r) {
        if (!m_updatingUI) {
            m_currentColor.setRed(r);
            updateAllFromColor(m_currentColor, m_redSpin);
        }
    });
    
    connect(m_greenSlider, &QSlider::valueChanged, m_greenSpin, &QSpinBox::setValue);
    connect(m_greenSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_greenSlider, &QSlider::setValue);
    connect(m_greenSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int g) {
        if (!m_updatingUI) {
            m_currentColor.setGreen(g);
            updateAllFromColor(m_currentColor, m_greenSpin);
        }
    });
    
    connect(m_blueSlider, &QSlider::valueChanged, m_blueSpin, &QSpinBox::setValue);
    connect(m_blueSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_blueSlider, &QSlider::setValue);
    connect(m_blueSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int b) {
        if (!m_updatingUI) {
            m_currentColor.setBlue(b);
            updateAllFromColor(m_currentColor, m_blueSpin);
        }
    });
    
    connect(m_alphaSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int a) {
        if (!m_updatingUI) {
            m_currentColor.setAlpha(a);
            updateAllFromColor(m_currentColor, m_alphaSpin);
        }
    });
    
    // Input fields
    connect(m_hexInput, &QLineEdit::editingFinished, this, &ColorPickerWidget::onHexInputChanged);
    connect(m_rgbInput, &QLineEdit::editingFinished, this, &ColorPickerWidget::onRgbInputChanged);
    connect(m_hslInput, &QLineEdit::editingFinished, this, &ColorPickerWidget::onHslInputChanged);
    connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &ColorPickerWidget::onFormatChanged);
    
    // Palette clicks
    connect(m_paletteList, &QListWidget::itemClicked, this, &ColorPickerWidget::onPaletteColorClicked);
    connect(m_recentList, &QListWidget::itemClicked, this, &ColorPickerWidget::onRecentColorClicked);
    connect(m_favoritesList, &QListWidget::itemClicked, this, &ColorPickerWidget::onRecentColorClicked);
    
    // Eyedropper
    connect(m_eyeDropper, &EyeDropper::colorPicked, this, &ColorPickerWidget::onEyeDropperColorPicked);
}

void ColorPickerWidget::setColor(const QColor& color) {
    m_currentColor = color;
    updateAllFromColor(color);
    addToRecentColors(color);
    emit colorChanged(color);
}

// Note: getColor() is implemented inline in header as: return m_currentColor;
// Do not define it here to avoid redefinition

QString ColorPickerWidget::getColorString(ColorFormat format) const {
    return formatColor(m_currentColor, format);
}

void ColorPickerWidget::setColorFromString(const QString& colorStr) {
    QColor color = parseColorString(colorStr);
    if (color.isValid()) {
        setColor(color);
    }
}

void ColorPickerWidget::updateAllFromColor(const QColor& color, QObject* source) {
    m_updatingUI = true;
    
    m_currentColor = color;
    
    if (source != m_colorWheel) m_colorWheel->setColor(color);
    if (source != m_colorSquare) {
        int h, s, v;
        color.getHsv(&h, &s, &v);
        m_colorSquare->setHue(h);
        m_colorSquare->setColor(color);
    }
    m_alphaSlider->setColor(color);
    m_alphaSlider->setAlpha(color.alpha());
    
    updateSliders();
    updateInputs();
    updatePreview();
    updateHarmonies();
    updateContrastInfo();
    
    m_updatingUI = false;
    emit colorChanged(color);
}

void ColorPickerWidget::updateSliders() {
    int h, s, v;
    m_currentColor.getHsv(&h, &s, &v);
    
    m_hueSlider->setValue(h);
    m_satSlider->setValue(s);
    m_valSlider->setValue(v);
    
    m_redSlider->setValue(m_currentColor.red());
    m_greenSlider->setValue(m_currentColor.green());
    m_blueSlider->setValue(m_currentColor.blue());
    m_alphaSpin->setValue(m_currentColor.alpha());
}

void ColorPickerWidget::updateInputs() {
    m_hexInput->setText(m_currentColor.name(QColor::HexRgb).toUpper());
    m_rgbInput->setText(QString("rgb(%1, %2, %3)")
        .arg(m_currentColor.red())
        .arg(m_currentColor.green())
        .arg(m_currentColor.blue()));
    
    int h, s, l;
    m_currentColor.getHsl(&h, &s, &l);
    m_hslInput->setText(QString("hsl(%1, %2%, %3%)")
        .arg(h)
        .arg(s * 100 / 255)
        .arg(l * 100 / 255));
}

void ColorPickerWidget::updatePreview() {
    QPalette pal = m_colorPreview->palette();
    pal.setColor(QPalette::Window, m_currentColor);
    m_colorPreview->setPalette(pal);
}

void ColorPickerWidget::updateHarmonies() {
    int h, s, v;
    m_currentColor.getHsv(&h, &s, &v);
    
    // Complementary
    QColor comp = QColor::fromHsv((h + 180) % 360, s, v);
    QPalette pal;
    pal.setColor(QPalette::Window, comp);
    m_complementaryLabel->setPalette(pal);
    
    // Triadic
    QColor triad1 = QColor::fromHsv((h + 120) % 360, s, v);
    QColor triad2 = QColor::fromHsv((h + 240) % 360, s, v);
    pal.setColor(QPalette::Window, triad1);
    m_triadicLabel1->setPalette(pal);
    pal.setColor(QPalette::Window, triad2);
    m_triadicLabel2->setPalette(pal);
    
    // Analogous
    QColor analog1 = QColor::fromHsv((h + 30) % 360, s, v);
    QColor analog2 = QColor::fromHsv((h + 330) % 360, s, v);
    pal.setColor(QPalette::Window, analog1);
    m_analogousLabel1->setPalette(pal);
    pal.setColor(QPalette::Window, analog2);
    m_analogousLabel2->setPalette(pal);
}

void ColorPickerWidget::updateContrastInfo() {
    double contrastWhite = calculateContrast(m_currentColor, Qt::white);
    double contrastBlack = calculateContrast(m_currentColor, Qt::black);
    
    QString rating;
    double maxContrast = qMax(contrastWhite, contrastBlack);
    if (maxContrast >= 7.0) rating = "AAA";
    else if (maxContrast >= 4.5) rating = "AA";
    else if (maxContrast >= 3.0) rating = "A";
    else rating = "Fail";
    
    m_contrastLabel->setText(QString("Contrast: %1:1 (%2)")
        .arg(maxContrast, 0, 'f', 2).arg(rating));
}

double ColorPickerWidget::calculateContrast(const QColor& c1, const QColor& c2) const {
    double l1 = relativeLuminance(c1);
    double l2 = relativeLuminance(c2);
    
    if (l1 < l2) std::swap(l1, l2);
    return (l1 + 0.05) / (l2 + 0.05);
}

double ColorPickerWidget::relativeLuminance(const QColor& color) const {
    auto srgbToLinear = [](double c) {
        return c <= 0.03928 ? c / 12.92 : qPow((c + 0.055) / 1.055, 2.4);
    };
    
    double r = srgbToLinear(color.redF());
    double g = srgbToLinear(color.greenF());
    double b = srgbToLinear(color.blueF());
    
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

QString ColorPickerWidget::formatColor(const QColor& color, ColorFormat format) const {
    switch (format) {
        case ColorFormat::Hex:
            return color.alpha() == 255 
                ? color.name(QColor::HexRgb).toUpper()
                : color.name(QColor::HexArgb).toUpper();
        
        case ColorFormat::RGB:
            return color.alpha() == 255
                ? QString("rgb(%1, %2, %3)").arg(color.red()).arg(color.green()).arg(color.blue())
                : QString("rgba(%1, %2, %3, %4)").arg(color.red()).arg(color.green()).arg(color.blue())
                    .arg(color.alpha() / 255.0, 0, 'f', 2);
        
        case ColorFormat::HSL: {
            int h, s, l;
            color.getHsl(&h, &s, &l);
            return QString("hsl(%1, %2%, %3%)").arg(h).arg(s * 100 / 255).arg(l * 100 / 255);
        }
        
        case ColorFormat::HSV: {
            int h, s, v;
            color.getHsv(&h, &s, &v);
            return QString("hsv(%1, %2%, %3%)").arg(h).arg(s * 100 / 255).arg(v * 100 / 255);
        }
        
        case ColorFormat::CMYK: {
            int c, m, y, k;
            color.getCmyk(&c, &m, &y, &k);
            return QString("cmyk(%1%, %2%, %3%, %4%)")
                .arg(c * 100 / 255).arg(m * 100 / 255).arg(y * 100 / 255).arg(k * 100 / 255);
        }
        
        default:
            return color.name();
    }
}

QColor ColorPickerWidget::parseColorString(const QString& str) const {
    QString s = str.trimmed();
    
    // HEX
    if (s.startsWith('#')) {
        return QColor(s);
    }
    
    // RGB/RGBA
    QRegularExpression rgbRe(R"(rgba?\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*(?:,\s*([\d.]+))?\s*\))");
    auto match = rgbRe.match(s);
    if (match.hasMatch()) {
        int r = match.captured(1).toInt();
        int g = match.captured(2).toInt();
        int b = match.captured(3).toInt();
        int a = match.captured(4).isEmpty() ? 255 : static_cast<int>(match.captured(4).toDouble() * 255);
        return QColor(r, g, b, a);
    }
    
    // HSL/HSLA
    QRegularExpression hslRe(R"(hsla?\s*\(\s*(\d+)\s*,\s*(\d+)%\s*,\s*(\d+)%\s*(?:,\s*([\d.]+))?\s*\))");
    match = hslRe.match(s);
    if (match.hasMatch()) {
        int h = match.captured(1).toInt();
        int s_val = match.captured(2).toInt() * 255 / 100;
        int l = match.captured(3).toInt() * 255 / 100;
        int a = match.captured(4).isEmpty() ? 255 : static_cast<int>(match.captured(4).toDouble() * 255);
        QColor color;
        color.setHsl(h, s_val, l, a);
        return color;
    }
    
    // Named colors
    return QColor(s);
}

void ColorPickerWidget::addToRecentColors(const QColor& color) {
    m_recentColors.removeAll(color);
    m_recentColors.prepend(color);
    if (m_recentColors.size() > 20) {
        m_recentColors.resize(20);
    }
    
    // Update UI
    m_recentList->clear();
    for (const QColor& c : m_recentColors) {
        QListWidgetItem* item = new QListWidgetItem();
        QPixmap pixmap(24, 24);
        pixmap.fill(c);
        item->setIcon(QIcon(pixmap));
        item->setData(Qt::UserRole, c);
        m_recentList->addItem(item);
    }
}

void ColorPickerWidget::loadPalettes() {
    // Material Design palette
    ColorPalette material;
    material.name = "Material Design";
    material.colors = {
        QColor("#F44336"), QColor("#E91E63"), QColor("#9C27B0"), QColor("#673AB7"),
        QColor("#3F51B5"), QColor("#2196F3"), QColor("#03A9F4"), QColor("#00BCD4"),
        QColor("#009688"), QColor("#4CAF50"), QColor("#8BC34A"), QColor("#CDDC39"),
        QColor("#FFEB3B"), QColor("#FFC107"), QColor("#FF9800"), QColor("#FF5722"),
        QColor("#795548"), QColor("#9E9E9E"), QColor("#607D8B")
    };
    m_palettes.append(material);
    
    // Flat UI palette
    ColorPalette flatUI;
    flatUI.name = "Flat UI";
    flatUI.colors = {
        QColor("#1abc9c"), QColor("#2ecc71"), QColor("#3498db"), QColor("#9b59b6"),
        QColor("#34495e"), QColor("#16a085"), QColor("#27ae60"), QColor("#2980b9"),
        QColor("#8e44ad"), QColor("#2c3e50"), QColor("#f1c40f"), QColor("#e67e22"),
        QColor("#e74c3c"), QColor("#ecf0f1"), QColor("#95a5a6"), QColor("#f39c12"),
        QColor("#d35400"), QColor("#c0392b"), QColor("#bdc3c7"), QColor("#7f8c8d")
    };
    m_palettes.append(flatUI);
    
    // Update combo and list
    m_paletteCombo->clear();
    for (const ColorPalette& pal : m_palettes) {
        m_paletteCombo->addItem(pal.name);
    }
    
    connect(m_paletteCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx >= 0 && idx < m_palettes.size()) {
            m_paletteList->clear();
            for (const QColor& c : m_palettes[idx].colors) {
                QListWidgetItem* item = new QListWidgetItem();
                QPixmap pixmap(32, 32);
                pixmap.fill(c);
                item->setIcon(QIcon(pixmap));
                item->setData(Qt::UserRole, c);
                item->setToolTip(c.name());
                m_paletteList->addItem(item);
            }
        }
    });
    
    m_paletteCombo->setCurrentIndex(0);
}

void ColorPickerWidget::savePalettes() {
    // Save custom palettes to settings
}

void ColorPickerWidget::loadRecentColors() {
    RawrXD::Integration::ScopedTimer timer("ColorPickerWidget", "loadRecentColors", "io");
    QStringList colors = m_settings->value("ColorPicker/RecentColors").toStringList();
    for (const QString& c : colors) {
        m_recentColors.append(QColor(c));
    }
}

void ColorPickerWidget::saveRecentColors() {
    RawrXD::Integration::ScopedTimer timer("ColorPickerWidget", "saveRecentColors", "io");
    QStringList colors;
    for (const QColor& c : m_recentColors) {
        colors.append(c.name(QColor::HexArgb));
    }
    m_settings->setValue("ColorPicker/RecentColors", colors);
}

// Slots
void ColorPickerWidget::onWheelColorChanged(const QColor& color) {
    if (!m_updatingUI) {
        int h, s, v;
        color.getHsv(&h, &s, &v);
        m_colorSquare->setHue(h);
        updateAllFromColor(color, m_colorWheel);
    }
}

void ColorPickerWidget::onSquareColorChanged(const QColor& color) {
    if (!m_updatingUI) {
        updateAllFromColor(color, m_colorSquare);
    }
}

void ColorPickerWidget::onAlphaChanged(int alpha) {
    if (!m_updatingUI) {
        m_currentColor.setAlpha(alpha);
        updateAllFromColor(m_currentColor, m_alphaSlider);
    }
}

void ColorPickerWidget::onHexInputChanged() {
    QColor color(m_hexInput->text());
    if (color.isValid()) {
        setColor(color);
    }
}

void ColorPickerWidget::onRgbInputChanged() {
    QColor color = parseColorString(m_rgbInput->text());
    if (color.isValid()) {
        setColor(color);
    }
}

void ColorPickerWidget::onHslInputChanged() {
    QColor color = parseColorString(m_hslInput->text());
    if (color.isValid()) {
        setColor(color);
    }
}

void ColorPickerWidget::onFormatChanged(int index) {
    m_outputFormat = static_cast<ColorFormat>(m_formatCombo->itemData(index).toInt());
    emit formatChanged(m_outputFormat);
}

void ColorPickerWidget::onPaletteColorClicked(QListWidgetItem* item) {
    QColor color = item->data(Qt::UserRole).value<QColor>();
    setColor(color);
}

void ColorPickerWidget::onRecentColorClicked(QListWidgetItem* item) {
    QColor color = item->data(Qt::UserRole).value<QColor>();
    setColor(color);
}

void ColorPickerWidget::onEyeDropperColorPicked(const QColor& color) {
    setColor(color);
}

void ColorPickerWidget::copyToClipboard() {
    QString colorStr = formatColor(m_currentColor, m_outputFormat);
    QApplication::clipboard()->setText(colorStr);
}

void ColorPickerWidget::pasteFromClipboard() {
    QString text = QApplication::clipboard()->text();
    setColorFromString(text);
}

void ColorPickerWidget::startEyeDropper() {
    m_eyeDropper->startPicking();
}

void ColorPickerWidget::addToFavorites() {
    if (!m_favoriteColors.contains(m_currentColor)) {
        m_favoriteColors.append(m_currentColor);
        
        QListWidgetItem* item = new QListWidgetItem();
        QPixmap pixmap(24, 24);
        pixmap.fill(m_currentColor);
        item->setIcon(QIcon(pixmap));
        item->setData(Qt::UserRole, m_currentColor);
        m_favoritesList->addItem(item);
    }
}

void ColorPickerWidget::generateHarmonies() {
    updateHarmonies();
}

void ColorPickerWidget::setOutputFormat(ColorFormat format) {
    m_outputFormat = format;
    int index = m_formatCombo->findData(static_cast<int>(format));
    if (index >= 0) {
        m_formatCombo->setCurrentIndex(index);
    }
}

void ColorPickerWidget::setAlphaEnabled(bool enabled) {
    m_alphaEnabled = enabled;
    m_alphaSlider->setVisible(enabled);
    m_alphaSpin->setEnabled(enabled);
}

void ColorPickerWidget::addPalette(const ColorPalette& palette) {
    m_palettes.append(palette);
    m_paletteCombo->addItem(palette.name);
}

void ColorPickerWidget::removePalette(const QString& name) {
    for (int i = 0; i < m_palettes.size(); ++i) {
        if (m_palettes[i].name == name && m_palettes[i].isCustom) {
            m_palettes.removeAt(i);
            m_paletteCombo->removeItem(i);
            break;
        }
    }
}

void ColorPickerWidget::clearRecentColors() {
    m_recentColors.clear();
    m_recentList->clear();
}

