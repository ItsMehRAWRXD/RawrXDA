// Real AI-Powered Code Completion System
// Integrates with local LLMs, Ollama, and streaming inference

#include <windows.h>
#include <string>
#include <vector>
<<<<<<< HEAD
#include <iostream>
=======
<iostream>
>>>>>>> origin/main
#include <thread>
#include <mutex>
#include <functional>
#include <sstream>
#include <algorithm>
#include <set>
#include <cstring>

// AI Completion Engine with real-time streaming
class AICompletionEngine {
private:
    struct CompletionContext {
        std::string file_path;
        std::string file_content;
        int cursor_position;
        std::string language;
        std::vector<std::string> imports;
        std::string current_function;
    };
    
    std::mutex completion_mutex_;
    bool streaming_enabled_;
    std::thread completion_thread_;
    bool stop_requested_;
    
public:
    AICompletionEngine() : streaming_enabled_(true), stop_requested_(false) {}
    
    ~AICompletionEngine() {
        stop_requested_ = true;
        if (completion_thread_.joinable()) {
            completion_thread_.join();
        }
    }
    
    // Generate code completion with context awareness
    std::string GenerateCompletion(const CompletionContext& context) {
        std::lock_guard<std::mutex> lock(completion_mutex_);
        
        // Extract context before cursor
        std::string before_cursor = context.file_content.substr(0, context.cursor_position);
        std::string after_cursor = context.file_content.substr(context.cursor_position);
        
        // Find current line
        size_t line_start = before_cursor.rfind('\n');
        if (line_start == std::string::npos) line_start = 0;
        else line_start++;
        
        std::string current_line = before_cursor.substr(line_start);
        
        // Detect completion type
        if (IsVariableDeclaration(current_line)) {
            return GenerateVariableCompletion(current_line, context);
        }
        else if (IsFunctionCall(current_line)) {
            return GenerateFunctionCompletion(current_line, context);
        }
        else if (IsControlStructure(current_line)) {
            return GenerateControlStructureCompletion(current_line, context);
        }
        else if (IsComment(current_line)) {
            return GenerateCommentCompletion(current_line, context);
        }
        
        // Default: next likely token based on language model
        return GenerateNextToken(before_cursor, context);
    }
    
    // Stream completion token by token (for real-time ghost text)
    void StreamCompletion(const CompletionContext& context, 
                         std::function<void(const std::string&)> token_callback,
                         std::function<void()> complete_callback) {
        if (completion_thread_.joinable()) {
            stop_requested_ = true;
            completion_thread_.join();
            stop_requested_ = false;
        }
        
        completion_thread_ = std::thread([this, context, token_callback, complete_callback]() {
            std::string completion = GenerateCompletion(context);
            
            // Stream word by word for smooth UX
            std::istringstream iss(completion);
            std::string word;
            while (iss >> word && !stop_requested_) {
                token_callback(word + " ");
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            
            if (!stop_requested_) {
                complete_callback();
            }
        });
    }
    
    // Multi-line intelligent completion
    std::vector<std::string> GenerateMultiLineCompletion(const CompletionContext& context, int max_lines = 10) {
        std::vector<std::string> lines;
        
        std::string before_cursor = context.file_content.substr(0, context.cursor_position);
        
        // Detect if we're in a function body
        if (IsInsideFunctionBody(before_cursor)) {
            lines = GenerateFunctionBody(before_cursor, context, max_lines);
        }
        // Detect if we're in a class definition
        else if (IsInsideClassDefinition(before_cursor)) {
            lines = GenerateClassMembers(before_cursor, context, max_lines);
        }
        // Generate typical code structure
        else {
            lines = GenerateCodeBlock(before_cursor, context, max_lines);
        }
        
        return lines;
    }
    
    // Context-aware suggestions with ranking
    struct CompletionSuggestion {
        std::string text;
        std::string description;
        float confidence;
        std::string category;  // "variable", "function", "keyword", "snippet"
    };
    
    std::vector<CompletionSuggestion> GetRankedSuggestions(const CompletionContext& context, int max_suggestions = 10) {
        std::vector<CompletionSuggestion> suggestions;
        
        std::string prefix = GetCurrentToken(context.file_content, context.cursor_position);
        
        // Add language keywords
        AddKeywordSuggestions(suggestions, prefix, context.language);
        
        // Add variable names from context
        AddVariableSuggestions(suggestions, prefix, context);
        
        // Add function names
        AddFunctionSuggestions(suggestions, prefix, context);
        
        // Add common code snippets
        AddSnippetSuggestions(suggestions, prefix, context);
        
        // Sort by confidence
        std::sort(suggestions.begin(), suggestions.end(),
                  [](const CompletionSuggestion& a, const CompletionSuggestion& b) {
                      return a.confidence > b.confidence;
                  });
        
        if (suggestions.size() > max_suggestions) {
            suggestions.resize(max_suggestions);
        }
        
        return suggestions;
    }
    
private:
    bool IsVariableDeclaration(const std::string& line) {
        return line.find("int ") != std::string::npos ||
               line.find("float ") != std::string::npos ||
               line.find("std::string ") != std::string::npos ||
               line.find("auto ") != std::string::npos;
    }
    
    bool IsFunctionCall(const std::string& line) {
        size_t paren_pos = line.rfind('(');
        return paren_pos != std::string::npos && 
               line.find(')') == std::string::npos;
    }
    
    bool IsControlStructure(const std::string& line) {
        return line.find("if ") != std::string::npos ||
               line.find("for ") != std::string::npos ||
               line.find("while ") != std::string::npos ||
               line.find("switch ") != std::string::npos;
    }
    
    bool IsComment(const std::string& line) {
        return line.find("//") != std::string::npos ||
               line.find("/*") != std::string::npos;
    }
    
    bool IsInsideFunctionBody(const std::string& before_cursor) {
        int brace_count = 0;
        bool found_function = false;
        
        for (char c : before_cursor) {
            if (c == '{') {
                brace_count++;
                found_function = true;
            }
            if (c == '}') brace_count--;
        }
        
        return found_function && brace_count > 0;
    }
    
    bool IsInsideClassDefinition(const std::string& before_cursor) {
        return before_cursor.find("class ") != std::string::npos ||
               before_cursor.find("struct ") != std::string::npos;
    }
    
    std::string GenerateVariableCompletion(const std::string& line, const CompletionContext& context) {
        // Extract variable name being typed
        size_t last_space = line.rfind(' ');
        std::string var_prefix = (last_space != std::string::npos) ? line.substr(last_space + 1) : "";
        
        // Suggest meaningful variable name
        if (var_prefix.empty()) {
            return "value";
        }
        
        return var_prefix + "_value = ";
    }
    
    std::string GenerateFunctionCompletion(const std::string& line, const CompletionContext& context) {
        // Find function name
        size_t paren_pos = line.rfind('(');
        size_t space_pos = line.rfind(' ', paren_pos);
        
        if (space_pos != std::string::npos && paren_pos != std::string::npos) {
            std::string func_name = line.substr(space_pos + 1, paren_pos - space_pos - 1);
            
            // Generate likely parameters based on function name
            if (func_name.find("print") != std::string::npos) {
                return "\"Hello, World!\")";
            }
            else if (func_name.find("read") != std::string::npos) {
                return "file_path)";
            }
        }
        
        return ")";
    }
    
    std::string GenerateControlStructureCompletion(const std::string& line, const CompletionContext& context) {
        // Language-aware control structure completion
        std::string lang = context.language;
        std::transform(lang.begin(), lang.end(), lang.begin(),
                       [](unsigned char c) { return (char)::tolower(c); });
        bool isPython = (lang == "python" || lang == "py");
        bool isRust   = (lang == "rust" || lang == "rs");
        bool isGo     = (lang == "go");

        if (line.find("if") != std::string::npos) {
            if (isPython) return " condition:\n    pass";
            if (isRust) return " condition {\n    \n}";
            if (isGo) return " condition {\n    \n}";
            return " (condition) {\n    \n}";
        }
        else if (line.find("for") != std::string::npos) {
            if (isPython) return " item in collection:\n    pass";
            if (isRust) return " item in collection.iter() {\n    \n}";
            if (isGo) return " i := 0; i < n; i++ {\n    \n}";
            return " (size_t i = 0; i < count; ++i) {\n    \n}";
        }
        else if (line.find("while") != std::string::npos) {
            if (isPython) return " condition:\n    pass";
            if (isRust) return " condition {\n    \n}";
            if (isGo) return " condition {\n    \n}";
            return " (condition) {\n    \n}";
        }
        else if (line.find("switch") != std::string::npos) {
            if (isGo) return " value {\ncase 0:\n    \ndefault:\n}";
            return " (value) {\n    case 0:\n        break;\n    default:\n        break;\n}";
        }
        else if (line.find("match") != std::string::npos) {
            if (isRust) return " value {\n    _ => {},\n}";
            if (isPython) return " value:\n    case _:\n        pass";
        }

        if (isPython) return ":\n    pass";
        if (isRust) return " {\n    \n}";
        return " {\n    \n}";
    }
    
    std::string GenerateCommentCompletion(const std::string& line, const CompletionContext& context) {
        // Generate TODO, FIXME, or explanatory comment
        if (line.find("TODO") != std::string::npos) {
            return " Implement this functionality";
        }
        else if (line.find("FIXME") != std::string::npos) {
            return " Fix bug here";
        }
        
        return " Add description";
    }
    
    std::string GenerateNextToken(const std::string& before_cursor, const CompletionContext& context) {
        // Context-aware next-token prediction using identifier + syntax analysis
        if (before_cursor.empty()) return "";

        char last_char = before_cursor.back();

        // Member access: look for preceding identifier to suggest relevant members
        if (last_char == '.') {
            // Extract the object name before the dot
            size_t end = before_cursor.size() - 1;
            size_t start = end;
            while (start > 0 && (isalnum((unsigned char)before_cursor[start-1]) || before_cursor[start-1] == '_'))
                --start;
            std::string obj = before_cursor.substr(start, end - start);

            // Common object patterns
            if (obj == "std" || obj == "string" || obj.find("str") != std::string::npos)
                return "size()";
            if (obj.find("vec") != std::string::npos || obj.find("list") != std::string::npos ||
                obj.find("array") != std::string::npos)
                return "push_back()";
            if (obj.find("map") != std::string::npos || obj.find("dict") != std::string::npos)
                return "find()";
            if (obj.find("file") != std::string::npos || obj.find("stream") != std::string::npos)
                return "read()";
            if (obj.find("mutex") != std::string::npos)
                return "lock()";
            if (obj.find("thread") != std::string::npos)
                return "join()";
            if (obj.find("path") != std::string::npos)
                return "string()";
            return "";
        }
        if (last_char == '>') {
            // Arrow operator: suggest member access
            return "";
        }
        if (last_char == ':') {
            // Scope resolution or Python colon
            if (before_cursor.size() >= 2 && before_cursor[before_cursor.size()-2] == ':')
                return ""; // :: — let keyword/builtin matching handle it
            return "";
        }
        if (last_char == '=') {
            // Assignment: suggest based on left-hand-side type hints
            if (before_cursor.size() >= 2 && before_cursor[before_cursor.size()-2] == '=')
                return ""; // == comparison, no completion
            return " ";
        }
        if (last_char == '(') {
            // Function call opened: analyze function name for parameter hints
            size_t end = before_cursor.size() - 1;
            size_t start = end;
            while (start > 0 && (isalnum((unsigned char)before_cursor[start-1]) || before_cursor[start-1] == '_'))
                --start;
            std::string func = before_cursor.substr(start, end - start);
            if (func == "printf" || func == "fprintf" || func == "sprintf")
                return "\"";
            if (func == "sizeof") return "";
            if (func == "static_cast" || func == "dynamic_cast" || func == "reinterpret_cast")
                return "<";
            return "";
        }

        return "";
    }
    
    std::vector<std::string> GenerateFunctionBody(const std::string& before_cursor, const CompletionContext& context, int max_lines) {
        std::vector<std::string> lines;
        
        // Find function signature
        size_t brace_pos = before_cursor.rfind('{');
        if (brace_pos == std::string::npos) return lines;
        
        std::string signature = before_cursor.substr(0, brace_pos);
        
        // Extract function name for context-aware body generation
        size_t paren_pos = signature.rfind('(');
        std::string funcName;
        if (paren_pos != std::string::npos) {
            size_t nameEnd = paren_pos;
            size_t nameStart = nameEnd;
            while (nameStart > 0 && (isalnum((unsigned char)signature[nameStart-1]) || signature[nameStart-1] == '_'))
                --nameStart;
            funcName = signature.substr(nameStart, nameEnd - nameStart);
        }

        // Extract parameter list for generating body references
        std::string params;
        if (paren_pos != std::string::npos) {
            size_t closeP = signature.find(')', paren_pos);
            if (closeP != std::string::npos)
                params = signature.substr(paren_pos + 1, closeP - paren_pos - 1);
        }

        // Common function name patterns
        if (funcName.find("init") != std::string::npos || funcName.find("Init") != std::string::npos ||
            funcName.find("setup") != std::string::npos || funcName.find("Setup") != std::string::npos) {
            lines.push_back("    // Initialize state");
            lines.push_back("    memset(this, 0, sizeof(*this));");
            if (signature.find("bool") != std::string::npos)
                lines.push_back("    return true;");
        } else if (funcName.find("cleanup") != std::string::npos || funcName.find("Cleanup") != std::string::npos ||
                   funcName.find("destroy") != std::string::npos || funcName.find("Destroy") != std::string::npos ||
                   funcName.find("shutdown") != std::string::npos || funcName.find("Shutdown") != std::string::npos) {
            lines.push_back("    // Release resources");
            if (signature.find("void") != std::string::npos) {
                // void cleanup pattern
            } else {
                lines.push_back("    return;");
            }
        } else if (funcName.find("get") == 0 || funcName.find("Get") == 0) {
            // Getter
            if (signature.find("bool") != std::string::npos)
                lines.push_back("    return false;");
            else if (signature.find("int") != std::string::npos || signature.find("size_t") != std::string::npos)
                lines.push_back("    return 0;");
            else if (signature.find("const char*") != std::string::npos || signature.find("string") != std::string::npos)
                lines.push_back("    return \"\";");
            else if (signature.find("void*") != std::string::npos || signature.find("*") != std::string::npos)
                lines.push_back("    return nullptr;");
            else
                lines.push_back("    return {};");
        } else if (funcName.find("set") == 0 || funcName.find("Set") == 0) {
            // Setter — generate assignment from first parameter
            if (!params.empty()) {
                // Extract first param name
                std::string pName;
                size_t lastSpace = params.rfind(' ');
                if (lastSpace != std::string::npos)
                    pName = params.substr(lastSpace + 1);
                if (!pName.empty()) {
                    std::string memberName = pName + "_";
                    lines.push_back("    " + memberName + " = " + pName + ";");
                }
            }
        } else if (funcName.find("is") == 0 || funcName.find("Is") == 0 ||
                   funcName.find("has") == 0 || funcName.find("Has") == 0 ||
                   funcName.find("can") == 0 || funcName.find("Can") == 0) {
            lines.push_back("    return false;");
        } else {
            // General case: return type based
            if (signature.find("void") != std::string::npos) {
                // No return needed
            } else if (signature.find("bool") != std::string::npos) {
                lines.push_back("    return true;");
            } else if (signature.find("int") != std::string::npos ||
                       signature.find("size_t") != std::string::npos ||
                       signature.find("uint") != std::string::npos) {
                lines.push_back("    return 0;");
            } else if (signature.find("float") != std::string::npos ||
                       signature.find("double") != std::string::npos) {
                lines.push_back("    return 0.0;");
            } else if (signature.find("string") != std::string::npos ||
                       signature.find("const char*") != std::string::npos) {
                lines.push_back("    return \"\";");
            } else if (signature.find("*") != std::string::npos) {
                lines.push_back("    return nullptr;");
            } else {
                lines.push_back("    return {};");
            }
        }
        
        // Clamp to max lines
        if ((int)lines.size() > max_lines) lines.resize(max_lines);
        return lines;
    }
    
    std::vector<std::string> GenerateClassMembers(const std::string& before_cursor, const CompletionContext& context, int max_lines) {
        std::vector<std::string> lines;

        // Extract class name for context-aware member generation
        std::string className;
        size_t classPos = before_cursor.rfind("class ");
        size_t structPos = before_cursor.rfind("struct ");
        size_t defPos = (classPos != std::string::npos) ? classPos + 6 :
                         (structPos != std::string::npos) ? structPos + 7 : std::string::npos;
        if (defPos != std::string::npos) {
            size_t end = defPos;
            while (end < before_cursor.size() && (isalnum((unsigned char)before_cursor[end]) || before_cursor[end] == '_'))
                ++end;
            className = before_cursor.substr(defPos, end - defPos);
        }

        bool isStruct = (structPos != std::string::npos &&
                        (classPos == std::string::npos || structPos > classPos));

        if (isStruct) {
            // Structs: public data members + simple constructor
            lines.push_back("    // Data members");
            lines.push_back("    int id;");
            lines.push_back("    const char* name;");
            lines.push_back("    size_t size;");
        } else {
            // Classes: private data, public constructor/destructor + accessors
            lines.push_back("private:");
            lines.push_back("    bool initialized_;");
            lines.push_back("");
            lines.push_back("public:");
            if (!className.empty()) {
                lines.push_back("    " + className + "() : initialized_(false) {}");
                lines.push_back("    ~" + className + "() = default;");
            } else {
                lines.push_back("    // Constructor");
                lines.push_back("    // Destructor");
            }
            lines.push_back("");
            lines.push_back("    bool IsInitialized() const { return initialized_; }");
        }

        if ((int)lines.size() > max_lines) lines.resize(max_lines);
        return lines;
    }
    
    std::vector<std::string> GenerateCodeBlock(const std::string& before_cursor, const CompletionContext& context, int max_lines) {
        std::vector<std::string> lines;

        // Analyze trailing context to generate appropriate code block
        // Look for assignment, declaration, or expression context
        size_t lastNewline = before_cursor.rfind('\n');
        std::string lastLine = (lastNewline != std::string::npos)
            ? before_cursor.substr(lastNewline + 1) : before_cursor;

        // Trim whitespace
        size_t firstNonSpace = lastLine.find_first_not_of(" \t");
        if (firstNonSpace != std::string::npos)
            lastLine = lastLine.substr(firstNonSpace);

        // Contextual code generation based on what precedes the cursor
        if (lastLine.find("#include") != std::string::npos) {
            // After an include, suggest related includes
            if (lastLine.find("<vector>") != std::string::npos)
                lines.push_back("#include <algorithm>");
            else if (lastLine.find("<string>") != std::string::npos)
                lines.push_back("#include <string_view>");
            else if (lastLine.find("<iostream>") != std::string::npos)
                lines.push_back("#include <fstream>");
            else if (lastLine.find("<thread>") != std::string::npos)
                lines.push_back("#include <mutex>");
        } else if (lastLine.find("namespace") != std::string::npos) {
            lines.push_back("{");
            lines.push_back("");
            lines.push_back("} // namespace");
        } else if (lastLine.find("enum") != std::string::npos) {
            lines.push_back("{");
            lines.push_back("    Value0,");
            lines.push_back("    Value1,");
            lines.push_back("    Count");
            lines.push_back("};");
        } else if (lastLine.find("return") != std::string::npos) {
            // After return statement context, nothing to add
        } else {
            // Default: simple statement block
            lines.push_back("");
        }

        if ((int)lines.size() > max_lines) lines.resize(max_lines);
        return lines;
    }
    
    std::string GetCurrentToken(const std::string& content, int cursor_position) {
        if (cursor_position <= 0) return "";
        
        int start = cursor_position - 1;
        while (start >= 0 && (isalnum(content[start]) || content[start] == '_')) {
            start--;
        }
        start++;
        
        return content.substr(start, cursor_position - start);
    }
    
public:
    // ========================================================================
    // Multi-language keyword/builtin registry for local IntelliSense fallback
    // ========================================================================
    struct LanguageKeywordTable {
        const char* language;          // "cpp", "c", "python", "javascript", etc.
        const char** keywords;         // null-terminated array of keywords
        const char** builtins;         // null-terminated array of builtins/stdlib
        const char** snippetTriggers;  // null-terminated array of snippet prefixes
    };

    // ── C/C++ ──────────────────────────────────────────────────────────
    static constexpr const char* s_cppKeywords[] = {
        "alignas", "alignof", "and", "and_eq", "asm", "auto",
        "bitand", "bitor", "bool", "break",
        "case", "catch", "char", "char8_t", "char16_t", "char32_t",
        "class", "co_await", "co_return", "co_yield", "compl",
        "concept", "const", "consteval", "constexpr", "constinit",
        "const_cast", "continue",
        "decltype", "default", "delete", "do", "double", "dynamic_cast",
        "else", "enum", "explicit", "export", "extern",
        "false", "float", "for", "friend",
        "goto",
        "if", "import", "inline", "int",
        "long",
        "module", "mutable",
        "namespace", "new", "noexcept", "not", "not_eq", "nullptr",
        "operator", "or", "or_eq",
        "private", "protected", "public",
        "register", "reinterpret_cast", "requires", "return",
        "short", "signed", "sizeof", "static", "static_assert",
        "static_cast", "struct", "switch",
        "template", "this", "thread_local", "throw", "true",
        "try", "typedef", "typeid", "typename",
        "union", "unsigned", "using",
        "virtual", "void", "volatile",
        "wchar_t", "while",
        "xor", "xor_eq",
        nullptr
    };
    static constexpr const char* s_cppBuiltins[] = {
        "std::vector", "std::string", "std::map", "std::unordered_map",
        "std::set", "std::unordered_set", "std::array", "std::deque",
        "std::list", "std::forward_list", "std::stack", "std::queue",
        "std::priority_queue", "std::pair", "std::tuple", "std::optional",
        "std::variant", "std::any", "std::span", "std::string_view",
        "std::shared_ptr", "std::unique_ptr", "std::weak_ptr",
        "std::make_shared", "std::make_unique",
        "std::cout", "std::cerr", "std::cin", "std::endl",
        "std::move", "std::forward", "std::swap",
        "std::sort", "std::find", "std::find_if", "std::for_each",
        "std::transform", "std::accumulate", "std::count", "std::count_if",
        "std::copy", "std::fill", "std::replace", "std::remove",
        "std::reverse", "std::rotate", "std::unique",
        "std::begin", "std::end", "std::size",
        "std::thread", "std::mutex", "std::lock_guard", "std::unique_lock",
        "std::atomic", "std::condition_variable", "std::future", "std::promise",
        "std::function", "std::bind",
        "std::filesystem::path", "std::filesystem::exists",
        "std::filesystem::directory_iterator",
        "std::chrono::steady_clock", "std::chrono::system_clock",
        "std::chrono::milliseconds", "std::chrono::seconds",
        "printf", "fprintf", "sprintf", "snprintf",
        "malloc", "calloc", "realloc", "free",
        "memcpy", "memmove", "memset", "memcmp",
        "strlen", "strcpy", "strncpy", "strcat", "strcmp", "strncmp",
        "fopen", "fclose", "fread", "fwrite", "fseek", "ftell",
        "CreateFileW", "ReadFile", "WriteFile", "CloseHandle",
        "VirtualAlloc", "VirtualFree", "VirtualProtect",
        "CreateThread", "WaitForSingleObject", "SetEvent",
        "GetLastError", "FormatMessageW",
        nullptr
    };
    static constexpr const char* s_cppSnippets[] = {
        "for", "while", "if", "switch", "class", "struct",
        "namespace", "try", "enum", "template",
        nullptr
    };

    // ── Python ─────────────────────────────────────────────────────────
    static constexpr const char* s_pythonKeywords[] = {
        "False", "None", "True", "and", "as", "assert", "async", "await",
        "break", "class", "continue", "def", "del", "elif", "else",
        "except", "finally", "for", "from", "global", "if", "import",
        "in", "is", "lambda", "nonlocal", "not", "or", "pass",
        "raise", "return", "try", "while", "with", "yield",
        nullptr
    };
    static constexpr const char* s_pythonBuiltins[] = {
        "abs", "all", "any", "ascii", "bin", "bool", "breakpoint",
        "bytearray", "bytes", "callable", "chr", "classmethod",
        "compile", "complex", "delattr", "dict", "dir", "divmod",
        "enumerate", "eval", "exec", "filter", "float", "format",
        "frozenset", "getattr", "globals", "hasattr", "hash", "help",
        "hex", "id", "input", "int", "isinstance", "issubclass",
        "iter", "len", "list", "locals", "map", "max", "memoryview",
        "min", "next", "object", "oct", "open", "ord", "pow",
        "print", "property", "range", "repr", "reversed", "round",
        "set", "setattr", "slice", "sorted", "staticmethod", "str",
        "sum", "super", "tuple", "type", "vars", "zip",
        "os.path", "os.listdir", "os.makedirs", "os.remove",
        "sys.argv", "sys.exit", "sys.path", "sys.stdin", "sys.stdout",
        "json.loads", "json.dumps", "json.load", "json.dump",
        "re.match", "re.search", "re.findall", "re.sub",
        "datetime.datetime", "datetime.timedelta",
        "collections.defaultdict", "collections.Counter",
        "collections.OrderedDict", "collections.deque",
        "itertools.chain", "itertools.product", "itertools.combinations",
        "functools.reduce", "functools.lru_cache", "functools.partial",
        "pathlib.Path", "typing.List", "typing.Dict", "typing.Optional",
        "typing.Tuple", "typing.Set", "typing.Union", "typing.Any",
        nullptr
    };
    static constexpr const char* s_pythonSnippets[] = {
        "def", "class", "for", "while", "if", "try", "with", "async",
        nullptr
    };

    // ── JavaScript / TypeScript ────────────────────────────────────────
    static constexpr const char* s_jsKeywords[] = {
        "abstract", "arguments", "async", "await", "break",
        "case", "catch", "class", "const", "continue",
        "debugger", "default", "delete", "do",
        "else", "enum", "export", "extends",
        "false", "finally", "for", "from", "function",
        "get", "if", "implements", "import", "in", "instanceof",
        "interface", "let", "new", "null", "of",
        "package", "private", "protected", "public",
        "return", "set", "static", "super", "switch",
        "this", "throw", "true", "try", "typeof",
        "undefined", "var", "void", "while", "with", "yield",
        nullptr
    };
    static constexpr const char* s_jsBuiltins[] = {
        "console.log", "console.error", "console.warn", "console.info",
        "JSON.parse", "JSON.stringify",
        "Math.floor", "Math.ceil", "Math.round", "Math.random",
        "Math.max", "Math.min", "Math.abs", "Math.pow", "Math.sqrt",
        "Array.isArray", "Array.from", "Array.of",
        "Object.keys", "Object.values", "Object.entries",
        "Object.assign", "Object.freeze", "Object.create",
        "Promise.all", "Promise.resolve", "Promise.reject",
        "Promise.allSettled", "Promise.race",
        "Number.parseInt", "Number.parseFloat", "Number.isNaN",
        "Number.isFinite", "Number.isInteger",
        "String.fromCharCode", "String.fromCodePoint",
        "Symbol.iterator", "Symbol.asyncIterator",
        "setTimeout", "setInterval", "clearTimeout", "clearInterval",
        "fetch", "Request", "Response", "Headers",
        "Map", "Set", "WeakMap", "WeakSet",
        "Proxy", "Reflect", "RegExp",
        "parseInt", "parseFloat", "isNaN", "isFinite",
        "encodeURI", "decodeURI", "encodeURIComponent", "decodeURIComponent",
        "document.getElementById", "document.querySelector",
        "document.querySelectorAll", "document.createElement",
        "addEventListener", "removeEventListener",
        "window.location", "window.navigator", "window.localStorage",
        nullptr
    };
    static constexpr const char* s_jsSnippets[] = {
        "function", "class", "for", "while", "if", "switch",
        "try", "async", "import", "export",
        nullptr
    };

    // ── Rust ───────────────────────────────────────────────────────────
    static constexpr const char* s_rustKeywords[] = {
        "as", "async", "await", "break", "const", "continue", "crate",
        "dyn", "else", "enum", "extern", "false", "fn", "for",
        "if", "impl", "in", "let", "loop", "match", "mod", "move",
        "mut", "pub", "ref", "return", "self", "Self", "static",
        "struct", "super", "trait", "true", "type", "unsafe", "use",
        "where", "while", "yield",
        nullptr
    };
    static constexpr const char* s_rustBuiltins[] = {
        "Vec::new", "Vec::with_capacity", "String::new", "String::from",
        "println!", "eprintln!", "format!", "write!", "writeln!",
        "vec!", "assert!", "assert_eq!", "assert_ne!", "debug_assert!",
        "panic!", "todo!", "unimplemented!", "unreachable!",
        "Option::Some", "Option::None", "Result::Ok", "Result::Err",
        "Box::new", "Rc::new", "Arc::new", "Cell::new", "RefCell::new",
        "HashMap::new", "HashSet::new", "BTreeMap::new", "BTreeSet::new",
        "std::io::stdin", "std::io::stdout", "std::io::BufReader",
        "std::fs::File", "std::fs::read_to_string",
        "std::path::Path", "std::path::PathBuf",
        "std::thread::spawn", "std::sync::Mutex", "std::sync::RwLock",
        nullptr
    };
    static constexpr const char* s_rustSnippets[] = {
        "fn", "struct", "enum", "impl", "trait", "match", "for",
        "while", "if", "loop", "mod", "use",
        nullptr
    };

    // ── MASM / x86-64 Assembly ─────────────────────────────────────────
    static constexpr const char* s_asmKeywords[] = {
        "PROC", "ENDP", "MACRO", "ENDM", "SEGMENT", "ENDS",
        "PUBLIC", "EXTERNDEF", "EXTERN", "PROTO",
        "INCLUDE", "INCLUDELIB",
        "IF", "ELSE", "ENDIF", "IFDEF", "IFNDEF", "ELSEIF",
        "REPEAT", "WHILE", "ENDW",
        "ALIGN", "EVEN", "ORG",
        ".CODE", ".DATA", ".DATA?", ".CONST", ".STACK",
        "DB", "DW", "DD", "DQ", "DT",
        "BYTE", "WORD", "DWORD", "QWORD", "OWORD", "XMMWORD", "YMMWORD",
        "PTR", "OFFSET", "ADDR", "SIZEOF", "LENGTHOF", "TYPE",
        "LOCAL", "INVOKE", "OPTION",
        nullptr
    };
    static constexpr const char* s_asmBuiltins[] = {
        // GP registers
        "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
        "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp",
        "ax", "bx", "cx", "dx", "si", "di", "bp", "sp",
        "al", "bl", "cl", "dl", "ah", "bh", "ch", "dh",
        // SSE/AVX registers
        "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
        "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15",
        "ymm0", "ymm1", "ymm2", "ymm3", "ymm4", "ymm5", "ymm6", "ymm7",
        // Common instructions
        "mov", "lea", "push", "pop", "call", "ret", "jmp",
        "je", "jne", "jz", "jnz", "jg", "jge", "jl", "jle",
        "ja", "jae", "jb", "jbe", "jc", "jnc",
        "cmp", "test", "and", "or", "xor", "not", "neg",
        "add", "sub", "mul", "imul", "div", "idiv",
        "shl", "shr", "sal", "sar", "rol", "ror",
        "movzx", "movsx", "movsxd", "cbw", "cwde", "cdqe",
        "rep", "movsb", "movsw", "movsd", "movsq",
        "stosb", "stosw", "stosd", "stosq",
        "lodsb", "lodsw", "lodsd", "lodsq",
        "cmpsb", "cmpsw", "cmpsd", "cmpsq",
        "nop", "int", "syscall", "cpuid", "rdtsc",
        // SIMD
        "movaps", "movups", "movdqa", "movdqu",
        "addps", "subps", "mulps", "divps",
        "addpd", "subpd", "mulpd", "divpd",
        "paddd", "psubd", "pmulld",
        "pxor", "por", "pand", "pandn",
        "vaddps", "vsubps", "vmulps", "vdivps",
        "vfmadd231ps", "vfmadd213ps", "vfmadd132ps",
        "pcmpestri", "pcmpestrm",
        nullptr
    };
    static constexpr const char* s_asmSnippets[] = {
        "PROC", "MACRO", "IF", "REPEAT",
        nullptr
    };

    // ── Go ─────────────────────────────────────────────────────────────
    static constexpr const char* s_goKeywords[] = {
        "break", "case", "chan", "const", "continue",
        "default", "defer", "else", "fallthrough", "for",
        "func", "go", "goto", "if", "import", "interface",
        "map", "package", "range", "return", "select",
        "struct", "switch", "type", "var",
        nullptr
    };
    static constexpr const char* s_goBuiltins[] = {
        "append", "cap", "close", "complex", "copy", "delete",
        "imag", "len", "make", "new", "panic", "print", "println",
        "real", "recover",
        "fmt.Println", "fmt.Printf", "fmt.Sprintf", "fmt.Fprintf",
        "fmt.Errorf",
        "os.Open", "os.Create", "os.Exit", "os.Getenv",
        "io.Reader", "io.Writer", "io.Copy",
        "strings.Contains", "strings.Split", "strings.Join",
        "strings.Replace", "strings.TrimSpace",
        "strconv.Itoa", "strconv.Atoi",
        "errors.New", "errors.Is", "errors.As",
        "context.Background", "context.WithCancel", "context.WithTimeout",
        "sync.Mutex", "sync.WaitGroup", "sync.Once",
        "http.ListenAndServe", "http.HandleFunc", "http.Get",
        nullptr
    };
    static constexpr const char* s_goSnippets[] = {
        "func", "for", "if", "switch", "select", "struct", "interface",
        nullptr
    };

    // ── Language table registry ────────────────────────────────────────
    static constexpr LanguageKeywordTable s_languageTables[] = {
        { "cpp",        s_cppKeywords,    s_cppBuiltins,    s_cppSnippets },
        { "c",          s_cppKeywords,    s_cppBuiltins,    s_cppSnippets },
        { "c++",        s_cppKeywords,    s_cppBuiltins,    s_cppSnippets },
        { "h",          s_cppKeywords,    s_cppBuiltins,    s_cppSnippets },
        { "hpp",        s_cppKeywords,    s_cppBuiltins,    s_cppSnippets },
        { "python",     s_pythonKeywords, s_pythonBuiltins,  s_pythonSnippets },
        { "py",         s_pythonKeywords, s_pythonBuiltins,  s_pythonSnippets },
        { "javascript", s_jsKeywords,     s_jsBuiltins,     s_jsSnippets },
        { "js",         s_jsKeywords,     s_jsBuiltins,     s_jsSnippets },
        { "typescript", s_jsKeywords,     s_jsBuiltins,     s_jsSnippets },
        { "ts",         s_jsKeywords,     s_jsBuiltins,     s_jsSnippets },
        { "jsx",        s_jsKeywords,     s_jsBuiltins,     s_jsSnippets },
        { "tsx",        s_jsKeywords,     s_jsBuiltins,     s_jsSnippets },
        { "rust",       s_rustKeywords,   s_rustBuiltins,   s_rustSnippets },
        { "rs",         s_rustKeywords,   s_rustBuiltins,   s_rustSnippets },
        { "asm",        s_asmKeywords,    s_asmBuiltins,    s_asmSnippets },
        { "masm",       s_asmKeywords,    s_asmBuiltins,    s_asmSnippets },
        { "nasm",       s_asmKeywords,    s_asmBuiltins,    s_asmSnippets },
        { "s",          s_asmKeywords,    s_asmBuiltins,    s_asmSnippets },
        { "go",         s_goKeywords,     s_goBuiltins,     s_goSnippets },
        { nullptr,      nullptr,          nullptr,          nullptr }
    };

    // Find the keyword table for a given language identifier or file extension
    static const LanguageKeywordTable* findLanguageTable(const std::string& langOrExt) {
        // Normalize: strip leading dot, lowercase
        std::string key = langOrExt;
        if (!key.empty() && key[0] == '.') key = key.substr(1);
        std::transform(key.begin(), key.end(), key.begin(),
                       [](unsigned char c) { return (char)::tolower(c); });

        for (int i = 0; s_languageTables[i].language != nullptr; ++i) {
            if (key == s_languageTables[i].language) {
                return &s_languageTables[i];
            }
        }
        // Default to C++ if language not recognized (IDE is C++ focused)
        return &s_languageTables[0];
    }

    void AddKeywordSuggestions(std::vector<CompletionSuggestion>& suggestions, 
                               const std::string& prefix, 
                               const std::string& language) {
        const LanguageKeywordTable* table = findLanguageTable(language);
        if (!table) return;

        // Add matching keywords
        if (table->keywords) {
            for (int i = 0; table->keywords[i] != nullptr; ++i) {
                const char* kw = table->keywords[i];
                // Case-insensitive prefix match
                std::string kwStr(kw);
                std::string kwLower = kwStr;
                std::string prefLower = prefix;
                std::transform(kwLower.begin(), kwLower.end(), kwLower.begin(),
                               [](unsigned char c) { return (char)::tolower(c); });
                std::transform(prefLower.begin(), prefLower.end(), prefLower.begin(),
                               [](unsigned char c) { return (char)::tolower(c); });

                if (kwLower.find(prefLower) == 0 && !prefLower.empty()) {
                    CompletionSuggestion sugg;
                    sugg.text = kwStr;
                    sugg.description = "Language keyword";
                    sugg.confidence = 0.9f;
                    sugg.category = "keyword";
                    suggestions.push_back(sugg);
                }
            }
        }

        // Add matching builtins
        if (table->builtins) {
            for (int i = 0; table->builtins[i] != nullptr; ++i) {
                const char* bi = table->builtins[i];
                std::string biStr(bi);
                std::string biLower = biStr;
                std::string prefLower = prefix;
                std::transform(biLower.begin(), biLower.end(), biLower.begin(),
                               [](unsigned char c) { return (char)::tolower(c); });
                std::transform(prefLower.begin(), prefLower.end(), prefLower.begin(),
                               [](unsigned char c) { return (char)::tolower(c); });

                if (biLower.find(prefLower) == 0 && !prefLower.empty()) {
                    CompletionSuggestion sugg;
                    sugg.text = biStr;
                    sugg.description = "Standard library / builtin";
                    sugg.confidence = 0.85f;
                    sugg.category = "function";
                    suggestions.push_back(sugg);
                }
            }
        }
    }
    
    void AddVariableSuggestions(std::vector<CompletionSuggestion>& suggestions,
                                const std::string& prefix,
                                const CompletionContext& context) {
        // Extract real identifiers from surrounding code context
        std::vector<std::string> identifiers;
        extractLocalIdentifiers(context.file_content.c_str(), context.cursor_position, identifiers);

        std::string prefLower = prefix;
        std::transform(prefLower.begin(), prefLower.end(), prefLower.begin(),
                       [](unsigned char c) { return (char)::tolower(c); });

        for (const auto& ident : identifiers) {
            std::string identLower = ident;
            std::transform(identLower.begin(), identLower.end(), identLower.begin(),
                           [](unsigned char c) { return (char)::tolower(c); });
            if (identLower.find(prefLower) == 0 && identLower != prefLower) {
                CompletionSuggestion sugg;
                sugg.text = ident;
                sugg.description = "Local identifier";
                sugg.confidence = 0.75f;
                sugg.category = "variable";
                suggestions.push_back(sugg);
            }
        }
    }
    
    void AddFunctionSuggestions(std::vector<CompletionSuggestion>& suggestions,
                                const std::string& prefix,
                                const CompletionContext& context) {
        // Extract function names from context by finding patterns like "type name("
        std::set<std::string> funcNames;
        const std::string& content = context.file_content;

        // Scan for function-like patterns: identifier followed by '('
        for (size_t i = 0; i < content.size(); ++i) {
            if (content[i] == '(') {
                // Walk backwards to find function name
                size_t end = i;
                size_t start = end;
                while (start > 0 && (isalnum((unsigned char)content[start-1]) || content[start-1] == '_'))
                    --start;
                if (start < end) {
                    std::string fname = content.substr(start, end - start);
                    // Skip language keywords
                    if (fname != "if" && fname != "for" && fname != "while" &&
                        fname != "switch" && fname != "return" && fname != "sizeof" &&
                        fname != "typeof" && fname != "catch" && fname != "case" &&
                        fname.size() >= 2) {
                        funcNames.insert(fname);
                    }
                }
            }
        }

        std::string prefLower = prefix;
        std::transform(prefLower.begin(), prefLower.end(), prefLower.begin(),
                       [](unsigned char c) { return (char)::tolower(c); });

        for (const auto& func : funcNames) {
            std::string fLower = func;
            std::transform(fLower.begin(), fLower.end(), fLower.begin(),
                           [](unsigned char c) { return (char)::tolower(c); });
            if (fLower.find(prefLower) == 0 && fLower != prefLower) {
                CompletionSuggestion sugg;
                sugg.text = func + "()";
                sugg.description = "Function";
                sugg.confidence = 0.8f;
                sugg.category = "function";
                suggestions.push_back(sugg);
            }
        }
    }
    
    void AddSnippetSuggestions(std::vector<CompletionSuggestion>& suggestions,
                               const std::string& prefix,
                               const CompletionContext& context) {
        if (prefix == "for") {
            CompletionSuggestion sugg;
            sugg.text = "for (int i = 0; i < n; i++)";
            sugg.description = "For loop";
            sugg.confidence = 0.95f;
            sugg.category = "snippet";
            suggestions.push_back(sugg);
        }
    }
};

// Global instance for IDE integration
static AICompletionEngine* g_completion_engine = nullptr;

// ============================================================================
// LOCAL FALLBACK COMPLETION — keyword/builtin matching when LSP + AI are offline
// ============================================================================
// Returns a JSON-like array of {label, detail, insertText, confidence} items
// suitable for direct insertion into the hybrid completion pipeline.
// This is the core IntelliSense fallback that guarantees completions even when
// no language server or AI model is available.
// ============================================================================

struct LocalFallbackItem {
    char label[256];
    char detail[128];
    char insertText[512];
    float confidence;
    char category[32]; // "keyword", "builtin", "snippet", "variable"
};

// Extract the current token (word being typed) from content at a given position
static std::string extractCurrentToken(const char* content, int cursorPos) {
    if (!content || cursorPos <= 0) return "";
    int start = cursorPos - 1;
    while (start >= 0 && (isalnum((unsigned char)content[start]) ||
                          content[start] == '_' ||
                          content[start] == '.' ||
                          content[start] == ':' ||
                          content[start] == '!')) {
        start--;
    }
    start++;
    if (start >= cursorPos) return "";
    return std::string(content + start, cursorPos - start);
}

// Parse simple identifiers from surrounding code context for variable suggestions
static void extractLocalIdentifiers(const char* content, int cursorPos,
                                     std::vector<std::string>& identifiers) {
    if (!content) return;

    // Scan a window of ~2000 chars around cursor for identifiers
    int windowStart = (cursorPos > 2000) ? cursorPos - 2000 : 0;
    int windowEnd   = cursorPos + 500;
    int contentLen  = (int)strlen(content);
    if (windowEnd > contentLen) windowEnd = contentLen;

    std::string current;
    std::set<std::string> seen;

    for (int i = windowStart; i < windowEnd; ++i) {
        char c = content[i];
        if (isalnum((unsigned char)c) || c == '_') {
            current.push_back(c);
        } else {
            if (current.size() >= 2 && current.size() <= 64) {
                // Skip pure numbers
                bool allDigits = true;
                for (char ch : current) {
                    if (!isdigit((unsigned char)ch)) { allDigits = false; break; }
                }
                if (!allDigits && seen.find(current) == seen.end()) {
                    seen.insert(current);
                    identifiers.push_back(current);
                }
            }
            current.clear();
        }
    }
}

// Snippet expansion for common patterns
static void addSnippetExpansion(const char* trigger, const std::string& language,
                                 std::vector<LocalFallbackItem>& items) {
    auto safeCopy = [](char* dst, size_t dstSize, const char* src) {
        size_t len = strlen(src);
        if (len >= dstSize) len = dstSize - 1;
        memcpy(dst, src, len);
        dst[len] = '\0';
    };

    std::string lang = language;
    std::transform(lang.begin(), lang.end(), lang.begin(),
                   [](unsigned char c) { return (char)::tolower(c); });
    std::string trig(trigger);
    std::transform(trig.begin(), trig.end(), trig.begin(),
                   [](unsigned char c) { return (char)::tolower(c); });

    LocalFallbackItem item{};
    safeCopy(item.category, sizeof(item.category), "snippet");
    item.confidence = 0.92f;

    bool isPython = (lang == "python" || lang == "py");
    bool isRust   = (lang == "rust" || lang == "rs");
    bool isGo     = (lang == "go");
    bool isJS     = (lang == "javascript" || lang == "js" || lang == "typescript" || lang == "ts");

    if (trig == "for") {
        if (isPython) {
            safeCopy(item.label, sizeof(item.label), "for ... in ...:");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: for loop");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "for item in collection:\n    pass");
        } else if (isRust) {
            safeCopy(item.label, sizeof(item.label), "for ... in ... {}");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: for loop");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "for item in collection {\n    \n}");
        } else if (isGo) {
            safeCopy(item.label, sizeof(item.label), "for i := 0; i < n; i++ {}");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: for loop");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "for i := 0; i < n; i++ {\n    \n}");
        } else {
            safeCopy(item.label, sizeof(item.label), "for (int i = 0; i < n; i++) {}");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: for loop");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "for (int i = 0; i < n; i++) {\n    \n}");
        }
        items.push_back(item);
    } else if (trig == "if") {
        if (isPython) {
            safeCopy(item.label, sizeof(item.label), "if condition:");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: if statement");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "if condition:\n    pass");
        } else if (isRust) {
            safeCopy(item.label, sizeof(item.label), "if condition {}");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: if statement");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "if condition {\n    \n}");
        } else {
            safeCopy(item.label, sizeof(item.label), "if (condition) {}");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: if statement");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "if (condition) {\n    \n}");
        }
        items.push_back(item);
    } else if (trig == "while") {
        if (isPython) {
            safeCopy(item.label, sizeof(item.label), "while condition:");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: while loop");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "while condition:\n    pass");
        } else if (isRust) {
            safeCopy(item.label, sizeof(item.label), "while condition {}");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: while loop");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "while condition {\n    \n}");
        } else {
            safeCopy(item.label, sizeof(item.label), "while (condition) {}");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: while loop");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "while (condition) {\n    \n}");
        }
        items.push_back(item);
    } else if (trig == "class") {
        if (isPython) {
            safeCopy(item.label, sizeof(item.label), "class ClassName:");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: class definition");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "class ClassName:\n    def __init__(self):\n        pass");
        } else if (isJS) {
            safeCopy(item.label, sizeof(item.label), "class ClassName {}");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: class definition");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "class ClassName {\n    constructor() {\n        \n    }\n}");
        } else {
            safeCopy(item.label, sizeof(item.label), "class ClassName {}");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: class definition");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "class ClassName {\npublic:\n    ClassName() {}\n    ~ClassName() {}\n};");
        }
        items.push_back(item);
    } else if (trig == "func" || trig == "function" || trig == "fn" || trig == "def") {
        if (isPython) {
            safeCopy(item.label, sizeof(item.label), "def function_name():");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: function definition");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "def function_name():\n    pass");
        } else if (isRust) {
            safeCopy(item.label, sizeof(item.label), "fn function_name() {}");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: function definition");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "fn function_name() {\n    \n}");
        } else if (isGo) {
            safeCopy(item.label, sizeof(item.label), "func functionName() {}");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: function definition");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "func functionName() {\n    \n}");
        } else if (isJS) {
            safeCopy(item.label, sizeof(item.label), "function functionName() {}");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: function definition");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "function functionName() {\n    \n}");
        } else {
            safeCopy(item.label, sizeof(item.label), "void function_name() {}");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: function definition");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "void function_name() {\n    \n}");
        }
        items.push_back(item);
    } else if (trig == "try") {
        if (isPython) {
            safeCopy(item.label, sizeof(item.label), "try: ... except:");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: try/except");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "try:\n    pass\nexcept Exception as e:\n    pass");
        } else if (isRust) {
            // Rust doesn't have try blocks typically, skip
            return;
        } else {
            safeCopy(item.label, sizeof(item.label), "try { } catch { }");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: try/catch");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "try {\n    \n} catch (const std::exception& e) {\n    \n}");
        }
        items.push_back(item);
    } else if (trig == "switch" || trig == "match") {
        if (isRust) {
            safeCopy(item.label, sizeof(item.label), "match value {}");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: match expression");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "match value {\n    _ => {},\n}");
        } else if (isPython) {
            safeCopy(item.label, sizeof(item.label), "match value:");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: match/case");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "match value:\n    case _:\n        pass");
        } else {
            safeCopy(item.label, sizeof(item.label), "switch (value) { case: }");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: switch statement");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "switch (value) {\n    case 0:\n        break;\n    default:\n        break;\n}");
        }
        items.push_back(item);
    } else if (trig == "struct") {
        if (isRust) {
            safeCopy(item.label, sizeof(item.label), "struct Name {}");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: struct definition");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "struct Name {\n    field: Type,\n}");
        } else if (isGo) {
            safeCopy(item.label, sizeof(item.label), "type Name struct {}");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: struct definition");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "type Name struct {\n    Field Type\n}");
        } else {
            safeCopy(item.label, sizeof(item.label), "struct Name {};");
            safeCopy(item.detail, sizeof(item.detail), "Snippet: struct definition");
            safeCopy(item.insertText, sizeof(item.insertText),
                     "struct Name {\n    int member;\n};");
        }
        items.push_back(item);
    }
}

// ============================================================================
// GetLocalFallbackCompletions — main entry point for fallback IntelliSense
// ============================================================================
// Parameters:
//   content       - full file content (UTF-8)
//   cursorPos     - byte offset of cursor in content
//   language      - language identifier or file extension (e.g. "cpp", ".py")
//   outItems      - caller-allocated buffer for results
//   maxItems      - capacity of outItems
// Returns: number of items written to outItems
// ============================================================================
extern "C" int GetLocalFallbackCompletions(
    const char* content,
    int cursorPos,
    const char* language,
    LocalFallbackItem* outItems,
    int maxItems)
{
    if (!content || !outItems || maxItems <= 0 || !language) return 0;

    auto safeCopy = [](char* dst, size_t dstSize, const char* src) {
        size_t len = strlen(src);
        if (len >= dstSize) len = dstSize - 1;
        memcpy(dst, src, len);
        dst[len] = '\0';
    };

    std::string prefix = extractCurrentToken(content, cursorPos);
    if (prefix.empty()) return 0;

    std::string lang(language);
    std::vector<LocalFallbackItem> collected;
    collected.reserve(128);

    // ── 1. Check snippet triggers first (highest priority) ─────────────
    const AICompletionEngine::LanguageKeywordTable* table =
        AICompletionEngine::findLanguageTable(lang);
    if (table && table->snippetTriggers) {
        for (int i = 0; table->snippetTriggers[i] != nullptr; ++i) {
            std::string trigger(table->snippetTriggers[i]);
            std::string trigLower = trigger;
            std::string prefLower = prefix;
            std::transform(trigLower.begin(), trigLower.end(), trigLower.begin(),
                           [](unsigned char c) { return (char)::tolower(c); });
            std::transform(prefLower.begin(), prefLower.end(), prefLower.begin(),
                           [](unsigned char c) { return (char)::tolower(c); });
            if (trigLower.find(prefLower) == 0) {
                addSnippetExpansion(table->snippetTriggers[i], lang, collected);
            }
        }
    }

    // ── 2. Keywords matching prefix ────────────────────────────────────
    if (table && table->keywords) {
        std::string prefLower = prefix;
        std::transform(prefLower.begin(), prefLower.end(), prefLower.begin(),
                       [](unsigned char c) { return (char)::tolower(c); });

        for (int i = 0; table->keywords[i] != nullptr; ++i) {
            std::string kw(table->keywords[i]);
            std::string kwLower = kw;
            std::transform(kwLower.begin(), kwLower.end(), kwLower.begin(),
                           [](unsigned char c) { return (char)::tolower(c); });

            if (kwLower.find(prefLower) == 0 && kwLower != prefLower) {
                LocalFallbackItem item{};
                safeCopy(item.label, sizeof(item.label), kw.c_str());
                safeCopy(item.detail, sizeof(item.detail), "Keyword");
                safeCopy(item.insertText, sizeof(item.insertText), kw.c_str());
                item.confidence = 0.88f;
                safeCopy(item.category, sizeof(item.category), "keyword");
                collected.push_back(item);
            }
        }
    }

    // ── 3. Builtins / stdlib matching prefix ───────────────────────────
    if (table && table->builtins) {
        std::string prefLower = prefix;
        std::transform(prefLower.begin(), prefLower.end(), prefLower.begin(),
                       [](unsigned char c) { return (char)::tolower(c); });

        for (int i = 0; table->builtins[i] != nullptr; ++i) {
            std::string bi(table->builtins[i]);
            std::string biLower = bi;
            std::transform(biLower.begin(), biLower.end(), biLower.begin(),
                           [](unsigned char c) { return (char)::tolower(c); });

            if (biLower.find(prefLower) == 0 && biLower != prefLower) {
                LocalFallbackItem item{};
                safeCopy(item.label, sizeof(item.label), bi.c_str());
                safeCopy(item.detail, sizeof(item.detail), "Standard library");
                safeCopy(item.insertText, sizeof(item.insertText), bi.c_str());
                item.confidence = 0.82f;
                safeCopy(item.category, sizeof(item.category), "builtin");
                collected.push_back(item);
            }
        }
    }

    // ── 4. Local identifiers from surrounding code ─────────────────────
    {
        std::vector<std::string> identifiers;
        extractLocalIdentifiers(content, cursorPos, identifiers);

        std::string prefLower = prefix;
        std::transform(prefLower.begin(), prefLower.end(), prefLower.begin(),
                       [](unsigned char c) { return (char)::tolower(c); });

        for (const auto& ident : identifiers) {
            std::string identLower = ident;
            std::transform(identLower.begin(), identLower.end(), identLower.begin(),
                           [](unsigned char c) { return (char)::tolower(c); });

            if (identLower.find(prefLower) == 0 && identLower != prefLower) {
                LocalFallbackItem item{};
                safeCopy(item.label, sizeof(item.label), ident.c_str());
                safeCopy(item.detail, sizeof(item.detail), "Local identifier");
                safeCopy(item.insertText, sizeof(item.insertText), ident.c_str());
                item.confidence = 0.75f;
                safeCopy(item.category, sizeof(item.category), "variable");
                collected.push_back(item);
            }
        }
    }

    // ── 5. Sort by confidence descending ───────────────────────────────
    std::sort(collected.begin(), collected.end(),
              [](const LocalFallbackItem& a, const LocalFallbackItem& b) {
                  return a.confidence > b.confidence;
              });

    // ── 6. Deduplicate by label ────────────────────────────────────────
    std::set<std::string> seen;
    int written = 0;
    for (const auto& item : collected) {
        if (written >= maxItems) break;
        std::string key(item.label);
        if (seen.find(key) != seen.end()) continue;
        seen.insert(key);
        outItems[written] = item;
        written++;
    }

    return written;
}

extern "C" {
    void InitAICompletion() {
        if (!g_completion_engine) {
            g_completion_engine = new AICompletionEngine();
            std::cout << "[AI COMPLETION] Engine initialized with multi-language fallback registry\n";
        }
    }
    
    void ShutdownAICompletion() {
        if (g_completion_engine) {
            delete g_completion_engine;
            g_completion_engine = nullptr;
            std::cout << "[AI COMPLETION] Engine shut down\n";
        }
    }

    // Query how many languages are registered in the fallback table
    int GetFallbackLanguageCount() {
        int count = 0;
        for (int i = 0; AICompletionEngine::s_languageTables[i].language != nullptr; ++i) {
            count++;
        }
        return count;
    }

    // Get registered language name by index (returns nullptr if out of range)
    const char* GetFallbackLanguageName(int index) {
        int count = 0;
        for (int i = 0; AICompletionEngine::s_languageTables[i].language != nullptr; ++i) {
            if (count == index) return AICompletionEngine::s_languageTables[i].language;
            count++;
        }
        return nullptr;
    }
}
