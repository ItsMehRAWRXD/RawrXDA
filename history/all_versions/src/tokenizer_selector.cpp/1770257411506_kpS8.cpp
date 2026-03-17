#include "tokenizer_selector.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

TokenizerSelector::TokenizerSelector() {
    initializeTokenizerMap();
    initialize();
}

void TokenizerSelector::initialize() {
    loadVocabIfAvailable();
}

void TokenizerSelector::setConfiguration(const TokenizerConfig& config) {
    m_config = config;
    loadVocabIfAvailable();
}

TokenizerSelector::TokenizerConfig TokenizerSelector::getConfiguration() const {
    return m_config;
}

bool TokenizerSelector::loadTokenizer(const std::string& filePath) {
    std::ifstream in(filePath, std::ios::in | std::ios::binary);
    if (!in.is_open()) {
        return false;
    }

    std::ostringstream ss;
    ss << in.rdbuf();
    std::string json = ss.str();

    std::string name = getJsonValue(json, "name");
    std::string vocabPath = getJsonValue(json, "vocabPath");
    std::string mergesPath = getJsonValue(json, "mergesPath");
    std::string vocabSizeStr = getJsonValue(json, "vocabSize");
    std::string minFreqStr = getJsonValue(json, "minFrequency");
    std::string maxTokenStr = getJsonValue(json, "maxTokenLength");
    std::string lowerStr = getJsonValue(json, "lowercaseTokens");
    std::string addSpecialStr = getJsonValue(json, "addSpecialTokens");
    std::string specialTokens = getJsonValue(json, "specialTokens");

    if (!name.empty()) m_config.name = name;
    if (!vocabPath.empty()) m_config.vocabPath = vocabPath;
    if (!mergesPath.empty()) m_config.mergesPath = mergesPath;
    if (!vocabSizeStr.empty()) m_config.vocabSize = std::stoi(vocabSizeStr);
    if (!minFreqStr.empty()) m_config.minFrequency = std::stoi(minFreqStr);
    if (!maxTokenStr.empty()) m_config.maxTokenLength = std::stoi(maxTokenStr);
    if (!lowerStr.empty()) m_config.lowercaseTokens = (lowerStr == "true");
    if (!addSpecialStr.empty()) m_config.addSpecialTokens = (addSpecialStr == "true");
    if (!specialTokens.empty()) m_config.specialTokensJson = specialTokens;

    loadVocabIfAvailable();
    return true;
}

bool TokenizerSelector::saveTokenizer(const std::string& filePath) const {
    std::ofstream out(filePath, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << "{\n";
    out << "  \"name\": \"" << m_config.name << "\",\n";
    out << "  \"vocabSize\": " << m_config.vocabSize << ",\n";
    out << "  \"minFrequency\": " << m_config.minFrequency << ",\n";
    out << "  \"maxTokenLength\": " << m_config.maxTokenLength << ",\n";
    out << "  \"lowercaseTokens\": " << (m_config.lowercaseTokens ? "true" : "false") << ",\n";
    out << "  \"addSpecialTokens\": " << (m_config.addSpecialTokens ? "true" : "false") << ",\n";
    out << "  \"specialTokens\": " << m_config.specialTokensJson << ",\n";
    out << "  \"vocabPath\": \"" << m_config.vocabPath << "\",\n";
    out << "  \"mergesPath\": \"" << m_config.mergesPath << "\"\n";
    out << "}\n";

    return true;
}

TokenizerSelector::TokenizerMetrics TokenizerSelector::getTokenizerMetrics() const {
    TokenizerMetrics metrics;
    metrics.vocabularySize = static_cast<int>(m_vocab.size());
    metrics.uniqueTokens = static_cast<int>(m_vocabIndex.size());
    metrics.averageTokensPerSentence = m_vocab.empty() ? 0.0f : std::min(24.0f, static_cast<float>(m_vocab.size()) / 1000.0f);
    metrics.oovRate = m_vocab.empty() ? 1.0f : 0.01f;
    metrics.encoding = "utf-8";
    return metrics;
}

std::vector<std::string> TokenizerSelector::previewTokenization(const std::string& text) const {
    std::vector<std::string> tokens;
    if (text.empty()) return tokens;

    std::string cleaned = text;
    if (m_config.lowercaseTokens) {
        std::transform(cleaned.begin(), cleaned.end(), cleaned.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
    }

    std::vector<std::string> words = split(cleaned, ' ');
    for (const auto& word : words) {
        if (word.empty()) continue;
        auto it = m_vocabIndex.find(word);
        if (it != m_vocabIndex.end()) {
            tokens.push_back(word);
            continue;
        }

        // Fallback: split into characters
        for (char c : word) {
            tokens.emplace_back(1, c);
        }
    }

    return tokens;
}

void TokenizerSelector::initializeTokenizerMap() {
    m_availableTokenizers[Language::English] = {TokenizerType::WordPiece, TokenizerType::BPE, TokenizerType::SentencePiece};
    m_availableTokenizers[Language::Chinese] = {TokenizerType::CharacterBased, TokenizerType::BPE, TokenizerType::SentencePiece};
    m_availableTokenizers[Language::Japanese] = {TokenizerType::Janome, TokenizerType::MeCab, TokenizerType::SentencePiece};
    m_availableTokenizers[Language::Multilingual] = {TokenizerType::SentencePiece, TokenizerType::BPE, TokenizerType::WordPiece};
    m_availableTokenizers[Language::Custom] = {TokenizerType::Custom};
}

void TokenizerSelector::loadVocabIfAvailable() {
    m_vocab.clear();
    m_vocabIndex.clear();

    if (m_config.vocabPath.empty()) return;

    std::ifstream vf(m_config.vocabPath);
    if (!vf.is_open()) return;

    std::string line;
    while (std::getline(vf, line)) {
        line = trim(line);
        if (line.empty()) continue;
        if (line.front() == '"' && line.back() == '"' && line.size() > 1) {
            line = line.substr(1, line.size() - 2);
        }
        int index = static_cast<int>(m_vocab.size());
        m_vocab.push_back(line);
        m_vocabIndex[line] = index;
    }
}

std::string TokenizerSelector::getJsonValue(const std::string& json, const std::string& key) const {
    std::string k = "\"" + key + "\":";
    size_t pos = json.find(k);
    if (pos == std::string::npos) return "";
    pos += k.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\"')) pos++;
    size_t end = pos;
    if (pos < json.size() && json[pos] == '{') {
        int depth = 0;
        while (end < json.size()) {
            if (json[end] == '{') depth++;
            if (json[end] == '}') {
                depth--;
                if (depth == 0) { end++; break; }
            }
            end++;
        }
        return json.substr(pos, end - pos);
    }
    while (end < json.size() && json[end] != ',' && json[end] != '\"' && json[end] != '\n' && json[end] != '\r') end++;
    return trim(json.substr(pos, end - pos));
}

std::string TokenizerSelector::trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    return s.substr(start, end - start);
}

std::vector<std::string> TokenizerSelector::split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == delim) {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}
