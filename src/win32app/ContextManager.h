// ============================================================================
// Context Manager - 256k Token Window Management
// ============================================================================

#pragma once

#include <string>
#include <deque>
#include <vector>
#include <map>

// ============================================================================
// TOKEN COUNTER
// ============================================================================

class TokenCounter
{
public:
    // Estimate tokens using various heuristics
    static size_t countTokens(const std::string& text)
    {
        // Method 1: Word-based estimation (most common)
        // Average: 1 token = 1.3 words, 1 word ≈ 4.7 chars

        size_t charCount = text.length();
        size_t wordCount = 0;
        bool inWord = false;

        for (char c : text) {
            if (std::isspace(c)) {
                inWord = false;
            } else if (!inWord) {
                wordCount++;
                inWord = true;
            }
        }

        // Estimate tokens
        double tokens = (charCount / 4.7) * 1.3;  // Word-based
        return static_cast<size_t>(tokens) + 1;
    }

    // Count tokens for structured data (JSON, code, etc.)
    static size_t countCodeTokens(const std::string& code)
    {
        // Code typically has more tokens per character
        // More whitespace and punctuation
        size_t baseTokens = countTokens(code);
        return static_cast<size_t>(baseTokens * 1.2);  // 20% more for code
    }

    // Count tokens for markdown/formatted text
    static size_t countMarkdownTokens(const std::string& markdown)
    {
        // Similar to code
        return countCodeTokens(markdown);
    }
};

// ============================================================================
// CONTEXT WINDOW
// ============================================================================

class ContextManager
{
public:
    struct Message {
        std::string sender;     // "Agent" or "User"
        std::string content;
        std::string timestamp;
        size_t tokens;
        std::vector<std::string> files;
    };

    struct ContextSnapshot {
        size_t totalTokens;
        size_t messageCount;
        size_t oldestIndex;
        std::deque<Message> messages;
    };

    ContextManager(size_t maxTokens = 256000)
        : m_maxTokens(maxTokens), m_currentTokens(0), m_messageIndex(0),
          m_prunedMessages(0), m_compressionMode(false)
    {
    }

    // Add message to context
    void addMessage(const std::string& sender, const std::string& content,
                    const std::vector<std::string>& files = {})
    {
        Message msg;
        msg.sender = sender;
        msg.content = content;
        msg.tokens = TokenCounter::countTokens(content);
        msg.files = files;

        // Add timestamp
        time_t now = time(nullptr);
        struct tm timeinfo;
        localtime_s(&timeinfo, &now);
        char buf[32];
        strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
        msg.timestamp = std::string(buf);

        m_messages.push_back(msg);
        m_currentTokens += msg.tokens;

        // Prune old messages if needed
        pruneIfNeeded();
    }

    // Check if context is at capacity
    bool isAtCapacity() const
    {
        return m_currentTokens >= m_maxTokens;
    }

    // Get compression ratio (0.0 = empty, 1.0 = full)
    double getCompressionRatio() const
    {
        return static_cast<double>(m_currentTokens) / static_cast<double>(m_maxTokens);
    }

    // Get remaining tokens
    size_t getRemainingTokens() const
    {
        return m_currentTokens < m_maxTokens ? (m_maxTokens - m_currentTokens) : 0;
    }

    // Get number of messages
    size_t getMessageCount() const
    {
        return m_messages.size();
    }

    // Get current context snapshot
    ContextSnapshot getSnapshot() const
    {
        ContextSnapshot snap;
        snap.totalTokens = m_currentTokens;
        snap.messageCount = m_messages.size();
        snap.oldestIndex = m_messageIndex - m_messages.size();
        snap.messages = m_messages;
        return snap;
    }

    // Get all messages as formatted text
    std::string getContextAsText() const
    {
        std::ostringstream oss;
        for (const auto& msg : m_messages) {
            oss << "[" << msg.timestamp << "] " << msg.sender << ": "
                << msg.content << "\n";

            if (!msg.files.empty()) {
                oss << "  Files: ";
                for (size_t i = 0; i < msg.files.size(); ++i) {
                    if (i > 0) oss << ", ";
                    oss << msg.files[i];
                }
                oss << "\n";
            }
            oss << "\n";
        }
        return oss.str();
    }

    // Get statistics
    std::string getStatistics() const
    {
        std::ostringstream oss;
        oss << "=== Context Window Statistics ===\n";
        oss << "Max Tokens: " << m_maxTokens << "\n";
        oss << "Current Tokens: " << m_currentTokens << "\n";
        oss << "Compression: " << (getCompressionRatio() * 100.0) << "%\n";
        oss << "Messages: " << m_messages.size() << "\n";
        oss << "Pruned Messages: " << m_prunedMessages << "\n";
        oss << "Oldest Message Index: " << (m_messageIndex - m_messages.size()) << "\n";

        if (!m_messages.empty()) {
            size_t minTokens = m_messages[0].tokens;
            size_t maxTokens = m_messages[0].tokens;
            size_t avgTokens = 0;

            for (const auto& msg : m_messages) {
                avgTokens += msg.tokens;
                if (msg.tokens < minTokens) minTokens = msg.tokens;
                if (msg.tokens > maxTokens) maxTokens = msg.tokens;
            }

            avgTokens /= m_messages.size();
            oss << "Token Stats: Min=" << minTokens << " Max=" << maxTokens
                << " Avg=" << avgTokens << "\n";
        }

        return oss.str();
    }

    // Clear context
    void clear()
    {
        m_messages.clear();
        m_currentTokens = 0;
        m_prunedMessages = 0;
    }

    // Set compression mode (aggressive pruning)
    void setCompressionMode(bool enabled)
    {
        m_compressionMode = enabled;
        if (enabled && isAtCapacity()) {
            pruneAggressive();
        }
    }

private:
    void pruneIfNeeded()
    {
        while (m_currentTokens > m_maxTokens && !m_messages.empty()) {
            auto& oldest = m_messages.front();

            if (m_compressionMode) {
                // In compression mode, be more aggressive
                pruneAggressive();
            } else {
                // Normal mode: remove oldest message
                m_currentTokens -= oldest.tokens;
                m_messages.pop_front();
                m_prunedMessages++;
                m_messageIndex++;
            }
        }
    }

    void pruneAggressive()
    {
        // Remove oldest 10% of messages
        size_t toRemove = m_messages.size() / 10;
        if (toRemove < 1) toRemove = 1;

        for (size_t i = 0; i < toRemove && !m_messages.empty(); ++i) {
            auto& msg = m_messages.front();
            m_currentTokens -= msg.tokens;
            m_messages.pop_front();
            m_prunedMessages++;
            m_messageIndex++;
        }
    }

    size_t m_maxTokens;
    size_t m_currentTokens;
    size_t m_messageIndex;
    size_t m_prunedMessages;
    bool m_compressionMode;
    std::deque<Message> m_messages;
};

// ============================================================================
// CONTEXT HISTORY LOG
// ============================================================================

class ContextHistoryLog
{
public:
    ContextHistoryLog(const std::string& logPath)
        : m_logPath(logPath), m_enabled(true)
    {
    }

    void logSnapshot(const ContextManager& ctx)
    {
        if (!m_enabled) return;

        std::ofstream log(m_logPath, std::ios::app);
        if (log.is_open()) {
            log << "=== Context Snapshot ===\n";
            log << ctx.getStatistics();
            log << "\n";
            log.close();
        }
    }

    void logPrune(size_t messagesPruned, size_t tokenFreed)
    {
        if (!m_enabled) return;

        std::ofstream log(m_logPath, std::ios::app);
        if (log.is_open()) {
            log << "[PRUNE] Removed " << messagesPruned << " messages, freed " << tokenFreed << " tokens\n";
            log.close();
        }
    }

    void setEnabled(bool enabled)
    {
        m_enabled = enabled;
    }

private:
    std::string m_logPath;
    bool m_enabled;
};
