#include "rawrxd_tokenizer.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <limits>

// Safety limits to prevent unbounded resource allocation
static constexpr int MAX_VOCAB_SIZE = 1000000;  // 1M tokens max
static constexpr size_t MAX_TOKENS_PER_TEXT = 1000000;  // 1M tokens per encoding

namespace
{
size_t skipJsonWhitespace(const std::string& text, size_t pos)
{
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0)
        ++pos;
    return pos;
}

bool parseJsonStringToken(const std::string& text, size_t& pos, std::string& out)
{
    out.clear();
    pos = skipJsonWhitespace(text, pos);
    if (pos >= text.size() || text[pos] != '"')
        return false;

    ++pos;
    bool escaping = false;
    while (pos < text.size())
    {
        const char ch = text[pos++];
        if (escaping)
        {
            switch (ch)
            {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                default: out.push_back(ch); break;
            }
            escaping = false;
            continue;
        }
        if (ch == '\\')
        {
            escaping = true;
            continue;
        }
        if (ch == '"')
            return true;
        out.push_back(ch);
    }

    return false;
}
}

bool RawrXDTokenizer::parseOrderedVocabJson(const std::string& jsonText, std::vector<std::string>& outVocab)
{
    outVocab.clear();
    const std::string vocabNeedle = "\"vocab\"";
    size_t pos = jsonText.find(vocabNeedle);
    if (pos == std::string::npos)
        return false;

    pos = skipJsonWhitespace(jsonText, pos + vocabNeedle.size());
    if (pos >= jsonText.size() || jsonText[pos] != ':')
        return false;

    pos = skipJsonWhitespace(jsonText, pos + 1);
    if (pos >= jsonText.size() || jsonText[pos] != '[')
        return false;

    ++pos;
    while (pos < jsonText.size())
    {
        pos = skipJsonWhitespace(jsonText, pos);
        if (pos >= jsonText.size())
            return false;
        if (jsonText[pos] == ']')
            return !outVocab.empty();

        std::string token;
        if (!parseJsonStringToken(jsonText, pos, token))
            return false;
        outVocab.push_back(token);
        if (outVocab.size() >= MAX_VOCAB_SIZE)
            return true;

        pos = skipJsonWhitespace(jsonText, pos);
        if (pos >= jsonText.size())
            return false;
        if (jsonText[pos] == ',')
        {
            ++pos;
            continue;
        }
        if (jsonText[pos] == ']')
            return true;
        return false;
    }

    return false;
}

void RawrXDTokenizer::refreshSpecialTokenIds()
{
    BOS_ID = INVALID_TOKEN_ID;
    EOS_ID = INVALID_TOKEN_ID;
    UNK_ID = INVALID_TOKEN_ID;
    PAD_ID = INVALID_TOKEN_ID;

    const auto assignIfPresent = [&](const char* text, uint32_t& dst)
    {
        auto it = vocab.find(text);
        if (it != vocab.end())
            dst = static_cast<uint32_t>(it->second);
    };

    assignIfPresent("<s>", BOS_ID);
    assignIfPresent("<|begin_of_text|>", BOS_ID);
    assignIfPresent("</s>", EOS_ID);
    assignIfPresent("<|end_of_text|>", EOS_ID);
    assignIfPresent("<unk>", UNK_ID);
    assignIfPresent("<pad>", PAD_ID);
}

void RawrXDTokenizer::appendNormalizedPiece(const std::string& piece, std::string& out)
{
    if (piece.empty())
        return;

    if (!piece.empty() && piece.front() == '<' && piece.back() == '>')
        return;

    for (size_t i = 0; i < piece.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(piece[i]);
        if (c == 0xE2 && i + 2 < piece.size() &&
            static_cast<unsigned char>(piece[i + 1]) == 0x96 &&
            static_cast<unsigned char>(piece[i + 2]) == 0x81)
        {
            out.push_back(' ');
            i += 2;
            continue;
        }
        out.push_back(static_cast<char>(c));
    }
}

bool RawrXDTokenizer::LoadFromVocab(const std::vector<std::string>& vocabStrings) {
    // Reset state
    vocab.clear();
    reverse_vocab.clear();
    
    // Load from vocab strings
    int idx = 0;
    for (const auto& token : vocabStrings) {
        if (static_cast<size_t>(idx) >= MAX_VOCAB_SIZE) {
            std::cerr << "[Tokenizer] Vocab size limit exceeded (" << MAX_VOCAB_SIZE 
                      << "), stopping load" << std::endl;
            break;
        }
        
        vocab[token] = idx;
        reverse_vocab[idx] = token;
        idx++;
    }
    
    refreshSpecialTokenIds();

    std::cout << "[Tokenizer] Loaded " << reverse_vocab.size() << " vocabulary entries from GGUF" << std::endl;
    
    if (vocab.empty()) {
        std::cerr << "[Tokenizer] WARNING: Vocabulary is empty after load from GGUF" << std::endl;
        return false;
    }
    
    return true;
}

bool RawrXDTokenizer::Load(const std::string& vocabPath) {
    vocab.clear();
    reverse_vocab.clear();
    refreshSpecialTokenIds();

    std::ifstream f(vocabPath, std::ios::binary);
    if (!f.is_open()) {
        std::cout << "[Tokenizer] Vocab file not found, using byte encoding fallback: "
                  << vocabPath << std::endl;
        for (int i = 0; i < 256; ++i) {
            std::string s(1, static_cast<char>(i));
            vocab[s] = i;
            reverse_vocab[i] = s;
        }
        return true;
    }

    std::ostringstream buffer;
    buffer << f.rdbuf();
    const std::string content = buffer.str();

    std::vector<std::string> orderedVocab;
    if (!content.empty() && content.front() == '{' && parseOrderedVocabJson(content, orderedVocab)) {
        const bool loaded = LoadFromVocab(orderedVocab);
        if (loaded)
            std::cout << "[Tokenizer] Loaded ordered vocab from tokenizer.json: " << vocabPath << std::endl;
        return loaded;
    }

    std::string line;
    std::istringstream lines(content);
    int idx = 0;
    while (std::getline(lines, line)) {
        if (line.empty())
            continue;
        if (static_cast<size_t>(idx) >= MAX_VOCAB_SIZE)
            break;
        vocab[line] = idx;
        reverse_vocab[idx] = line;
        ++idx;
    }

    if (vocab.empty()) {
        std::cerr << "[Tokenizer] WARNING: Vocabulary is empty after load" << std::endl;
        return false;
    }

    refreshSpecialTokenIds();
    std::cout << "[Tokenizer] Loaded " << reverse_vocab.size() << " vocabulary entries" << std::endl;
    return true;
}

std::vector<uint32_t> RawrXDTokenizer::Encode(const std::string& text) {
    std::vector<uint32_t> tokens;
    
    // Reject empty text gracefully
    if (text.empty()) {
        if (BOS_ID != INVALID_TOKEN_ID)
            tokens.push_back(BOS_ID);
        return tokens;
    }
    
    // Upfront validation: reject input that will definitely exceed token limit
    // Estimate: each input byte can become 1-2 tokens (conservatively estimate 1:1)
    if (text.length() + 1 >= MAX_TOKENS_PER_TEXT) {
        std::cerr << "[Tokenizer] ERROR: Encode input too large (" << text.length() 
                  << " bytes, limit ~" << MAX_TOKENS_PER_TEXT << " tokens), rejecting" << std::endl;
        if (BOS_ID != INVALID_TOKEN_ID)
            tokens.push_back(BOS_ID);
        return tokens;
    }
    
    tokens.reserve(std::min(text.length() + 1, MAX_TOKENS_PER_TEXT));
    
    if (BOS_ID != INVALID_TOKEN_ID)
        tokens.push_back(BOS_ID);
    
    size_t pos = 0;
    size_t len = text.length();
    
    // Byte-level tokenization — greedy longest-prefix matching
    while (pos < len) {
        // Check token limit before adding (defensive; input should have been rejected)
        if (tokens.size() >= MAX_TOKENS_PER_TEXT) {
            std::cerr << "[Tokenizer] ERROR: Encode token limit reached mid-stream at pos=" << pos 
                      << " (input length=" << len << "), stopping encode" << std::endl;
            break;
        }
        
        uint8_t c = (uint8_t)text[pos];
        std::string s(1, (char)c);

        auto it = vocab.find(s);
        if (it != vocab.end())
            tokens.push_back(static_cast<uint32_t>(it->second));
        else
            tokens.push_back(static_cast<uint32_t>(c));
        pos++;
    }
    
    return tokens;
}

std::string RawrXDTokenizer::Decode(const std::vector<uint32_t>& tokens) {
    if (tokens.size() > MAX_TOKENS_PER_TEXT) {
        std::cerr << "[Tokenizer] ERROR: Decode input too large (" << tokens.size()
                  << " tokens, limit " << MAX_TOKENS_PER_TEXT << "), truncating" << std::endl;
    }
    const size_t decodeCount = std::min(tokens.size(), MAX_TOKENS_PER_TEXT);

    std::string res;
    res.reserve(decodeCount * 4); // Heuristic: ~4 bytes per token
    
    for (size_t i = 0; i < decodeCount; ++i) {
        const uint32_t t = tokens[i];
        if (t == BOS_ID || t == EOS_ID || t == PAD_ID) continue;
        
        // Validate token ID before lookup
        auto it = reverse_vocab.find(static_cast<int>(t));
        if (it != reverse_vocab.end()) {
            appendNormalizedPiece(it->second, res);
        } else if (t < 256) {
            res += static_cast<char>(t);
        } else {
            std::cerr << "[Tokenizer] WARNING: Unknown token ID in decode: " << t << std::endl;
            char marker[24] = {};
            std::snprintf(marker, sizeof(marker), "<tok:%u>", t);
            res += marker;
        }
    }
    
    return res;
}

// Decode with UTF-8 validation and corruption detection
std::string RawrXDTokenizer::DecodeSafe(const std::vector<uint32_t>& tokens)
{
    // First, perform standard decode
    std::string decoded = Decode(tokens);

    // Check for replacement characters (indicates corruption)
    if (UTF8Validator::contains_replacement_char(decoded))
    {
        std::cerr << "[Tokenizer] WARNING: Detected replacement characters (U+FFFD) in decoded output" << std::endl;
    }

    // Validate UTF-8 encoding
    if (!UTF8Validator::is_valid_utf8_string(decoded.c_str()))
    {
        std::cerr << "[Tokenizer] WARNING: Decoded output contains invalid UTF-8 sequences, sanitizing" << std::endl;
        decoded = UTF8Validator::sanitize_utf8_string(decoded.c_str());
    }

    return decoded;
}
