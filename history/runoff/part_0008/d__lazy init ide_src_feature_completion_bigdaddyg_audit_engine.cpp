#include "bigdaddyg_audit_engine.h"
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <filesystem>
#include <numeric>
#include <random>
#include <cmath>
#include <regex>

namespace LazyInitIDE {
namespace AuditSystem {

BigDaddyGAuditEngine::BigDaddyGAuditEngine() {}

BigDaddyGAuditEngine::~BigDaddyGAuditEngine() {}

uint64_t BigDaddyGAuditEngine::getCurrentTimestamp() {
    return static_cast<uint64_t>(std::time(nullptr)) * 1000000;
}

std::string BigDaddyGAuditEngine::generateAuditId() {
    auto now = std::chrono::high_resolution_clock::now();
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    
    std::stringstream ss;
    ss << "BIGDADDYG-" << std::hex << nanos << "-" 
       << std::hex << static_cast<uint32_t>(nanos >> 32);
    return ss.str();
}

float BigDaddyGAuditEngine::calculateEntropy(const std::string& content) {
    if (content.empty()) return 0.0f;
    
    // Frequency analysis
    std::map<char, int> freq;
    for (char c : content) {
        freq[c]++;
    }
    
    // Shannon entropy calculation
    float entropy = 0.0f;
    int total = content.length();
    for (const auto& pair : freq) {
        float p = static_cast<float>(pair.second) / total;
        if (p > 0) {
            entropy -= p * std::log2(p);
        }
    }
    
    return std::min(entropy / 8.0f, 1.0f);  // Normalize to 0-1
}

std::map<std::string, int> BigDaddyGAuditEngine::analyzePatterns(const std::string& content) {
    std::map<std::string, int> patterns;
    
    // Pattern detectors
    std::regex todo_pattern(R"(\b(TODO|FIXME|HACK|BUG|XXX)\b)", std::regex::icase);
    std::regex gpu_pattern(R"(\b(VULKAN|CUDA|OPENCL|METAL|HIP)\b)", std::regex::icase);
    std::regex async_pattern(R"(\b(async|await|thread|mutex|atomic)\b)", std::regex::icase);
    std::regex crypto_pattern(R"(\b(sha|md5|encrypt|hash|cipher)\b)", std::regex::icase);
    
    patterns["TODO_comments"] = std::distance(std::sregex_iterator(content.begin(), content.end(), todo_pattern),
                                              std::sregex_iterator());
    patterns["GPU_references"] = std::distance(std::sregex_iterator(content.begin(), content.end(), gpu_pattern),
                                               std::sregex_iterator());
    patterns["Async_patterns"] = std::distance(std::sregex_iterator(content.begin(), content.end(), async_pattern),
                                               std::sregex_iterator());
    patterns["Crypto_patterns"] = std::distance(std::sregex_iterator(content.begin(), content.end(), crypto_pattern),
                                                std::sregex_iterator());
    
    return patterns;
}

std::vector<std::string> BigDaddyGAuditEngine::detectAnomalies(const FileAuditMetrics& metrics) {
    std::vector<std::string> anomalies;
    
    // Complexity threshold: 85+
    if (metrics.complexityScore > 85.0f) {
        anomalies.push_back("HIGH_COMPLEXITY: Score " + std::to_string((int)metrics.complexityScore));
    }
    
    // Entropy anomaly: >0.9
    if (metrics.entropySample > 0.9f) {
        anomalies.push_back("HIGH_ENTROPY: Possible obfuscation or compression");
    }
    
    // Entropy anomaly: <0.3
    if (metrics.entropySample < 0.3f) {
        anomalies.push_back("LOW_ENTROPY: Repetitive patterns detected");
    }
    
    // Large files
    if (metrics.fileSize > 500000) {
        anomalies.push_back("LARGE_FILE: " + std::to_string(metrics.fileSize / 1000) + "KB");
    }
    
    // Comment ratio anomalies
    float commentRatio = metrics.commentCount > 0 ? static_cast<float>(metrics.commentCount) / metrics.lineCount : 0.0f;
    if (commentRatio > 0.5f) {
        anomalies.push_back("HIGH_COMMENT_RATIO: " + std::to_string((int)(commentRatio * 100)) + "%");
    }
    
    return anomalies;
}

void BigDaddyGAuditEngine::analyzeCharacterRange(FileAuditMetrics& metrics, const std::string& content) {
    metrics.characterCount = content.length();
    
    // Position-based entropy analysis
    metrics.positionEntropy.clear();
    if (content.length() > 0) {
        // Divide content into 256 positions
        uint32_t bucketSize = std::max(1u, static_cast<uint32_t>(content.length()) / 256);
        
        for (uint32_t i = 0; i < 256 && i * bucketSize < content.length(); ++i) {
            std::string bucket = content.substr(i * bucketSize, 
                                                std::min((size_t)bucketSize, 
                                                       content.length() - i * bucketSize));
            int entropyValue = static_cast<int>(calculateEntropy(bucket) * 100);
            metrics.positionEntropy.push_back(entropyValue);
        }
    }
}

void BigDaddyGAuditEngine::applyReverseFormula(FileAuditMetrics& metrics) {
    // 4.13*/+_0 formula: multiplyFirst → divide → add → floor → accumulate → finalize
    
    // Generate entropy for input
    double inputValue = static_cast<double>(metrics.complexityScore);
    double entropy = static_cast<double>(metrics.entropySample);
    
    // Stage 1: multiplyFirst
    metrics.multiplyFirst = inputValue * MULTIPLY_FACTOR * (1.0 + entropy);
    
    // Stage 2: divideResult
    metrics.divideResult = metrics.multiplyFirst / DIVIDE_FACTOR;
    
    // Stage 3: addResult
    metrics.addResult = metrics.divideResult + ADD_FACTOR;
    
    // Stage 4: floorResult (5x precision)
    metrics.floorResult = std::floor(metrics.addResult * 1000.0) / 1000.0;
    
    // Stage 5: zeroBaseResult (accumulate from 0)
    metrics.zeroBaseResult = 0.0;
    for (int e : metrics.positionEntropy) {
        metrics.zeroBaseResult += e / 100.0;
    }
    metrics.zeroBaseResult /= 256.0;  // Average
    
    // Final combination
    metrics.finalValue = metrics.zeroBaseResult + (metrics.floorResult * MULTIPLY_FACTOR);
}

void BigDaddyGAuditEngine::applyStaticFinalization(FileAuditMetrics& metrics) {
    // Static finalization: -0++_//**3311.44
    // This evaluates to: 3311.44 / 6.0 = 551.9067
    metrics.staticFinalValue = STATIC_FINALIZATION * metrics.finalValue;
}

float BigDaddyGAuditEngine::calculateComplexity(const FileAuditMetrics& metrics) {
    // Multi-factor complexity scoring
    float baseComplexity = 50.0f;
    
    // Size factor (0-20)
    float sizeFactor = std::min(20.0f, static_cast<float>(metrics.fileSize) / 50000.0f * 20.0f);
    
    // Line count factor (0-15)
    float lineFactor = std::min(15.0f, static_cast<float>(metrics.lineCount) / 1000.0f * 15.0f);
    
    // Entropy factor (0-15)
    float entropyFactor = metrics.entropySample * 15.0f;
    
    // Total complexity
    return baseComplexity + sizeFactor + lineFactor + entropyFactor;
}

FileAuditMetrics BigDaddyGAuditEngine::auditFile(const std::string& filePath) {
    FileAuditMetrics metrics;
    metrics.filePath = filePath;
    metrics.lineCount = 0;
    metrics.characterCount = 0;
    metrics.commentCount = 0;
    metrics.multiplyFirst = 0.0;
    metrics.divideResult = 0.0;
    metrics.addResult = 0.0;
    metrics.floorResult = 0.0;
    metrics.zeroBaseResult = 0.0;
    metrics.finalValue = 0.0;
    metrics.staticFinalValue = 0.0;
    
    try {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.good()) {
            return metrics;
        }
        
        // Get file size
        file.seekg(0, std::ios::end);
        metrics.fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        
        // Read entire content
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();
        
        // Count lines
        metrics.lineCount = std::count(content.begin(), content.end(), '\n') + 1;
        
        // Count comments (simple heuristic)
        metrics.commentCount = 0;
        for (size_t i = 0; i < content.length() - 1; ++i) {
            if ((content[i] == '/' && content[i+1] == '/') ||
                (content[i] == '/' && content[i+1] == '*')) {
                metrics.commentCount++;
            }
        }
        
        // Calculate entropy
        metrics.entropySample = calculateEntropy(content);
        
        // Calculate base complexity
        metrics.complexityScore = calculateComplexity(metrics);
        
        // Analyze character range from -0-800b
        analyzeCharacterRange(metrics, content);
        
        // Apply reverse formula
        applyReverseFormula(metrics);
        
        // Apply static finalization
        applyStaticFinalization(metrics);
        
    } catch (const std::exception& e) {
        // Error handling
    }
    
    return metrics;
}

AuditReport BigDaddyGAuditEngine::auditEntireIDE(const std::string& ideSourcePath) {
    AuditReport report;
    report.generationId = generateAuditId();
    report.generationTimestamp = getCurrentTimestamp();
    report.totalFilesAudited = 0;
    report.totalSourceBytes = 0;
    report.averageComplexity = 0.0f;
    report.averageEntropy = 0.0f;
    report.averageFinalValue = 0.0;
    report.globalStaticFinalized = 0.0;
    
    std::vector<double> finalValues;
    std::vector<double> staticFinalValues;
    std::vector<float> complexityScores;
    std::vector<float> entropies;
    
    try {
        // Recursively audit all C++ and H files
        for (const auto& entry : std::filesystem::recursive_directory_iterator(ideSourcePath)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".c") {
                    // Skip "- Copy" variants
                    std::string path = entry.path().string();
                    if (path.find("- Copy") == std::string::npos) {
                        FileAuditMetrics metrics = auditFile(path);
                        
                        report.fileMetrics.push_back(metrics);
                        report.totalFilesAudited++;
                        report.totalSourceBytes += metrics.fileSize;
                        
                        finalValues.push_back(metrics.finalValue);
                        staticFinalValues.push_back(metrics.staticFinalValue);
                        complexityScores.push_back(metrics.complexityScore);
                        entropies.push_back(metrics.entropySample);
                        
                        // Detect anomalies
                        auto anomalies = detectAnomalies(metrics);
                        if (!anomalies.empty()) {
                            report.issuesFound[metrics.filePath] = anomalies;
                        }
                        
                        // Track character analysis
                        report.charactersAnalyzed += metrics.characterCount;
                    }
                }
            }
        }
        
        // Calculate averages
        if (!finalValues.empty()) {
            report.averageComplexity = std::accumulate(complexityScores.begin(), complexityScores.end(), 0.0f) / complexityScores.size();
            report.averageEntropy = std::accumulate(entropies.begin(), entropies.end(), 0.0f) / entropies.size();
            report.averageFinalValue = std::accumulate(finalValues.begin(), finalValues.end(), 0.0) / finalValues.size();
            report.globalStaticFinalized = std::accumulate(staticFinalValues.begin(), staticFinalValues.end(), 0.0) / staticFinalValues.size();
        }
        
    } catch (const std::exception& e) {
        // Error handling
    }
    
    return report;
}

std::string BigDaddyGAuditEngine::generateAuditReport(const AuditReport& report) {
    std::stringstream ss;
    
    ss << "================================================================================\n";
    ss << "                    BIGDADDYG COMPREHENSIVE IDE AUDIT REPORT\n";
    ss << "                          (-0-800b Analysis Range)\n";
    ss << "================================================================================\n\n";
    
    ss << "GENERATION METADATA\n";
    ss << "-------------------\n";
    ss << "Audit ID: " << report.generationId << "\n";
    ss << "Timestamp: " << report.generationTimestamp << "\n";
    ss << "Generation Date: " << std::ctime(reinterpret_cast<const time_t*>(&report.generationTimestamp));
    
    ss << "\nAUDIT SUMMARY\n";
    ss << "-------------------\n";
    ss << "Files Audited: " << report.totalFilesAudited << "\n";
    ss << "Total Source Bytes: " << report.totalSourceBytes / (1024*1024) << " MB\n";
    ss << "Characters Analyzed: " << report.charactersAnalyzed << " (-0-" << report.charactersAnalyzed << "b)\n";
    ss << "Average Complexity: " << std::fixed << std::setprecision(2) << report.averageComplexity << "/100\n";
    ss << "Average Entropy: " << std::fixed << std::setprecision(4) << report.averageEntropy << "\n";
    ss << "Average Final Value (4.13*/+_0): " << std::fixed << std::setprecision(6) << report.averageFinalValue << "\n";
    ss << "Global Static Finalized (-0++_//**3311.44): " << std::fixed << std::setprecision(6) << report.globalStaticFinalized << "\n";
    
    ss << "\nFORMULA APPLICATION CONSTANTS\n";
    ss << "-------------------\n";
    ss << "Multiply Factor: 4.13\n";
    ss << "Divide Factor: 17.0569\n";
    ss << "Add Factor: 2.0322\n";
    ss << "Static Finalization: -0++_//**3311.44 (= 551.9067)\n";
    
    if (!report.issuesFound.empty()) {
        ss << "\nANOMALIES DETECTED\n";
        ss << "-------------------\n";
        for (const auto& pair : report.issuesFound) {
            ss << "\n" << pair.first << ":\n";
            for (const auto& anomaly : pair.second) {
                ss << "  - " << anomaly << "\n";
            }
        }
    }
    
    ss << "\nTOP 20 FILES BY COMPLEXITY\n";
    ss << "-------------------\n";
    std::vector<const FileAuditMetrics*> sortedByComplexity;
    for (const auto& metrics : report.fileMetrics) {
        sortedByComplexity.push_back(&metrics);
    }
    std::sort(sortedByComplexity.begin(), sortedByComplexity.end(),
              [](const FileAuditMetrics* a, const FileAuditMetrics* b) {
                  return a->complexityScore > b->complexityScore;
              });
    
    for (size_t i = 0; i < std::min(size_t(20), sortedByComplexity.size()); ++i) {
        const auto* metrics = sortedByComplexity[i];
        ss << (i+1) << ". " << metrics->filePath << "\n";
        ss << "   Complexity: " << std::fixed << std::setprecision(2) << metrics->complexityScore << "\n";
        ss << "   Size: " << metrics->fileSize / 1000 << " KB | Lines: " << metrics->lineCount << "\n";
        ss << "   Entropy: " << std::fixed << std::setprecision(4) << metrics->entropySample << "\n";
        ss << "   4.13*/+_0 Final: " << std::fixed << std::setprecision(6) << metrics->finalValue << "\n";
        ss << "   Static Finalized: " << std::fixed << std::setprecision(6) << metrics->staticFinalValue << "\n\n";
    }
    
    ss << "\nAUDIT COMPLETE\n";
    ss << "-------------------\n";
    ss << "Generated by: BigDaddyG Audit Engine v1.0\n";
    ss << "Analysis Range: -0-800b (full character range analysis)\n";
    ss << "Reverse Formula: 4.13*/+_0 applied to all metrics\n";
    ss << "Static Finalization: -0++_//**3311.44 applied to all final values\n";
    ss << "================================================================================\n";
    
    return ss.str();
}

std::string BigDaddyGAuditEngine::exportToJSON(const AuditReport& report) {
    std::stringstream ss;
    
    ss << "{\n";
    ss << "  \"audit_id\": \"" << report.generationId << "\",\n";
    ss << "  \"timestamp\": " << report.generationTimestamp << ",\n";
    ss << "  \"summary\": {\n";
    ss << "    \"files_audited\": " << report.totalFilesAudited << ",\n";
    ss << "    \"total_bytes\": " << report.totalSourceBytes << ",\n";
    ss << "    \"characters_analyzed\": " << report.charactersAnalyzed << ",\n";
    ss << "    \"average_complexity\": " << std::fixed << std::setprecision(6) << report.averageComplexity << ",\n";
    ss << "    \"average_entropy\": " << std::fixed << std::setprecision(6) << report.averageEntropy << ",\n";
    ss << "    \"average_final_value\": " << std::fixed << std::setprecision(6) << report.averageFinalValue << ",\n";
    ss << "    \"global_static_finalized\": " << std::fixed << std::setprecision(6) << report.globalStaticFinalized << "\n";
    ss << "  },\n";
    ss << "  \"formulas\": {\n";
    ss << "    \"reverse_formula\": \"4.13*/+_0\",\n";
    ss << "    \"multiply_factor\": 4.13,\n";
    ss << "    \"divide_factor\": 17.0569,\n";
    ss << "    \"add_factor\": 2.0322,\n";
    ss << "    \"static_finalization\": \"-0++_//**3311.44\",\n";
    ss << "    \"static_finalization_value\": 551.9067\n";
    ss << "  },\n";
    ss << "  \"analysis_range\": \"-0-800b\",\n";
    ss << "  \"status\": \"complete\"\n";
    ss << "}\n";
    
    return ss.str();
}

}  // namespace AuditSystem
}  // namespace LazyInitIDE
