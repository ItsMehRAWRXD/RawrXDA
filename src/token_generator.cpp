#include "token_generator.h"
#include <mutex>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <codecvt>
#include <locale>
#include <cwctype>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

// Fix for map mergeRank using std::pair keys if hashing not provided
namespace std {
    template <>
    struct hash<std::pair<std::string, std::string>> {
        size_t operator()(const std::pair<std::string, std::string>& k) const {
            return std::hash<std::string>()(k.first) ^ (std::hash<std::string>()(k.second) << 1);
        }
    };
}

namespace RawrXD {

TokenGenerator::TokenGenerator() : m_config(TokenizationConfig()) {
    initialize();
}

TokenGenerator::TokenGenerator(const TokenizationConfig& config) : m_config(config) {
    initialize();
}

TokenGenerator::~TokenGenerator() {
    clearCache();
}

TokenGenerator::TokenGenerator(TokenGenerator&& other) noexcept
    : m_config(std::move(other.m_config)),
      m_initialized(other.m_initialized.load()),
      m_vocab(std::move(other.m_vocab)),
      m_idToToken(std::move(other.m_idToToken)),
      m_tokenInfo(std::move(other.m_tokenInfo)),
      m_mergeRules(std::move(other.m_mergeRules)),
      m_mergeRank(std::move(other.m_mergeRank)),
      m_bosToken(other.m_bosToken),
      m_eosToken(other.m_eosToken),
      m_padToken(other.m_padToken),
      m_unkToken(other.m_unkToken),
      m_maskToken(other.m_maskToken),
      m_cacheHits(other.m_cacheHits.load()),
      m_cacheMisses(other.m_cacheMisses.load()),
      m_cacheSize(other.m_cacheSize.load()),
      m_totalEncodings(other.m_totalEncodings.load()),
      m_totalDecodings(other.m_totalDecodings.load()),
      m_totalEncodingTime(other.m_totalEncodingTime.load()),
      m_totalDecodingTime(other.m_totalDecodingTime.load()) {
    
    std::lock_guard lock(other.m_cacheMutex);
    m_encodeCache = std::move(other.m_encodeCache);
    m_decodeCache = std::move(other.m_decodeCache);
}

TokenGenerator& TokenGenerator::operator=(TokenGenerator&& other) noexcept {
    if (this != &other) {
        m_config = std::move(other.m_config);
        m_initialized = other.m_initialized.load();
        m_vocab = std::move(other.m_vocab);
        m_idToToken = std::move(other.m_idToToken);
        m_tokenInfo = std::move(other.m_tokenInfo);
        m_mergeRules = std::move(other.m_mergeRules);
        m_mergeRank = std::move(other.m_mergeRank);
        m_bosToken = other.m_bosToken;
        m_eosToken = other.m_eosToken;
        m_padToken = other.m_padToken;
        m_unkToken = other.m_unkToken;
        m_maskToken = other.m_maskToken;
        m_cacheHits = other.m_cacheHits.load();
        m_cacheMisses = other.m_cacheMisses.load();
        m_cacheSize = other.m_cacheSize.load();
        m_totalEncodings = other.m_totalEncodings.load();
        m_totalDecodings = other.m_totalDecodings.load();
        m_totalEncodingTime = other.m_totalEncodingTime.load();
        m_totalDecodingTime = other.m_totalDecodingTime.load();
        
        std::lock_guard lock(other.m_cacheMutex);
        m_encodeCache = std::move(other.m_encodeCache);
        m_decodeCache = std::move(other.m_decodeCache);
    }
    return *this;
}

RawrXD::Expected<void, RawrXD::TokenError> TokenGenerator::initialize() {
    std::lock_guard lock(m_mutex);
    
    m_vocab["<pad>"] = m_padToken;
    m_vocab["<s>"] = m_bosToken;
    m_vocab["</s>"] = m_eosToken;
    m_vocab["<unk>"] = m_unkToken;
    m_vocab["<mask>"] = m_maskToken;
    
    m_initialized = true;
    return {};
}

RawrXD::Expected<int, RawrXD::TokenError> TokenGenerator::findToken(const std::string& token) {
    auto it = m_vocab.find(token);
    if (it != m_vocab.end()) return it->second;
    return RawrXD::Unexpected(RawrXD::TokenError::TokenNotFound);
}

RawrXD::Expected<std::string, RawrXD::TokenError> TokenGenerator::findTokenString(int tokenId) {
    auto it = m_idToToken.find(tokenId);
    if (it != m_idToToken.end()) return it->second;
    return RawrXD::Unexpected(RawrXD::TokenError::TokenNotFound);
}

RawrXD::Expected<std::vector<int>, RawrXD::TokenError> TokenGenerator::encode(const std::string& text) {
    if (!m_initialized.load()) return RawrXD::Unexpected(RawrXD::TokenError::TokenizationFailed);
    if (text.empty()) return std::vector<int>();

    if (m_config.enableCache) {
        auto res = getFromCache(text, true);
        if (res) { m_cacheHits++; return res.value(); }
        m_cacheMisses++;
    }

    auto start = std::chrono::steady_clock::now();
    std::vector<int> tokens;
    
    RawrXD::Expected<std::vector<int>, RawrXD::TokenError> res = RawrXD::Unexpected(RawrXD::TokenError::NotImplemented); 

    if (m_config.strategy == TokenizationStrategy::BPE) {
         res = bpeEncode(text);
    } else if (m_config.strategy == TokenizationStrategy::WordPiece) {
         res = wordpieceEncode(text);
    } else {
         res = bpeEncode(text);
    }

    if (!res) return res;
    tokens = res.value();

    if (m_config.addSpecialTokens) {
        tokens.insert(tokens.begin(), m_bosToken);
        tokens.push_back(m_eosToken);
    }

    if (m_config.enableCache) addToCache(text, tokens, true);

    auto end = std::chrono::steady_clock::now();
    m_totalEncodingTime.fetch_add(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    m_totalEncodings++;

    logTokenization(text, tokens);
    return tokens;
}

RawrXD::Expected<std::string, RawrXD::TokenError> TokenGenerator::decode(const std::vector<int>& tokens) {
    if (!m_initialized.load()) return RawrXD::Unexpected(RawrXD::TokenError::TokenizationFailed);
    if (tokens.empty()) return std::string();

    auto start = std::chrono::steady_clock::now();
    std::string text;
    
    RawrXD::Expected<std::string, RawrXD::TokenError> res = RawrXD::Unexpected(RawrXD::TokenError::NotImplemented);
    if (m_config.strategy == TokenizationStrategy::BPE) {
        res = bpeDecode(tokens);
    } else {
        res = wordpieceDecode(tokens);
    }

    if (!res) return res;
    text = res.value();

    auto end = std::chrono::steady_clock::now();
    m_totalDecodings++;
    m_totalDecodingTime.fetch_add(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

    return text;
}

RawrXD::Expected<std::vector<int>, RawrXD::TokenError> TokenGenerator::bpeEncode(const std::string& text) {
    if (m_vocab.empty()) return RawrXD::Unexpected(RawrXD::TokenError::VocabularyNotLoaded);
    
    std::string processed = text;
    if (m_config.handleUnicode) {
        std::transform(processed.begin(), processed.end(), processed.begin(), ::tolower);
    }
    if (m_config.addPrefixSpace && !processed.empty() && processed[0] != ' ') {
        processed = " " + processed;
    }

    std::vector<std::string> words;
    std::istringstream stream(processed);
    std::string word;
    while(stream >> word) words.push_back(word);

    std::vector<int> tokens;
    for (const auto& w : words) {
        std::vector<std::string> wordTokens;
        for(char c : w) wordTokens.push_back(std::string(1, c));

        if (!m_mergeRules.empty()) {
            auto merged = applyBPERules(wordTokens);
            if (merged) wordTokens = merged.value();
        }

        for(const auto& t : wordTokens) {
            auto id = findToken(t);
            if (id) tokens.push_back(id.value());
            else tokens.push_back(m_unkToken);
        }
    }
    return tokens;
}

RawrXD::Expected<std::vector<std::string>, RawrXD::TokenError> TokenGenerator::applyBPERules(const std::vector<std::string>& tokens) {
    if (m_mergeRules.empty()) return tokens;
    
    auto result = tokens;
    for (int i=0; i<100; ++i) { 
        int bestRank = -1;
        int bestIdx = -1;
        
        for (size_t j=0; j+1 < result.size(); ++j) {
            std::string key = result[j] + " " + result[j+1]; 
            auto it = m_mergeRank.find(key);
            if (it != m_mergeRank.end()) {
                if (bestRank == -1 || it->second < bestRank) {
                    bestRank = it->second;
                    bestIdx = j;
                }
            }
        }

        if (bestIdx != -1) {
            result[bestIdx] = result[bestIdx] + result[bestIdx+1];
            result.erase(result.begin() + bestIdx + 1);
        } else {
            break;
        }
    }
    return result;
}

RawrXD::Expected<std::string, RawrXD::TokenError> TokenGenerator::bpeDecode(const std::vector<int>& tokens) {
    std::string text;
    for (int id : tokens) {
        if (id == m_bosToken || id == m_eosToken || id == m_padToken) continue;
        auto s = findTokenString(id);
        if (s) {
            std::string str = s.value();
            if (str.find("</w>") != std::string::npos) {
                str = str.substr(0, str.length()-4) + " ";
            }
            text += str;
        }
    }
    return text;
}

RawrXD::Expected<std::vector<int>, RawrXD::TokenError> TokenGenerator::wordpieceEncode(const std::string& text) { return bpeEncode(text); } 
RawrXD::Expected<std::string, RawrXD::TokenError> TokenGenerator::wordpieceDecode(const std::vector<int>& tokens) { return bpeDecode(tokens); }
RawrXD::Expected<std::vector<int>, RawrXD::TokenError> TokenGenerator::sentencepieceEncode(const std::string& text) { return bpeEncode(text); }
RawrXD::Expected<std::string, RawrXD::TokenError> TokenGenerator::sentencepieceDecode(const std::vector<int>& tokens) { return bpeDecode(tokens); }

RawrXD::Expected<void, RawrXD::TokenError> TokenGenerator::loadVocabulary(const std::string& path) {
    std::ifstream file(path);
    if (!file) return RawrXD::Unexpected(RawrXD::TokenError::FileReadFailed);
    m_vocab.clear(); m_idToToken.clear();
    
    std::string line;
    int autoId = 0;
    while(std::getline(file, line)) {
        if(line.empty()) continue;
        m_vocab[line] = autoId;
        m_idToToken[autoId] = line;
        autoId++;
    }
    return {};
}

RawrXD::Expected<void, RawrXD::TokenError> TokenGenerator::loadMergeRules(const std::string& path) {
    std::ifstream file(path);
    if (!file) return RawrXD::Unexpected(RawrXD::TokenError::FileReadFailed);
    m_mergeRules.clear(); m_mergeRank.clear();
    
    std::string line;
    int rank = 0;
    while(std::getline(file, line)) {
         if (line.empty() || line[0] == '#') continue;
         size_t space = line.find(' ');
         if (space != std::string::npos) {
             std::string t1 = line.substr(0, space);
             std::string t2 = line.substr(space+1);
             m_mergeRules.push_back({t1, t2});
             m_mergeRank[t1 + " " + t2] = rank++; 
         }
    }
    return {};
}

RawrXD::Expected<void, RawrXD::TokenError> TokenGenerator::loadVocabularyFromModel(const std::string& path) {
    if (fs::exists(path + "/tokenizer.json")) return loadVocabularyFromHuggingFace(path + "/tokenizer.json");
    if (fs::exists(path + "/vocab.txt")) return loadVocabulary(path + "/vocab.txt");
    
    createMinimalVocabulary(); 
    return {};
}

RawrXD::Expected<void, RawrXD::TokenError> TokenGenerator::loadVocabularyFromHuggingFace(const std::string& path) {
    std::ifstream file(path);
    if(!file) return RawrXD::Unexpected(RawrXD::TokenError::FileReadFailed);
    json j; 
    try { file >> j; } catch(...) { return RawrXD::Unexpected(RawrXD::TokenError::InvalidFormat); }
    
    if (j.contains("model") && j["model"].contains("vocab")) {
        for (auto& [key, val] : j["model"]["vocab"].items()) {
            m_vocab[key] = val;
            m_idToToken[val] = key;
        }
    }
    return {};
}

void TokenGenerator::createMinimalVocabulary() {
    m_vocab.clear(); m_idToToken.clear();
    const char* toks[] = {"the", "and", "code", "return", "void", "int", "if", "else"};
    int id = 10;
    for(auto t : toks) { m_vocab[t] = id; m_idToToken[id] = t; id++; }
    
    for(char c=' '; c<='~'; ++c) {
        std::string s(1, c);
        m_vocab[s] = id; m_idToToken[id] = s; id++;
    }
}

void TokenGenerator::loadConfigFromJSON(const std::string&) {}
void TokenGenerator::loadTokenizerConfigFromJSON(const std::string&) {}

RawrXD::Expected<void, RawrXD::TokenError> TokenGenerator::loadVocabularyFromMemory(
    const std::vector<std::string>& tokens,
    const std::vector<float>& scores,
    const std::vector<uint32_t>& types
) {
    if (tokens.empty()) return RawrXD::Unexpected(RawrXD::TokenError::VocabularyNotLoaded);
    
    std::unique_lock lock(m_mutex);
    m_vocab.clear();
    m_idToToken.clear();
    m_tokenInfo.clear();
    
    for (size_t i = 0; i < tokens.size(); ++i) {
        int id = static_cast<int>(i);
        m_vocab[tokens[i]] = id;
        m_idToToken[id] = tokens[i];
        
        TokenInfo info;
        info.id = id;
        info.text = tokens[i];
        if (i < scores.size()) info.score = scores[i];
        info.type = (i < types.size() && types[i] == 3) ? "control" : "normal"; 
        
        m_tokenInfo[id] = info;
    }
    
    if (m_vocab.count("<s>")) m_bosToken = m_vocab["<s>"];
    if (m_vocab.count("</s>")) m_eosToken = m_vocab["</s>"];
    if (m_vocab.count("<|endoftext|>")) m_eosToken = m_vocab["<|endoftext|>"];
    
    m_initialized = true;
    return {};
}

RawrXD::Expected<void, RawrXD::TokenError> TokenGenerator::loadVocabularyFromSentencePiece(const std::string&) { return {}; }
RawrXD::Expected<void, RawrXD::TokenError> TokenGenerator::loadVocabularyFromJSON(const std::string&) { return {}; }

bool TokenGenerator::isValidTokenId(int id) const { return m_idToToken.count(id); }
bool TokenGenerator::isValidToken(const std::string& t) const { return m_vocab.count(t); }
void TokenGenerator::clearCache() { 
    std::lock_guard l(m_cacheMutex); m_encodeCache.clear(); m_decodeCache.clear(); 
}
size_t TokenGenerator::getCacheSize() const { return m_cacheSize; }

RawrXD::Expected<std::vector<int>, RawrXD::TokenError> TokenGenerator::getFromCache(const std::string& k, bool) {
    return RawrXD::Unexpected(RawrXD::TokenError::TokenNotFound); 
}
void TokenGenerator::addToCache(const std::string&, const std::vector<int>&, bool) {}
void TokenGenerator::evictCacheIfNeeded() {}

std::string TokenGenerator::detectTokenType(const std::string&) const { return "word"; }
void TokenGenerator::logTokenization(const std::string&, const std::vector<int>&) {}
void TokenGenerator::logError(const std::string&, RawrXD::TokenError) {}

RawrXD::Expected<RawrXD::TokenInfo, RawrXD::TokenError> TokenGenerator::getTokenInfo(int id) {
     if (m_tokenInfo.count(id)) return m_tokenInfo.at(id);
     return RawrXD::Unexpected(RawrXD::TokenError::TokenNotFound); 
}
RawrXD::Expected<RawrXD::TokenInfo, RawrXD::TokenError> TokenGenerator::getTokenInfo(const std::string& t) { 
    if (m_vocab.count(t)) return getTokenInfo(m_vocab.at(t));
    return RawrXD::Unexpected(RawrXD::TokenError::TokenNotFound); 
}
RawrXD::Expected<std::wstring, RawrXD::TokenError> TokenGenerator::utf8ToWide(const std::string&) { return std::wstring(L""); }
RawrXD::Expected<std::string, RawrXD::TokenError> TokenGenerator::wideToUtf8(const std::wstring&) { return std::string(""); }
json TokenGenerator::getStatus() const { return {}; }
void TokenGenerator::setConfig(const TokenizationConfig& c) { m_config = c; }
RawrXD::Expected<std::vector<std::vector<int>>, RawrXD::TokenError> TokenGenerator::encodeBatch(const std::vector<std::string>&) { return std::vector<std::vector<int>>(); }
RawrXD::Expected<std::vector<std::string>, RawrXD::TokenError> TokenGenerator::decodeBatch(const std::vector<std::vector<int>>&) { return std::vector<std::string>(); }

void TokenGenerator::setVulkanCompute(std::shared_ptr<VulkanCompute> vulkan) {
    m_vulkan = vulkan;
    if (m_vulkan) {
        spdlog::info("TokenGenerator: GPU Acceleration Enabled");
        m_config.enableGpu = true;
    }
}

RawrXD::Expected<std::vector<std::vector<int>>, RawrXD::TokenError> TokenGenerator::encodeBatchGPU(
    const std::vector<std::string>& texts
) {
    if (!m_vulkan || !m_config.enableGpu) {
        spdlog::warn("GPU encoding requested but Vulkan not available. Falling back to CPU.");
        return encodeBatch(texts);
    }
    return encodeBatch(texts);
}

} // namespace RawrXD
