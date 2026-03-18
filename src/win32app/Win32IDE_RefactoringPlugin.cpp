#include "Win32IDE.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string toLowerCopy(const std::string& value) {
    std::string out = value;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool isIdentifier(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    const auto isHead = [](unsigned char c) { return std::isalpha(c) || c == '_'; };
    const auto isTail = [](unsigned char c) { return std::isalnum(c) || c == '_'; };
    if (!isHead(static_cast<unsigned char>(value.front()))) {
        return false;
    }
    for (size_t i = 1; i < value.size(); ++i) {
        if (!isTail(static_cast<unsigned char>(value[i]))) {
            return false;
        }
    }
    return true;
}

std::string extractSelection(const std::string& text, int startPos, int endPos) {
    if (startPos < 0 || endPos <= startPos || endPos > static_cast<int>(text.size())) {
        return {};
    }
    return text.substr(static_cast<size_t>(startPos), static_cast<size_t>(endPos - startPos));
}

std::string replaceWholeIdentifier(const std::string& code, const std::string& from, const std::string& to) {
    if (!isIdentifier(from) || !isIdentifier(to)) {
        return code;
    }

    std::string out;
    out.reserve(code.size() + 32);

    const auto isIdChar = [](unsigned char c) { return std::isalnum(c) || c == '_'; };

    size_t i = 0;
    while (i < code.size()) {
        if (i + from.size() <= code.size() && code.compare(i, from.size(), from) == 0) {
            const bool leftOk = (i == 0) || !isIdChar(static_cast<unsigned char>(code[i - 1]));
            const bool rightOk = (i + from.size() == code.size()) || !isIdChar(static_cast<unsigned char>(code[i + from.size()]));
            if (leftOk && rightOk) {
                out += to;
                i += from.size();
                continue;
            }
        }
        out.push_back(code[i]);
        ++i;
    }

    return out;
}

std::string organizeIncludes(const std::string& code) {
    std::istringstream in(code);
    std::vector<std::string> includeLines;
    std::vector<std::string> bodyLines;

    std::string line;
    while (std::getline(in, line)) {
        const std::string trimmed = toLowerCopy(line);
        if (trimmed.rfind("#include", 0) == 0) {
            includeLines.push_back(line);
        } else {
            bodyLines.push_back(line);
        }
    }

    std::sort(includeLines.begin(), includeLines.end());
    includeLines.erase(std::unique(includeLines.begin(), includeLines.end()), includeLines.end());

    std::ostringstream out;
    for (const auto& inc : includeLines) {
        out << inc << "\n";
    }
    if (!includeLines.empty() && !bodyLines.empty()) {
        out << "\n";
    }
    for (size_t i = 0; i < bodyLines.size(); ++i) {
        out << bodyLines[i];
        if (i + 1 < bodyLines.size()) {
            out << "\n";
        }
    }
    return out.str();
}

std::string removeClearlyDeadCode(const std::string& code) {
    std::regex deadIfFalse(R"(\n?[ \t]*if\s*\(\s*false\s*\)\s*\{[^{}]*\})");
    std::regex deadWhileFalse(R"(\n?[ \t]*while\s*\(\s*false\s*\)\s*\{[^{}]*\})");

    std::string out = std::regex_replace(code, deadIfFalse, "");
    out = std::regex_replace(out, deadWhileFalse, "");
    return out;
}

std::string extractMethodSimple(const std::string& code, int startPos, int endPos, const std::string& methodName) {
    if (startPos < 0 || endPos <= startPos || endPos > static_cast<int>(code.size()) || !isIdentifier(methodName)) {
        return code;
    }

    const std::string block = code.substr(static_cast<size_t>(startPos), static_cast<size_t>(endPos - startPos));

    std::ostringstream method;
    method << "\nstatic void " << methodName << "() {\n";

    std::istringstream in(block);
    std::string line;
    while (std::getline(in, line)) {
        method << "    " << line << "\n";
    }
    method << "}\n";

    std::string out = code;
    out.replace(static_cast<size_t>(startPos), static_cast<size_t>(endPos - startPos), methodName + "();");
    out += method.str();
    return out;
}

std::string convertToAutoSimple(const std::string& code, int startPos, int endPos) {
    if (startPos < 0 || endPos <= startPos || endPos > static_cast<int>(code.size())) {
        return code;
    }

    std::string selection = code.substr(static_cast<size_t>(startPos), static_cast<size_t>(endPos - startPos));
    std::regex typedDecl(R"(\b(?:int|long|float|double|bool|char|std::string)\s+([A-Za-z_][A-Za-z0-9_]*)\s*=)");
    selection = std::regex_replace(selection, typedDecl, "auto $1 =");

    std::string out = code;
    out.replace(static_cast<size_t>(startPos), static_cast<size_t>(endPos - startPos), selection);
    return out;
}

}  // namespace

class RefactoringPluginManager {
public:
    explicit RefactoringPluginManager(Win32IDE* ide) : m_ide(ide) {}
    ~RefactoringPluginManager();

    std::vector<std::string> getAvailable(const std::string& code, int startPos, int endPos) const {
        std::vector<std::string> ops;
        const std::string selection = extractSelection(code, startPos, endPos);
        if (!selection.empty()) {
            if (isIdentifier(selection)) {
                ops.push_back("Rename Variable");
            }
            ops.push_back("Extract Method");
            ops.push_back("Convert to Auto");
        }
        ops.push_back("Organize Includes");
        ops.push_back("Remove Dead Code");
        return ops;
    }

    bool execute(const std::string& operation,
                 const std::string& code,
                 int startPos,
                 int endPos,
                 const std::string& param,
                 std::string& outCode) const {
        outCode = code;

        if (operation == "Rename Variable") {
            const std::string selected = extractSelection(code, startPos, endPos);
            if (!isIdentifier(selected) || !isIdentifier(param)) {
                return false;
            }
            outCode = replaceWholeIdentifier(code, selected, param);
            return true;
        }

        if (operation == "Extract Method") {
            const std::string method = isIdentifier(param) ? param : "extractedMethod";
            outCode = extractMethodSimple(code, startPos, endPos, method);
            return outCode != code;
        }

        if (operation == "Convert to Auto") {
            outCode = convertToAutoSimple(code, startPos, endPos);
            return outCode != code;
        }

        if (operation == "Organize Includes") {
            outCode = organizeIncludes(code);
            return outCode != code;
        }

        if (operation == "Remove Dead Code") {
            outCode = removeClearlyDeadCode(code);
            return outCode != code;
        }

        return false;
    }

private:
    Win32IDE* m_ide;
};

RefactoringPluginManager::~RefactoringPluginManager() = default;

void Win32IDE::initRefactoringPlugin() {
    if (!m_refactoringManager) {
        m_refactoringManager = std::make_unique<RefactoringPluginManager>(this);
    }
}

std::vector<std::string> Win32IDE::getAvailableRefactorings(int startPos, int endPos) {
    if (!m_refactoringManager) {
        initRefactoringPlugin();
    }
    const std::string code = getEditorText();
    return m_refactoringManager ? m_refactoringManager->getAvailable(code, startPos, endPos) : std::vector<std::string>{};
}

bool Win32IDE::executeRefactoring(const std::string& operation, int startPos, int endPos, const std::string& param) {
    if (!m_refactoringManager) {
        initRefactoringPlugin();
    }
    const std::string code = getEditorText();
    std::string outCode;
    const bool ok = m_refactoringManager && m_refactoringManager->execute(operation, code, startPos, endPos, param, outCode);
    return ok && applyRefactoring(outCode);
}

bool Win32IDE::applyRefactoring(const std::string& refactoredCode) {
    if (!m_hwndEditor) {
        return false;
    }
    SetWindowTextA(m_hwndEditor, refactoredCode.c_str());
    updateSyntaxHighlighting();
    return true;
}

std::string Win32IDE::getEditorText() const {
    if (!m_hwndEditor) {
        return {};
    }
    const int length = GetWindowTextLengthA(m_hwndEditor);
    if (length <= 0) {
        return {};
    }
    std::string text(static_cast<size_t>(length), '\0');
    GetWindowTextA(m_hwndEditor, text.data(), length + 1);
    return text;
}

void Win32IDE::handleRefactoringCommand() {
    if (!m_hwndEditor) {
        return;
    }
    DWORD selStart = 0;
    DWORD selEnd = 0;
    SendMessageA(m_hwndEditor, EM_GETSEL, reinterpret_cast<WPARAM>(&selStart), reinterpret_cast<LPARAM>(&selEnd));
    showRefactoringMenu(static_cast<int>(selStart), static_cast<int>(selEnd));
}

void Win32IDE::showRefactoringMenu(int startPos, int endPos) {
    const auto options = getAvailableRefactorings(startPos, endPos);
    if (options.empty()) {
        appendToOutput("[Refactor] No refactoring available for current selection.\n", "Refactor", OutputSeverity::Warning);
        return;
    }

    std::ostringstream oss;
    oss << "[Refactor] Available operations:\n";
    for (const auto& op : options) {
        oss << "  - " << op << "\n";
    }
    appendToOutput(oss.str(), "Refactor", OutputSeverity::Info);
}
