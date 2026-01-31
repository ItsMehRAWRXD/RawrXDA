#include "response_parser.h"
#include <sstream>
#include <algorithm>
#include <cctype>

ResponseParser::ResponseParser(
    std::shared_ptr<Logger> logger,
    std::shared_ptr<Metrics> metrics)
    : m_logger(logger), m_metrics(metrics) {
    if (m_logger) {

                       m_statementBoundaries.size(), m_customDelimiters.size());
    }
}

std::vector<ParsedCompletion> ResponseParser::parseResponse(const std::string& response) {
    if (m_logger) m_logger->debug("Parsing complete response ({} chars)", response.length());

    // Strategy: Try multiple parsing approaches in order
    // 1. First split by statement boundaries (most accurate for code)
    // 2. Then by line boundaries
    // 3. Finally by token boundaries as fallback

    auto completions = splitByStatementBoundaries(response);
    if (completions.empty()) {
        if (m_logger) m_logger->debug("No completions found via statement boundaries, trying line boundaries");
        completions = splitByLineBoundaries(response);
    }
    if (completions.empty()) {
        if (m_logger) m_logger->debug("No completions found via line boundaries, trying token boundaries");
        completions = splitByTokenBoundaries(response);
    }

    // If still empty, create single completion from entire response
    if (completions.empty()) {
        if (m_logger) m_logger->debug("No boundaries found, creating single completion from response");
        ParsedCompletion comp;
        comp.text = response;
        comp.tokenCount = estimateTokenCount(response);
        comp.confidence = calculateConfidence(response);
        comp.boundary = "END_OF_STREAM";
        comp.isComplete = true;
        completions.push_back(comp);
    }

    // Score each completion
    for (auto& comp : completions) {
        comp.confidence = calculateConfidence(comp.text);
    }

    m_totalCharsParsed += response.length();
    if (m_metrics) m_metrics->recordHistogram("parsed_completions_per_response", completions.size());
    if (m_logger) m_logger->info("Parsed {} completions from response ({} chars total)", 
                   completions.size(), m_totalCharsParsed);

    return completions;
}

std::vector<ParsedCompletion> ResponseParser::parseChunk(const std::string& chunk) {


    // Add to buffer
    m_buffer += chunk;

    std::vector<ParsedCompletion> result;

    // Try to extract complete statements from buffer
    size_t lastBoundary = 0;

    for (const auto& boundary : m_statementBoundaries) {
        size_t pos = 0;
        while ((pos = m_buffer.find(boundary, pos)) != std::string::npos) {
            // Extract text up to and including boundary
            std::string completionText = m_buffer.substr(lastBoundary, pos - lastBoundary + boundary.length());
            
            if (!completionText.empty()) {
                ParsedCompletion comp;
                comp.text = completionText;
                comp.tokenCount = estimateTokenCount(completionText);
                comp.confidence = calculateConfidence(completionText);
                comp.boundary = boundary;
                comp.isComplete = true;
                result.push_back(comp);

                if (m_logger) m_logger->debug("Extracted completion: {} chars, boundary: '{}'",
                               completionText.length(), boundary);
            }

            lastBoundary = pos + boundary.length();
            pos += boundary.length();
        }
    }

    // Keep unparsed data in buffer
    if (lastBoundary > 0 && lastBoundary < m_buffer.length()) {
        m_buffer = m_buffer.substr(lastBoundary);
    } else if (lastBoundary > 0) {
        m_buffer.clear();
    }

    m_totalCharsParsed += chunk.length();
    if (m_metrics) m_metrics->recordHistogram("chunk_parsed_completions", result.size());

    return result;
}

std::vector<ParsedCompletion> ResponseParser::flush() {
    if (m_logger) m_logger->debug("Flushing buffer ({} chars remaining)", m_buffer.length());

    std::vector<ParsedCompletion> result;

    if (!m_buffer.empty()) {
        ParsedCompletion comp;
        comp.text = m_buffer;
        comp.tokenCount = estimateTokenCount(m_buffer);
        comp.confidence = calculateConfidence(m_buffer);
        comp.boundary = "BUFFER_END";
        comp.isComplete = false; // Incomplete due to buffer end
        result.push_back(comp);

        if (m_logger) m_logger->info("Flushed {} chars from buffer", m_buffer.length());
        m_buffer.clear();
    }

    return result;
}

std::vector<ParsedCompletion> ResponseParser::splitByStatementBoundaries(const std::string& text) {
    if (m_logger) m_logger->debug("Splitting by statement boundaries");

    std::vector<ParsedCompletion> completions;
    size_t lastPos = 0;

    // Check for each boundary type
    for (const auto& boundary : m_statementBoundaries) {
        size_t pos = 0;
        std::vector<size_t> positions;

        // Find all occurrences of this boundary
        while ((pos = text.find(boundary, pos)) != std::string::npos) {
            positions.push_back(pos);
            pos += boundary.length();
        }

        // Create completions at each boundary
        for (size_t boundaryPos : positions) {
            if (boundaryPos >= lastPos) {
                std::string segment = text.substr(lastPos, boundaryPos - lastPos + boundary.length());
                
                if (!segment.empty() && segment.length() > 2) { // Ignore tiny fragments
                    ParsedCompletion comp;
                    comp.text = segment;
                    comp.tokenCount = estimateTokenCount(segment);
                    comp.confidence = calculateConfidence(segment);
                    comp.boundary = boundary;
                    comp.isComplete = true;
                    completions.push_back(comp);

                    if (m_logger) m_logger->debug("Statement boundary '{}': {} chars", boundary, segment.length());
                }

                lastPos = boundaryPos + boundary.length();
            }
        }
    }

    // Add remaining text if significant
    if (lastPos < text.length()) {
        std::string remaining = text.substr(lastPos);
        if (remaining.length() > 2) {
            ParsedCompletion comp;
            comp.text = remaining;
            comp.tokenCount = estimateTokenCount(remaining);
            comp.confidence = calculateConfidence(remaining);
            comp.boundary = "END_OF_TEXT";
            comp.isComplete = false;
            completions.push_back(comp);
        }
    }

    return completions;
}

std::vector<ParsedCompletion> ResponseParser::splitByLineBoundaries(const std::string& text) {
    if (m_logger) m_logger->debug("Splitting by line boundaries");

    std::vector<ParsedCompletion> completions;
    std::istringstream stream(text);
    std::string line;

    while (std::getline(stream, line)) {
        if (!line.empty()) {
            ParsedCompletion comp;
            comp.text = line + "\n";
            comp.tokenCount = estimateTokenCount(comp.text);
            comp.confidence = calculateConfidence(comp.text);
            comp.boundary = "NEWLINE";
            comp.isComplete = true;
            completions.push_back(comp);
        }
    }

    return completions;
}

std::vector<ParsedCompletion> ResponseParser::splitByTokenBoundaries(const std::string& text) {
    if (m_logger) m_logger->debug("Splitting by token boundaries");

    std::vector<ParsedCompletion> completions;
    std::istringstream stream(text);
    std::string token;

    while (stream >> token) {
        ParsedCompletion comp;
        comp.text = token + " ";
        comp.tokenCount = 1;
        comp.confidence = calculateConfidence(comp.text);
        comp.boundary = "WHITESPACE";
        comp.isComplete = false;
        completions.push_back(comp);
    }

    return completions;
}

std::pair<BoundaryType, std::string> ResponseParser::detectBoundary(const std::string& text) {
    // Check for custom delimiters first (highest priority)
    for (const auto& delimiter : m_customDelimiters) {
        if (text.find(delimiter) != std::string::npos) {
            return {BoundaryType::CUSTOM, delimiter};
        }
    }

    // Check for statement boundaries
    for (const auto& boundary : m_statementBoundaries) {
        if (text.find(boundary) != std::string::npos) {
            return {BoundaryType::STATEMENT, boundary};
        }
    }

    // Check for line boundaries
    if (text.find('\n') != std::string::npos) {
        return {BoundaryType::LINE, "\n"};
    }

    // Check for token boundaries (whitespace)
    if (std::any_of(text.begin(), text.end(), [](char c) { return std::isspace(c); })) {
        return {BoundaryType::TOKEN, " "};
    }

    // No boundary found
    return {BoundaryType::END_OF_STREAM, ""};
}

double ResponseParser::calculateConfidence(const std::string& completion) {
    if (completion.empty()) return 0.0;

    double confidence = 0.5; // Base confidence

    // Bonus for statement boundaries (complete code is more likely correct)
    for (const auto& boundary : m_statementBoundaries) {
        if (completion.find(boundary) != std::string::npos) {
            confidence += 0.15;
            break;
        }
    }

    // Bonus for reasonable length (too short = snippet, too long = rambling)
    size_t len = completion.length();
    if (len >= 10 && len <= 200) {
        confidence += 0.10;
    }

    // Penalty for unmatched brackets
    int openBrackets = std::count(completion.begin(), completion.end(), '{') +
                       std::count(completion.begin(), completion.end(), '[') +
                       std::count(completion.begin(), completion.end(), '(');
    int closeBrackets = std::count(completion.begin(), completion.end(), '}') +
                        std::count(completion.begin(), completion.end(), ']') +
                        std::count(completion.begin(), completion.end(), ')');
    
    if (openBrackets == closeBrackets && openBrackets > 0) {
        confidence += 0.15;
    } else if (openBrackets != closeBrackets) {
        confidence -= 0.10;
    }

    // Cap confidence at 1.0
    return std::min(confidence, 1.0);
}

int ResponseParser::estimateTokenCount(const std::string& text) {
    // Rough estimation: 1 token ≈ 4 characters or 1 word
    // More accurate: count whitespace + punctuation

    int tokens = 0;
    bool inWord = false;

    for (char c : text) {
        if (std::isspace(c)) {
            if (inWord) {
                tokens++;
                inWord = false;
            }
        } else if (std::ispunct(c)) {
            if (inWord) {
                tokens++;
                inWord = false;
            }
            tokens++; // Punctuation is a token
        } else {
            inWord = true;
        }
    }

    if (inWord) {
        tokens++;
    }

    // If very few tokens detected, estimate from characters
    if (tokens < 2) {
        tokens = std::max(1, static_cast<int>(text.length() / 4));
    }

    return tokens;
}

std::vector<std::pair<std::string, double>> ResponseParser::getStatistics() const {
    return {
        {"total_chars_parsed", static_cast<double>(m_totalCharsParsed)},
        {"buffer_size", static_cast<double>(m_buffer.length())}
    };
}

void ResponseParser::reset() {
    if (m_logger) m_logger->debug("Resetting parser state");
    m_buffer.clear();
    m_totalCharsParsed = 0;
}

void ResponseParser::setCustomDelimiters(const std::vector<std::string>& delimiters) {
    if (m_logger) m_logger->info("Setting {} custom delimiters", delimiters.size());
    m_customDelimiters = delimiters;
}

void ResponseParser::setStatementBoundaries(const std::vector<std::string>& boundaries) {
    if (m_logger) m_logger->info("Setting {} statement boundaries", boundaries.size());
    m_statementBoundaries = boundaries;
}
