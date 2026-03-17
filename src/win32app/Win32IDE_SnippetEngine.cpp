// ============================================================================
// Win32IDE_SnippetEngine.cpp — VS Code-compatible Snippet Tab-Stop Engine
// ============================================================================
// Parses ${N:placeholder} / $N / ${N} / ${0:finalCursor} syntax and provides:
//   - Tab-stop field cycling (Tab → next, Shift+Tab → prev)
//   - Placeholder text selection at each stop
//   - Linked fields (same $N → mirror edits)
//   - $0 final cursor position
//   - Multi-language built-in snippet libraries
//   - JSON snippet file loading (VS Code-compatible format)
// ============================================================================

#include "Win32IDE.h"
#include <richedit.h>
#include <fstream>
#include <regex>
#include <algorithm>
#include <sstream>
#include <set>

// ─── Snippet Field (one tab stop) ────────────────────────────────────
struct SnippetField {
    int         tabStopIndex  = 0;      // The $N number
    std::string placeholder;            // Default text for this field
    int         startOffset   = 0;      // Char offset in expanded text
    int         endOffset     = 0;      // End char offset
};

// ─── Active Snippet Session ──────────────────────────────────────────
struct SnippetSession {
    bool        active           = false;
    int         insertionBase    = 0;    // Char position where snippet was inserted
    std::string expandedText;            // The snippet text after parsing (placeholders resolved)
    std::vector<SnippetField> fields;    // All tab-stop fields, sorted by tabStopIndex
    int         currentFieldIdx  = -1;   // Index into fields[] currently selected
    int         fieldCount       = 0;    // Total unique tab-stop numbers
};

static SnippetSession g_snippetSession;

// ─── Parse snippet body → expanded text + fields ─────────────────────
// Supports: $1, ${1}, ${1:placeholder}, ${2:default ${3:nested}}, $0
// Nested is handled one level deep; deeper nesting is stripped.
static void parseSnippetBody(const std::string& body,
                             std::string& outExpanded,
                             std::vector<SnippetField>& outFields) {
    outExpanded.clear();
    outFields.clear();

    std::set<int> seenTabStops;
    size_t i = 0;
    while (i < body.size()) {
        if (body[i] == '\\' && i + 1 < body.size()) {
            // Escaped character
            outExpanded += body[i + 1];
            i += 2;
            continue;
        }

        if (body[i] == '$') {
            // Check for ${N:...} or ${N} or $N
            if (i + 1 < body.size() && body[i + 1] == '{') {
                // Find the matching }
                size_t braceStart = i + 2;
                int braceDepth = 1;
                size_t braceEnd = braceStart;
                while (braceEnd < body.size() && braceDepth > 0) {
                    if (body[braceEnd] == '{') braceDepth++;
                    else if (body[braceEnd] == '}') braceDepth--;
                    if (braceDepth > 0) braceEnd++;
                }

                if (braceDepth == 0) {
                    std::string inside = body.substr(braceStart, braceEnd - braceStart);

                    // Parse "N:placeholder" or just "N"
                    size_t colonPos = inside.find(':');
                    int tabStop = 0;
                    std::string placeholder;

                    if (colonPos != std::string::npos) {
                        tabStop = std::atoi(inside.substr(0, colonPos).c_str());
                        placeholder = inside.substr(colonPos + 1);
                        // Strip any nested ${...} in placeholder (one level)
                        std::string cleanPlaceholder;
                        size_t pi = 0;
                        while (pi < placeholder.size()) {
                            if (placeholder[pi] == '$' && pi + 1 < placeholder.size() && placeholder[pi + 1] == '{') {
                                // Skip ${ and find matching }
                                size_t nBrace = pi + 2;
                                int nd = 1;
                                while (nBrace < placeholder.size() && nd > 0) {
                                    if (placeholder[nBrace] == '{') nd++;
                                    else if (placeholder[nBrace] == '}') nd--;
                                    if (nd > 0) nBrace++;
                                }
                                // Extract inner placeholder text
                                std::string innerContent = placeholder.substr(pi + 2, nBrace - pi - 2);
                                size_t innerColon = innerContent.find(':');
                                if (innerColon != std::string::npos) {
                                    cleanPlaceholder += innerContent.substr(innerColon + 1);
                                } else {
                                    // Just ${N} — no text
                                }
                                pi = nBrace + 1;
                            } else {
                                cleanPlaceholder += placeholder[pi];
                                pi++;
                            }
                        }
                        placeholder = cleanPlaceholder;
                    } else {
                        tabStop = std::atoi(inside.c_str());
                        placeholder = "";
                    }

                    SnippetField field;
                    field.tabStopIndex = tabStop;
                    field.placeholder  = placeholder;
                    field.startOffset  = (int)outExpanded.size();
                    outExpanded += placeholder;
                    field.endOffset    = (int)outExpanded.size();
                    outFields.push_back(field);
                    seenTabStops.insert(tabStop);

                    i = braceEnd + 1;
                    continue;
                }
            } else if (i + 1 < body.size() && std::isdigit(body[i + 1])) {
                // Simple $N
                size_t numStart = i + 1;
                size_t numEnd   = numStart;
                while (numEnd < body.size() && std::isdigit(body[numEnd])) numEnd++;

                int tabStop = std::atoi(body.substr(numStart, numEnd - numStart).c_str());

                SnippetField field;
                field.tabStopIndex = tabStop;
                field.placeholder  = "";
                field.startOffset  = (int)outExpanded.size();
                field.endOffset    = (int)outExpanded.size();
                outFields.push_back(field);
                seenTabStops.insert(tabStop);

                i = numEnd;
                continue;
            }
        }

        // Handle \n, \t in snippet body
        if (body[i] == '\\' && i + 1 < body.size()) {
            if (body[i + 1] == 'n') { outExpanded += '\n'; i += 2; continue; }
            if (body[i + 1] == 't') { outExpanded += '\t'; i += 2; continue; }
        }

        outExpanded += body[i];
        i++;
    }

    // Sort fields by tabStopIndex (but keep $0 at the END)
    std::sort(outFields.begin(), outFields.end(), [](const SnippetField& a, const SnippetField& b) {
        if (a.tabStopIndex == 0) return false;  // $0 goes last
        if (b.tabStopIndex == 0) return true;   // $0 goes last
        return a.tabStopIndex < b.tabStopIndex;
    });

    // If no $0 field, add one at the end of expanded text
    if (seenTabStops.find(0) == seenTabStops.end()) {
        SnippetField finalField;
        finalField.tabStopIndex = 0;
        finalField.placeholder  = "";
        finalField.startOffset  = (int)outExpanded.size();
        finalField.endOffset    = (int)outExpanded.size();
        outFields.push_back(finalField);
    }
}

// ─── Insert snippet with tab-stop session ────────────────────────────
void Win32IDE::insertSnippetWithTabStops(const std::string& snippetBody) {
    if (!m_hwndEditor) return;

    // End any active snippet session
    endSnippetSession();

    // Get the current cursor/selection range
    CHARRANGE cr;
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&cr);

    // If there's a selection, delete it first (snippet replaces selection)
    if (cr.cpMax > cr.cpMin) {
        SendMessageA(m_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)"");
        SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&cr);
    }

    int insertionBase = cr.cpMin;

    // Get current line's indentation for multi-line snippets
    int lineIndex = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, cr.cpMin, 0);
    int lineStart = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);

    // Read line to get indentation
    char lineBuf[1024] = {0};
    *(WORD*)lineBuf = sizeof(lineBuf) - 2;
    int lineLen = (int)SendMessageA(m_hwndEditor, EM_GETLINE, lineIndex, (LPARAM)lineBuf);
    lineBuf[lineLen] = '\0';

    std::string indentation;
    for (int c = 0; c < lineLen && (lineBuf[c] == ' ' || lineBuf[c] == '\t'); c++) {
        indentation += lineBuf[c];
    }

    // Apply indentation to each line of snippet body (skip first line)
    std::string adjustedBody = snippetBody;
    // Replace literal \n with actual newlines + indentation
    std::string processed;
    for (size_t i = 0; i < adjustedBody.size(); i++) {
        if (adjustedBody[i] == '\n') {
            processed += "\r\n" + indentation;
        } else {
            processed += adjustedBody[i];
        }
    }

    // Parse snippet body into expanded text + tab-stop fields
    std::string expandedText;
    std::vector<SnippetField> fields;
    parseSnippetBody(processed, expandedText, fields);

    // Insert the expanded text
    SendMessageA(m_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)expandedText.c_str());

    // Set up session
    g_snippetSession.active        = true;
    g_snippetSession.insertionBase = insertionBase;
    g_snippetSession.expandedText  = expandedText;
    g_snippetSession.fields        = fields;
    g_snippetSession.currentFieldIdx = -1;

    // Count unique non-zero tab stops
    std::set<int> uniq;
    for (auto& f : fields) uniq.insert(f.tabStopIndex);
    g_snippetSession.fieldCount = (int)uniq.size();

    // Select the first field (tab stop $1, or $0 if only $0 exists)
    snippetNextField();
}

// ─── Navigate to next tab-stop field ─────────────────────────────────
bool Win32IDE::snippetNextField() {
    if (!g_snippetSession.active || g_snippetSession.fields.empty()) return false;

    g_snippetSession.currentFieldIdx++;

    if (g_snippetSession.currentFieldIdx >= (int)g_snippetSession.fields.size()) {
        // Past the last field — end session
        endSnippetSession();
        return false;
    }

    selectCurrentSnippetField();
    return true;
}

// ─── Navigate to previous tab-stop field ─────────────────────────────
bool Win32IDE::snippetPrevField() {
    if (!g_snippetSession.active || g_snippetSession.fields.empty()) return false;

    g_snippetSession.currentFieldIdx--;
    if (g_snippetSession.currentFieldIdx < 0) {
        g_snippetSession.currentFieldIdx = 0;
    }

    selectCurrentSnippetField();
    return true;
}

// ─── Select the current field's text range ───────────────────────────
void Win32IDE::selectCurrentSnippetField() {
    if (!g_snippetSession.active || !m_hwndEditor) return;
    if (g_snippetSession.currentFieldIdx < 0 ||
        g_snippetSession.currentFieldIdx >= (int)g_snippetSession.fields.size()) return;

    const SnippetField& field = g_snippetSession.fields[g_snippetSession.currentFieldIdx];

    int base = g_snippetSession.insertionBase;
    CHARRANGE cr;
    cr.cpMin = base + field.startOffset;
    cr.cpMax = base + field.endOffset;

    SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&cr);
    SendMessage(m_hwndEditor, EM_SCROLLCARET, 0, 0);

    // If this is $0 (the final cursor position), end the session
    if (field.tabStopIndex == 0) {
        endSnippetSession();
    }
}

// ─── End the active snippet session ──────────────────────────────────
void Win32IDE::endSnippetSession() {
    g_snippetSession.active = false;
    g_snippetSession.fields.clear();
    g_snippetSession.currentFieldIdx = -1;
    g_snippetSession.expandedText.clear();
}

// ─── Check if a snippet session is active ────────────────────────────
bool Win32IDE::isSnippetSessionActive() const {
    return g_snippetSession.active;
}

// ─── Handle Tab key during snippet session ───────────────────────────
bool Win32IDE::handleSnippetTab(bool shiftHeld) {
    if (!g_snippetSession.active) return false;

    if (shiftHeld) {
        return snippetPrevField();
    } else {
        return snippetNextField();
    }
}

// ─── Load snippets from VS Code-format JSON file ────────────────────
// Format: { "Snippet Name": { "prefix": "trigger", "body": [...], "description": "..." } }
void Win32IDE::loadSnippetsFromJson(const std::string& filePath, const std::string& language) {
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) return;

    try {
        std::string content((std::istreambuf_iterator<char>(ifs)),
                             std::istreambuf_iterator<char>());
        nlohmann::json root = nlohmann::json::parse(content, nullptr, false);
        if (root.is_discarded()) return;

        for (auto& [name, value] : root.items()) {
            if (!value.is_object()) continue;

            CodeSnippet snippet;
            snippet.name = name;
            snippet.trigger = value.value("prefix", name);
            snippet.description = value.value("description", "");

            // Body can be a string or array of strings
            if (value.contains("body")) {
                if (value["body"].is_array()) {
                    std::string body;
                    for (size_t i = 0; i < value["body"].size(); i++) {
                        if (i > 0) body += "\n";
                        body += value["body"][i].get<std::string>();
                    }
                    snippet.code = body;
                } else if (value["body"].is_string()) {
                    snippet.code = value["body"].get<std::string>();
                }
            }

            // Parse trigger as scope-specific key
            std::string key = language + ":" + snippet.trigger;
            m_snippetTriggers[key] = m_codeSnippets.size();
            m_snippetTriggers[snippet.trigger] = m_codeSnippets.size();
            m_codeSnippets.push_back(snippet);
        }
    } catch (...) {
        // Malformed JSON — skip
    }
}

// ─── Load built-in snippets for all languages ────────────────────────
void Win32IDE::loadBuiltInSnippets() {
    m_codeSnippets.clear();
    m_snippetTriggers.clear();

    auto addSnippet = [&](const std::string& lang, const std::string& trigger,
                          const std::string& name, const std::string& desc,
                          const std::string& body) {
        CodeSnippet s;
        s.name        = name;
        s.trigger     = trigger;
        s.description = desc;
        s.code        = body;
        std::string key = lang + ":" + trigger;
        m_snippetTriggers[key] = m_codeSnippets.size();
        m_snippetTriggers[trigger] = m_codeSnippets.size();
        m_codeSnippets.push_back(s);
    };

    // ── C++ Snippets ─────────────────────────────────────────────────
    addSnippet("cpp", "for",    "For Loop",           "For loop with index",
        "for (${1:int} ${2:i} = ${3:0}; ${2:i} < ${4:count}; ${2:i}++) {\n    $0\n}");
    addSnippet("cpp", "forauto","Range For",          "Range-based for loop",
        "for (auto& ${1:item} : ${2:container}) {\n    $0\n}");
    addSnippet("cpp", "if",     "If Statement",       "If conditional",
        "if (${1:condition}) {\n    $0\n}");
    addSnippet("cpp", "ife",    "If-Else",            "If-else conditional",
        "if (${1:condition}) {\n    ${2:// true}\n} else {\n    ${3:// false}\n}");
    addSnippet("cpp", "class",  "Class",              "Class declaration",
        "class ${1:ClassName} {\npublic:\n    ${1:ClassName}(${2:}) {\n        $0\n    }\n    ~${1:ClassName}() = default;\n\nprivate:\n    ${3:// members}\n};");
    addSnippet("cpp", "struct", "Struct",             "Struct declaration",
        "struct ${1:Name} {\n    ${2:int member};\n};");
    addSnippet("cpp", "fn",     "Function",           "Function definition",
        "${1:void} ${2:functionName}(${3:}) {\n    $0\n}");
    addSnippet("cpp", "main",   "Main Function",      "int main entry point",
        "int main(int argc, char* argv[]) {\n    $0\n    return 0;\n}");
    addSnippet("cpp", "try",    "Try-Catch",          "Try-catch block",
        "try {\n    ${1:// code}\n} catch (const ${2:std::exception}& ${3:e}) {\n    ${4:// handle error}\n}");
    addSnippet("cpp", "inc",    "Include",            "#include header",
        "#include <${1:iostream}>");
    addSnippet("cpp", "incl",   "Include Local",      "#include local header",
        "#include \"${1:header.h}\"");
    addSnippet("cpp", "cout",   "std::cout",          "Console output",
        "std::cout << ${1:\"message\"} << std::endl;");
    addSnippet("cpp", "map",    "std::map",           "Map declaration",
        "std::map<${1:std::string}, ${2:int}> ${3:myMap};");
    addSnippet("cpp", "vec",    "std::vector",        "Vector declaration",
        "std::vector<${1:int}> ${2:myVec};");
    addSnippet("cpp", "up",     "unique_ptr",         "std::unique_ptr",
        "std::unique_ptr<${1:Type}> ${2:ptr} = std::make_unique<${1:Type}>(${3:});");
    addSnippet("cpp", "sp",     "shared_ptr",         "std::shared_ptr",
        "std::shared_ptr<${1:Type}> ${2:ptr} = std::make_shared<${1:Type}>(${3:});");
    addSnippet("cpp", "lam",    "Lambda",             "Lambda expression",
        "[${1:&}](${2:}) {\n    $0\n}");
    addSnippet("cpp", "ns",     "Namespace",          "Namespace block",
        "namespace ${1:name} {\n\n$0\n\n} // namespace ${1:name}");
    addSnippet("cpp", "guard",  "Include Guard",      "Header include guard",
        "#ifndef ${1:HEADER_H}\n#define ${1:HEADER_H}\n\n$0\n\n#endif // ${1:HEADER_H}");
    addSnippet("cpp", "pragma", "Pragma Once",        "Pragma once directive",
        "#pragma once\n\n$0");

    // ── Python Snippets ──────────────────────────────────────────────
    addSnippet("python", "def",    "Function",        "Function definition",
        "def ${1:function_name}(${2:params}):\n    ${3:\"\"\"${4:Docstring.}\"\"\"}\n    $0");
    addSnippet("python", "class",  "Class",           "Class definition",
        "class ${1:ClassName}:\n    def __init__(self${2:, params}):\n        ${3:pass}\n");
    addSnippet("python", "if",     "If Statement",    "If conditional",
        "if ${1:condition}:\n    $0");
    addSnippet("python", "ife",    "If-Else",         "If-else conditional",
        "if ${1:condition}:\n    ${2:pass}\nelse:\n    ${3:pass}");
    addSnippet("python", "for",    "For Loop",        "For loop",
        "for ${1:item} in ${2:iterable}:\n    $0");
    addSnippet("python", "while",  "While Loop",      "While loop",
        "while ${1:condition}:\n    $0");
    addSnippet("python", "try",    "Try-Except",      "Try-except block",
        "try:\n    ${1:pass}\nexcept ${2:Exception} as ${3:e}:\n    ${4:raise}");
    addSnippet("python", "with",   "With Statement",  "Context manager",
        "with ${1:expression} as ${2:target}:\n    $0");
    addSnippet("python", "main",   "Main Guard",      "if __name__ == '__main__'",
        "if __name__ == \"__main__\":\n    ${1:main()}");
    addSnippet("python", "lam",    "Lambda",          "Lambda expression",
        "lambda ${1:x}: ${2:x}");
    addSnippet("python", "comp",   "List Comprehension","List comprehension",
        "[${1:expr} for ${2:item} in ${3:iterable}]");

    // ── JavaScript/TypeScript Snippets ───────────────────────────────
    addSnippet("javascript", "fn",      "Function",      "Function declaration",
        "function ${1:name}(${2:params}) {\n    $0\n}");
    addSnippet("javascript", "afn",     "Arrow Function","Arrow function",
        "const ${1:name} = (${2:params}) => {\n    $0\n};");
    addSnippet("javascript", "class",   "Class",         "ES6 class",
        "class ${1:ClassName} {\n    constructor(${2:}) {\n        $0\n    }\n}");
    addSnippet("javascript", "if",      "If Statement",  "If conditional",
        "if (${1:condition}) {\n    $0\n}");
    addSnippet("javascript", "for",     "For Loop",      "For loop",
        "for (let ${1:i} = 0; ${1:i} < ${2:array.length}; ${1:i}++) {\n    $0\n}");
    addSnippet("javascript", "forof",   "For-Of Loop",   "For-of loop",
        "for (const ${1:item} of ${2:iterable}) {\n    $0\n}");
    addSnippet("javascript", "forin",   "For-In Loop",   "For-in loop",
        "for (const ${1:key} in ${2:object}) {\n    $0\n}");
    addSnippet("javascript", "try",     "Try-Catch",     "Try-catch block",
        "try {\n    ${1:// code}\n} catch (${2:error}) {\n    ${3:console.error(${2:error})}\n}");
    addSnippet("javascript", "prom",    "Promise",       "New Promise",
        "new Promise((resolve, reject) => {\n    $0\n});");
    addSnippet("javascript", "cl",      "Console Log",   "console.log",
        "console.log(${1:'message'});");
    addSnippet("javascript", "imp",     "Import",        "ES6 import",
        "import { ${2:module} } from '${1:package}';");
    addSnippet("javascript", "impd",    "Import Default","Default import",
        "import ${2:module} from '${1:package}';");
    addSnippet("javascript", "exp",     "Export",        "Named export",
        "export { ${1:module} };");
    addSnippet("javascript", "expd",    "Export Default","Default export",
        "export default ${1:expression};");
    addSnippet("javascript", "async",   "Async Function","Async function",
        "async function ${1:name}(${2:params}) {\n    $0\n}");
    addSnippet("javascript", "await",   "Await",         "Await expression",
        "const ${1:result} = await ${2:promise};");

    // ── C# Snippets ─────────────────────────────────────────────────
    addSnippet("csharp", "class",    "Class",           "Class definition",
        "public class ${1:ClassName}\n{\n    $0\n}");
    addSnippet("csharp", "struct",   "Struct",          "Struct definition",
        "public struct ${1:Name}\n{\n    $0\n}");
    addSnippet("csharp", "interface","Interface",       "Interface definition",
        "public interface ${1:IName}\n{\n    $0\n}");
    addSnippet("csharp", "prop",     "Property",        "Auto property",
        "public ${1:string} ${2:Name} { get; set; }");
    addSnippet("csharp", "propf",    "Full Property",   "Full property with backing field",
        "private ${1:string} _${2:name};\npublic ${1:string} ${3:Name}\n{\n    get => _${2:name};\n    set => _${2:name} = value;\n}");
    addSnippet("csharp", "ctor",     "Constructor",     "Constructor",
        "public ${1:ClassName}(${2:})\n{\n    $0\n}");
    addSnippet("csharp", "for",      "For Loop",        "For loop",
        "for (int ${1:i} = 0; ${1:i} < ${2:length}; ${1:i}++)\n{\n    $0\n}");
    addSnippet("csharp", "foreach",  "ForEach",         "Foreach loop",
        "foreach (var ${1:item} in ${2:collection})\n{\n    $0\n}");
    addSnippet("csharp", "try",      "Try-Catch",       "Try-catch block",
        "try\n{\n    ${1:// code}\n}\ncatch (${2:Exception} ${3:ex})\n{\n    ${4:throw;}\n}");
    addSnippet("csharp", "using",    "Using",           "Using statement",
        "using (var ${1:resource} = ${2:new Resource()})\n{\n    $0\n}");
    addSnippet("csharp", "async",    "Async Method",    "Async method",
        "public async Task${1:<${2:T}>} ${3:MethodName}(${4:})\n{\n    $0\n}");

    // ── PowerShell Snippets ──────────────────────────────────────────
    addSnippet("powershell", "function", "Function",    "PowerShell function",
        "function ${1:FunctionName} {\n    param(\n        ${2:$Parameter}\n    )\n    \n    $0\n}");
    addSnippet("powershell", "if",       "If Statement","If conditional",
        "if (${1:$condition}) {\n    $0\n}");
    addSnippet("powershell", "foreach",  "ForEach",     "ForEach loop",
        "foreach (${1:$item} in ${2:$collection}) {\n    $0\n}");
    addSnippet("powershell", "try",      "Try-Catch",   "Try-catch block",
        "try {\n    ${1:# Code that might throw}\n}\ncatch {\n    ${2:# Error handling}\n}");
    addSnippet("powershell", "param",    "Param Block", "Parameter block",
        "[CmdletBinding()]\nparam(\n    [Parameter(Mandatory)]\n    [${1:string}]${2:$Name}\n)");
    addSnippet("powershell", "switch",   "Switch",      "Switch statement",
        "switch (${1:$variable}) {\n    ${2:'value1'} { $0 }\n    default { }\n}");

    // ── MASM x64 Snippets ────────────────────────────────────────────
    addSnippet("asm", "proc",   "Procedure",    "x64 MASM procedure with prologue/epilogue",
        "${1:ProcName} PROC\n    push rbp\n    mov rbp, rsp\n    sub rsp, ${2:32}\n    \n    $0\n    \n    add rsp, ${2:32}\n    pop rbp\n    ret\n${1:ProcName} ENDP");
    addSnippet("asm", "data",   "Data Section", ".data section",
        ".data\n    ${1:varName} ${2:QWORD} ${3:0}\n\n.code\n$0");
    addSnippet("asm", "extern", "Extern Decl",  "External function declaration",
        "EXTERN ${1:FunctionName}:PROC");
    addSnippet("asm", "invoke", "Function Call","x64 function call (shadow space)",
        "    sub rsp, 28h\n    ${1:mov rcx, param1}\n    call ${2:FunctionName}\n    add rsp, 28h");
    addSnippet("asm", "loop",   "Loop",         "Loop structure",
        "    mov ${1:rcx}, ${2:count}\n${3:LoopLabel}:\n    $0\n    dec ${1:rcx}\n    jnz ${3:LoopLabel}");

    // ── Rust Snippets ────────────────────────────────────────────────
    addSnippet("rust", "fn",      "Function",    "Function definition",
        "fn ${1:name}(${2:}) -> ${3:()}\n{\n    $0\n}");
    addSnippet("rust", "struct",  "Struct",      "Struct definition",
        "struct ${1:Name} {\n    ${2:field}: ${3:Type},\n}");
    addSnippet("rust", "impl",    "Impl Block",  "Implementation block",
        "impl ${1:Type} {\n    $0\n}");
    addSnippet("rust", "match",   "Match",       "Match expression",
        "match ${1:expr} {\n    ${2:pattern} => ${3:value},\n    _ => ${4:default},\n}");
    addSnippet("rust", "test",    "Test",        "Unit test function",
        "#[test]\nfn ${1:test_name}() {\n    $0\n}");
    addSnippet("rust", "main",    "Main",        "Main function",
        "fn main() {\n    $0\n}");

    // Try to load user snippets from workspace .vscode/snippets/
    loadUserSnippetFiles();
}

// ─── Load user snippet JSON files from .vscode directory ─────────────
void Win32IDE::loadUserSnippetFiles() {
    // Look for .vscode/*.code-snippets and .vscode/snippets/*.json
    std::vector<std::string> searchPaths;

    if (!m_currentDirectory.empty()) {
        searchPaths.push_back(m_currentDirectory + "\\.vscode");
        searchPaths.push_back(m_currentDirectory + "\\snippets");
    }
    // Also check appdata
    char appData[MAX_PATH];
    if (GetEnvironmentVariableA("APPDATA", appData, MAX_PATH)) {
        std::string snippetDir = std::string(appData) + "\\RawrXD\\snippets";
        searchPaths.push_back(snippetDir);
    }

    for (auto& dir : searchPaths) {
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA((dir + "\\*.json").c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    std::string filePath = dir + "\\" + fd.cFileName;
                    // Infer language from filename (e.g., "cpp.json", "python.json")
                    std::string lang = fd.cFileName;
                    size_t dot = lang.rfind('.');
                    if (dot != std::string::npos) lang = lang.substr(0, dot);
                    loadSnippetsFromJson(filePath, lang);
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }

        // Also check .code-snippets files (global snippets)
        hFind = FindFirstFileA((dir + "\\*.code-snippets").c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    std::string filePath = dir + "\\" + fd.cFileName;
                    loadSnippetsFromJson(filePath, "global");
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }
    }
}

// ─── Find matching snippet for a trigger prefix ─────────────────────
// Called when user types a trigger word and presses Tab or selects from completion
int Win32IDE::findSnippetByTrigger(const std::string& trigger, const std::string& language) {
    // First try language-specific
    std::string key = language + ":" + trigger;
    auto it = m_snippetTriggers.find(key);
    if (it != m_snippetTriggers.end() && it->second < m_codeSnippets.size()) {
        return (int)it->second;
    }

    // Fall back to any language
    it = m_snippetTriggers.find(trigger);
    if (it != m_snippetTriggers.end() && it->second < m_codeSnippets.size()) {
        return (int)it->second;
    }

    return -1;
}

// ─── Get snippet completions matching a prefix ───────────────────────
std::vector<std::pair<std::string, std::string>> Win32IDE::getSnippetCompletions(
    const std::string& prefix, const std::string& language) {
    std::vector<std::pair<std::string, std::string>> results;

    for (auto& snippet : m_codeSnippets) {
        std::string trig = snippet.trigger.empty() ? snippet.name : snippet.trigger;
        // Check if trigger starts with prefix (case-insensitive)
        if (trig.size() >= prefix.size()) {
            bool match = true;
            for (size_t i = 0; i < prefix.size(); i++) {
                if (std::tolower(trig[i]) != std::tolower(prefix[i])) {
                    match = false;
                    break;
                }
            }
            if (match) {
                results.push_back({trig, snippet.description});
            }
        }
    }

    return results;
}

// ─── Expand snippet trigger at cursor ────────────────────────────────
// Returns true if a snippet was found and expanded
bool Win32IDE::tryExpandSnippetAtCursor() {
    if (!m_hwndEditor) return false;

    // Get the word before the cursor
    CHARRANGE cr;
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&cr);
    if (cr.cpMin != cr.cpMax) return false;  // Has selection — not a trigger

    // Read backwards to find word start
    int pos = cr.cpMin;
    int lineIndex = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, pos, 0);
    int lineStart = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);

    // Get line text
    char lineBuf[1024] = {0};
    *(WORD*)lineBuf = sizeof(lineBuf) - 2;
    int lineLen = (int)SendMessageA(m_hwndEditor, EM_GETLINE, lineIndex, (LPARAM)lineBuf);
    lineBuf[lineLen] = '\0';

    int colPos = pos - lineStart;
    if (colPos <= 0) return false;

    // Walk back to find trigger word boundary
    int wordStart = colPos;
    while (wordStart > 0 && (std::isalnum(lineBuf[wordStart - 1]) || lineBuf[wordStart - 1] == '_')) {
        wordStart--;
    }

    if (wordStart >= colPos) return false;  // No word before cursor

    std::string trigger(lineBuf + wordStart, colPos - wordStart);

    // Determine current language
    std::string lang = "text";
    if (!m_currentFile.empty()) {
        size_t dot = m_currentFile.rfind('.');
        if (dot != std::string::npos) {
            std::string ext = m_currentFile.substr(dot + 1);
            if (ext == "cpp" || ext == "cxx" || ext == "cc" || ext == "h" || ext == "hpp") lang = "cpp";
            else if (ext == "py") lang = "python";
            else if (ext == "js") lang = "javascript";
            else if (ext == "ts") lang = "typescript";
            else if (ext == "cs") lang = "csharp";
            else if (ext == "ps1" || ext == "psm1") lang = "powershell";
            else if (ext == "asm" || ext == "masm") lang = "asm";
            else if (ext == "rs") lang = "rust";
            else if (ext == "java") lang = "java";
            else if (ext == "go") lang = "go";
        }
    }

    int idx = findSnippetByTrigger(trigger, lang);
    if (idx < 0) return false;

    // Delete the trigger text
    CHARRANGE selRange;
    selRange.cpMin = lineStart + wordStart;
    selRange.cpMax = pos;
    SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&selRange);
    SendMessageA(m_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)"");

    // Insert the snippet with tab-stop navigation
    insertSnippetWithTabStops(m_codeSnippets[idx].code);
    return true;
}
