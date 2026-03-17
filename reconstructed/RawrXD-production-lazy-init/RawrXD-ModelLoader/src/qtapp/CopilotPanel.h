#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QFrame>

namespace RawrXD {

class CopilotPanel : public QWidget {
    Q_OBJECT

public:
    explicit CopilotPanel(QWidget* parent = nullptr);
    ~CopilotPanel();

    void addSuggestion(const QString& title, const QString& type);

signals:
    void applyCompletion(const QString& completion);
    void refactorSelected(const QString& type);
    void generateTestsSelected();

private:
    QVBoxLayout* m_mainLayout;
    QListWidget* m_suggestionList;
    QPushButton* m_refactorButton;
    QPushButton* m_testButton;
    
    void setupUi();
};

} // namespace RawrXD
