/**
 * @file code_review.cpp
 * @brief Inline code review and comments implementation
 * Batch 5 - Item 69: Code review
 */

#include "collaboration/code_review.h"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <filesystem>

namespace RawrXD::Collaboration {

CodeReview::CodeReview()
    : m_initialized(false) {
}

CodeReview::~CodeReview() {
    shutdown();
}

bool CodeReview::initialize() {
    m_initialized = true;
    loadThreads();
    return true;
}

void CodeReview::shutdown() {
    if (m_initialized) {
        saveThreads();
        m_initialized = false;
    }
}

std::string CodeReview::addComment(const std::string& filePath,
                                   int startLine, int startColumn,
                                   int endLine, int endColumn,
                                   const std::string& content,
                                   CommentSeverity severity) {
    if (!m_initialized) {
        return "";
    }
    
    // Generate comment ID
    std::string commentId = generateCommentId();
    
    Comment comment;
    comment.id = commentId;
    comment.author = m_currentUser;
    comment.content = content;
    comment.timestamp = std::chrono::system_clock::now();
    comment.status = CommentStatus::Active;
    comment.severity = severity;
    
    // Find or create thread
    std::string threadId = findOrCreateThread(filePath, startLine, startColumn, endLine, endColumn);
    
    auto it = m_threads.find(threadId);
    if (it != m_threads.end()) {
        it->second.comments.push_back(comment);
        it->second.isResolved = false;
    }
    
    if (m_commentCallback) {
        m_commentCallback(comment);
    }
    
    return commentId;
}

void CodeReview::editComment(const std::string& commentId, const std::string& newContent) {
    for (auto& [threadId, thread] : m_threads) {
        for (auto& comment : thread.comments) {
            if (comment.id == commentId) {
                comment.content = newContent;
                comment.edited = std::chrono::system_clock::now();
                
                if (m_commentCallback) {
                    m_commentCallback(comment);
                }
                return;
            }
        }
    }
}

void CodeReview::deleteComment(const std::string& commentId) {
    for (auto& [threadId, thread] : m_threads) {
        auto it = std::remove_if(thread.comments.begin(), thread.comments.end(),
            [&commentId](const Comment& c) { return c.id == commentId; });
        
        if (it != thread.comments.end()) {
            thread.comments.erase(it, thread.comments.end());
            
            // Remove thread if empty
            if (thread.comments.empty()) {
                m_threads.erase(threadId);
            }
            return;
        }
    }
}

void CodeReview::replyToComment(const std::string& commentId, const std::string& content) {
    // Find the thread containing this comment
    for (auto& [threadId, thread] : m_threads) {
        for (const auto& comment : thread.comments) {
            if (comment.id == commentId) {
                // Add reply as new comment in same thread
                std::string replyId = generateCommentId();
                
                Comment reply;
                reply.id = replyId;
                reply.author = m_currentUser;
                reply.content = content;
                reply.timestamp = std::chrono::system_clock::now();
                reply.status = CommentStatus::Active;
                reply.severity = CommentSeverity::Info;
                
                thread.comments.push_back(reply);
                
                if (m_commentCallback) {
                    m_commentCallback(reply);
                }
                return;
            }
        }
    }
}

std::vector<ReviewThread> CodeReview::getThreads(const std::string& filePath) const {
    std::vector<ReviewThread> result;
    
    for (const auto& [id, thread] : m_threads) {
        if (filePath.empty() || thread.filePath == filePath) {
            result.push_back(thread);
        }
    }
    
    return result;
}

std::optional<ReviewThread> CodeReview::getThread(const std::string& threadId) const {
    auto it = m_threads.find(threadId);
    if (it != m_threads.end()) {
        return it->second;
    }
    return std::nullopt;
}

void CodeReview::resolveThread(const std::string& threadId) {
    auto it = m_threads.find(threadId);
    if (it != m_threads.end()) {
        it->second.isResolved = true;
        it->second.resolvedBy = m_currentUser;
        it->second.resolvedAt = std::chrono::system_clock::now();
        
        if (m_threadCallback) {
            m_threadCallback(it->second);
        }
    }
}

void CodeReview::unresolveThread(const std::string& threadId) {
    auto it = m_threads.find(threadId);
    if (it != m_threads.end()) {
        it->second.isResolved = false;
        it->second.resolvedBy.clear();
        
        if (m_threadCallback) {
            m_threadCallback(it->second);
        }
    }
}

void CodeReview::setCommentStatus(const std::string& commentId, CommentStatus status) {
    for (auto& [threadId, thread] : m_threads) {
        for (auto& comment : thread.comments) {
            if (comment.id == commentId) {
                comment.status = status;
                
                if (m_commentCallback) {
                    m_commentCallback(comment);
                }
                return;
            }
        }
    }
}

void CodeReview::setCommentSeverity(const std::string& commentId, CommentSeverity severity) {
    for (auto& [threadId, thread] : m_threads) {
        for (auto& comment : thread.comments) {
            if (comment.id == commentId) {
                comment.severity = severity;
                
                if (m_commentCallback) {
                    m_commentCallback(comment);
                }
                return;
            }
        }
    }
}

void CodeReview::addReaction(const std::string& commentId, const std::string& reaction) {
    for (auto& [threadId, thread] : m_threads) {
        for (auto& comment : thread.comments) {
            if (comment.id == commentId) {
                if (std::find(comment.reactions.begin(), comment.reactions.end(), reaction) == comment.reactions.end()) {
                    comment.reactions.push_back(reaction);
                }
                return;
            }
        }
    }
}

void CodeReview::removeReaction(const std::string& commentId, const std::string& reaction) {
    for (auto& [threadId, thread] : m_threads) {
        for (auto& comment : thread.comments) {
            if (comment.id == commentId) {
                auto it = std::remove(comment.reactions.begin(), comment.reactions.end(), reaction);
                comment.reactions.erase(it, comment.reactions.end());
                return;
            }
        }
    }
}

ReviewSummary CodeReview::getSummary() const {
    ReviewSummary summary = {};
    
    for (const auto& [id, thread] : m_threads) {
        for (const auto& comment : thread.comments) {
            summary.totalComments++;
            
            if (thread.isResolved) {
                summary.resolvedComments++;
            } else {
                summary.activeComments++;
            }
            
            if (comment.severity == CommentSeverity::Blocking) {
                summary.blockingComments++;
            }
            
            summary.commentsByAuthor[comment.author]++;
        }
    }
    
    return summary;
}

ReviewSummary CodeReview::getSummary(const std::string& filePath) const {
    ReviewSummary summary = {};
    
    for (const auto& [id, thread] : m_threads) {
        if (thread.filePath == filePath) {
            for (const auto& comment : thread.comments) {
                summary.totalComments++;
                
                if (thread.isResolved) {
                    summary.resolvedComments++;
                } else {
                    summary.activeComments++;
                }
                
                if (comment.severity == CommentSeverity::Blocking) {
                    summary.blockingComments++;
                }
                
                summary.commentsByAuthor[comment.author]++;
            }
        }
    }
    
    return summary;
}

bool CodeReview::importFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    
    // Simplified JSON parsing
    std::string line;
    while (std::getline(file, line)) {
        // Parse thread
        if (line.find("\"threadId\"") != std::string::npos) {
            ReviewThread thread;
            // Simplified parsing
            m_threads[thread.id] = thread;
        }
    }
    
    return true;
}

bool CodeReview::exportToFile(const std::string& filePath) {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    
    file << "{\n";
    file << "  \"threads\": [\n";
    
    size_t i = 0;
    for (const auto& [id, thread] : m_threads) {
        file << "    {\n";
        file << "      \"id\": \"" << thread.id << "\",\n";
        file << "      \"filePath\": \"" << thread.filePath << "\",\n";
        file << "      \"startLine\": " << thread.startLine << ",\n";
        file << "      \"isResolved\": " << (thread.isResolved ? "true" : "false") << "\n";
        file << "    }";
        if (i < m_threads.size() - 1) file << ",";
        file << "\n";
        i++;
    }
    
    file << "  ]\n";
    file << "}\n";
    
    return true;
}

void CodeReview::setCurrentUser(const std::string& userId, const std::string& userName) {
    m_currentUser = userId;
    m_currentUserName = userName;
}

void CodeReview::onCommentAdded(CommentCallback callback) {
    m_commentCallback = callback;
}

void CodeReview::onThreadUpdated(ThreadCallback callback) {
    m_threadCallback = callback;
}

void CodeReview::onSummaryUpdated(SummaryCallback callback) {
    m_summaryCallback = callback;
}

std::string CodeReview::generateCommentId() {
    static int counter = 0;
    return "comment_" + std::to_string(++counter);
}

std::string CodeReview::generateThreadId() {
    static int counter = 0;
    return "thread_" + std::to_string(++counter);
}

std::string CodeReview::findOrCreateThread(const std::string& filePath,
                                           int startLine, int startColumn,
                                           int endLine, int endColumn) {
    // Check if there's an existing thread at this location
    for (const auto& [id, thread] : m_threads) {
        if (thread.filePath == filePath &&
            thread.startLine == startLine &&
            thread.startColumn == startColumn) {
            return id;
        }
    }
    
    // Create new thread
    std::string threadId = generateThreadId();
    
    ReviewThread thread;
    thread.id = threadId;
    thread.filePath = filePath;
    thread.startLine = startLine;
    thread.startColumn = startColumn;
    thread.endLine = endLine;
    thread.endColumn = endColumn;
    thread.isResolved = false;
    
    m_threads[threadId] = thread;
    
    return threadId;
}

void CodeReview::loadThreads() {
    std::string filePath = "code_review_threads.json";
    if (std::filesystem::exists(filePath)) {
        importFromFile(filePath);
    }
}

void CodeReview::saveThreads() {
    exportToFile("code_review_threads.json");
}

} // namespace RawrXD::Collaboration
