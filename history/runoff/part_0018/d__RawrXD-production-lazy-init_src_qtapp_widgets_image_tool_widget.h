/**
 * @file image_tool_widget.h
 * @brief Image viewer and editor with basic editing capabilities
 * 
 * Production-ready implementation providing:
 * - Image viewing with zoom and pan
 * - Basic editing (resize, crop, rotate, flip)
 * - Format conversion
 * - Color manipulation
 * - Metadata display
 * - Batch processing
 */

#ifndef IMAGE_TOOL_WIDGET_H
#define IMAGE_TOOL_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QToolBar>
#include <QToolButton>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QLabel>
#include <QSplitter>
#include <QListWidget>
#include <QTreeWidget>
#include <QMenu>
#include <QAction>
#include <QSettings>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>
#include <QThread>
#include <QMutex>
#include <memory>
#include <vector>
#include <functional>

// Forward declarations
class ImageProcessor;

/**
 * @brief Image editing operations
 */
enum class ImageOperation {
    None,
    Crop,
    Resize,
    Rotate,
    FlipHorizontal,
    FlipVertical,
    BrightnessContrast,
    HueSaturation,
    ColorBalance,
    Sharpen,
    Blur,
    NoiseReduction,
    EdgeDetection
};

/**
 * @brief Image format for export
 */
enum class ImageFormat {
    PNG,
    JPEG,
    BMP,
    TIFF,
    GIF,
    WebP,
    ICO
};

/**
 * @brief Image metadata entry
 */
struct ImageMetadata {
    QString key;
    QString value;
    QString category;
};

/**
 * @brief Image processing task for batch operations
 */
struct ImageTask {
    QString inputPath;
    QString outputPath;
    ImageFormat format;
    QList<ImageOperation> operations;
    QVariantMap parameters;
};

/**
 * @brief Custom graphics view for image display
 */
class ImageGraphicsView : public QGraphicsView {
    Q_OBJECT
    
public:
    explicit ImageGraphicsView(QWidget* parent = nullptr);
    
    void setImage(const QImage& image);
    QImage getImage() const;
    
    void zoomIn();
    void zoomOut();
    void zoomToFit();
    void resetZoom();
    void setZoom(qreal factor);
    qreal getZoom() const;
    
    void setSelectionRect(const QRectF& rect);
    QRectF getSelectionRect() const;
    void clearSelection();
    
    // Selection modes
    void setSelectionMode(bool enabled);
    bool isSelectionMode() const;

signals:
    void zoomChanged(qreal factor);
    void selectionChanged(const QRectF& rect);
    void imageClicked(const QPointF& pos);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QGraphicsScene* m_scene;
    QGraphicsPixmapItem* m_pixmapItem;
    QGraphicsRectItem* m_selectionRect;
    
    qreal m_zoomFactor;
    bool m_selectionMode;
    bool m_selecting;
    QPointF m_selectionStart;
    
    static constexpr qreal MIN_ZOOM = 0.1;
    static constexpr qreal MAX_ZOOM = 5.0;
    static constexpr qreal ZOOM_STEP = 0.1;
};

/**
 * @brief Background image processor for heavy operations
 */
class ImageProcessor : public QObject {
    Q_OBJECT
    
public:
    explicit ImageProcessor(QObject* parent = nullptr);
    
    void processImage(const QImage& input, const QList<ImageOperation>& operations, const QVariantMap& params = QVariantMap());
    void processBatch(const QList<ImageTask>& tasks);
    void cancel();
    
signals:
    void processingFinished(const QImage& result);
    void batchProgress(int current, int total, const QString& currentFile);
    void batchFinished();
    void error(const QString& error);

private:
    QImage applyOperation(const QImage& image, ImageOperation op, const QVariantMap& params);
    QImage cropImage(const QImage& image, const QRect& rect);
    QImage resizeImage(const QImage& image, const QSize& size, bool keepAspect = true);
    QImage rotateImage(const QImage& image, qreal angle);
    QImage flipImage(const QImage& image, bool horizontal);
    QImage adjustBrightnessContrast(const QImage& image, int brightness, int contrast);
    QImage adjustHueSaturation(const QImage& image, int hue, int saturation);
    QImage sharpenImage(const QImage& image, qreal factor);
    QImage blurImage(const QImage& image, qreal radius);
    
    bool m_cancelled;
    QMutex m_mutex;
};

/**
 * @class ImageToolWidget
 * @brief Full-featured image viewer and editor
 * 
 * Features:
 * - Image viewing with zoom and pan
 * - Basic editing operations
 * - Format conversion
 * - Metadata display
 * - Batch processing
 * - Undo/redo support
 */
class ImageToolWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit ImageToolWidget(QWidget* parent = nullptr);
    ~ImageToolWidget() override;
    
    // File operations
    bool openImage(const QString& filePath);
    bool saveImage(const QString& filePath);
    void exportImage(const QString& filePath, ImageFormat format);
    
    // Image operations
    void applyOperation(ImageOperation operation, const QVariantMap& params = QVariantMap());
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;
    
    // View operations
    void zoomIn();
    void zoomOut();
    void zoomToFit();
    void resetZoom();
    void setZoom(qreal factor);
    qreal getZoom() const;
    
    // Selection
    void setSelectionMode(bool enabled);
    void cropToSelection();
    void clearSelection();
    
    // Batch processing
    void processBatch(const QList<ImageTask>& tasks);
    void cancelBatch();
    
    // Metadata
    QList<ImageMetadata> getMetadata() const;
    void setMetadata(const QList<ImageMetadata>& metadata);
    
    // Settings
    void saveState(QSettings& settings);
    void restoreState(QSettings& settings);

public slots:
    void refresh();
    void showMetadata();
    void showHistogram();

signals:
    void imageLoaded(const QString& filePath);
    void imageSaved(const QString& filePath);
    void operationApplied(ImageOperation operation);
    void batchProgress(int current, int total, const QString& currentFile);
    void batchFinished();
    void error(const QString& error);

private slots:
    void onOpenAction();
    void onSaveAction();
    void onExportAction();
    void onBatchProcessAction();
    void onZoomChanged(qreal factor);
    void onSelectionChanged(const QRectF& rect);
    void onImageClicked(const QPointF& pos);
    void onProcessingFinished(const QImage& result);
    void onBatchProgress(int current, int total, const QString& currentFile);
    void onBatchFinished();
    void onError(const QString& error);
    void showContextMenu(const QPoint& pos);

private:
    void setupUI();
    void setupToolbar();
    void setupSidebar();
    void setupImageView();
    void setupConnections();
    
    void updateWindowTitle();
    void updateMetadataPanel();
    void updateHistogram();
    void updateUndoRedoActions();
    
    QList<ImageMetadata> extractMetadata(const QString& filePath);
    QString formatToString(ImageFormat format) const;
    ImageFormat stringToFormat(const QString& str) const;
    
    void pushToHistory(const QImage& image);
    QImage getCurrentImage() const;
    
    // UI Components
    QVBoxLayout* m_mainLayout;
    QToolBar* m_toolbar;
    QSplitter* m_splitter;
    
    // Toolbar actions
    QAction* m_openAction;
    QAction* m_saveAction;
    QAction* m_exportAction;
    QAction* m_batchAction;
    QAction* m_undoAction;
    QAction* m_redoAction;
    QAction* m_zoomInAction;
    QAction* m_zoomOutAction;
    QAction* m_zoomFitAction;
    QAction* m_cropAction;
    QAction* m_resizeAction;
    QAction* m_rotateAction;
    QAction* m_flipHAction;
    QAction* m_flipVAction;
    QAction* m_brightnessAction;
    QAction* m_metadataAction;
    QAction* m_histogramAction;
    
    // Toolbar widgets
    QComboBox* m_operationCombo;
    QSlider* m_zoomSlider;
    QLabel* m_zoomLabel;
    QLabel* m_sizeLabel;
    QLabel* m_formatLabel;
    
    // Sidebar
    QTabWidget* m_sidebarTabs;
    QTreeWidget* m_metadataTree;
    QWidget* m_histogramWidget;
    QListWidget* m_historyList;
    
    // Image view
    ImageGraphicsView* m_imageView;
    
    // Data
    QImage m_currentImage;
    QImage m_originalImage;
    QString m_currentFilePath;
    QList<ImageMetadata> m_metadata;
    
    // History for undo/redo
    QList<QImage> m_history;
    int m_historyIndex;
    static constexpr int MAX_HISTORY_SIZE = 50;
    
    // Processing
    std::unique_ptr<ImageProcessor> m_processor;
    QThread* m_processorThread;
    
    // State
    bool m_modified;
    ImageOperation m_currentOperation;
    QRectF m_selectionRect;
    
    // Constants
    static constexpr qreal MIN_ZOOM = 0.1;
    static constexpr qreal MAX_ZOOM = 5.0;
    static constexpr qreal ZOOM_STEP = 0.1;
};

#endif // IMAGE_TOOL_WIDGET_H
