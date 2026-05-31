/**
 * @file ai_refactoring_engine.cpp
 * @brief AI-powered code refactoring implementation
 * Batch 5 - Item 74: AI refactoring engine
 */

#include "ai/ai_refactoring_engine.h"
#include <sstream>
#include <algorithm>
#include <regex>

namespace RawrXD::AI {

AIRefactoringEngine::AIRefactoringEngine()
    : m_initialized(false) {
}

AIRefactoringEngine::~AIRefactoringEngine() {
    shutdown();
}

bool AIRefactoringEngine::initialize() {
    m_initialized = true;
    return true;
}

void AIRefactoringEngine::shutdown() {
    m_initialized = false;
}

RefactoringResult AIRefactoringEngine::refactor(const RefactoringRequest& request) {
    RefactoringResult result;
    
    if (!m_initialized) {
        result.error = "Engine not initialized";
        return result;
    }
    
    switch (request.type) {
        case RefactoringType::ExtractFunction:
            result = extractFunction(request.code, 0, 0, 0, 0, request.newName);
            break;
        case RefactoringType::ExtractVariable:
            result = extractVariable(request.code, request.target, request.newName);
            break;
        case RefactoringType::Inline:
            result = inlineVariable(request.code, request.target);
            break;
        case RefactoringType::Rename:
            result = renameSymbol(request.code, request.target, request.newName);
            break;
        case RefactoringType::ConvertToModern:
            result = convertToModern(request.code, request.language);
            break;
        case RefactoringType::OptimizeImports:
            result = optimizeImports(request.code);
            break;
        case RefactoringType::SimplifyExpression:
            result = simplifyExpression(request.code, request.target);
            break;
        case RefactoringType::RemoveDeadCode:
            result = removeDeadCode(request.code);
            break;
        default:
            result.error = "Refactoring type not implemented";
            break;
    }
    
    return result;
}

std::future<RefactoringResult> AIRefactoringEngine::refactorAsync(const RefactoringRequest& request) {
    return std::async(std::launch::async, [this, request]() {
        return refactor(request);
    });
}

RefactoringResult AIRefactoringEngine::extractFunction(const std::string& code,
                                                          int startLine, int startColumn,
                                                          int endLine, int endColumn,
                                                          const std::string& functionName) {
    RefactoringResult result;
    
    // Find the code to extract
    std::vector<std::string> lines = splitLines(code);
    
    if (startLine < 0 || endLine >= static_cast<int>(lines.size())) {
        result.error = "Invalid line range";
        return result;
    }
    
    // Extract the selected code
    std::stringstream extractedCode;
    for (int i = startLine; i <= endLine && i < static_cast<int>(lines.size()); i++) {
        if (i == startLine) {
            extractedCode << lines[i].substr(startColumn) << "\n";
        } else if (i == endLine) {
            extractedCode << lines[i].substr(0, endColumn) << "\n";
        } else {
            extractedCode << lines[i] << "\n";
        }
    }
    
    // Create the new function
    std::string newFunction = "void " + functionName + "() {\n" +
                              extractedCode.str() +
                              "}\n";
    
    // Create change for new function
    RefactoringChange newFuncChange;
    newFuncChange.filePath = "";
    newFuncChange.startLine = 0;
    newFuncChange.startColumn = 0;
    newFuncChange.endLine = 0;
    newFuncChange.endColumn = 0;
    newFuncChange.newText = newFunction;
    newFuncChange.description = "Extract new function: " + functionName;
    result.changes.push_back(newFuncChange);
    
    // Create change to replace extracted code with function call
    RefactoringChange replaceChange;
    replaceChange.filePath = "";
    replaceChange.startLine = startLine;
    replaceChange.startColumn = startColumn;
    replaceChange.endLine = endLine;
    replaceChange.endColumn = endColumn;
    replaceChange.oldText = extractedCode.str();
    replaceChange.newText = functionName + "();";
    replaceChange.description = "Replace with function call";
    result.changes.push_back(replaceChange);
    
    result.explanation = "Extracted code into function '" + functionName + "()'";
    result.isSafe = true;
    result.isComplete = true;
    
    return result;
}

RefactoringResult AIRefactoringEngine::extractVariable(const std::string& code,
                                                         const std::string& expression,
                                                         const std::string& variableName) {
    RefactoringResult result;
    
    size_t pos = code.find(expression);
    if (pos == std::string::npos) {
        result.error = "Expression not found";
        return result;
    }
    
    // Find line and column
    int line = 0;
    int col = 0;
    for (size_t i = 0; i < pos; i++) {
        if (code[i] == '\n') {
            line++;
            col = 0;
        } else {
            col++;
        }
    }
    
    // Create variable declaration
    std::string declaration = "auto " + variableName + " = " + expression + ";\n";
    
    // Add declaration before first use
    RefactoringChange declChange;
    declChange.filePath = "";
    declChange.startLine = line;
    declChange.startColumn = 0;
    declChange.endLine = line;
    declChange.endColumn = 0;
    declChange.newText = declaration;
    declChange.description = "Add variable declaration";
    result.changes.push_back(declChange);
    
    // Replace expression with variable
    RefactoringChange replaceChange;
    replaceChange.filePath = "";
    replaceChange.startLine = line;
    replaceChange.startColumn = col;
    replaceChange.endLine = line;
    replaceChange.endColumn = col + static_cast<int>(expression.length());
    replaceChange.oldText = expression;
    replaceChange.newText = variableName;
    replaceChange.description = "Replace with variable";
    result.changes.push_back(replaceChange);
    
    result.explanation = "Extracted expression into variable '" + variableName + "'";
    result.isSafe = true;
    result.isComplete = true;
    
    return result;
}

RefactoringResult AIRefactoringEngine::inlineVariable(const std::string& code,
                                                         const std::string& variableName) {
    RefactoringResult result;
    
    // Find variable declaration
    std::regex varRegex("(auto|int|double|string|bool)\\s+" + variableName + "\\s*=\\s*([^;]+);");
    std::smatch match;
    
    if (!std::regex_search(code, match, varRegex)) {
        result.error = "Variable declaration not found";
        return result;
    }
    
    std::string value = match[2].str();
    
    // Find all uses and replace with value
    std::string newCode = code;
    std::regex useRegex("\\b" + variableName + "\\b");
    newCode = std::regex_replace(newCode, useRegex, value);
    
    // Remove declaration
    std::regex declRegex("(auto|int|double|string|bool)\\s+" + variableName + "\\s*=\\s*[^;]+;\\s*");
    newCode = std::regex_replace(newCode, declRegex, "");
    
    RefactoringChange change;
    change.filePath = "";
    change.startLine = 0;
    change.startColumn = 0;
    change.endLine = static_cast<int>(splitLines(code).size());
    change.endColumn = 0;
    change.oldText = code;
    change.newText = newCode;
    change.description = "Inline variable: " + variableName;
    result.changes.push_back(change);
    
    result.explanation = "Inlined variable '" + variableName + "'";
    result.isSafe = false; // May have side effects
    result.warnings.push_back("Check for side effects from multiple evaluations");
    result.isComplete = true;
    
    return result;
}

RefactoringResult AIRefactoringEngine::renameSymbol(const std::string& code,
                                                       const std::string& oldName,
                                                       const std::string& newName) {
    RefactoringResult result;
    
    // Replace all occurrences
    std::string newCode = code;
    std::regex symbolRegex("\\b" + oldName + "\\b");
    newCode = std::regex_replace(newCode, symbolRegex, newName);
    
    RefactoringChange change;
    change.filePath = "";
    change.startLine = 0;
    change.startColumn = 0;
    change.endLine = static_cast<int>(splitLines(code).size());
    change.endColumn = 0;
    change.oldText = code;
    change.newText = newCode;
    change.description = "Rename " + oldName + " to " + newName;
    result.changes.push_back(change);
    
    result.explanation = "Renamed '" + oldName + "' to '" + newName + "'";
    result.isSafe = true;
    result.isComplete = true;
    
    return result;
}

RefactoringResult AIRefactoringEngine::convertToModern(const std::string& code,
                                                         const std::string& language) {
    RefactoringResult result;
    
    std::string newCode = code;
    
    if (language == "cpp" || language == "c++") {
        // Replace NULL with nullptr
        newCode = std::regex_replace(newCode, std::regex("\\bNULL\\b"), "nullptr");
        
        // Replace typedef with using
        newCode = std::regex_replace(newCode, 
            std::regex("typedef\\s+(\\w+)\\s+(\\w+);"),
            "using $2 = $1;");
        
        // Replace raw loops with range-based for
        // This is simplified - real implementation would be more complex
    }
    
    RefactoringChange change;
    change.filePath = "";
    change.startLine = 0;
    change.startColumn = 0;
    change.endLine = static_cast<int>(splitLines(code).size());
    change.endColumn = 0;
    change.oldText = code;
    change.newText = newCode;
    change.description = "Convert to modern " + language;
    result.changes.push_back(change);
    
    result.explanation = "Converted code to modern " + language + " style";
    result.isSafe = true;
    result.isComplete = true;
    
    return result;
}

RefactoringResult AIRefactoringEngine::optimizeImports(const std::string& code) {
    RefactoringResult result;
    
    // Find all includes
    std::regex includeRegex("#include\\s+[<\"]([^>\"]+)[>\"]");
    std::set<std::string> includes;
    
    auto words_begin = std::sregex_iterator(code.begin(), code.end(), includeRegex);
    auto words_end = std::sregex_iterator();
    
    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        includes.insert((*i)[1].str());
    }
    
    // Remove duplicates and sort
    std::vector<std::string> sortedIncludes(includes.begin(), includes.end());
    std::sort(sortedIncludes.begin(), sortedIncludes.end());
    
    // Generate new include section
    std::stringstream newIncludes;
    for (const auto& inc : sortedIncludes) {
        if (inc.find(".h") != std::string::npos) {
            newIncludes << "#include \"" << inc << "\"\n";
        } else {
            newIncludes << "#include <" << inc << ">\n";
        }
    }
    
    // Find and replace includes
    std::string newCode = std::regex_replace(code, includeRegex, "");
    
    // Add sorted includes at beginning
    newCode = newIncludes.str() + "\n" + newCode;
    
    RefactoringChange change;
    change.filePath = "";
    change.startLine = 0;
    change.startColumn = 0;
    change.endLine = static_cast<int>(splitLines(code).size());
    change.endColumn = 0;
    change.oldText = code;
    change.newText = newCode;
    change.description = "Optimize imports/includes";
    result.changes.push_back(change);
    
    result.explanation = "Organized and removed duplicate imports";
    result.isSafe = true;
    result.isComplete = true;
    
    return result;
}

RefactoringResult AIRefactoringEngine::simplifyExpression(const std::string& code,
                                                           const std::string& expression) {
    RefactoringResult result;
    
    // Simplify common patterns
    std::string simplified = expression;
    
    // Simplify: !!x to x
    simplified = std::regex_replace(simplified, std::regex("!!"), "");
    
    // Simplify: x == true to x
    simplified = std::regex_replace(simplified, std::regex("==\\s*true\\b"), "");
    
    // Simplify: x == false to !x
    simplified = std::regex_replace(simplified, std::regex("(\\w+)\\s*==\\s*false"), "!$1");
    
    if (simplified != expression) {
        RefactoringChange change;
        change.filePath = "";
        change.startLine = 0;
        change.startColumn = 0;
        change.endLine = 0;
        change.endColumn = static_cast<int>(expression.length());
        change.oldText = expression;
        change.newText = simplified;
        change.description = "Simplify expression";
        result.changes.push_back(change);
        
        result.explanation = "Simplified expression";
        result.isSafe = true;
    } else {
        result.explanation = "No simplifications found";
    }
    
    result.isComplete = true;
    return result;
}

RefactoringResult AIRefactoringEngine::removeDeadCode(const std::string& code) {
    RefactoringResult result;
    
    std::string newCode = code;
    
    // Remove code after return
    std::regex deadCodeRegex("(return[^;]*;)([^}]*)(?=})");
    newCode = std::regex_replace(newCode, deadCodeRegex, "$1\n");
    
    // Remove unused variables (simplified)
    // Real implementation would require semantic analysis
    
    if (newCode != code) {
        RefactoringChange change;
        change.filePath = "";
        change.startLine = 0;
        change.startColumn = 0;
        change.endLine = static_cast<int>(splitLines(code).size());
        change.endColumn = 0;
        change.oldText = code;
        change.newText = newCode;
        change.description = "Remove dead code";
        result.changes.push_back(change);
        
        result.explanation = "Removed unreachable code";
        result.isSafe = true;
    } else {
        result.explanation = "No dead code found";
    }
    
    result.isComplete = true;
    return result;
}

std::vector<CodeSmell> AIRefactoringEngine::detectSmells(const std::string& code) {
    std::vector<CodeSmell> smells;
    
    std::vector<std::string> lines = splitLines(code);
    
    for (size_t i = 0; i < lines.size(); i++) {
        const std::string& line = lines[i];
        
        // Check for long functions
        if (i > 0 && line.find("{") != std::string::npos) {
            int braceCount = 1;
            size_t j = i + 1;
            while (j < lines.size() && braceCount > 0) {
                if (lines[j].find("{") != std::string::npos) braceCount++;
                if (lines[j].find("}") != std::string::npos) braceCount--;
                j++;
            }
            
            if (j - i > 50) {
                CodeSmell smell;
                smell.type = "LongFunction";
                smell.description = "Function is too long (" + std::to_string(j - i) + " lines)";
                smell.line = static_cast<int>(i) + 1;
                smell.column = 0;
                smell.severity = 2;
                smell.suggestion = "Extract parts of the function into smaller functions";
                smells.push_back(smell);
            }
        }
        
        // Check for magic numbers
        std::regex magicNumberRegex("[^a-zA-Z_]([0-9]{2,})[^a-zA-Z_]");
        std::smatch match;
        if (std::regex_search(line, match, magicNumberRegex)) {
            CodeSmell smell;
            smell.type = "MagicNumber";
            smell.description = "Magic number: " + match[1].str();
            smell.line = static_cast<int>(i) + 1;
            smell.column = static_cast<int>(match.position(1));
            smell.severity = 1;
            smell.suggestion = "Replace with named constant";
            smells.push_back(smell);
        }
        
        // Check for TODO comments
        if (line.find("TODO") != std::string::npos || line.find("FIXME") != std::string::npos) {
            CodeSmell smell;
            smell.type = "TodoComment";
            smell.description = "TODO/FIXME comment found";
            smell.line = static_cast<int>(i) + 1;
            smell.column = 0;
            smell.severity = 0;
            smell.suggestion = "Address the TODO or remove the comment";
            smells.push_back(smell);
        }
    }
    
    return smells;
}

bool AIRefactoringEngine::canUndo() const {
    return m_undoStack.size() > 1; // Current state is on top
}

bool AIRefactoringEngine::canRedo() const {
    return !m_redoStack.empty();
}

RefactoringResult AIRefactoringEngine::undo() {
    RefactoringResult result;
    
    if (!canUndo()) {
        result.error = "Nothing to undo";
        return result;
    }
    
    // Save current state to redo stack
    m_redoStack.push_back(m_undoStack.back());
    m_undoStack.pop_back();
    
    result.isComplete = true;
    return result;
}

RefactoringResult AIRefactoringEngine::redo() {
    RefactoringResult result;
    
    if (!canRedo()) {
        result.error = "Nothing to redo";
        return result;
    }
    
    // Restore from redo stack
    m_undoStack.push_back(m_redoStack.back());
    m_redoStack.pop_back();
    
    result.isComplete = true;
    return result;
}

void AIRefactoringEngine::onRefactoringComplete(RefactoringCallback callback) {
    m_refactoringCallback = callback;
}

void AIRefactoringEngine::onSmellsDetected(SmellsCallback callback) {
    m_smellsCallback = callback;
}

std::vector<std::string> AIRefactoringEngine::splitLines(const std::string& code) {
    std::vector<std::string> lines;
    std::istringstream stream(code);
    std::string line;
    
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    
    return lines;
}

} // namespace RawrXD::AI
