#pragma once
#include <QTreeWidget>
#include <QMenu>
#include <QHash>

/**
 * @brief Widget for per-layer mixed-precision quantization
 * 
 * Displays all model tensors in a tree view and allows
 * right-click selection of quantization type for each layer.
 * This enables Cursor-style mixed precision where critical
 * layers use higher precision (F16, Q8_K) and less important
 * layers use aggressive quantization (Q4_0).
 */
class LayerQuantWidget : public QTreeWidget {
    Q_OBJECT
public:
    explicit LayerQuantWidget(QWidget* parent = nullptr);
    
    /**
     * @brief Add a tensor to the tree
     * @param tensorName Name of the tensor
     * @param defaultQuant Initial quantization mode
     */
    void addTensor(const QString& tensorName, const QString& defaultQuant = "Q4_0");
    
    /**
     * @brief Clear all tensors
     */
    void clearTensors();

signals:
    /**
     * @brief Emitted when user changes quantization for a tensor
     * @param tensorName Name of the tensor
     * @param quant New quantization mode
     */
    void quantChanged(const QString& tensorName, const QString& quant);

private slots:
    void onCustomContextMenu(const QPoint& pos);

private:
    QHash<QString, QTreeWidgetItem*> m_tensorItems;
};
