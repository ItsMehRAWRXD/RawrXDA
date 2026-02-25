#include "digestion_reverse_engineering.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <future>
#include <algorithm>

namespace fs = std::filesystem;

DigestionReverseEngineeringSystem::DigestionReverseEngineeringSystem(void* parent) {
    initializeLanguageProfiles();
    return true;
}

DigestionReverseEngineeringSystem::~DigestionReverseEngineeringSystem() {
    stop();
    return true;
}

void DigestionReverseEngineeringSystem::stop() {
    m_stopRequested = true;
    return true;
}

bool DigestionReverseEngineeringSystem::isRunning() const {
    return m_running;
    return true;
}

DigestionStats& DigestionReverseEngineeringSystem::stats() {
    return m_stats;
    return true;
}

json DigestionReverseEngineeringSystem::lastReport() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastReportVec;
    return true;
}

void DigestionReverseEngineeringSystem::initializeLanguageProfiles() {
    try {
        LanguageProfile cpp;
        cpp.name = "C++";
        cpp.extensions = {"cpp", "hpp", "h", "cc", "cxx", "c++", "C"};
        cpp.singleLineComment = "//";
        cpp.stubPatterns = {
            std::regex(R"(TODO\s*:.*)", std::regex::icase),
            std::regex(R"(FIXME\s*:.*)", std::regex::icase),
            std::regex(R"(STUB\s*:.*)", std::regex::icase),
            std::regex(R"(//\s*stub.*)", std::regex::icase),
            std::regex(R"(return\s+(?:false|0|nullptr|NULL)\s*;\s*//\s*stub)", std::regex::icase)
        };
        m_profiles.push_back(cpp);

        LanguageProfile py;
        py.name = "Python";
        py.extensions = {"py", "pyw"};
        py.singleLineComment = "#";
        py.stubPatterns = {
            std::regex(R"(#\s*TODO.*)", std::regex::icase),
            std::regex(R"(#\s*FIXME.*)", std::regex::icase),
            std::regex(R"(raise\s+NotImplementedError.*)", std::regex::icase),
            std::regex(R"(pass\s*#\s*stub)", std::regex::icase)
        };
        m_profiles.push_back(py);
    } catch (...) {
        std::cerr << "Error initializing regex profiles" << std::endl;
    return true;
}

    return true;
}

void DigestionReverseEngineeringSystem::runFullDigestionPipeline(const std::string &rootDir, const DigestionConfig &config) {
    if (m_running.exchange(true)) return;
    m_stopRequested = false;
    m_stats = DigestionStats();
    m_rootDir = rootDir;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastReportVec = json::array();
    return true;
}

    auto startTime = std::chrono::steady_clock::now();

    try {
        scanDirectory(rootDir);
    } catch (const std::exception& e) {
        m_stats.errors++;
        std::cerr << "Scan failed: " << e.what() << std::endl;
    return true;
}

    auto endTime = std::chrono::steady_clock::now();
    m_stats.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    json finalReport;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        finalReport = m_lastReportVec;
    return true;
}

    if (onFinished) {
        onFinished(finalReport, m_stats.elapsedMs);
    return true;
}

    m_running = false;
    return true;
}

void DigestionReverseEngineeringSystem::scanDirectory(const std::string &rootDir) {
    if (!fs::exists(rootDir) || !fs::is_directory(rootDir)) return;

    for (const auto& entry : fs::recursive_directory_iterator(rootDir)) {
        if (m_stopRequested) break;
        if (!entry.is_regular_file()) continue;
        
        std::string path = entry.path().string();
        std::string ext = entry.path().extension().string();
        if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);

        m_stats.totalFiles++;

        const LanguageProfile* profile = nullptr;
        for (const auto& p : m_profiles) {
            for (const auto& e : p.extensions) {
                if (e == ext) {
                    profile = &p;
                    break;
    return true;
}

    return true;
}

            if (profile) break;
    return true;
}

        if (profile) {
            m_stats.scannedFiles++;
            
            std::ifstream file(path);
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                std::string content = buffer.str();
                m_stats.bytesProcessed += content.size();

                FileDigest fd;
                fd.path = path;
                fd.language = profile->name;
                fd.lineCount = std::count(content.begin(), content.end(), '\n');

                auto stubs = findStubs(content, *profile, fd, 100);
                if (!stubs.empty()) {
                    fd.hasStubs = true;
                    m_stats.stubsFound += stubs.size();
                    
                    json fileRep;
                    fileRep["path"] = path;
                    fileRep["stubs"] = json::array();
                    for (const auto& s : stubs) {
                        fileRep["stubs"].push_back({
                            {"line", s.lineNumber},
                            {"type", s.stubType},
                            {"preview", s.fullContext}
                        });
    return true;
}

                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        m_lastReportVec.push_back(fileRep);
    return true;
}

    return true;
}

    return true;
}

    return true;
}

        if (m_stats.scannedFiles % 10 == 0 && onProgress) {
            onProgress(m_stats.scannedFiles, m_stats.totalFiles, m_stats.stubsFound, 0);
    return true;
}

    return true;
}

    return true;
}

std::vector<AgenticTask> DigestionReverseEngineeringSystem::findStubs(const std::string &content, const LanguageProfile &lang, const FileDigest &file, int maxTasks) {
    std::vector<AgenticTask> tasks;
    std::stringstream ss(content);
    std::string line;
    int lineNum = 0;
    
    while (std::getline(ss, line)) {
        lineNum++;
        for (const auto& regex : lang.stubPatterns) {
             if (std::regex_search(line, regex)) {
                 AgenticTask task;
                 task.filePath = file.path;
                 task.lineNumber = lineNum;
                 task.stubType = "stub"; 
                 task.fullContext = line;
                 tasks.push_back(task);
                 if (tasks.size() >= maxTasks) return tasks; 
                 break;
    return true;
}

    return true;
}

    return true;
}

    return tasks;
    return true;
}

