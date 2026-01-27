#include "layer_quant_widget.hpp"
#include <QHeaderView>

LayerQuantWidget::LayerQuantWidget(QWidget* parent) 
    : QTreeWidget(parent)
{
    setHeaderLabels({"Tensor", "Current Quant"});
    setContextMenuPolicy(Qt::CustomContextMenu);
    setAlternatingRowColors(true);
    setSortingEnabled(true);
    
    // Adjust column widths
    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(0, QHeaderView::Stretch);
    header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    
    connect(this, &QTreeWidget::customContextMenuRequested,
            this, &LayerQuantWidget::onCustomContextMenu);
}

void LayerQuantWidget::addTensor(const QString& tensorName, const QString& defaultQuant)
{
    QTreeWidgetItem* item = new QTreeWidgetItem(this);
    item->setText(0, tensorName);
    item->setText(1, defaultQuant);
    item->setToolTip(0, tensorName);
    
    // Color-code by quant type
    if (defaultQuant.contains("F16") || defaultQuant.contains("F32")) {
        item->setForeground(1, Qt::darkGreen);  // High precision
    } else if (defaultQuant.contains("Q8")) {
        item->setForeground(1, Qt::blue);  // Medium-high precision
    } else if (defaultQuant.contains("Q6")) {
        item->setForeground(1, Qt::darkCyan);  // Medium precision
    } else if (defaultQuant.contains("Q5")) {
        item->setForeground(1, QColor(255, 140, 0));  // Medium-low precision
    } else {
        item->setForeground(1, Qt::darkRed);  // Low precision (Q4)
    }
    
    m_tensorItems[tensorName] = item;
}

void LayerQuantWidget::clearTensors()
{
    clear();
    m_tensorItems.clear();
}

void LayerQuantWidget::onCustomContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = itemAt(pos);
    if (!item) return;
    
    QString tensorName = item->text(0);
    QString currentQuant = item->text(1);
    
    QMenu menu;
    menu.setTitle("Select Quantization");
    
    // Group quantizations by category
    QMenu* highPrecMenu = menu.addMenu("High Precision (F16/F32)");
    QMenu* mediumPrecMenu = menu.addMenu("Medium Precision (Q5-Q8)");
    QMenu* lowPrecMenu = menu.addMenu("Low Precision (Q4)");
    
    // High precision options
    QStringList highPrec = {"F32", "F16"};
    for (const QString& q : highPrec) {
        QAction* action = highPrecMenu->addAction(q);
        action->setCheckable(true);
        action->setChecked(currentQuant == q);
        action->setData(q);
    }
    
    // Medium precision options
    QStringList mediumPrec = {"Q8_K", "Q6_K", "Q5_1", "Q5_0"};
    for (const QString& q : mediumPrec) {
        QAction* action = mediumPrecMenu->addAction(q);
        action->setCheckable(true);
        action->setChecked(currentQuant == q);
        action->setData(q);
    }
    
    // Low precision options
    QStringList lowPrec = {"Q4_1", "Q4_0"};
    for (const QString& q : lowPrec) {
        QAction* action = lowPrecMenu->addAction(q);
        action->setCheckable(true);
        action->setChecked(currentQuant == q);
        action->setData(q);
    }
    
    // Execute menu
    QAction* chosen = menu.exec(mapToGlobal(pos));
    if (chosen && chosen->data().toString() != currentQuant) {
        QString newQuant = chosen->data().toString();
        item->setText(1, newQuant);
        
        // Update color
        if (newQuant.contains("F16") || newQuant.contains("F32")) {
            item->setForeground(1, Qt::darkGreen);
        } else if (newQuant.contains("Q8")) {
            item->setForeground(1, Qt::blue);
        } else if (newQuant.contains("Q6")) {
            item->setForeground(1, Qt::darkCyan);
        } else if (newQuant.contains("Q5")) {
            item->setForeground(1, QColor(255, 140, 0));
        } else {
            item->setForeground(1, Qt::darkRed);
        }
        
        emit quantChanged(tensorName, newQuant);
    }
}
