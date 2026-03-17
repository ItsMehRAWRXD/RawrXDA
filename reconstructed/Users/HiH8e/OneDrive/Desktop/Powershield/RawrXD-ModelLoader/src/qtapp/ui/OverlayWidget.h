#pragma once
#include <QWidget>
#include <QString>

class OverlayWidget : public QWidget {
    Q_OBJECT
public:
    explicit OverlayWidget(QWidget* parent = nullptr);

    void setGhostText(const QString& text);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QString ghostText_;
};
