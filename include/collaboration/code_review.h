#pragma once
/**
 * @file code_review.h
 * @brief Inline code review and comments
 * Batch 5 - Item 69: Code review
 */

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <chrono>

namespace RawrXD::Collaboration {

enum class CommentStatus {
    Active,
    Resolved,
    Dismissed
};

enum class CommentSeverity {
    Info,
    Suggestion,
    Warning,
    Blocking
};

struct Comment {
    std::string id;
    std::string author;
    std::string authorAvatar;
    std::string content;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::system_clock::time_point edited;
    CommentStatus status;
    CommentSeverity severity;
    std::vector<Comment> replies;
    std::vector<std::string> reactions;
};

struct ReviewThread {
    std::string id;
    std::string filePath;
    int startLine;
    int startColumn;
    int endLine;
    int endColumn;
    std::vector<Comment> comments;
    bool isResolved;
    std::string resolvedBy;
    std::chrono::system_clock::time_point resolvedAt;
};

struct ReviewSummary {
    int totalComments;
    int resolvedComments;
    int activeComments;
    int blockingComments;
    std::map<std::string, int> commentsByAuthor;
};

class CodeReview {
public:
    CodeReview();
    ~CodeReview();

    // Initialization
    bool initialize();
    void shutdown();

    // Comments
    std::string addComment(const std::string& filePath,
                          int startLine, int startColumn,
                          int endLine, int endColumn,
                          const std::string& content,
                          CommentSeverity severity = CommentSeverity::Suggestion);
    void editComment(const std::string& commentId, const std::string& newContent);
    void deleteComment(const std::string& commentId);
    void replyToComment(const std::string& commentId, const std::string& content);

    // Threads
    std::vector<ReviewThread> getThreads(const std::string& filePath = "") const;
    std::optional<ReviewThread> getThread(const std::string& threadId) const;
    void resolveThread(const std::string& threadId);
    void unresolveThread(const std::string& threadId);

    // Status
    void setCommentStatus(const std::string& commentId, CommentStatus status);
    void setCommentSeverity(const std::string& commentId, CommentSeverity severity);

    // Reactions
    void addReaction(const std::string& commentId, const std::string& reaction);
    void removeReaction(const std::string& commentId, const std::string& reaction);

    // Summary
    ReviewSummary getSummary() const;
    ReviewSummary getSummary(const std::string& filePath) const;

    // Import/Export
    bool exportReview(const std::string& path);
    bool importReview(const std::string& path);
    std::string serializeReview() const;
    bool deserializeReview(const std::string& data);

    // Events
    using CommentCallback = std::function<void(const Comment&)>;
    using ThreadCallback = std::function<void(const ReviewThread&)>;
    void onCommentAdded(CommentCallback callback);
    void onCommentEdited(CommentCallback callback);
    void onCommentDeleted(std::function<void(const std::string&)> callback);
    void onThreadResolved(ThreadCallback callback);

private:
    std::map<std::string, ReviewThread> m_threads;
    std::map<std::string, std::string> m_commentToThread;
    uint32_t m_nextId{1};

    CommentCallback m_addedCallback;
    CommentCallback m_editedCallback;
    std::function<void(const std::string&)> m_deletedCallback;
    ThreadCallback m_resolvedCallback;

    std::string generateId();
    void notifyCommentAdded(const Comment& comment);
    void notifyCommentEdited(const Comment& comment);
    void notifyCommentDeleted(const std::string& commentId);
    void notifyThreadResolved(const ReviewThread& thread);
};

// Global instance
CodeReview& getCodeReview();

} // namespace RawrXD::Collaboration
