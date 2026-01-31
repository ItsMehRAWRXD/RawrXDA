#pragma once


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

public:
    explicit LayerQuantWidget(void* parent = nullptr);
    
    /**
     * @brief Add a tensor to the tree
     * @param tensorName Name of the tensor
     * @param defaultQuant Initial quantization mode
     */
    void addTensor(const std::string& tensorName, const std::string& defaultQuant = "Q4_0");
    
    /**
     * @brief Clear all tensors
     */
    void clearTensors();

    /**
     * @brief Emitted when user changes quantization for a tensor
     * @param tensorName Name of the tensor
     * @param quant New quantization mode
     */
    void quantChanged(const std::string& tensorName, const std::string& quant);

private:
    void onCustomContextMenu(const QPoint& pos);

private:
    std::unordered_map<std::string, QTreeWidgetItem*> m_tensorItems;
};

