#include "build_system_widget.h"

#include <QVBoxLayout>
#include <QLabel>

BuildSystemWidget::BuildSystemWidget(QWidget* parent)
    : QWidget(parent), m_statusLabel(new QLabel(tr("Build System Panel (stub)"), this))
{
    auto* layout = new QVBoxLayout();
    layout->addWidget(m_statusLabel);
    layout->addStretch();
    setLayout(layout);
}

void BuildSystemWidget::setStatusText(const QString& text)
{
    m_statusLabel->setText(text);
}
