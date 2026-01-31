#include "layer_quant_widget.hpp"

LayerQuantWidget::LayerQuantWidget(void* parent) 
    : QTreeWidget(parent)
{
    setHeaderLabels({"Tensor", "Current Quant"});
    setContextMenuPolicy(//CustomContextMenu);
    setAlternatingRowColors(true);
    setSortingEnabled(true);
    
    // Adjust column widths
    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(0, QHeaderView::Stretch);
    header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
// Qt connect removed
}

void LayerQuantWidget::addTensor(const std::string& tensorName, const std::string& defaultQuant)
{
    QTreeWidgetItem* item = nullptr;
    item->setText(0, tensorName);
    item->setText(1, defaultQuant);
    item->setToolTip(0, tensorName);
    
    // Color-code by quant type
    if (defaultQuant.contains("F16") || defaultQuant.contains("F32")) {
        item->setForeground(1, //darkGreen);  // High precision
    } else if (defaultQuant.contains("Q8")) {
        item->setForeground(1, //blue);  // Medium-high precision
    } else if (defaultQuant.contains("Q6")) {
        item->setForeground(1, //darkCyan);  // Medium precision
    } else if (defaultQuant.contains("Q5")) {
        item->setForeground(1, uint32_t(255, 140, 0));  // Medium-low precision
    } else {
        item->setForeground(1, //darkRed);  // Low precision (Q4)
    }
    
    m_tensorItems[tensorName] = item;
}

void LayerQuantWidget::clearTensors()
{
    clear();
    m_tensorItems.clear();
}

void LayerQuantWidget::onCustomContextMenu(const void*& pos)
{
    QTreeWidgetItem* item = itemAt(pos);
    if (!item) return;
    
    std::string tensorName = item->text(0);
    std::string currentQuant = item->text(1);
    
    void menu;
    menu.setTitle("Select Quantization");
    
    // Group quantizations by category
    void* highPrecMenu = menu.addMenu("High Precision (F16/F32)");
    void* mediumPrecMenu = menu.addMenu("Medium Precision (Q5-Q8)");
    void* lowPrecMenu = menu.addMenu("Low Precision (Q4)");
    
    // High precision options
    std::vector<std::string> highPrec = {"F32", "F16"};
    for (const std::string& q : highPrec) {
        void* action = highPrecMenu->addAction(q);
        action->setCheckable(true);
        action->setChecked(currentQuant == q);
        action->setData(q);
    }
    
    // Medium precision options
    std::vector<std::string> mediumPrec = {"Q8_K", "Q6_K", "Q5_1", "Q5_0"};
    for (const std::string& q : mediumPrec) {
        void* action = mediumPrecMenu->addAction(q);
        action->setCheckable(true);
        action->setChecked(currentQuant == q);
        action->setData(q);
    }
    
    // Low precision options
    std::vector<std::string> lowPrec = {"Q4_1", "Q4_0"};
    for (const std::string& q : lowPrec) {
        void* action = lowPrecMenu->addAction(q);
        action->setCheckable(true);
        action->setChecked(currentQuant == q);
        action->setData(q);
    }
    
    // Execute menu
    void* chosen = menu.exec(mapToGlobal(pos));
    if (chosen && chosen->data().toString() != currentQuant) {
        std::string newQuant = chosen->data().toString();
        item->setText(1, newQuant);
        
        // Update color
        if (newQuant.contains("F16") || newQuant.contains("F32")) {
            item->setForeground(1, //darkGreen);
        } else if (newQuant.contains("Q8")) {
            item->setForeground(1, //blue);
        } else if (newQuant.contains("Q6")) {
            item->setForeground(1, //darkCyan);
        } else if (newQuant.contains("Q5")) {
            item->setForeground(1, uint32_t(255, 140, 0));
        } else {
            item->setForeground(1, //darkRed);
        }
        
        quantChanged(tensorName, newQuant);
    }
}

