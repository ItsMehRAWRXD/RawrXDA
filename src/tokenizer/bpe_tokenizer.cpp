#include "bpe_tokenizer.hpp"

#include "streaming_gguf_loader.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace tokenizer
{
namespace
{

static inline std::vector<std::string> readLines_(const std::string& path)
{
    std::vector<std::string> lines;
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
        return lines;
    std::string line;
    while (std::getline(f, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

}  // namespace

BPETokenizer::BPETokenizer()
{
    m_byte_enc = build_byte_encoder();
    m_byte_dec = build_byte_decoder(m_byte_enc);
}

void BPETokenizer::refresh_specials_()
{
    const auto assign = [&](const char* s, TokenId& dst)
    {
        auto it = m_token_to_id.find(s);
        if (it != m_token_to_id.end())
            dst = it->second;
    };
    assign("<pad>", m_pad);
    assign("<s>", m_bos);
    assign("</s>", m_eos);
    assign("<unk>", m_unk);
}

bool BPETokenizer::load_vocab(const std::vector<std::string>& vocab_lines, const std::vector<std::string>& merges_lines)
{
    m_vocab.clear();
    m_token_to_id.clear();
    m_merges.clear();

    TokenId id = 0;
    m_vocab.reserve(vocab_lines.size());
    for (const auto& tok : vocab_lines)
    {
        TokenInfo info;
        info.id = id;
        info.text = tok;
        info.score = 0.0f;
        info.type = 0;
        m_vocab.push_back(info);
        m_token_to_id[tok] = id;
        ++id;
    }

    int rank = 0;
    for (const auto& line : merges_lines)
    {
        if (line.empty() || line[0] == '#')
            continue;
        size_t sp = line.find(' ');
        if (sp == std::string::npos)
            continue;
        std::string a = line.substr(0, sp);
        std::string b = line.substr(sp + 1);
        if (!a.empty() && !b.empty())
            m_merges[{a, b}] = rank++;
    }

    refresh_specials_();
    return !m_vocab.empty();
}

bool BPETokenizer::load_from_file(const std::string& vocab_path, const std::string& merges_path)
{
    const auto vocab_lines = readLines_(vocab_path);
    const auto merges_lines = readLines_(merges_path);
    return load_vocab(vocab_lines, merges_lines);
}

bool BPETokenizer::load_from_gguf(const std::string& gguf_path)
{
    // Preferred: tokenizer files beside GGUF (tokenizer.json/merges.txt or vocab.txt/merges.txt).
    // Fallback: build vocab directly from GGUF token list (no merges -> still functional).
    std::filesystem::path p(gguf_path);
    const auto dir = p.parent_path();

    std::string merges = (dir / "merges.txt").string();
    std::string vocabTxt = (dir / "vocab.txt").string();

    // tokenizer.json often contains an ordered vocab list; RawrXDTokenizer can read it,
    // but here we keep this BPE tokenizer self-contained with line-based vocab.
    if (std::filesystem::exists(vocabTxt) && std::filesystem::exists(merges))
    {
        return load_from_file(vocabTxt, merges);
    }

    // Fallback: use GGUF metadata tokenizer tokens.
    RawrXD::StreamingGGUFLoader loader;
    if (!loader.Open(gguf_path))
        return false;
    if (!loader.ParseHeader())
        return false;
    if (!loader.ParseMetadata())
        return false;
    const auto meta = loader.GetMetadata();
    if (meta.tokens.empty())
        return false;

    // GGUF token list already includes special tokens and byte-fallback tokens for most models.
    std::vector<std::string> emptyMerges;
    return load_vocab(meta.tokens, emptyMerges);
}

std::vector<std::string> BPETokenizer::pretokenize_(const std::string& text) const
{
    // MSVC std::regex doesn't support Unicode properties reliably.
    // Implement a simple GPT-2-ish pretokenizer:
    // - group whitespace
    // - group alnum runs
    // - group everything else as single-char tokens (punct/symbol)
    std::vector<std::string> out;
    out.reserve(text.size());

    size_t i = 0;
    while (i < text.size())
    {
        unsigned char c = (unsigned char)text[i];
        if (std::isspace(c))
        {
            size_t j = i + 1;
            while (j < text.size() && std::isspace((unsigned char)text[j]))
                ++j;
            out.push_back(text.substr(i, j - i));
            i = j;
            continue;
        }
        if (std::isalnum(c))
        {
            size_t j = i + 1;
            while (j < text.size() && std::isalnum((unsigned char)text[j]))
                ++j;
            out.push_back(text.substr(i, j - i));
            i = j;
            continue;
        }
        out.push_back(text.substr(i, 1));
        i += 1;
    }

    if (m_add_prefix_space && !out.empty() && !out[0].empty() && out[0][0] != ' ')
    {
        out[0] = " " + out[0];
    }

    // Byte-encode each token into unicode-safe form.
    std::vector<std::string> encoded;
    encoded.reserve(out.size());
    for (const auto& tok : out)
    {
        std::string e;
        for (unsigned char b : tok)
        {
            auto it = m_byte_enc.find((uint8_t)b);
            if (it != m_byte_enc.end())
                e += it->second;
        }
        encoded.push_back(std::move(e));
    }
    return encoded;
}

std::vector<std::string> BPETokenizer::bpe_(const std::string& token) const
{
    // Standard greedy BPE using merge ranks.
    std::vector<std::string> word;
    auto cps = utf8_to_codepoints(token);
    word.reserve(cps.size());
    for (uint32_t cp : cps)
    {
        word.push_back(codepoints_to_utf8({cp}));
    }
    if (word.size() <= 1 || m_merges.empty())
        return word;

    while (true)
    {
        int bestRank = INT_MAX;
        std::pair<std::string, std::string> bestPair;

        for (size_t i = 0; i + 1 < word.size(); ++i)
        {
            auto it = m_merges.find({word[i], word[i + 1]});
            if (it != m_merges.end() && it->second < bestRank)
            {
                bestRank = it->second;
                bestPair = it->first;
            }
        }

        if (bestRank == INT_MAX)
            break;

        std::vector<std::string> nw;
        nw.reserve(word.size());
        size_t i = 0;
        while (i < word.size())
        {
            if (i + 1 < word.size() && word[i] == bestPair.first && word[i + 1] == bestPair.second)
            {
                nw.push_back(word[i] + word[i + 1]);
                i += 2;
            }
            else
            {
                nw.push_back(word[i]);
                i += 1;
            }
        }
        word.swap(nw);
        if (word.size() <= 1)
            break;
    }

    return word;
}

Encoding BPETokenizer::encode(const std::string& text, bool add_special_tokens) const
{
    Encoding enc;
    if (m_vocab.empty())
        return enc;

    if (add_special_tokens && m_add_bos && m_bos != INVALID_TOKEN)
    {
        enc.token_ids.push_back(m_bos);
        enc.attention_mask.push_back(1);
        enc.offsets.push_back({0, 0});
    }

    auto pretoks = pretokenize_(text);
    uint32_t cursor = 0;
    for (const auto& t : pretoks)
    {
        const auto pieces = bpe_(t);
        for (const auto& p : pieces)
        {
            TokenId id = token_to_id(p);
            if (id == INVALID_TOKEN)
                id = (m_unk != INVALID_TOKEN) ? m_unk : 0;
            enc.token_ids.push_back(id);
            enc.attention_mask.push_back(1);
            // Offsets are approximate under byte-encoding; keep monotonic cursor.
            const uint32_t start = cursor;
            const uint32_t end = cursor + (uint32_t)p.size();
            enc.offsets.push_back({start, end});
            cursor = end;
        }
    }

    if (add_special_tokens && m_add_eos && m_eos != INVALID_TOKEN)
    {
        enc.token_ids.push_back(m_eos);
        enc.attention_mask.push_back(1);
        enc.offsets.push_back({(uint32_t)text.size(), (uint32_t)text.size()});
    }

    return enc;
}

std::string BPETokenizer::decode(const std::vector<TokenId>& tokens, const DecodeOptions& options) const
{
    std::string out;
    for (TokenId id : tokens)
    {
        if (id < 0 || (size_t)id >= m_vocab.size())
            continue;
        if (options.skip_special_tokens && is_special_token(id))
            continue;
        const std::string& tok = m_vocab[(size_t)id].text;
        out += tok;
        if (options.spaces_between_tokens)
            out.push_back(' ');
    }

    // Byte decode: interpret GPT-2 unicode bytes back to bytes.
    std::string bytes;
    // This is a best-effort decoder: scan the output as UTF-8 codepoints and map each codepoint string.
    auto cps = utf8_to_codepoints(out);
    for (uint32_t cp : cps)
    {
        std::string s = codepoints_to_utf8({cp});
        auto it = m_byte_dec.find(s);
        if (it != m_byte_dec.end())
            bytes.push_back((char)it->second);
    }
    return bytes.empty() ? out : bytes;
}

TokenId BPETokenizer::token_to_id(const std::string& token) const
{
    auto it = m_token_to_id.find(token);
    if (it != m_token_to_id.end())
        return it->second;
    return (m_unk != INVALID_TOKEN) ? m_unk : INVALID_TOKEN;
}

std::string BPETokenizer::id_to_token(TokenId id) const
{
    if (id >= 0 && (size_t)id < m_vocab.size())
        return m_vocab[(size_t)id].text;
    return "";
}

const TokenInfo& BPETokenizer::get_token_info(TokenId id) const
{
    static TokenInfo invalid{};
    if (id >= 0 && (size_t)id < m_vocab.size())
        return m_vocab[(size_t)id];
    return invalid;
}

bool BPETokenizer::is_special_token(TokenId id) const
{
    return id == m_pad || id == m_bos || id == m_eos || id == m_unk || id == m_sep || id == m_cls || id == m_mask;
}

bool BPETokenizer::is_control_token(TokenId id) const
{
    if (id < 0 || (size_t)id >= m_vocab.size())
        return false;
    return m_vocab[(size_t)id].type == 2;
}

bool BPETokenizer::is_byte_token(TokenId id) const
{
    if (id < 0 || (size_t)id >= m_vocab.size())
        return false;
    return m_vocab[(size_t)id].type == 5;
}

void BPETokenizer::set_special_tokens(TokenId pad, TokenId bos, TokenId eos, TokenId unk, TokenId sep, TokenId cls,
                                      TokenId mask)
{
    m_pad = pad;
    m_bos = bos;
    m_eos = eos;
    m_unk = unk;
    m_sep = sep;
    m_cls = cls;
    m_mask = mask;
}

}  // namespace tokenizer
