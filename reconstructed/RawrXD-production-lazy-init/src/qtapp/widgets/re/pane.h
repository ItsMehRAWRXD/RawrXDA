#pragma once
#include <QWidget>

class REManager;

class REPane : public QWidget {
    Q_OBJECT
public:
    explicit REPane(REManager *manager, QWidget *parent = nullptr);
    void setBinary(const QString &path);
    void setProcess(quint64 pid);
    void showCausalGraphStats();

private:
    REManager *m_manager;
};
