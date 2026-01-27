#pragma once

#include <string>
#include <vector>
#include <memory>
#include <regex>

#include "logging/logger.h"
#include "metrics/metrics.h"

/**
 * ResponseParser: Parses streaming/buffered model responses
 * 
 * Splits model output by natural code/language boundaries:
 * - Statement boundaries (;, }, {, etc.)
 * - Line boundaries (\n)
 * - Token boundaries (whitespace)
 * - Custom delimiters (||, <END>, etc.)
 */

struct ParsedCompletion {
    std::string text;
    int tokenCount;
    double confidence;
    std::string boundary; // What boundary ended this completion
    bool isComplete;      // True if ended with statement delimiter
};

enum class BoundaryType {
    STATEMENT,      // ;, }, ), ], etc.
    LINE,          // \n
    TOKEN,         // Whitespace
    CUSTOM,        // User-defined delimiter
    END_OF_STREAM  // End of model output
};

class ResponseParser {
private:
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;

    // Configuration
    std::vector<std::string> m_statementBoundaries = {";", "}", "{", ")", "]", "]\n", "}\n"};
    std::vector<std::string> m_customDelimiters = {"||", "<END>", "[END]", "</s>", "<|endoftext|>"};
    
    // State tracking
    std::string m_buffer;
    size_t m_totalCharsParsed = 0;

public:
    ResponseParser(
        std::shared_ptr<Logger> logger,
        std::shared_ptr<Metrics> metrics
    );

    /**
     * Parse complete response text into individual completions
     * @param response Full model response text
     * @return Vector of parsed completions
     */
    std::vector<ParsedCompletion> parseResponse(const std::string& response);

    /**
     * Parse streaming response chunks (incremental)
     * @param chunk New data chunk from stream
     * @return Completions ready for display (may be empty if chunk is incomplete)
     */
    std::vector<ParsedCompletion> parseChunk(const std::string& chunk);

    /**
     * Flush any remaining buffered data
     * @return Final completions from buffer
     */
    std::vector<ParsedCompletion> flush();

    /**
     * Split text by statement boundaries
     * @param text Input text to split
     * @return Vector of completions split at boundaries
     */
    std::vector<ParsedCompletion> splitByStatementBoundaries(const std::string& text);

    /**
     * Split text by line boundaries
     * @param text Input text to split
     * @return Vector of completions split at lines
     */
    std::vector<ParsedCompletion> splitByLineBoundaries(const std::string& text);

    /**
     * Split text by token boundaries (whitespace)
     * @param text Input text to split
     * @return Vector of completions split at tokens
     */
    std::vector<ParsedCompletion> splitByTokenBoundaries(const std::string& text);

    /**
     * Detect which boundary type ended this text segment
     * @param text Text to analyze
     * @return BoundaryType and the actual boundary string
     */
    std::pair<BoundaryType, std::string> detectBoundary(const std::string& text);

    /**
     * Calculate confidence score based on completion characteristics
     * @param completion Text to score
     * @return Confidence value 0.0-1.0
     */
    double calculateConfidence(const std::string& completion);

    /**
     * Count tokens in text (approximation: words + punctuation)
     * @param text Text to count
     * @return Approximate token count
     */
    int estimateTokenCount(const std::string& text);

    /**
     * Get parsing statistics
     * @return Map of metric names to values
     */
    std::vector<std::pair<std::string, double>> getStatistics() const;

    /**
     * Reset parser state
     */
    void reset();

    /**
     * Configure custom delimiters
     * @param delimiters List of custom boundary strings
     */
    void setCustomDelimiters(const std::vector<std::string>& delimiters);

    /**
     * Configure statement boundaries
     * @param boundaries List of statement boundary strings
     */
    void setStatementBoundaries(const std::vector<std::string>& boundaries);
};
