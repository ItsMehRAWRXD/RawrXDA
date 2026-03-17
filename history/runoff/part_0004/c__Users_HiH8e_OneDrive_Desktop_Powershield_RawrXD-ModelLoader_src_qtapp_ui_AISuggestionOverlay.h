#pragma once
#include <QWidget>
#include <QPointer>
#include <QTimer>

class QPlainTextEdit;

// AISuggestionOverlay renders ghost text suggestions over an editor.
class AISuggestionOverlay : public QWidget {
    Q_OBJECT
public:
    explicit AISuggestionOverlay(QWidget* parent = nullptr);

    void attachToEditor(QPlainTextEdit* editor);
    void setSuggestionText(const QString& text);
    // Compatibility alias for existing code
    void setGhostText(const QString& text);
    void clearSuggestion();
    void clear();
    void setOpacity(qreal opacity);

signals:
    void accepted(const QString& text);
    void dismissed();

public slots:
    void onStreamChunk(const QString& chunk);
    void onStreamCompleted();
    void onStreamError(const QString& message);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QPointer<QPlainTextEdit> m_editor;
    QString m_text;
    qreal m_opacity{0.35};
    QTimer m_cursorBlinkTimer;
    bool m_cursorOn{true};
};
