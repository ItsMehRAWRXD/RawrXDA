#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QListWidget>

class WelcomeScreenWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WelcomeScreenWidget(QWidget* parent = nullptr);
    ~WelcomeScreenWidget();

private slots:
    void openProject();
    void createNewProject();

private:
    QListWidget* m_recentProjects;
    QPushButton* m_openButton;
    QPushButton* m_newButton;
};

