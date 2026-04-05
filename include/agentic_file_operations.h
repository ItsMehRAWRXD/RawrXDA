#ifndef AGENTIC_FILE_OPERATIONS_H
#define AGENTIC_FILE_OPERATIONS_H

// C++20 / Win32 — no Qt. File operation approval workflow with callbacks.

#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <fstream>
#include <iterator>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

// ============ Action types (replaces Qt dialog) ============

enum class AgenticFileActionType
{
    CREATE_FILE,
    MODIFY_FILE,
    DELETE_FILE
};

// ============ AgenticFileOperations — callback-based, no QObject ============

struct FileActionRecord
{
    std::string filePath;
    AgenticFileActionType actionType = AgenticFileActionType::CREATE_FILE;
    std::string content;
    std::string oldContent;
    std::chrono::system_clock::time_point timestamp{};
};

class AgenticFileOperations
{
public:
    using ApprovalCallback = std::function<bool(const std::string& filePath, AgenticFileActionType type, const std::string* content)>;
    using NotifyCallback   = std::function<void(const std::string& filePath)>;

    AgenticFileOperations() = default;

    void setApprovalCallback(ApprovalCallback cb) { m_approvalCb = std::move(cb); }
    void setOnFileCreated(NotifyCallback cb)     { m_onCreated  = std::move(cb); }
    void setOnFileModified(NotifyCallback cb)    { m_onModified = std::move(cb); }
    void setOnFileDeleted(NotifyCallback cb)     { m_onDeleted  = std::move(cb); }
    void setOnOperationUndone(NotifyCallback cb) { m_onUndone   = std::move(cb); }
    void setOnOperationCancelled(NotifyCallback cb) { m_onCancelled = std::move(cb); }

    void createFileWithApproval(const std::string& filePath, const std::string& content);
    void modifyFileWithApproval(const std::string& filePath, const std::string& newContent);
    void deleteFileWithApproval(const std::string& filePath);

    void undoLastAction();
    bool canUndo() const { return !m_actionHistory.empty(); }
    size_t getHistorySize() const { return m_actionHistory.size(); }

#ifdef _WIN32
    void setParentWindow(HWND hwnd) { m_parentHwnd = hwnd; }
#endif

private:
    bool requestApproval(const std::string& filePath, AgenticFileActionType type, const std::string* content);

    static constexpr size_t MAX_HISTORY = 50;
    std::vector<FileActionRecord> m_actionHistory;
    ApprovalCallback m_approvalCb;
    NotifyCallback   m_onCreated;
    NotifyCallback   m_onModified;
    NotifyCallback   m_onDeleted;
    NotifyCallback   m_onUndone;
    NotifyCallback   m_onCancelled;
#ifdef _WIN32
    HWND m_parentHwnd = nullptr;
#endif
};

inline bool AgenticFileOperations::requestApproval(const std::string& filePath, AgenticFileActionType type, const std::string* content)
{
    if (m_approvalCb)
        return m_approvalCb(filePath, type, content);
#ifdef _WIN32
    std::wstring msg = L"Allow file operation?\nPath: ";
    msg += std::wstring(filePath.begin(), filePath.end());
    return MessageBoxW(m_parentHwnd, msg.c_str(), L"Agentic File Operation", MB_YESNO | MB_ICONQUESTION) == IDYES;
#else
    (void)filePath; (void)type; (void)content;
    return true;
#endif
}

inline void AgenticFileOperations::createFileWithApproval(const std::string& filePath, const std::string& content)
{
    if (!requestApproval(filePath, AgenticFileActionType::CREATE_FILE, &content)) {
        if (m_onCancelled) m_onCancelled(filePath);
        return;
    }
    std::ofstream f(filePath);
    if (f) f << content;
    if (m_actionHistory.size() < MAX_HISTORY)
        m_actionHistory.push_back({ filePath, AgenticFileActionType::CREATE_FILE, content, {}, std::chrono::system_clock::now() });
    if (m_onCreated) m_onCreated(filePath);
}

inline void AgenticFileOperations::modifyFileWithApproval(const std::string& filePath, const std::string& newContent)
{
    std::string oldContent;
    { std::ifstream f(filePath); if (f) oldContent.assign(std::istreambuf_iterator<char>(f), {}); }
    if (!requestApproval(filePath, AgenticFileActionType::MODIFY_FILE, &newContent)) {
        if (m_onCancelled) m_onCancelled(filePath);
        return;
    }
    std::ofstream f(filePath);
    if (f) f << newContent;
    if (m_actionHistory.size() < MAX_HISTORY)
        m_actionHistory.push_back({ filePath, AgenticFileActionType::MODIFY_FILE, newContent, oldContent, std::chrono::system_clock::now() });
    if (m_onModified) m_onModified(filePath);
}

inline void AgenticFileOperations::deleteFileWithApproval(const std::string& filePath)
{
    std::string oldContent;
    {
        std::ifstream f(filePath);
        if (f) oldContent.assign(std::istreambuf_iterator<char>(f), {});
    }

    if (!requestApproval(filePath, AgenticFileActionType::DELETE_FILE, nullptr)) {
        if (m_onCancelled) m_onCancelled(filePath);
        return;
    }
    std::remove(filePath.c_str());
    if (m_actionHistory.size() < MAX_HISTORY)
        m_actionHistory.push_back({ filePath, AgenticFileActionType::DELETE_FILE, {}, oldContent, std::chrono::system_clock::now() });
    if (m_onDeleted) m_onDeleted(filePath);
}

inline void AgenticFileOperations::undoLastAction()
{
    if (m_actionHistory.empty()) return;
    FileActionRecord& last = m_actionHistory.back();
    if (last.actionType == AgenticFileActionType::MODIFY_FILE && !last.oldContent.empty()) {
        std::ofstream f(last.filePath);
        if (f) f << last.oldContent;
    } else if (last.actionType == AgenticFileActionType::CREATE_FILE) {
        std::remove(last.filePath.c_str());
    } else if (last.actionType == AgenticFileActionType::DELETE_FILE) {
        std::ofstream f(last.filePath);
        if (f) f << last.oldContent;
    }
    if (m_onUndone) m_onUndone(last.filePath);
    m_actionHistory.pop_back();
}

#endif // AGENTIC_FILE_OPERATIONS_H
