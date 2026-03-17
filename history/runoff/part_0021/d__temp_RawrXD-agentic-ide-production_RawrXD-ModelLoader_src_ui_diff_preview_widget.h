// Diff Preview Widget - Shows code changes with accept/reject controls
// Production-ready implementation following AI Toolkit guidelines
#pragma once

#include <QWidget>
#include <QString>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <functional>

namespace RawrXD {

struct DiffChange {
    QString filePath;
    QString originalContent;
    QString proposedContent;
    int startLine{0};
    int endLine{0};
    QString changeDescription;
};

class DiffPreviewWidget : public QWidget {
    Q_OBJECT

public:
    explicit DiffPreviewWidget(QWidget* parent = nullptr);
    ~DiffPreviewWidget() override = default;

    // Show a diff for review
    void showDiff(const DiffChange& change);
    
    // Clear current diff
    void clear();
    
    // Set callbacks for accept/reject actions
    void setAcceptCallback(std::function<void(const DiffChange&)> callback);
    void setRejectCallback(std::function<void(const DiffChange&)> callback);

signals:
    void diffAccepted(const DiffChange& change);
    void diffRejected(const DiffChange& change);
    void closed();

private slots:
    void onAcceptClicked();
    void onRejectClicked();
    void onAcceptAllClicked();
    void onRejectAllClicked();

private:
    void setupUI();
    void renderDiff();
    QString generateUnifiedDiff(const QString& original, const QString& proposed);
    
    // UI Components
    QLabel* m_fileLabel{nullptr};
    QLabel* m_descriptionLabel{nullptr};
    QTextEdit* m_diffDisplay{nullptr};
    QPushButton* m_acceptButton{nullptr};
    QPushButton* m_rejectButton{nullptr};
    QPushButton* m_acceptAllButton{nullptr};
    QPushButton* m_rejectAllButton{nullptr};
    
    // State
    DiffChange m_currentDiff;
    bool m_hasPendingDiff{false};
    
    // Callbacks
    std::function<void(const DiffChange&)> m_acceptCallback;
    std::function<void(const DiffChange&)> m_rejectCallback;
};

} // namespace RawrXD
