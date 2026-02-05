#pragma once
#include <string>
#include <vector>
#include <memory>

// Stub Logger if definition is missing
class Logger {
public:
    template<typename... Args>
    void info(Args&&...) {}
    template<typename... Args>
    void debug(Args&&...) {}
    template<typename... Args>
    void warn(Args&&...) {}
    template<typename... Args>
    void error(Args&&...) {}
};

// Stub Metrics if definition is missing
class Metrics {
public:
    void recordHistogram(const std::string&, double) {}
    void incrementCounter(const std::string&) {}
};

enum class BoundaryType {
    Statement,
    Block,
    Section,
    None
};

struct ParsedCompletion {
    std::string text;
    int tokenCount = 0;
    double confidence = 0.0;
    std::string boundary;
    bool isComplete = false;
};

class ResponseParser {
public:
    ResponseParser(std::shared_ptr<Logger> logger, std::shared_ptr<Metrics> metrics);
    
    std::vector<ParsedCompletion> parseResponse(const std::string& response);
    std::vector<ParsedCompletion> parseChunk(const std::string& chunk);
    
    std::vector<ParsedCompletion> flush();
    std::pair<BoundaryType, std::string> detectBoundary(const std::string& text);
    std::vector<std::pair<std::string, double>> getStatistics() const;
    void reset();
    void setCustomDelimiters(const std::vector<std::string>& delimiters);
    void setStatementBoundaries(const std::vector<std::string>& boundaries);

private:
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;
    
    std::vector<std::string> m_statementBoundaries = {";", "}\n", "\n\n"};
    std::vector<std::string> m_customDelimiters;
    std::string m_buffer;
    size_t m_totalCharsParsed = 0;

    std::vector<ParsedCompletion> splitByStatementBoundaries(const std::string& text);
    std::vector<ParsedCompletion> splitByLineBoundaries(const std::string& text);
    std::vector<ParsedCompletion> splitByTokenBoundaries(const std::string& text);
    
    int estimateTokenCount(const std::string& text);
    double calculateConfidence(const std::string& text);
};
