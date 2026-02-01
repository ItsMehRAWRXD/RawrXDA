#pragma once
#include <string>
#include <vector>
#include <memory>
#include "logger.h"
#include "metrics.h"

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
    std::vector<ParsedCompletion> flush();

private:
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;
    std::string m_buffer;
    
    std::vector<std::string> m_statementBoundaries = {";", "}", ")\n"};
    std::vector<std::string> m_customDelimiters = {"```", "<|endoftext|>"};

    std::vector<ParsedCompletion> splitByStatementBoundaries(const std::string& text);
    std::vector<ParsedCompletion> splitByLineBoundaries(const std::string& text);
    std::vector<ParsedCompletion> splitByTokenBoundaries(const std::string& text);
    int estimateTokenCount(const std::string& text);
    double calculateConfidence(const std::string& text);
};
