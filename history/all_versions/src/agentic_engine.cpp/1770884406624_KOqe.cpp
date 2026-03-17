#include "agentic_engine.h"
#include "cpu_inference_engine.h"
#include "native_agent.hpp"
#include "advanced_agent_features.hpp"
#include "reverse_engineering/RawrDumpBin.hpp"
#include "reverse_engineering/RawrCodex.hpp"
#include "reverse_engineering/RawrCompiler.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <regex>
#include <filesystem>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <numeric>

// MASM Telemetry bridge — lock-free atomic counters
#include "rawrxd_telemetry_exports.h"

AgenticEngine::AgenticEngine() : m_inferenceEngine(nullptr) {}

AgenticEngine::~AgenticEngine() {}

void AgenticEngine::initialize() {
    // Initialization logic if needed
}

std::string AgenticEngine::analyzeCode(const std::string& code) {
    return chat("Analyze this code:\n" + code);
}

std::string AgenticEngine::analyzeCodeQuality(const std::string& code) {
    return chat("Evaluate code quality for:\n" + code);
}

std::string AgenticEngine::detectPatterns(const std::string& code) {
    return chat("Detect patterns in:\n" + code);
}

std::string AgenticEngine::calculateMetrics(const std::string& code) {
    // Real code metrics calculation
    size_t totalLines = 0;
    size_t codeLines = 0;
    size_t commentLines = 0;
    size_t blankLines = 0;
    size_t functionCount = 0;
    size_t classCount = 0;
    int maxNestingDepth = 0;
    int currentNesting = 0;
    int cyclomaticComplexity = 1; // Base complexity
    size_t totalTokens = 0;
    bool inBlockComment = false;

    std::istringstream stream(code);
    std::string line;

    while (std::getline(stream, line)) {
        totalLines++;
        
        // Trim whitespace
        std::string trimmed = line;
        size_t start = trimmed.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            blankLines++;
            continue;
        }
        trimmed = trimmed.substr(start);

        // Handle block comments
        if (inBlockComment) {
            commentLines++;
            if (trimmed.find("*/") != std::string::npos) {
                inBlockComment = false;
            }
            continue;
        }

        if (trimmed.substr(0, 2) == "/*") {
            commentLines++;
            if (trimmed.find("*/") == std::string::npos) {
                inBlockComment = true;
            }
            continue;
        }

        if (trimmed.substr(0, 2) == "//" || trimmed[0] == '#') {
            commentLines++;
            continue;
        }

        codeLines++;

        // Count tokens (rough: whitespace-separated + operators)
        for (size_t i = 0; i < trimmed.size(); i++) {
            if (trimmed[i] == ' ' || trimmed[i] == '\t') totalTokens++;
        }
        totalTokens++; // for last token on line

        // Nesting depth tracking
        for (char c : line) {
            if (c == '{') {
                currentNesting++;
                if (currentNesting > maxNestingDepth) {
                    maxNestingDepth = currentNesting;
                }
            } else if (c == '}') {
                currentNesting--;
            }
        }

        // Cyclomatic complexity: count decision points
        // if, else if, for, while, case, catch, &&, ||, ?:
        static const std::regex decisionPattern(
            R"(\b(if|else\s+if|for|while|case|catch|switch)\b|\&\&|\|\||\?)"
        );
        std::sregex_iterator it(trimmed.begin(), trimmed.end(), decisionPattern);
        std::sregex_iterator end;
        cyclomaticComplexity += static_cast<int>(std::distance(it, end));

        // Function detection (C/C++/Java/JS patterns)
        static const std::regex funcPattern(
            R"(\b\w+\s+\w+\s*\([^)]*\)\s*(\{|$))"
        );
        if (std::regex_search(trimmed, funcPattern)) {
            functionCount++;
        }

        // Class detection
        static const std::regex classPattern(
            R"(\b(class|struct|interface|enum)\s+\w+)"
        );
        if (std::regex_search(trimmed, classPattern)) {
            classCount++;
        }
    }

    // Compute Halstead-inspired complexity estimate
    double maintainabilityIndex = 0.0;
    if (codeLines > 0 && totalTokens > 0) {
        double halsteadVolume = static_cast<double>(totalTokens) * std::log2(static_cast<double>(totalTokens));
        maintainabilityIndex = std::max(0.0,
            171.0 - 5.2 * std::log(halsteadVolume) - 0.23 * cyclomaticComplexity - 16.2 * std::log(static_cast<double>(codeLines)));
    }

    // Estimate algorithmic complexity based on nesting depth and patterns
    std::string estimatedComplexity;
    if (maxNestingDepth <= 1) estimatedComplexity = "O(n)";
    else if (maxNestingDepth == 2) estimatedComplexity = "O(n^2)";
    else if (maxNestingDepth == 3) estimatedComplexity = "O(n^3)";
    else estimatedComplexity = "O(n^" + std::to_string(maxNestingDepth) + ")";

    // Check for logarithmic patterns (binary search, divide-and-conquer)
    if (code.find("/= 2") != std::string::npos || code.find(">> 1") != std::string::npos ||
        code.find("/ 2") != std::string::npos) {
        if (maxNestingDepth <= 2) estimatedComplexity = "O(n log n)";
        else if (maxNestingDepth <= 1) estimatedComplexity = "O(log n)";
    }

    // Build JSON result
    std::ostringstream json;
    json << "{\n"
         << "  \"totalLines\": " << totalLines << ",\n"
         << "  \"codeLines\": " << codeLines << ",\n"
         << "  \"commentLines\": " << commentLines << ",\n"
         << "  \"blankLines\": " << blankLines << ",\n"
         << "  \"commentRatio\": " << (totalLines > 0 ? static_cast<double>(commentLines) / totalLines : 0.0) << ",\n"
         << "  \"functionCount\": " << functionCount << ",\n"
         << "  \"classCount\": " << classCount << ",\n"
         << "  \"cyclomaticComplexity\": " << cyclomaticComplexity << ",\n"
         << "  \"maxNestingDepth\": " << maxNestingDepth << ",\n"
         << "  \"estimatedComplexity\": \"" << estimatedComplexity << "\",\n"
         << "  \"maintainabilityIndex\": " << static_cast<int>(maintainabilityIndex) << ",\n"
         << "  \"tokens\": " << totalTokens << "\n"
         << "}";

    return json.str();
}

std::string AgenticEngine::suggestImprovements(const std::string& code) {
    return chat("Suggest improvements for:\n" + code);
}

std::string AgenticEngine::generateCode(const std::string& prompt) {
    return chat("Generate code: " + prompt);
}

std::string AgenticEngine::generateFunction(const std::string& signature, const std::string& description) {
    return chat("Generate function " + signature + ": " + description);
}

std::string AgenticEngine::generateClass(const std::string& className, const std::string& spec) {
    return chat("Generate class " + className + " with spec: " + spec);
}

std::string AgenticEngine::generateTests(const std::string& code) {
    return chat("Generate unit tests for:\n" + code);
}

std::string AgenticEngine::refactorCode(const std::string& code, const std::string& refactoringType) {
    return chat("Refactor code (" + refactoringType + "):\n" + code);
}

std::string AgenticEngine::planTask(const std::string& goal) {
    return chat("Plan task: " + goal);
}

std::string AgenticEngine::decomposeTask(const std::string& task) {
    return chat("Decompose task: " + task);
}

std::string AgenticEngine::generateWorkflow(const std::string& project) {
    return chat("Generate workflow for project: " + project);
}

std::string AgenticEngine::estimateComplexity(const std::string& task) {
    return chat("Estimate complexity for: " + task);
}

std::string AgenticEngine::understandIntent(const std::string& userInput) {
    return chat("What is the intent of: " + userInput);
}

std::string AgenticEngine::extractEntities(const std::string& text) {
    return chat("Extract entities from: " + text);
}

std::string AgenticEngine::generateNaturalResponse(const std::string& query, const std::string& context) {
    return chat("Context: " + context + "\nQuery: " + query);
}

std::string AgenticEngine::summarizeCode(const std::string& code) {
    return chat("Summarize code:\n" + code);
}

std::string AgenticEngine::explainError(const std::string& errorMessage) {
    return chat("Explain error: " + errorMessage);
}

void AgenticEngine::collectFeedback(const std::string&, bool, const std::string&) {}
void AgenticEngine::trainFromFeedback() {}
std::string AgenticEngine::getLearningStats() const { return "{}"; }
void AgenticEngine::adaptToUserPreferences(const std::string&) {}

bool AgenticEngine::validateInput(const std::string&) { return true; }
std::string AgenticEngine::sanitizeCode(const std::string& code) { return code; }
bool AgenticEngine::isCommandSafe(const std::string&) { return true; }

std::string AgenticEngine::grepFiles(const std::string& pattern, const std::string& path) {
    std::ostringstream results;
    int matchCount = 0;
    const int maxResults = 500;

    try {
        std::regex searchRegex(pattern, std::regex::ECMAScript | std::regex::icase);
        std::string searchPath = path.empty() ? "." : path;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                searchPath, std::filesystem::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            if (matchCount >= maxResults) break;

            // Skip binary and large files
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            static const std::unordered_set<std::string> textExts = {
                ".cpp", ".hpp", ".h", ".c", ".cc", ".cxx", ".hxx",
                ".py", ".js", ".ts", ".jsx", ".tsx", ".java", ".cs",
                ".go", ".rs", ".rb", ".php", ".swift", ".kt",
                ".json", ".xml", ".yaml", ".yml", ".toml", ".ini", ".cfg",
                ".md", ".txt", ".cmake", ".sh", ".bat", ".ps1",
                ".html", ".css", ".scss", ".less", ".sql", ".asm"
            };
            if (!textExts.count(ext)) continue;

            auto fileSize = entry.file_size();
            if (fileSize > 10 * 1024 * 1024) continue; // Skip >10MB

            std::ifstream file(entry.path(), std::ios::in);
            if (!file.is_open()) continue;

            std::string line;
            int lineNum = 0;
            while (std::getline(file, line)) {
                lineNum++;
                if (matchCount >= maxResults) break;

                if (std::regex_search(line, searchRegex)) {
                    results << entry.path().string() << ":" << lineNum << ": " << line << "\n";
                    matchCount++;
                }
            }
        }
    } catch (const std::regex_error& e) {
        // Fall back to literal string search
        std::string searchPath = path.empty() ? "." : path;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                searchPath, std::filesystem::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            if (matchCount >= maxResults) break;

            std::ifstream file(entry.path(), std::ios::in);
            if (!file.is_open()) continue;

            std::string line;
            int lineNum = 0;
            while (std::getline(file, line)) {
                lineNum++;
                if (matchCount >= maxResults) break;
                if (line.find(pattern) != std::string::npos) {
                    results << entry.path().string() << ":" << lineNum << ": " << line << "\n";
                    matchCount++;
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        results << "[Error: " << e.what() << "]\n";
    }

    if (matchCount == 0) return "[No matches found for: " + pattern + "]\n";
    return results.str();
}

std::string AgenticEngine::readFile(const std::string& filePath, int startLine, int endLine) {
    std::ifstream file(filePath, std::ios::in);
    if (!file.is_open()) {
        return "[Error: Cannot open file: " + filePath + "]";
    }

    std::ostringstream result;
    std::string line;
    int lineNum = 0;

    // Default: read entire file if no range specified
    if (startLine <= 0 && endLine <= 0) {
        startLine = 1;
        endLine = std::numeric_limits<int>::max();
    }
    if (startLine <= 0) startLine = 1;
    if (endLine <= 0) endLine = std::numeric_limits<int>::max();

    // Cap range to prevent excessive reads
    if (endLine - startLine > 10000) {
        endLine = startLine + 10000;
    }

    while (std::getline(file, line)) {
        lineNum++;
        if (lineNum < startLine) continue;
        if (lineNum > endLine) break;
        result << lineNum << "| " << line << "\n";
    }

    if (lineNum < startLine) {
        return "[Error: File has only " + std::to_string(lineNum) + " lines, requested start at " + std::to_string(startLine) + "]";
    }

    return result.str();
}

std::string AgenticEngine::searchFiles(const std::string& query, const std::string& path) {
    std::ostringstream results;
    int matchCount = 0;
    const int maxResults = 100;

    try {
        std::string searchPath = path.empty() ? "." : path;

        // Tokenize query into search terms
        std::vector<std::string> terms;
        std::istringstream iss(query);
        std::string term;
        while (iss >> term) {
            std::transform(term.begin(), term.end(), term.begin(), ::tolower);
            terms.push_back(term);
        }

        if (terms.empty()) return "[Error: Empty search query]\n";

        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                searchPath, std::filesystem::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            if (matchCount >= maxResults) break;

            // Check filename match first
            std::string filename = entry.path().filename().string();
            std::string filenameLower = filename;
            std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), ::tolower);

            bool filenameMatch = false;
            for (const auto& t : terms) {
                if (filenameLower.find(t) != std::string::npos) {
                    filenameMatch = true;
                    break;
                }
            }

            if (filenameMatch) {
                results << "[FILE] " << entry.path().string() << " (" << entry.file_size() << " bytes)\n";
                matchCount++;
                continue;
            }

            // Check file contents for text files
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            static const std::unordered_set<std::string> textExts = {
                ".cpp", ".hpp", ".h", ".c", ".py", ".js", ".ts", ".java",
                ".json", ".xml", ".yaml", ".yml", ".md", ".txt", ".cmake"
            };
            if (!textExts.count(ext)) continue;
            if (entry.file_size() > 5 * 1024 * 1024) continue;

            std::ifstream file(entry.path(), std::ios::in);
            if (!file.is_open()) continue;

            std::string content((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
            std::string contentLower = content;
            std::transform(contentLower.begin(), contentLower.end(), contentLower.begin(), ::tolower);

            // Score by number of terms found
            int score = 0;
            for (const auto& t : terms) {
                if (contentLower.find(t) != std::string::npos) score++;
            }

            if (score > 0) {
                results << "[CONTENT score=" << score << "/" << terms.size() << "] " 
                        << entry.path().string() << "\n";
                matchCount++;
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        results << "[Error: " << e.what() << "]\n";
    }

    if (matchCount == 0) return "[No files found matching: " + query + "]\n";
    return results.str();
}

std::string AgenticEngine::referenceSymbol(const std::string& symbol) {
    // Find all references to a symbol across the codebase
    // Uses simple text search with word-boundary awareness
    std::ostringstream results;
    int defCount = 0;
    int refCount = 0;
    const int maxResults = 200;

    try {
        // Build patterns for different reference types
        std::regex defPattern(
            R"(\b(class|struct|enum|void|int|bool|auto|float|double|string|char|unsigned|signed|long|short)\s+)" 
            + symbol + R"(\b)",
            std::regex::ECMAScript
        );
        std::regex callPattern(
            symbol + R"(\s*\()",
            std::regex::ECMAScript
        );
        std::regex memberPattern(
            R"((\.|->|::))" + symbol + R"(\b)",
            std::regex::ECMAScript
        );

        std::string searchPath = ".";
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                searchPath, std::filesystem::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            if (defCount + refCount >= maxResults) break;

            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            static const std::unordered_set<std::string> codeExts = {
                ".cpp", ".hpp", ".h", ".c", ".cc", ".cxx", ".hxx",
                ".py", ".js", ".ts", ".java", ".cs", ".go", ".rs"
            };
            if (!codeExts.count(ext)) continue;
            if (entry.file_size() > 5 * 1024 * 1024) continue;

            std::ifstream file(entry.path(), std::ios::in);
            if (!file.is_open()) continue;

            std::string line;
            int lineNum = 0;
            while (std::getline(file, line)) {
                lineNum++;
                if (defCount + refCount >= maxResults) break;

                // Check for symbol anywhere in line first (fast path)
                if (line.find(symbol) == std::string::npos) continue;

                // Classify the reference
                if (std::regex_search(line, defPattern)) {
                    results << "[DEF] " << entry.path().string() << ":" << lineNum << ": " << line << "\n";
                    defCount++;
                } else if (std::regex_search(line, callPattern)) {
                    results << "[CALL] " << entry.path().string() << ":" << lineNum << ": " << line << "\n";
                    refCount++;
                } else if (std::regex_search(line, memberPattern)) {
                    results << "[MEMBER] " << entry.path().string() << ":" << lineNum << ": " << line << "\n";
                    refCount++;
                } else {
                    // Generic reference
                    results << "[REF] " << entry.path().string() << ":" << lineNum << ": " << line << "\n";
                    refCount++;
                }
            }
        }
    } catch (const std::regex_error&) {
        // Fall back to simple string search
        return grepFiles(symbol, ".");
    } catch (const std::filesystem::filesystem_error& e) {
        results << "[Error: " << e.what() << "]\n";
    }

    std::ostringstream summary;
    summary << "=== Symbol: " << symbol << " ===\n"
            << "Definitions: " << defCount << " | References: " << refCount << "\n\n"
            << results.str();

    if (defCount + refCount == 0) {
        return "[No references found for symbol: " + symbol + "]\n";
    }

    return summary.str();
}

void AgenticEngine::updateConfig(const GenerationConfig& config) {
    m_config = config;
    if (m_inferenceEngine) {
        m_inferenceEngine->SetMaxMode(config.maxMode);
        m_inferenceEngine->SetDeepThinking(config.deepThinking);
        m_inferenceEngine->SetDeepResearch(config.deepResearch);
        // noRefusal is handled in the agent prompt builder
    }
}

std::string AgenticEngine::runDumpbin(const std::string& filePath, const std::string& mode) {
    RawrXD::ReverseEngineering::RawrDumpBin db;
    if (mode == "headers") return db.DumpHeaders(filePath);
    if (mode == "imports") return db.DumpImports(filePath);
    if (mode == "exports") return db.DumpExports(filePath);
    return db.DumpHeaders(filePath); // default
}

std::string AgenticEngine::runCodex(const std::string& filePath) {
    RawrXD::ReverseEngineering::RawrCodex codex;
    if (codex.LoadBinary(filePath)) {
        std::string result = "=== Codex Analysis: " + filePath + " ===\n";
        auto sections = codex.GetSections();
        result += "Sections: " + std::to_string(sections.size()) + "\n";
        for (const auto& s : sections) {
            result += "  " + s.name + " VA:0x" + std::to_string(s.virtualAddress)
                    + " Size:0x" + std::to_string(s.virtualSize) + "\n";
        }
        auto imports = codex.GetImports();
        result += "Imports: " + std::to_string(imports.size()) + "\n";
        for (size_t i = 0; i < std::min<size_t>(20, imports.size()); ++i) {
            result += "  " + imports[i].moduleName + "!" + imports[i].functionName + "\n";
        }
        auto exports = codex.GetExports();
        result += "Exports: " + std::to_string(exports.size()) + "\n";
        for (size_t i = 0; i < std::min<size_t>(20, exports.size()); ++i) {
            result += "  " + exports[i].name + "\n";
        }
        auto vulns = codex.DetectVulnerabilities();
        if (!vulns.empty()) {
            result += "Vulnerabilities: " + std::to_string(vulns.size()) + "\n";
            for (const auto& v : vulns) {
                result += "  [" + v.severity + "] " + v.type + ": " + v.description + "\n";
            }
        }
        return result;
    }
    return "Error: Could not analyze with Codex.";
}

std::string AgenticEngine::runCompiler(const std::string& sourceFile, const std::string& target) {
    RawrXD::ReverseEngineering::RawrCompiler compiler;
    auto result = compiler.CompileSource(sourceFile);
    if (result.success) {
        return "Compilation Successful: " + result.objectFile;
    } else {
        std::string errs;
        for (const auto& e : result.errors) errs += e + "\n";
        return "Compilation Failed:\n" + errs;
    }
}

std::string AgenticEngine::chat(const std::string& message) {
    if (!m_inferenceEngine) return "[Error: No Inference Engine]";
    
    RawrXD::NativeAgent agent(static_cast<RawrXD::CPUInferenceEngine*>(m_inferenceEngine));
    
    // Configure Agent from Engine config
    agent.SetMaxMode(m_config.maxMode);
    agent.SetDeepThink(m_config.deepThinking);
    agent.SetDeepResearch(m_config.deepResearch);
    agent.SetNoRefusal(m_config.noRefusal);

    std::string response;
    agent.SetOutputCallback([&](const std::string& token) {
        response += token;
    });
    
    agent.Ask(message);
    return response;
}

// ============================================================================
// SubAgent / Chaining / Swarm — convenience wrappers
// These use the engine's own chat() to run sub-tasks. For the full
// SubAgentManager with thread pools and progress tracking, use the
// bridge-level SubAgentManager directly.
// ============================================================================

std::string AgenticEngine::runSubAgent(const std::string& description, const std::string& prompt) {
    // Simple synchronous sub-agent: just call chat with the prompt
    return chat("You are a sub-agent tasked with: " + description + "\n\n" + prompt);
}

std::string AgenticEngine::executeChain(const std::vector<std::string>& steps,
                                         const std::string& initialInput) {
    std::string currentInput = initialInput;
    for (size_t i = 0; i < steps.size(); i++) {
        // Atomic counter increment — single lock xadd, practically invisible to profiler
#if defined(RAWRXD_LINK_TELEMETRY_KERNEL_ASM) || defined(RAWR_HAS_MASM)
        UTC_IncrementCounter(&g_Counter_AgentLoop);
#endif
        std::string prompt = steps[i];
        // Replace {{input}} placeholder
        const std::string placeholder = "{{input}}";
        size_t pos = 0;
        while ((pos = prompt.find(placeholder, pos)) != std::string::npos) {
            prompt.replace(pos, placeholder.size(), currentInput);
            pos += currentInput.size();
        }
        if (prompt == steps[i] && !currentInput.empty()) {
            prompt += "\n\nContext from previous step:\n" + currentInput;
        }
        currentInput = chat(prompt);
    }
    return currentInput;
}

std::string AgenticEngine::executeSwarm(const std::vector<std::string>& prompts,
                                         const std::string& mergeStrategy,
                                         int maxParallel) {
    // Simple sequential fallback (the real parallel version is in SubAgentManager)
    std::vector<std::string> results;
    for (const auto& prompt : prompts) {
        results.push_back(chat(prompt));
    }

    if (mergeStrategy == "vote") {
        // Pick most common
        std::unordered_map<std::string, int> votes;
        for (const auto& r : results) votes[r]++;
        std::string best;
        int bestCount = 0;
        for (const auto& [r, c] : votes) {
            if (c > bestCount) { bestCount = c; best = r; }
        }
        return best;
    }
    else if (mergeStrategy == "summarize") {
        std::string all;
        for (size_t i = 0; i < results.size(); i++) {
            all += "=== Task " + std::to_string(i + 1) + " ===\n" + results[i] + "\n\n";
        }
        return chat("Merge and synthesize these sub-agent outputs:\n\n" + all);
    }

    // Default: concatenate
    std::string merged;
    for (size_t i = 0; i < results.size(); i++) {
        merged += "=== Task " + std::to_string(i + 1) + " ===\n" + results[i] + "\n\n";
    }
    return merged;
}
