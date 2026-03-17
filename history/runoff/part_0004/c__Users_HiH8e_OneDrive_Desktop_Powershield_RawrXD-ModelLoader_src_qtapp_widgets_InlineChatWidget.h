#pragma once
/*  InlineChatWidget.h
    Week 3: Inline AI chat for quick fixes (JetBrains Fleet style)
    Shows popup at cursor with prefilled prompt, streams AI response  */

#include <QWidget>
#include <QPointer>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QTextEdit;
class QPushButton;
class QVBoxLayout;
QT_END_NAMESPACE

class InlineChatWidget : public QWidget
{
    Q_OBJECT
public:
    explicit InlineChatWidget(QWidget* parent = nullptr);
    ~InlineChatWidget();

    void setPrompt(const QString& text);
    QString prompt() const;
    void setPreview(const QString& diff);
    void clear();

signals:
    void accepted(const QString& generatedFix);
    void rejected();
    void promptSubmitted(const QString& prompt);

public slots:
    void show();
    void hide();
    void appendResponse(const QString& chunk);

private slots:
    void handleAccept();
    void handleReject();
    void handleSubmit();
    void syncPalette();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupUi();

private:
    QLineEdit* promptInput_{};
    QTextEdit* previewEdit_{};
    QPushButton* acceptBtn_{};
    QPushButton* rejectBtn_{};
    QPushButton* submitBtn_{};
    QString accumulatedResponse_{};
};
