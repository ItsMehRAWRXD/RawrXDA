/**
 * @file merge_tool.cpp
 * @brief Three-way merge conflict resolution implementation
 * Batch 5 - Item 63: Merge tool
 */

#include "git/merge_tool.h"
#include <sstream>
#include <algorithm>
#include <regex>

namespace RawrXD::Git {

MergeTool::MergeTool() 
    : m_hwnd(nullptr)
    , m_parentHwnd(nullptr)
    , m_currentConflict(0)
    , m_showBase(true)
    , m_showOurs(true)
    , m_showTheirs(true)
    , m_showResult(true)
    , m_syncScrolling(true) {
}

MergeTool::~MergeTool() {
    shutdown();
}

bool MergeTool::initialize(HWND parent) {
    m_parentHwnd = parent;
    
    // Register window class
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "RawrXDMergeTool";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    
    RegisterClassEx(&wc);
    
    // Create window
    m_hwnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        "RawrXDMergeTool",
        "Merge Tool",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL,
        0, 0, 800, 600,
        parent,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );
    
    return m_hwnd != nullptr;
}

void MergeTool::shutdown() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    UnregisterClass("RawrXDMergeTool", GetModuleHandle(nullptr));
}

HWND MergeTool::getHandle() const {
    return m_hwnd;
}

void MergeTool::resize(int width, int height) {
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
    }
}

void MergeTool::setMerge(const std::string& base, const std::string& ours, const std::string& theirs) {
    m_state.baseContent = base;
    m_state.oursContent = ours;
    m_state.theirsContent = theirs;
    m_state.resultContent = "";
    m_state.conflicts.clear();
    m_state.currentConflict = 0;
    m_state.resolvedConflicts = 0;
    
    parseConflicts();
    
    m_state.totalConflicts = static_cast<int>(m_state.conflicts.size());
    m_currentConflict = 0;
}

void MergeTool::setResult(const std::string& result) {
    m_state.resultContent = result;
}

std::string MergeTool::getResult() const {
    if (m_state.resultContent.empty()) {
        return generateResult();
    }
    return m_state.resultContent;
}

void MergeTool::clear() {
    m_state = MergeState{};
    m_currentConflict = 0;
}

void MergeTool::nextConflict() {
    if (m_currentConflict < m_state.totalConflicts - 1) {
        m_currentConflict++;
        m_state.currentConflict = m_currentConflict;
    }
}

void MergeTool::previousConflict() {
    if (m_currentConflict > 0) {
        m_currentConflict--;
        m_state.currentConflict = m_currentConflict;
    }
}

void MergeTool::gotoConflict(int index) {
    if (index >= 0 && index < m_state.totalConflicts) {
        m_currentConflict = index;
        m_state.currentConflict = index;
    }
}

int MergeTool::getCurrentConflict() const {
    return m_currentConflict;
}

int MergeTool::getConflictCount() const {
    return m_state.totalConflicts;
}

int MergeTool::getResolvedCount() const {
    return m_state.resolvedConflicts;
}

bool MergeTool::hasUnresolvedConflicts() const {
    return m_state.resolvedConflicts < m_state.totalConflicts;
}

void MergeTool::acceptOurs() {
    if (m_currentConflict >= 0 && m_currentConflict < m_state.totalConflicts) {
        auto& conflict = m_state.conflicts[m_currentConflict];
        conflict.resolution = ConflictResolution::Ours;
        conflict.isResolved = true;
        updateResolvedCount();
    }
}

void MergeTool::acceptTheirs() {
    if (m_currentConflict >= 0 && m_currentConflict < m_state.totalConflicts) {
        auto& conflict = m_state.conflicts[m_currentConflict];
        conflict.resolution = ConflictResolution::Theirs;
        conflict.isResolved = true;
        updateResolvedCount();
    }
}

void MergeTool::acceptBoth() {
    if (m_currentConflict >= 0 && m_currentConflict < m_state.totalConflicts) {
        auto& conflict = m_state.conflicts[m_currentConflict];
        conflict.resolution = ConflictResolution::Both;
        conflict.isResolved = true;
        updateResolvedCount();
    }
}

void MergeTool::acceptBase() {
    if (m_currentConflict >= 0 && m_currentConflict < m_state.totalConflicts) {
        auto& conflict = m_state.conflicts[m_currentConflict];
        conflict.resolution = ConflictResolution::Base;
        conflict.isResolved = true;
        updateResolvedCount();
    }
}

void MergeTool::markResolved() {
    if (m_currentConflict >= 0 && m_currentConflict < m_state.totalConflicts) {
        m_state.conflicts[m_currentConflict].isResolved = true;
        updateResolvedCount();
    }
}

void MergeTool::markUnresolved() {
    if (m_currentConflict >= 0 && m_currentConflict < m_state.totalConflicts) {
        m_state.conflicts[m_currentConflict].isResolved = false;
        updateResolvedCount();
    }
}

void MergeTool::editResult(const std::string& content) {
    m_state.resultContent = content;
}

void MergeTool::autoResolveSimple() {
    for (auto& conflict : m_state.conflicts) {
        if (!conflict.isResolved && isAutoResolvable(&conflict - &m_state.conflicts[0])) {
            // Simple auto-resolution: prefer ours if identical to base
            if (conflict.oursLines == conflict.baseLines) {
                conflict.resolution = ConflictResolution::Theirs;
                conflict.isResolved = true;
            } else if (conflict.theirsLines == conflict.baseLines) {
                conflict.resolution = ConflictResolution::Ours;
                conflict.isResolved = true;
            }
        }
    }
    updateResolvedCount();
}

void MergeTool::autoResolveWhitespace() {
    for (auto& conflict : m_state.conflicts) {
        if (!conflict.isResolved) {
            // Check if only whitespace differs
            auto trimOurs = trimWhitespace(joinLines(conflict.oursLines));
            auto trimTheirs = trimWhitespace(joinLines(conflict.theirsLines));
            
            if (trimOurs == trimTheirs) {
                conflict.resolution = ConflictResolution::Ours;
                conflict.isResolved = true;
            }
        }
    }
    updateResolvedCount();
}

bool MergeTool::isAutoResolvable(int conflictIndex) const {
    if (conflictIndex < 0 || conflictIndex >= m_state.totalConflicts) {
        return false;
    }
    
    const auto& conflict = m_state.conflicts[conflictIndex];
    
    // Can auto-resolve if ours or theirs matches base
    if (conflict.oursLines == conflict.baseLines || 
        conflict.theirsLines == conflict.baseLines) {
        return true;
    }
    
    // Can auto-resolve if ours and theirs are identical
    if (conflict.oursLines == conflict.theirsLines) {
        return true;
    }
    
    return false;
}

void MergeTool::setShowBase(bool show) {
    m_showBase = show;
}

void MergeTool::setShowOurs(bool show) {
    m_showOurs = show;
}

void MergeTool::setShowTheirs(bool show) {
    m_showTheirs = show;
}

void MergeTool::setShowResult(bool show) {
    m_showResult = show;
}

void MergeTool::setSyncScrolling(bool sync) {
    m_syncScrolling = sync;
}

void MergeTool::parseConflicts() {
    // Parse conflict markers from the content
    std::regex conflictRegex(
        "<<<<<<< (\\\\S*)\\n"
        "((?:(?!=======).)*\\n?)"
        "=======\\n"
        "((?:(?!>>>>>>>).)*\\n?)"
        ">>>>>>> (\\\\S*)\\n?"
    );
    
    std::string content = m_state.oursContent; // Simplified - in reality would parse from merge output
    std::smatch match;
    std::string::const_iterator searchStart(content.cbegin());
    
    int lineNum = 0;
    while (std::regex_search(searchStart, content.cend(), match, conflictRegex)) {
        ConflictBlock block;
        block.startLine = lineNum;
        block.resolution = ConflictResolution::None;
        block.isResolved = false;
        
        // Parse ours section
        std::string oursText = match[2].str();
        block.oursLines = splitLines(oursText);
        
        // Parse theirs section
        std::string theirsText = match[3].str();
        block.theirsLines = splitLines(theirsText);
        
        // Base would come from common ancestor
        block.baseLines = block.oursLines; // Simplified
        
        block.endLine = lineNum + static_cast<int>(block.oursLines.size()) + 
                       static_cast<int>(block.theirsLines.size()) + 3;
        
        m_state.conflicts.push_back(block);
        searchStart = match.suffix().first;
        lineNum = block.endLine;
    }
}

std::string MergeTool::generateResult() const {
    std::string result;
    
    for (const auto& conflict : m_state.conflicts) {
        switch (conflict.resolution) {
            case ConflictResolution::Ours:
                result += joinLines(conflict.oursLines);
                break;
            case ConflictResolution::Theirs:
                result += joinLines(conflict.theirsLines);
                break;
            case ConflictResolution::Both:
                result += joinLines(conflict.oursLines);
                result += joinLines(conflict.theirsLines);
                break;
            case ConflictResolution::Base:
                result += joinLines(conflict.baseLines);
                break;
            default:
                // Keep conflict markers
                result += "<<<<<<< HEAD\n";
                result += joinLines(conflict.oursLines);
                result += "=======\n";
                result += joinLines(conflict.theirsLines);
                result += ">>>>>>> branch\n";
                break;
        }
    }
    
    return result;
}

void MergeTool::updateResolvedCount() {
    m_state.resolvedConflicts = 0;
    for (const auto& conflict : m_state.conflicts) {
        if (conflict.isResolved) {
            m_state.resolvedConflicts++;
        }
    }
}

std::vector<std::string> MergeTool::splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string line;
    
    while (std::getline(ss, line)) {
        lines.push_back(line);
    }
    
    return lines;
}

std::string MergeTool::joinLines(const std::vector<std::string>& lines) {
    std::string result;
    for (const auto& line : lines) {
        result += line + "\n";
    }
    return result;
}

std::string MergeTool::trimWhitespace(const std::string& text) {
    size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    
    size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

} // namespace RawrXD::Git
