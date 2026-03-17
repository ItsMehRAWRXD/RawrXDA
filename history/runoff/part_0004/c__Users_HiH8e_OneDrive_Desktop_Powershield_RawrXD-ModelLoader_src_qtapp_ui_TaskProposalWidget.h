#pragma once

#include <QWidget>
#include <QString>

class QLabel;
class QPushButton;
class QPlainTextEdit;

class TaskProposalWidget : public QWidget {
    Q_OBJECT
public:
    explicit TaskProposalWidget(const QString& taskId,
                                const QString& agentLabel,
                                QWidget* parent = nullptr);

    void appendChunk(const QString& chunk);
    void setStatus(const QString& statusText);
    void updateHeader(const QString& agentType, const QString& statusText);
    QString taskId() const { return taskId_; }

private slots:
    void toggleBody();

private:
    QString taskId_;
    QString agentLabel_;
    QPushButton* headerButton_ {nullptr};
    QLabel* statusLabel_ {nullptr};
    QPlainTextEdit* body_ {nullptr};
    bool expanded_ {true};
};
