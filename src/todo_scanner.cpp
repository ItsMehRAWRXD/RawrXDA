// ============================================================================
// todo_scanner.cpp — Production-Ready TODO Scanner for RawrXD IDE
// ============================================================================
// Scans source files for TODO comments and integrates with TodoManager
// Supports multiple languages and comment formats
// ============================================================================

#include "todo_scanner.h"
#include "todo_manager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <filesystem>
#include <algorithm>

namespace RawrXD {

TodoScanner::TodoScanner(TodoManager* todoManager) 
    : todoManager_(todoManager) {
    // Initialize language patterns
    initializeLanguagePatterns();
}

void TodoScanner::initializeLanguagePatterns() {
    // C/C++/C#/Java/JavaScript/TypeScript
    LanguagePattern cppPattern;
    cppPattern.singleLine = "//";
    cppPattern.multiLineStart = "/*";
    cppPattern.multiLineEnd = "*/";
    cppPattern.filePatterns = {".cpp", ".c", ".h", ".hpp", ".cs", ".java", ".js", ".ts", ".m", ".mm"};
    languagePatterns_["cpp"] = cppPattern;

    // Python
    LanguagePattern pythonPattern;
    pythonPattern.singleLine = "#";
    pythonPattern.multiLineStart = "\"\"\"";
    pythonPattern.multiLineEnd = "\"\"\"";
    pythonPattern.filePatterns = {".py"};
    languagePatterns_["python"] = pythonPattern;

    // HTML/XML
    LanguagePattern htmlPattern;
    htmlPattern.singleLine = "";
    htmlPattern.multiLineStart = "<!--";
    htmlPattern.multiLineEnd = "-->";
    htmlPattern.filePatterns = {".html", ".htm", ".xml", ".xhtml", ".vue", ".svelte"};
    languagePatterns_["html"] = htmlPattern;

    // CSS/SCSS/LESS
    LanguagePattern cssPattern;
    cssPattern.singleLine = "//";
    cssPattern.multiLineStart = "/*";
    cssPattern.multiLineEnd = "*/";
    cssPattern.filePatterns = {".css", ".scss", ".less", ".sass"};
    languagePatterns_["css"] = cssPattern;

    // Shell scripts
    LanguagePattern shellPattern;
    shellPattern.singleLine = "#";
    shellPattern.multiLineStart = "";
    shellPattern.multiLineEnd = "";
    shellPattern.filePatterns = {".sh", ".bash", ".zsh", ".ps1", ".bat", ".cmd"};
    languagePatterns_["shell"] = shellPattern;

    // Rust
    LanguagePattern rustPattern;
    rustPattern.singleLine = "//";
    rustPattern.multiLineStart = "/*";
    rustPattern.multiLineEnd = "*/";
    rustPattern.filePatterns = {".rs"};
    languagePatterns_["rust"] = rustPattern;

    // Go
    LanguagePattern goPattern;
    goPattern.singleLine = "//";
    goPattern.multiLineStart = "/*";
    goPattern.multiLineEnd = "*/";
    goPattern.filePatterns = {".go"};
    languagePatterns_["go"] = goPattern;

    // PHP
    LanguagePattern phpPattern;
    phpPattern.singleLine = "//";
    phpPattern.multiLineStart = "/*";
    phpPattern.multiLineEnd = "*/";
    phpPattern.singleLineAlt = "#";
    phpPattern.filePatterns = {".php"};
    languagePatterns_["php"] = phpPattern;

    // Ruby
    LanguagePattern rubyPattern;
    rubyPattern.singleLine = "#";
    rubyPattern.multiLineStart = "=begin";
    rubyPattern.multiLineEnd = "=end";
    rubyPattern.filePatterns = {".rb"};
    languagePatterns_["ruby"] = rubyPattern;

    // Swift
    LanguagePattern swiftPattern;
    swiftPattern.singleLine = "//";
    swiftPattern.multiLineStart = "/*";
    swiftPattern.multiLineEnd = "*/";
    swiftPattern.filePatterns = {".swift"};
    languagePatterns_["swift"] = swiftPattern;

    // Kotlin
    LanguagePattern kotlinPattern;
    kotlinPattern.singleLine = "//";
    kotlinPattern.multiLineStart = "/*";
    kotlinPattern.multiLineEnd = "*/";
    kotlinPattern.filePatterns = {".kt", ".kts"};
    languagePatterns_["kotlin"] = kotlinPattern;
}

std::string TodoScanner::detectLanguage(const std::string& filePath) const {
    std::filesystem::path path(filePath);
    std::string extension = path.extension().string();
    
    // Convert to lowercase for case-insensitive matching
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    for (const auto& [lang, pattern] : languagePatterns_) {
        for (const auto& ext : pattern.filePatterns) {
            if (extension == ext) {
                return lang;
            }
        }
    }
    
    return "unknown";
}

ScanResult TodoScanner::scanFile(const std::string& filePath) {
    ScanResult result;
    result.filePath = filePath;
    
    if (!std::filesystem::exists(filePath)) {
        result.error = "File not found";
        return result;
    }
    
    std::string language = detectLanguage(filePath);
    if (language == "unknown") {
        result.error = "Unsupported language";
        return result;
    }
    
    const LanguagePattern& pattern = languagePatterns_.at(language);
    
    std::ifstream file(filePath);
    if (!file.is_open()) {
        result.error = "Cannot open file";
        return result;
    }
    
    std::string line;
    int lineNumber = 0;
    bool inMultiLineComment = false;
    std::string multiLineBuffer;
    int multiLineStartLine = 0;
    
    while (std::getline(file, line)) {
        lineNumber++;
        
        if (inMultiLineComment) {
            // Check for end of multi-line comment
            size_t endPos = line.find(pattern.multiLineEnd);
            if (endPos != std::string::npos) {
                // End of multi-line comment found
                multiLineBuffer += line.substr(0, endPos);
                inMultiLineComment = false;
                
                // Process the complete multi-line comment
                processCommentBlock(multiLineBuffer, filePath, multiLineStartLine, result);
                multiLineBuffer.clear();
                
                // Check if there's more content after the comment end
                std::string remaining = line.substr(endPos + pattern.multiLineEnd.length());
                processLine(remaining, filePath, lineNumber, pattern, result);
            } else {
                // Continue accumulating multi-line comment
                multiLineBuffer += line + "\n";
            }
        } else {
            // Check for multi-line comment start
            size_t multiStartPos = line.find(pattern.multiLineStart);
            if (multiStartPos != std::string::npos) {
                // Start of multi-line comment
                inMultiLineComment = true;
                multiLineStartLine = lineNumber;
                multiLineBuffer = line.substr(multiStartPos + pattern.multiLineStart.length()) + "\n";
                
                // Process content before the comment
                std::string beforeComment = line.substr(0, multiStartPos);
                processLine(beforeComment, filePath, lineNumber, pattern, result);
            } else {
                // Process single line
                processLine(line, filePath, lineNumber, pattern, result);
            }
        }
    }
    
    // Handle case where file ends with an open multi-line comment
    if (inMultiLineComment) {
        processCommentBlock(multiLineBuffer, filePath, multiLineStartLine, result);
    }
    
    file.close();
    return result;
}

void TodoScanner::processLine(const std::string& line, 
                             const std::string& filePath, 
                             int lineNumber,
                             const LanguagePattern& pattern,
                             ScanResult& result) {
    
    // Check for single-line comments
    std::vector<std::string> commentMarkers;
    if (!pattern.singleLine.empty()) {
        commentMarkers.push_back(pattern.singleLine);
    }
    if (!pattern.singleLineAlt.empty()) {
        commentMarkers.push_back(pattern.singleLineAlt);
    }
    
    for (const auto& marker : commentMarkers) {
        size_t commentPos = line.find(marker);
        if (commentPos != std::string::npos) {
            std::string commentText = line.substr(commentPos + marker.length());
            extractTodosFromComment(commentText, filePath, lineNumber, result);
            break;
        }
    }
}

void TodoScanner::processCommentBlock(const std::string& commentBlock,
                                     const std::string& filePath,
                                     int startLine,
                                     ScanResult& result) {
    
    std::istringstream stream(commentBlock);
    std::string line;
    int relativeLine = 0;
    
    while (std::getline(stream, line)) {
        extractTodosFromComment(line, filePath, startLine + relativeLine, result);
        relativeLine++;
    }
}

void TodoScanner::extractTodosFromComment(const std::string& commentText,
                                         const std::string& filePath,
                                         int lineNumber,
                                         ScanResult& result) {
    
    // Trim whitespace
    std::string trimmed = commentText;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
    trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);
    
    if (trimmed.empty()) {
        return;
    }
    
    // TODO patterns (case-insensitive)
    static const std::vector<std::regex> todoPatterns = {
        std::regex(R"((TODO|FIXME|BUG|HACK|NOTE|XXX|OPTIMIZE|PERF):?\s+(.*))", 
                  std::regex_constants::icase),
        std::regex(R"(\[?(TODO|FIXME|BUG|HACK|NOTE|XXX|OPTIMIZE|PERF)\]?:?\s+(.*))", 
                  std::regex_constants::icase),
        std::regex(R"(<!--\s*(TODO|FIXME|BUG|HACK|NOTE|XXX|OPTIMIZE|PERF):?\s+(.*)-->)", 
                  std::regex_constants::icase)
    };
    
    for (const auto& pattern : todoPatterns) {
        std::smatch match;
        if (std::regex_search(trimmed, match, pattern) && match.size() >= 3) {
            std::string type = match[1].str();
            std::string description = match[2].str();
            
            // Convert type to uppercase for consistency
            std::transform(type.begin(), type.end(), type.begin(), ::toupper);
            
            // Trim the description
            description.erase(0, description.find_first_not_of(" \t\r\n"));
            description.erase(description.find_last_not_of(" \t\r\n") + 1);
            
            if (!description.empty()) {
                TodoItem todo;
                todo.type = type;
                todo.description = description;
                todo.filePath = filePath;
                todo.lineNumber = lineNumber;
                
                result.todos.push_back(todo);
                
                // Add to TodoManager if available
                if (todoManager_) {
                    todoManager_->addTodo(description, filePath, lineNumber);
                }
            }
            break;
        }
    }
}

ScanResult TodoScanner::scanDirectory(const std::string& directoryPath, 
                                     const std::vector<std::string>& extensions) {
    ScanResult result;
    
    if (!std::filesystem::exists(directoryPath)) {
        result.error = "Directory not found";
        return result;
    }
    
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directoryPath)) {
            if (entry.is_regular_file()) {
                std::string filePath = entry.path().string();
                std::string fileExt = entry.path().extension().string();
                
                // Convert to lowercase for case-insensitive matching
                std::transform(fileExt.begin(), fileExt.end(), fileExt.begin(), ::tolower);
                
                // Check if file extension matches any of the requested extensions
                bool shouldScan = extensions.empty();
                if (!extensions.empty()) {
                    for (const auto& ext : extensions) {
                        std::string lowerExt = ext;
                        std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);
                        if (fileExt == lowerExt) {
                            shouldScan = true;
                            break;
                        }
                    }
                }
                
                if (shouldScan) {
                    ScanResult fileResult = scanFile(filePath);
                    if (fileResult.error.empty()) {
                        result.scannedFiles++;
                        result.todos.insert(result.todos.end(), 
                                          fileResult.todos.begin(), 
                                          fileResult.todos.end());
                    } else {
                        result.errors++;
                    }
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        result.error = e.what();
    }
    
    return result;
}

void TodoScanner::exportToJson(const ScanResult& result, const std::string& outputPath) {
    std::ofstream outFile(outputPath);
    if (!outFile.is_open()) {
        return;
    }
    
    outFile << "{\n";
    outFile << "  \"scannedFiles\": " << result.scannedFiles << ",\n";
    outFile << "  \"totalTodos\": " << result.todos.size() << ",\n";
    outFile << "  \"errors\": " << result.errors << ",\n";
    outFile << "  \"todos\": [\n";
    
    for (size_t i = 0; i < result.todos.size(); ++i) {
        const auto& todo = result.todos[i];
        outFile << "    {\n";
        outFile << "      \"type\": \"" << todo.type << "\",\n";
        outFile << "      \"description\": \"" << escapeJsonString(todo.description) << "\",\n";
        outFile << "      \"filePath\": \"" << escapeJsonString(todo.filePath) << "\",\n";
        outFile << "      \"lineNumber\": " << todo.lineNumber << "\n";
        outFile << "    }";
        
        if (i < result.todos.size() - 1) {
            outFile << ",";
        }
        outFile << "\n";
    }
    
    outFile << "  ]\n";
    outFile << "}\n";
    outFile.close();
}

std::string TodoScanner::escapeJsonString(const std::string& str) {
    std::string result;
    result.reserve(str.length() * 2);
    
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f) {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    result += buf;
                } else {
                    result += c;
                }
        }
    }
    
    return result;
}

std::vector<TodoItem> TodoScanner::getTodosByType(const std::string& type) const {
    std::vector<TodoItem> result;
    
    // This would typically work with a pre-scanned result
    // For now, return empty - in practice you'd store scanned results
    return result;
}

std::vector<TodoItem> TodoScanner::getTodosByFile(const std::string& filePath) const {
    std::vector<TodoItem> result;
    
    // This would typically work with a pre-scanned result
    // For now, return empty - in practice you'd store scanned results
    return result;
}

} // namespace RawrXD
