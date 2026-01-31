#ifndef CURSOR_WIDGET_H
#define CURSOR_WIDGET_H

#include <QWidget>
#include <QMap>
#include <QColor>
#include <QString>

// Presence: cursor position, user name, avatar color
class CursorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CursorWidget(QWidget *parent = nullptr);

    struct CursorInfo {
        int position;
        QString userName;
        QColor color;
    };

    // Add or update a cursor
    void updateCursor(const QString &userId, const CursorInfo &info);

    // Remove a cursor
    void removeCursor(const QString &userId);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QMap<QString, CursorInfo> m_cursors;
};

#endif // CURSOR_WIDGET_H