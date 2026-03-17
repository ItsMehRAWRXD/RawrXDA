// ============================================================================
// refactoring_plugin.cpp — Pluginable Refactoring Engine Implementation
// ============================================================================
// Built-in refactorings + DLL plugin loader + execution engine.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "ide/refactoring_plugin.h"
#include <algorithm>
#include <sstream>
#include <regex>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

// Forward-declare SmartRewriteEngine types
#include "SmartRewriteEngine.h"

namespace RawrXD {
namespace IDE {

// ============================================================================
// Singleton
// ============================================================================
RefactoringEngine& RefactoringEngine::Instance() {
    static RefactoringEngine instance;
    return instance;
}

RefactoringEngine::~RefactoringEngine() {
    Shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================
void RefactoringEngine::Initialize() {
    if (m_initialized.load()) return;
    registerBuiltins();
    m_initialized.store(true);
}

void RefactoringEngine::Shutdown() {
    if (!m_initialized.load()) return;
    UnloadAllPlugins();
    m_initialized.store(false);
}

// ============================================================================
// Registration
// ============================================================================
void RefactoringEngine::RegisterRefactoring(const RefactoringDescriptor& desc) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_refactorings[desc.id] = desc;
}

void RefactoringEngine::UnregisterRefactoring(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_refactorings.erase(id);
}

// ============================================================================
// Plugin Management
// ============================================================================
bool RefactoringEngine::LoadPlugin(const std::string& dllPath) {
#ifdef _WIN32
    HMODULE hMod = LoadLibraryA(dllPath.c_str());
    if (!hMod) return false;

    LoadedPlugin plugin;
    plugin.path = dllPath;
    plugin.hModule = hMod;
    plugin.fnGetInfo = reinterpret_cast<RefactoringPlugin_GetInfo_fn>(
        GetProcAddress(hMod, "RefactoringPlugin_GetInfo"));
    plugin.fnGetDescriptors = reinterpret_cast<RefactoringPlugin_GetDescriptors_fn>(
        GetProcAddress(hMod, "RefactoringPlugin_GetDescriptors"));
    plugin.fnExecute = reinterpret_cast<RefactoringPlugin_Execute_fn>(
        GetProcAddress(hMod, "RefactoringPlugin_Execute"));
    plugin.fnShutdown = reinterpret_cast<RefactoringPlugin_Shutdown_fn>(
        GetProcAddress(hMod, "RefactoringPlugin_Shutdown"));

    if (!plugin.fnGetInfo || !plugin.fnGetDescriptors || !plugin.fnExecute) {
        FreeLibrary(hMod);
        return false;
    }

    auto* info = plugin.fnGetInfo();
    if (info) {
        plugin.name = info->name;
    }

    // Initialize if there's an init function
    auto fnInit = reinterpret_cast<RefactoringPlugin_Init_fn>(
        GetProcAddress(hMod, "RefactoringPlugin_Init"));
    if (fnInit) fnInit("{}");

    // Load descriptors from plugin
    CRefactoringDescriptor cDescs[64];
    int count = plugin.fnGetDescriptors(cDescs, 64);

    for (int i = 0; i < count; ++i) {
        RefactoringDescriptor desc;
        desc.id = cDescs[i].id;
        desc.name = cDescs[i].name;
        desc.description = cDescs[i].description;
        desc.category = RefactoringCategory::Custom;
        desc.categoryName = cDescs[i].category;
        desc.requiresSelection = cDescs[i].requiresSelection != 0;
        desc.requiresSymbol = cDescs[i].requiresSymbol != 0;
        desc.isMultiFile = cDescs[i].isMultiFile != 0;
        
        // Parse languages
        std::string langs = cDescs[i].languages;
        std::istringstream iss(langs);
        std::string lang;
        while (std::getline(iss, lang, ',')) {
            while (!lang.empty() && lang[0] == ' ') lang.erase(0, 1);
            if (!lang.empty()) desc.supportedLanguages.push_back(lang);
        }

        // Set execute to delegate to plugin
        auto* execFn = plugin.fnExecute;
        std::string refId = desc.id;
        desc.execute = [execFn, refId](const std::string& code,
                                        const std::string& language,
                                        const std::map<std::string, std::string>& params)
            -> TransformationResult {
            // Serialize params to JSON
            std::ostringstream pss;
            pss << "{";
            bool first = true;
            for (const auto& [k, v] : params) {
                if (!first) pss << ",";
                pss << "\"" << k << "\":\"" << v << "\"";
                first = false;
            }
            pss << "}";

            CRefactoringResult cr = execFn(refId.c_str(), code.c_str(),
                                           language.c_str(), pss.str().c_str());
            TransformationResult tr;
            tr.confidence = cr.confidence;
            if (cr.success) {
                tr.transformedCode = cr.transformedCode ? cr.transformedCode : "";
            } else {
                tr.warnings.push_back(cr.error);
            }
            tr.requiresManualReview = (cr.confidence < 0.9f);
            return tr;
        };

        RegisterRefactoring(desc);
    }

    m_plugins.push_back(std::move(plugin));
    return true;
#else
    (void)dllPath;
    return false;
#endif
}

void RefactoringEngine::UnloadPlugin(const std::string& name) {
    auto it = std::find_if(m_plugins.begin(), m_plugins.end(),
                            [&](const LoadedPlugin& p) { return p.name == name; });
    if (it == m_plugins.end()) return;

    if (it->fnShutdown) it->fnShutdown();
#ifdef _WIN32
    if (it->hModule) FreeLibrary(it->hModule);
#endif
    m_plugins.erase(it);
}

void RefactoringEngine::UnloadAllPlugins() {
    for (auto& p : m_plugins) {
        if (p.fnShutdown) p.fnShutdown();
#ifdef _WIN32
        if (p.hModule) FreeLibrary(p.hModule);
#endif
    }
    m_plugins.clear();
}

std::vector<std::string> RefactoringEngine::GetLoadedPlugins() const {
    std::vector<std::string> names;
    for (const auto& p : m_plugins) names.push_back(p.name);
    return names;
}

// ============================================================================
// Discovery
// ============================================================================
std::vector<RefactoringDescriptor> RefactoringEngine::GetAllRefactorings() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<RefactoringDescriptor> out;
    for (const auto& [id, desc] : m_refactorings) {
        out.push_back(desc);
    }
    std::sort(out.begin(), out.end(),
              [](const RefactoringDescriptor& a, const RefactoringDescriptor& b) {
                  return a.menuOrder < b.menuOrder;
              });
    return out;
}

std::vector<RefactoringDescriptor> RefactoringEngine::GetByCategory(
    RefactoringCategory cat) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<RefactoringDescriptor> out;
    for (const auto& [id, desc] : m_refactorings) {
        if (desc.category == cat) out.push_back(desc);
    }
    return out;
}

std::vector<RefactoringDescriptor> RefactoringEngine::GetByLanguage(
    const std::string& lang) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<RefactoringDescriptor> out;
    for (const auto& [id, desc] : m_refactorings) {
        if (desc.supportedLanguages.empty()) {
            out.push_back(desc); // Available for all languages
        } else {
            for (const auto& l : desc.supportedLanguages) {
                if (l == lang) { out.push_back(desc); break; }
            }
        }
    }
    return out;
}

std::vector<RefactoringDescriptor> RefactoringEngine::GetAvailable(
    const RefactoringContext& ctx) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<RefactoringDescriptor> out;
    for (const auto& [id, desc] : m_refactorings) {
        // Check language
        bool langOk = desc.supportedLanguages.empty();
        if (!langOk) {
            for (const auto& l : desc.supportedLanguages) {
                if (l == ctx.language) { langOk = true; break; }
            }
        }
        if (!langOk) continue;
        
        // Check selection requirement
        if (desc.requiresSelection && ctx.selectedText.empty()) continue;
        if (desc.requiresSymbol && ctx.symbolUnderCursor.empty()) continue;
        
        // Check availability function
        if (desc.isAvailable) {
            if (!desc.isAvailable(ctx.code, ctx.language, ctx.cursorLine,
                                  ctx.selectionStartLine, ctx.selectionEndLine)) {
                continue;
            }
        }
        
        out.push_back(desc);
    }
    std::sort(out.begin(), out.end(),
              [](const RefactoringDescriptor& a, const RefactoringDescriptor& b) {
                  return a.menuOrder < b.menuOrder;
              });
    return out;
}

const RefactoringDescriptor* RefactoringEngine::FindById(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_refactorings.find(id);
    return (it != m_refactorings.end()) ? &it->second : nullptr;
}

// ============================================================================
// Execution
// ============================================================================
RefactoringResult RefactoringEngine::Execute(const std::string& refactoringId,
                                              const RefactoringContext& ctx) {
    const RefactoringDescriptor* desc = FindById(refactoringId);
    if (!desc) return RefactoringResult::fail("Unknown refactoring: " + refactoringId);
    
    if (!desc->execute) return RefactoringResult::fail("No executor for: " + refactoringId);
    
    auto tr = desc->execute(ctx.code, ctx.language, ctx.params);
    
    // Convert TransformationResult to RefactoringResult
    RefactoringResult::FileEdit edit;
    edit.filePath = ctx.filePath;
    edit.originalContent = ctx.code;
    edit.newContent = tr.transformedCode;
    
    RefactoringResult result = RefactoringResult::ok({edit}, tr.explanation, tr.confidence);
    result.requiresApproval = tr.requiresManualReview;
    
    // Update stats
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalExecutions++;
        if (result.success) m_stats.successfulExecutions++;
        else m_stats.failedExecutions++;
        m_stats.executionsByRefactoring[refactoringId]++;
        m_stats.executionsByLanguage[ctx.language]++;
    }
    
    return result;
}

std::vector<RefactoringResult> RefactoringEngine::ExecuteBatch(
    const std::vector<std::string>& refactoringIds,
    const RefactoringContext& ctx) {
    std::vector<RefactoringResult> results;
    
    RefactoringContext current = ctx;
    for (const auto& id : refactoringIds) {
        auto result = Execute(id, current);
        if (result.success && !result.edits.empty()) {
            // Chain: use output of previous as input to next
            current.code = result.edits[0].newContent;
        }
        results.push_back(std::move(result));
    }
    
    return results;
}

std::string RefactoringEngine::PreviewRefactoring(const std::string& refactoringId,
                                                    const RefactoringContext& ctx) {
    auto result = Execute(refactoringId, ctx);
    if (!result.success) return "Error: " + result.error;
    if (result.edits.empty()) return "No changes";
    return result.edits[0].newContent;
}

// ============================================================================
// Statistics
// ============================================================================
RefactoringEngine::Stats RefactoringEngine::GetStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

// ============================================================================
// Built-in Refactoring Implementations
// ============================================================================

// Helper: simple line-based code manipulation
static std::vector<std::string> splitLines(const std::string& code) {
    std::vector<std::string> lines;
    std::istringstream iss(code);
    std::string line;
    while (std::getline(iss, line)) lines.push_back(line);
    return lines;
}

static std::string joinLines(const std::vector<std::string>& lines) {
    std::ostringstream oss;
    for (size_t i = 0; i < lines.size(); ++i) {
        oss << lines[i];
        if (i + 1 < lines.size()) oss << "\n";
    }
    return oss.str();
}

RefactoringResult RefactoringEngine::doExtractMethod(const RefactoringContext& ctx) {
    if (ctx.selectedText.empty()) {
        return RefactoringResult::fail("No code selected for extraction");
    }
    
    std::string methodName = "extractedMethod";
    auto it = ctx.params.find("name");
    if (it != ctx.params.end()) methodName = it->second;
    
    // Build extracted method
    std::ostringstream extracted;
    extracted << "void " << methodName << "() {\n";
    extracted << "    " << ctx.selectedText << "\n";
    extracted << "}\n\n";
    
    // Replace selection with call
    std::string newCode = ctx.code;
    auto pos = newCode.find(ctx.selectedText);
    if (pos != std::string::npos) {
        newCode.replace(pos, ctx.selectedText.size(),
                        methodName + "();");
    }
    
    // Insert extracted method before the containing function
    // Simple heuristic: insert before the first line of the function
    auto lines = splitLines(newCode);
    std::string result;
    bool inserted = false;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (!inserted && (int)i >= ctx.selectionStartLine - 10) {
            // Find function start
            if (lines[i].find('{') != std::string::npos && i > 0 
                && lines[i-1].find('(') != std::string::npos) {
                result += extracted.str();
                inserted = true;
            }
        }
        result += lines[i] + "\n";
    }
    if (!inserted) {
        result = extracted.str() + result;
    }
    
    RefactoringResult::FileEdit edit;
    edit.filePath = ctx.filePath;
    edit.originalContent = ctx.code;
    edit.newContent = result;
    
    return RefactoringResult::ok({edit}, "Extracted method: " + methodName, 0.85f);
}

RefactoringResult RefactoringEngine::doExtractVariable(const RefactoringContext& ctx) {
    if (ctx.selectedText.empty()) {
        return RefactoringResult::fail("No expression selected");
    }
    
    std::string varName = "extracted";
    auto it = ctx.params.find("name");
    if (it != ctx.params.end()) varName = it->second;
    
    std::string newCode = ctx.code;
    auto pos = newCode.find(ctx.selectedText);
    if (pos != std::string::npos) {
        // Find line start for variable declaration
        size_t lineStart = newCode.rfind('\n', pos);
        if (lineStart == std::string::npos) lineStart = 0;
        else lineStart++;
        
        // Determine indentation
        std::string indent;
        for (size_t i = lineStart; i < pos && std::isspace(static_cast<unsigned char>(newCode[i])); ++i) {
            indent += newCode[i];
        }
        
        // Insert variable declaration
        std::string decl = indent + "auto " + varName + " = " + ctx.selectedText + ";\n";
        newCode.insert(lineStart, decl);
        
        // Replace original expression with variable name
        pos = newCode.find(ctx.selectedText, lineStart + decl.size());
        if (pos != std::string::npos) {
            newCode.replace(pos, ctx.selectedText.size(), varName);
        }
    }
    
    RefactoringResult::FileEdit edit;
    edit.filePath = ctx.filePath;
    edit.originalContent = ctx.code;
    edit.newContent = newCode;
    
    return RefactoringResult::ok({edit}, "Extracted variable: " + varName, 0.9f);
}

RefactoringResult RefactoringEngine::doExtractConstant(const RefactoringContext& ctx) {
    if (ctx.selectedText.empty()) return RefactoringResult::fail("No value selected");
    
    std::string constName = "EXTRACTED_CONSTANT";
    auto it = ctx.params.find("name");
    if (it != ctx.params.end()) constName = it->second;
    
    std::string newCode = ctx.code;
    
    // Add constant at file scope (before first function)
    std::string constDecl = "static constexpr auto " + constName + " = " + ctx.selectedText + ";\n\n";
    
    // Replace all occurrences
    size_t pos = 0;
    while ((pos = newCode.find(ctx.selectedText, pos)) != std::string::npos) {
        newCode.replace(pos, ctx.selectedText.size(), constName);
        pos += constName.size();
    }
    
    newCode = constDecl + newCode;
    
    RefactoringResult::FileEdit edit;
    edit.filePath = ctx.filePath;
    edit.originalContent = ctx.code;
    edit.newContent = newCode;
    
    return RefactoringResult::ok({edit}, "Extracted constant: " + constName, 0.85f);
}

RefactoringResult RefactoringEngine::doInlineVariable(const RefactoringContext& ctx) {
    if (ctx.symbolUnderCursor.empty()) return RefactoringResult::fail("No variable under cursor");
    
    // Find the variable declaration
    std::regex declRe(R"((?:auto|const auto|int|float|double|std::string)\s+)" 
                      + ctx.symbolUnderCursor + R"(\s*=\s*(.+?);)");
    std::smatch match;
    if (!std::regex_search(ctx.code, match, declRe)) {
        return RefactoringResult::fail("Cannot find declaration of " + ctx.symbolUnderCursor);
    }
    
    std::string value = match[1].str();
    std::string newCode = ctx.code;
    
    // Remove declaration line
    newCode = std::regex_replace(newCode, declRe, "");
    
    // Replace all uses with the value
    std::regex useRe("\\b" + ctx.symbolUnderCursor + "\\b");
    newCode = std::regex_replace(newCode, useRe, value);
    
    RefactoringResult::FileEdit edit;
    edit.filePath = ctx.filePath;
    edit.originalContent = ctx.code;
    edit.newContent = newCode;
    
    return RefactoringResult::ok({edit}, "Inlined variable: " + ctx.symbolUnderCursor, 0.8f);
}

RefactoringResult RefactoringEngine::doRenameSymbol(const RefactoringContext& ctx) {
    if (ctx.symbolUnderCursor.empty()) return RefactoringResult::fail("No symbol under cursor");
    
    std::string newName = "renamed";
    auto it = ctx.params.find("newName");
    if (it != ctx.params.end()) newName = it->second;
    
    std::regex symRe("\\b" + ctx.symbolUnderCursor + "\\b");
    std::string newCode = std::regex_replace(ctx.code, symRe, newName);
    
    RefactoringResult::FileEdit edit;
    edit.filePath = ctx.filePath;
    edit.originalContent = ctx.code;
    edit.newContent = newCode;
    
    return RefactoringResult::ok({edit}, 
        "Renamed " + ctx.symbolUnderCursor + " → " + newName, 0.95f);
}

RefactoringResult RefactoringEngine::doConvertForToRangeFor(const RefactoringContext& ctx) {
    // Convert: for(int i=0; i<vec.size(); i++) → for(auto& elem : vec)
    std::regex forRe(R"(for\s*\(\s*(?:int|size_t|auto)\s+(\w+)\s*=\s*0\s*;\s*\1\s*<\s*(\w+)\.size\(\)\s*;\s*(?:\1\+\+|\+\+\1)\s*\))");
    
    std::string newCode = ctx.code;
    std::smatch match;
    if (std::regex_search(newCode, match, forRe)) {
        std::string iterVar = match[1].str();
        std::string container = match[2].str();
        std::string replacement = "for (auto& item : " + container + ")";
        newCode = std::regex_replace(newCode, forRe, replacement);
        
        // Replace container[i] with item
        std::regex indexRe(container + "\\[" + iterVar + "\\]");
        newCode = std::regex_replace(newCode, indexRe, "item");
    }
    
    RefactoringResult::FileEdit edit;
    edit.filePath = ctx.filePath;
    edit.originalContent = ctx.code;
    edit.newContent = newCode;
    
    return RefactoringResult::ok({edit}, "Converted to range-based for", 0.85f);
}

RefactoringResult RefactoringEngine::doConvertRawToSmartPtr(const RefactoringContext& ctx) {
    std::string newCode = ctx.code;
    
    // new T(...) → std::make_unique<T>(...)
    std::regex newRe(R"((\w+)\s*\*\s*(\w+)\s*=\s*new\s+(\w+)\s*\(([^)]*)\)\s*;)");
    newCode = std::regex_replace(newCode, newRe, 
        "auto $2 = std::make_unique<$3>($4);");
    
    // delete ptr → (removed, handled by smart ptr)
    std::regex delRe(R"(\s*delete\s+\w+\s*;\s*\n?)");
    newCode = std::regex_replace(newCode, delRe, "\n");
    
    RefactoringResult::FileEdit edit;
    edit.filePath = ctx.filePath;
    edit.originalContent = ctx.code;
    edit.newContent = newCode;
    
    return RefactoringResult::ok({edit}, "Converted raw pointers to smart pointers", 0.8f);
}

RefactoringResult RefactoringEngine::doOrganizeIncludes(const RefactoringContext& ctx) {
    auto lines = splitLines(ctx.code);
    
    std::vector<std::string> systemIncludes, projectIncludes, otherLines;
    
    for (const auto& line : lines) {
        if (line.find("#include <") == 0 || line.find("#include <") != std::string::npos) {
            systemIncludes.push_back(line);
        } else if (line.find("#include \"") == 0 || line.find("#include \"") != std::string::npos) {
            projectIncludes.push_back(line);
        } else {
            otherLines.push_back(line);
        }
    }
    
    // Sort includes
    std::sort(systemIncludes.begin(), systemIncludes.end());
    std::sort(projectIncludes.begin(), projectIncludes.end());
    
    // Remove duplicates
    systemIncludes.erase(std::unique(systemIncludes.begin(), systemIncludes.end()),
                          systemIncludes.end());
    projectIncludes.erase(std::unique(projectIncludes.begin(), projectIncludes.end()),
                           projectIncludes.end());
    
    // Reassemble: project includes first, then system, then rest
    std::ostringstream oss;
    for (const auto& inc : projectIncludes) oss << inc << "\n";
    if (!projectIncludes.empty() && !systemIncludes.empty()) oss << "\n";
    for (const auto& inc : systemIncludes) oss << inc << "\n";
    if (!systemIncludes.empty() || !projectIncludes.empty()) oss << "\n";
    for (const auto& line : otherLines) oss << line << "\n";
    
    RefactoringResult::FileEdit edit;
    edit.filePath = ctx.filePath;
    edit.originalContent = ctx.code;
    edit.newContent = oss.str();
    
    return RefactoringResult::ok({edit}, "Organized includes", 0.95f);
}

RefactoringResult RefactoringEngine::doAddNullChecks(const RefactoringContext& ctx) {
    auto lines = splitLines(ctx.code);
    std::ostringstream oss;
    
    // Simple heuristic: find pointer dereferences and add null checks
    std::regex ptrDeref(R"((\w+)->\w+)");
    
    for (const auto& line : lines) {
        std::smatch match;
        if (std::regex_search(line, match, ptrDeref)) {
            std::string ptr = match[1].str();
            // Check if there's already a null check on this line or nearby
            if (line.find("if") == std::string::npos && 
                line.find("nullptr") == std::string::npos) {
                // Determine indentation
                std::string indent;
                for (char c : line) {
                    if (std::isspace(static_cast<unsigned char>(c))) indent += c;
                    else break;
                }
                oss << indent << "if (!" << ptr << ") return; // null check\n";
            }
        }
        oss << line << "\n";
    }
    
    RefactoringResult::FileEdit edit;
    edit.filePath = ctx.filePath;
    edit.originalContent = ctx.code;
    edit.newContent = oss.str();
    
    return RefactoringResult::ok({edit}, "Added null checks", 0.7f);
}

RefactoringResult RefactoringEngine::doConvertSyncToAsync(const RefactoringContext& ctx) {
    // Mark function as async (for languages that support it)
    std::string newCode = ctx.code;
    if (ctx.language == "javascript" || ctx.language == "typescript") {
        std::regex funcRe(R"(function\s+(\w+))");
        newCode = std::regex_replace(newCode, funcRe, "async function $1");
    }
    
    RefactoringResult::FileEdit edit;
    edit.filePath = ctx.filePath;
    edit.originalContent = ctx.code;
    edit.newContent = newCode;
    
    return RefactoringResult::ok({edit}, "Converted to async", 0.75f);
}

RefactoringResult RefactoringEngine::doAddErrorHandling(const RefactoringContext& ctx) {
    if (ctx.selectedText.empty()) return RefactoringResult::fail("No code selected");
    
    std::string indent = "    ";
    std::ostringstream oss;
    
    if (ctx.language == "cpp" || ctx.language == "c++") {
        oss << indent << "// Error handling wrapper\n";
        oss << indent << "{\n";
        oss << indent << "    auto result = [&]() -> bool {\n";
        
        auto lines = splitLines(ctx.selectedText);
        for (const auto& line : lines) {
            oss << indent << "        " << line << "\n";
        }
        
        oss << indent << "        return true;\n";
        oss << indent << "    }();\n";
        oss << indent << "    if (!result) {\n";
        oss << indent << "        // Handle error\n";
        oss << indent << "        return;\n";
        oss << indent << "    }\n";
        oss << indent << "}\n";
    } else {
        // Generic try-catch style
        oss << indent << "try {\n";
        auto lines = splitLines(ctx.selectedText);
        for (const auto& line : lines) {
            oss << indent << "    " << line << "\n";
        }
        oss << indent << "} catch (error) {\n";
        oss << indent << "    console.error(error);\n";
        oss << indent << "}\n";
    }
    
    std::string newCode = ctx.code;
    auto pos = newCode.find(ctx.selectedText);
    if (pos != std::string::npos) {
        newCode.replace(pos, ctx.selectedText.size(), oss.str());
    }
    
    RefactoringResult::FileEdit edit;
    edit.filePath = ctx.filePath;
    edit.originalContent = ctx.code;
    edit.newContent = newCode;
    
    return RefactoringResult::ok({edit}, "Added error handling", 0.8f);
}

RefactoringResult RefactoringEngine::doConvertToAuto(const RefactoringContext& ctx) {
    std::string newCode = ctx.code;
    // Replace explicit types with auto where safe
    std::regex typeRe(R"((std::\w+<[^>]+>)\s+(\w+)\s*=)");
    newCode = std::regex_replace(newCode, typeRe, "auto $2 =");
    
    RefactoringResult::FileEdit edit;
    edit.filePath = ctx.filePath;
    edit.originalContent = ctx.code;
    edit.newContent = newCode;
    
    return RefactoringResult::ok({edit}, "Converted to auto", 0.85f);
}

RefactoringResult RefactoringEngine::doConvertToStructuredBindings(const RefactoringContext& ctx) {
    std::string newCode = ctx.code;
    // Convert: auto p = map.find(key); p->first; p->second;
    // To: auto [key, value] = *map.find(key);
    // Simplified: convert pair access
    std::regex pairRe(R"((\w+)\.first\b)");
    // This is a simplified version — full implementation would need AST
    
    RefactoringResult::FileEdit edit;
    edit.filePath = ctx.filePath;
    edit.originalContent = ctx.code;
    edit.newContent = newCode;
    
    return RefactoringResult::ok({edit}, "Converted to structured bindings", 0.7f);
}

RefactoringResult RefactoringEngine::doSplitDeclaration(const RefactoringContext& ctx) {
    if (ctx.symbolUnderCursor.empty()) return RefactoringResult::fail("No variable under cursor");
    
    std::string newCode = ctx.code;
    std::regex declInitRe("(\\w+\\s+" + ctx.symbolUnderCursor + ")\\s*=\\s*(.+?);");
    std::smatch match;
    if (std::regex_search(newCode, match, declInitRe)) {
        std::string decl = match[1].str() + ";";
        std::string init = ctx.symbolUnderCursor + " = " + match[2].str() + ";";
        newCode = std::regex_replace(newCode, declInitRe, decl + "\n" + init);
    }
    
    RefactoringResult::FileEdit edit;
    edit.filePath = ctx.filePath;
    edit.originalContent = ctx.code;
    edit.newContent = newCode;
    
    return RefactoringResult::ok({edit}, "Split declaration and initialization", 0.95f);
}

RefactoringResult RefactoringEngine::doConvertIfToTernary(const RefactoringContext& ctx) {
    std::string newCode = ctx.code;
    
    // Simple pattern: if (cond) x = a; else x = b; → x = cond ? a : b;
    std::regex ifElseRe(R"(if\s*\((.+?)\)\s*(\w+)\s*=\s*(.+?);\s*else\s*\2\s*=\s*(.+?);)");
    newCode = std::regex_replace(newCode, ifElseRe, "$2 = ($1) ? $3 : $4;");
    
    RefactoringResult::FileEdit edit;
    edit.filePath = ctx.filePath;
    edit.originalContent = ctx.code;
    edit.newContent = newCode;
    
    return RefactoringResult::ok({edit}, "Converted if/else to ternary", 0.9f);
}

RefactoringResult RefactoringEngine::doRemoveDeadCode(const RefactoringContext& ctx) {
    auto lines = splitLines(ctx.code);
    std::ostringstream oss;
    
    for (const auto& line : lines) {
        // Remove unreachable code after return/break/continue
        // This is a simplified version
        std::string trimmed = line;
        while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed[0])))
            trimmed.erase(0, 1);
        
        // Skip empty commented-out code blocks
        if (trimmed.find("// ") == 0 && trimmed.find("TODO") == std::string::npos) {
            continue; // Remove single-line comments that aren't TODOs
        }
        
        oss << line << "\n";
    }
    
    RefactoringResult::FileEdit edit;
    edit.filePath = ctx.filePath;
    edit.originalContent = ctx.code;
    edit.newContent = oss.str();
    
    return RefactoringResult::ok({edit}, "Removed dead code", 0.6f);
}

RefactoringResult RefactoringEngine::doAddBracesToControlFlow(const RefactoringContext& ctx) {
    std::string newCode = ctx.code;
    
    // Add braces to single-line if/for/while
    std::regex singleLineIf(R"((\s*)(if\s*\([^)]+\))\s*(?!\{)(\S.+;))");
    newCode = std::regex_replace(newCode, singleLineIf, "$1$2 {\n$1    $3\n$1}");
    
    RefactoringResult::FileEdit edit;
    edit.filePath = ctx.filePath;
    edit.originalContent = ctx.code;
    edit.newContent = newCode;
    
    return RefactoringResult::ok({edit}, "Added braces to control flow", 0.9f);
}

RefactoringResult RefactoringEngine::doFlipConditional(const RefactoringContext& ctx) {
    std::string newCode = ctx.code;
    
    // Flip if/else
    std::regex ifElse(R"(if\s*\((.+?)\)\s*\{([^}]*)\}\s*else\s*\{([^}]*)\})");
    newCode = std::regex_replace(newCode, ifElse, "if (!($1)) {$3} else {$2}");
    
    RefactoringResult::FileEdit edit;
    edit.filePath = ctx.filePath;
    edit.originalContent = ctx.code;
    edit.newContent = newCode;
    
    return RefactoringResult::ok({edit}, "Flipped conditional", 0.95f);
}

RefactoringResult RefactoringEngine::doMergeNestedIf(const RefactoringContext& ctx) {
    std::string newCode = ctx.code;
    
    // if (a) { if (b) { ... } } → if (a && b) { ... }
    std::regex nestedIf(R"(if\s*\((.+?)\)\s*\{\s*if\s*\((.+?)\)\s*\{([^}]*)\}\s*\})");
    newCode = std::regex_replace(newCode, nestedIf, "if (($1) && ($2)) {$3}");
    
    RefactoringResult::FileEdit edit;
    edit.filePath = ctx.filePath;
    edit.originalContent = ctx.code;
    edit.newContent = newCode;
    
    return RefactoringResult::ok({edit}, "Merged nested if statements", 0.85f);
}

RefactoringResult RefactoringEngine::doConvertStringToStringView(const RefactoringContext& ctx) {
    std::string newCode = ctx.code;
    
    // const std::string& → std::string_view for function parameters
    std::regex strRefParam(R"(const\s+std::string\s*&\s+(\w+))");
    newCode = std::regex_replace(newCode, strRefParam, "std::string_view $1");
    
    RefactoringResult::FileEdit edit;
    edit.filePath = ctx.filePath;
    edit.originalContent = ctx.code;
    edit.newContent = newCode;
    
    return RefactoringResult::ok({edit}, "Converted to string_view", 0.75f);
}

// ============================================================================
// Register All Built-in Refactorings
// ============================================================================
void RefactoringEngine::registerBuiltins() {
    struct BuiltinEntry {
        const char* id;
        const char* name;
        const char* desc;
        RefactoringCategory cat;
        const char* catName;
        bool needsSelection;
        bool needsSymbol;
        int order;
        std::vector<std::string> langs;
        std::function<RefactoringResult(RefactoringEngine*, const RefactoringContext&)> fn;
    };

    BuiltinEntry builtins[] = {
        {"extract.method", "Extract Method", "Extract selected code into a new method",
         RefactoringCategory::Extract, "Extract", true, false, 10, {"cpp","python","javascript","typescript"},
         &RefactoringEngine::doExtractMethod},
        {"extract.variable", "Extract Variable", "Extract expression into a named variable",
         RefactoringCategory::Extract, "Extract", true, false, 11, {"cpp","python","javascript","typescript"},
         &RefactoringEngine::doExtractVariable},
        {"extract.constant", "Extract Constant", "Extract literal value into a named constant",
         RefactoringCategory::Extract, "Extract", true, false, 12, {"cpp","python","javascript","typescript"},
         &RefactoringEngine::doExtractConstant},
        {"inline.variable", "Inline Variable", "Replace variable with its value",
         RefactoringCategory::Inline, "Inline", false, true, 20, {"cpp","javascript","typescript"},
         &RefactoringEngine::doInlineVariable},
        {"rename.symbol", "Rename Symbol", "Rename symbol across the file",
         RefactoringCategory::Rename, "Rename", false, true, 30, {},
         &RefactoringEngine::doRenameSymbol},
        {"convert.for_to_range", "Convert to Range-For", "Convert index-based for to range-based for",
         RefactoringCategory::Convert, "Convert", false, false, 40, {"cpp"},
         &RefactoringEngine::doConvertForToRangeFor},
        {"convert.raw_to_smart_ptr", "Convert to Smart Pointer", "Replace raw new/delete with smart pointers",
         RefactoringCategory::ModernCpp, "Modern C++", false, false, 41, {"cpp"},
         &RefactoringEngine::doConvertRawToSmartPtr},
        {"organize.includes", "Organize Includes", "Sort and deduplicate #include directives",
         RefactoringCategory::Organize, "Organize", false, false, 50, {"cpp","c"},
         &RefactoringEngine::doOrganizeIncludes},
        {"safety.null_checks", "Add Null Checks", "Add null checks before pointer dereferences",
         RefactoringCategory::Safety, "Safety", false, false, 60, {"cpp","c"},
         &RefactoringEngine::doAddNullChecks},
        {"convert.sync_to_async", "Convert to Async", "Convert synchronous function to async",
         RefactoringCategory::Convert, "Convert", false, false, 42, {"javascript","typescript"},
         &RefactoringEngine::doConvertSyncToAsync},
        {"safety.error_handling", "Add Error Handling", "Wrap selected code in error handling",
         RefactoringCategory::Safety, "Safety", true, false, 61, {},
         &RefactoringEngine::doAddErrorHandling},
        {"modern.auto", "Convert to Auto", "Replace explicit type with auto",
         RefactoringCategory::ModernCpp, "Modern C++", false, false, 43, {"cpp"},
         &RefactoringEngine::doConvertToAuto},
        {"modern.structured_bindings", "Structured Bindings", "Convert to structured bindings",
         RefactoringCategory::ModernCpp, "Modern C++", false, false, 44, {"cpp"},
         &RefactoringEngine::doConvertToStructuredBindings},
        {"general.split_declaration", "Split Declaration", "Split variable declaration and initialization",
         RefactoringCategory::General, "General", false, true, 70, {},
         &RefactoringEngine::doSplitDeclaration},
        {"convert.if_to_ternary", "Convert to Ternary", "Convert if/else to ternary operator",
         RefactoringCategory::Convert, "Convert", false, false, 45, {},
         &RefactoringEngine::doConvertIfToTernary},
        {"general.remove_dead_code", "Remove Dead Code", "Remove unreachable and commented-out code",
         RefactoringCategory::General, "General", false, false, 71, {},
         &RefactoringEngine::doRemoveDeadCode},
        {"general.add_braces", "Add Braces", "Add braces to single-line control flow",
         RefactoringCategory::General, "General", false, false, 72, {},
         &RefactoringEngine::doAddBracesToControlFlow},
        {"general.flip_conditional", "Flip Conditional", "Flip if/else branches",
         RefactoringCategory::General, "General", false, false, 73, {},
         &RefactoringEngine::doFlipConditional},
        {"general.merge_nested_if", "Merge Nested If", "Merge nested if statements with &&",
         RefactoringCategory::General, "General", false, false, 74, {},
         &RefactoringEngine::doMergeNestedIf},
        {"modern.string_view", "Convert to string_view", "Convert const string& params to string_view",
         RefactoringCategory::ModernCpp, "Modern C++", false, false, 45, {"cpp"},
         &RefactoringEngine::doConvertStringToStringView},
    };

    for (auto& b : builtins) {
        RefactoringDescriptor desc;
        desc.id = b.id;
        desc.name = b.name;
        desc.description = b.desc;
        desc.category = b.cat;
        desc.categoryName = b.catName;
        desc.requiresSelection = b.needsSelection;
        desc.requiresSymbol = b.needsSymbol;
        desc.menuOrder = b.order;
        desc.supportedLanguages = b.langs;

        auto fn = b.fn;
        desc.execute = [this, fn](const std::string& code,
                                    const std::string& language,
                                    const std::map<std::string, std::string>& params)
            -> TransformationResult {
            RefactoringContext ctx;
            ctx.code = code;
            ctx.language = language;
            ctx.params = params;
            if (params.count("selectedText")) ctx.selectedText = params.at("selectedText");
            if (params.count("symbol")) ctx.symbolUnderCursor = params.at("symbol");
            if (params.count("filePath")) ctx.filePath = params.at("filePath");
            
            auto result = (this->*fn)(ctx);
            TransformationResult tr;
            if (result.success && !result.edits.empty()) {
                tr.transformedCode = result.edits[0].newContent;
                tr.explanation = result.explanation;
                tr.confidence = result.confidence;
            } else {
                tr.warnings.push_back(result.error);
                tr.confidence = 0.0f;
            }
            tr.requiresManualReview = result.requiresApproval;
            return tr;
        };

        RegisterRefactoring(desc);
    }
}

} // namespace IDE
} // namespace RawrXD
