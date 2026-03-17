#pragma once

#include <QWidget>

class QLabel;
class QVBoxLayout;

class BuildSystemWidget : public QWidget {
    Q_OBJECT
public:
    explicit BuildSystemWidget(QWidget* parent = nullptr);

    void setStatusText(const QString& text);

private:
    QLabel* m_statusLabel{nullptr};
};
