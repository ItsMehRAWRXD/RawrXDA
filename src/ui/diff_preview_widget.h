// Diff Preview Widget - Shows code changes with accept/reject controls
// Production-ready implementation following AI Toolkit guidelines
#pragma once


#include <functional>

namespace RawrXD {

struct DiffChange {
    std::string filePath;
    std::string originalContent;
    std::string proposedContent;
    int startLine{0};
    int endLine{0};
    std::string changeDescription;
};

class DiffPreviewWidget : public void {

public:
    explicit DiffPreviewWidget(void* parent = nullptr);
    ~DiffPreviewWidget() override = default;

    // Show a diff for review
    void showDiff(const DiffChange& change);
    
    // Clear current diff
    void clear();
    
    // Set callbacks for accept/reject actions
    void setAcceptCallback(std::function<void(const DiffChange&)> callback);
    void setRejectCallback(std::function<void(const DiffChange&)> callback);


    void diffAccepted(const DiffChange& change);
    void diffRejected(const DiffChange& change);
    void closed();

private:
    void onAcceptClicked();
    void onRejectClicked();
    void onAcceptAllClicked();
    void onRejectAllClicked();

private:
    void setupUI();
    void renderDiff();
    std::string generateUnifiedDiff(const std::string& original, const std::string& proposed);
    
    // UI Components
    void* m_fileLabel{nullptr};
    void* m_descriptionLabel{nullptr};
    void* m_diffDisplay{nullptr};
    void* m_acceptButton{nullptr};
    void* m_rejectButton{nullptr};
    void* m_acceptAllButton{nullptr};
    void* m_rejectAllButton{nullptr};
    
    // State
    DiffChange m_currentDiff;
    bool m_hasPendingDiff{false};
    
    // Callbacks
    std::function<void(const DiffChange&)> m_acceptCallback;
    std::function<void(const DiffChange&)> m_rejectCallback;
};

} // namespace RawrXD

