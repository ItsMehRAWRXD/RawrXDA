// Diff Preview Dock - Day 2 Deliverable
// Shows side-by-side diff with Accept/Reject buttons
#pragma once


class DiffDock : public QDockWidget
{

public:
    explicit DiffDock(void *parent = nullptr);

    void setDiff(const std::string &original, const std::string &suggested);


    void accepted(const std::string &suggested);
    void rejected();

private:
    QTextEdit *m_left;
    QTextEdit *m_right;
    QPushButton *m_acceptBtn;
    QPushButton *m_rejectBtn;
};

