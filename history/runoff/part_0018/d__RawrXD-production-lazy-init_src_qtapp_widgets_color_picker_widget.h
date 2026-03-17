/**
 * @file color_picker_widget.h
 * @brief Full Color Picker Widget implementation for RawrXD IDE
 * @author RawrXD Team
 */

#pragma once

#include <QWidget>
#include <QColor>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <QSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QListWidget>
#include <QTabWidget>
#include <QToolBar>
#include <QSettings>
#include <QTimer>
#include <QImage>

/**
 * @brief Color format types
 */
enum class ColorFormat {
    Hex,        // #RRGGBB or #RRGGBBAA
    RGB,        // rgb(r, g, b) or rgba(r, g, b, a)
    HSL,        // hsl(h, s%, l%) or hsla(h, s%, l%, a)
    HSV,        // hsv(h, s%, v%)
    CMYK,       // cmyk(c%, m%, y%, k%)
    HexShort,   // #RGB
    CSS         // CSS named colors
};

/**
 * @brief Structure for color palette
 */
struct ColorPalette {
    QString name;
    QVector<QColor> colors;
    bool isCustom = false;
};

/**
 * @brief Color wheel widget for hue/saturation selection
 */
class ColorWheel : public QWidget {
    Q_OBJECT

public:
    explicit ColorWheel(QWidget* parent = nullptr);
    
    QColor getColor() const { return m_color; }
    void setColor(const QColor& color);

signals:
    void colorChanged(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateColorFromPosition(const QPoint& pos);
    void rebuildWheel();
    
    QColor m_color;
    QImage m_wheelImage;
    QPoint m_selectorPos;
    int m_wheelWidth = 30;
};

/**
 * @brief Saturation/Value picker square
 */
class ColorSquare : public QWidget {
    Q_OBJECT

public:
    explicit ColorSquare(QWidget* parent = nullptr);
    
    void setHue(int hue);
    QColor getColor() const { return m_color; }
    void setColor(const QColor& color);

signals:
    void colorChanged(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    void updateColorFromPosition(const QPoint& pos);
    void rebuildSquare();
    
    QColor m_color;
    int m_hue = 0;
    QImage m_squareImage;
    QPoint m_selectorPos;
};

/**
 * @brief Alpha/opacity slider
 */
class AlphaSlider : public QWidget {
    Q_OBJECT

public:
    explicit AlphaSlider(QWidget* parent = nullptr);
    
    void setColor(const QColor& color);
    int getAlpha() const { return m_alpha; }
    void setAlpha(int alpha);

signals:
    void alphaChanged(int alpha);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QColor m_color;
    int m_alpha = 255;
};

/**
 * @brief Eyedropper/color picker from screen
 */
class EyeDropper : public QWidget {
    Q_OBJECT

public:
    explicit EyeDropper(QWidget* parent = nullptr);
    
    void startPicking();
    void stopPicking();

signals:
    void colorPicked(const QColor& color);
    void pickingCancelled();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void updatePreview();

private:
    QTimer* m_updateTimer;
    QColor m_currentColor;
    QPoint m_currentPos;
    bool m_isPicking = false;
    QImage m_magnifier;
};

/**
 * @brief Full Color Picker Widget
 * 
 * Features:
 * - Multiple color selection methods (wheel, square, sliders)
 * - Multiple output formats (HEX, RGB, HSL, HSV, CMYK)
 * - Color palettes (Material, Flat UI, Custom)
 * - Recent colors history
 * - Eyedropper tool for picking colors from screen
 * - Color contrast checker (WCAG)
 * - Color harmonies (complementary, triadic, etc.)
 * - Gradient generator
 * - Copy to clipboard in various formats
 */
class ColorPickerWidget : public QWidget {
    Q_OBJECT

public:
    explicit ColorPickerWidget(QWidget* parent = nullptr);
    ~ColorPickerWidget();

    // Color management
    QColor getColor() const { return m_currentColor; }
    void setColor(const QColor& color);
    
    QString getColorString(ColorFormat format = ColorFormat::Hex) const;
    void setColorFromString(const QString& colorStr);
    
    // Format
    void setOutputFormat(ColorFormat format);
    ColorFormat getOutputFormat() const { return m_outputFormat; }
    
    // Alpha
    void setAlphaEnabled(bool enabled);
    bool isAlphaEnabled() const { return m_alphaEnabled; }
    
    // Palettes
    void addPalette(const ColorPalette& palette);
    void removePalette(const QString& name);
    QVector<ColorPalette> getPalettes() const { return m_palettes; }
    
    // Recent colors
    QVector<QColor> getRecentColors() const { return m_recentColors; }
    void clearRecentColors();

signals:
    void colorChanged(const QColor& color);
    void colorSelected(const QColor& color);
    void formatChanged(ColorFormat format);

public slots:
    void copyToClipboard();
    void pasteFromClipboard();
    void startEyeDropper();
    void addToFavorites();
    void generateHarmonies();

private slots:
    void onWheelColorChanged(const QColor& color);
    void onSquareColorChanged(const QColor& color);
    void onAlphaChanged(int alpha);
    void onHexInputChanged();
    void onRgbInputChanged();
    void onHslInputChanged();
    void onFormatChanged(int index);
    void onPaletteColorClicked(QListWidgetItem* item);
    void onRecentColorClicked(QListWidgetItem* item);
    void onEyeDropperColorPicked(const QColor& color);

private:
    void setupUI();
    void setupToolbar();
    void setupColorPickers();
    void setupSliders();
    void setupInputs();
    void setupPalettes();
    void setupHarmonies();
    void connectSignals();
    
    void updateAllFromColor(const QColor& color, QObject* source = nullptr);
    void updatePreview();
    void updateInputs();
    void updateSliders();
    void updateHarmonies();
    void updateContrastInfo();
    
    void addToRecentColors(const QColor& color);
    void loadPalettes();
    void savePalettes();
    void loadRecentColors();
    void saveRecentColors();
    
    QString formatColor(const QColor& color, ColorFormat format) const;
    QColor parseColorString(const QString& str) const;
    double calculateContrast(const QColor& c1, const QColor& c2) const;
    double relativeLuminance(const QColor& color) const;

private:
    // UI Components
    QToolBar* m_toolbar;
    QTabWidget* m_tabWidget;
    
    // Color pickers
    ColorWheel* m_colorWheel;
    ColorSquare* m_colorSquare;
    AlphaSlider* m_alphaSlider;
    EyeDropper* m_eyeDropper;
    
    // Preview
    QLabel* m_colorPreview;
    QLabel* m_contrastLabel;
    
    // Sliders
    QSlider* m_hueSlider;
    QSlider* m_satSlider;
    QSlider* m_valSlider;
    QSlider* m_redSlider;
    QSlider* m_greenSlider;
    QSlider* m_blueSlider;
    
    // SpinBoxes
    QSpinBox* m_hueSpin;
    QSpinBox* m_satSpin;
    QSpinBox* m_valSpin;
    QSpinBox* m_redSpin;
    QSpinBox* m_greenSpin;
    QSpinBox* m_blueSpin;
    QSpinBox* m_alphaSpin;
    
    // Input fields
    QLineEdit* m_hexInput;
    QLineEdit* m_rgbInput;
    QLineEdit* m_hslInput;
    QComboBox* m_formatCombo;
    
    // Palettes
    QListWidget* m_paletteList;
    QComboBox* m_paletteCombo;
    QListWidget* m_recentList;
    QListWidget* m_favoritesList;
    
    // Harmonies
    QWidget* m_harmoniesWidget;
    QLabel* m_complementaryLabel;
    QLabel* m_triadicLabel1;
    QLabel* m_triadicLabel2;
    QLabel* m_analogousLabel1;
    QLabel* m_analogousLabel2;
    
    // State
    QColor m_currentColor;
    ColorFormat m_outputFormat = ColorFormat::Hex;
    bool m_alphaEnabled = true;
    bool m_updatingUI = false;
    
    QVector<ColorPalette> m_palettes;
    QVector<QColor> m_recentColors;
    QVector<QColor> m_favoriteColors;
    
    QSettings* m_settings;
};
