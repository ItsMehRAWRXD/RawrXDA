// Diff Preview Dock - Day 2 Deliverable
// Shows side-by-side diff with Accept/Reject buttons
#pragma once

#include <QDockWidget>
#include <QString>

class QTextEdit;
class QPushButton;

class DiffDock : public QDockWidget
{
    Q_OBJECT
public:
    explicit DiffDock(QWidget *parent = nullptr);

    void setDiff(const QString &original, const QString &suggested);

signals:
    void accepted(const QString &suggested);
    void rejected();

private:
    QTextEdit *m_left;
    QTextEdit *m_right;
    QPushButton *m_acceptBtn;
    QPushButton *m_rejectBtn;
};
