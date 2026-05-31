#include "sentencepiece_tokenizer.hpp"

#include "sentencepiece_protobuf.hpp"
#include "streaming_gguf_loader.h"


#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>

namespace tokenizer
{
namespace
{

static inline std::string strip_cr_(std::string s)
{
    if (!s.empty() && s.back() == '\r')
        s.pop_back();
    return s;
}

}  // namespace

SentencePieceTokenizer::SentencePieceTokenizer() : m_trie(std::make_unique<TrieNode>()) {}

void SentencePieceTokenizer::set_special_tokens(TokenId pad, TokenId bos, TokenId eos, TokenId unk)
{
    m_pad = pad;
    m_bos = bos;
    m_eos = eos;
    m_unk = unk;
}

bool SentencePieceTokenizer::load_from_blob(const uint8_t* data, size_t size)
{
    if (!data || size == 0)
        return false;

    sentencepiece::ModelProto proto;
    if (!proto.parse(data, size))
        return false;

    std::vector<std::string> tokens;
    std::vector<float> scores;
    std::vector<int32_t> types;
    tokens.reserve(proto.pieces.size());
    scores.reserve(proto.pieces.size());
    types.reserve(proto.pieces.size());

    for (const auto& p : proto.pieces)
    {
        tokens.push_back(p.piece);
        scores.push_back(p.score);
        types.push_back((int32_t)p.type);
    }

    if (!load_from_token_list(tokens, scores, types))
        return false;

    const auto inRange = [&](int32_t id) -> bool { return id >= 0 && (size_t)id < m_vocab.size(); };
    TokenId pad = m_pad;
    TokenId bos = m_bos;
    TokenId eos = m_eos;
    TokenId unk = m_unk;

    if (inRange(proto.trainer_spec.pad_id))
        pad = (TokenId)proto.trainer_spec.pad_id;
    if (inRange(proto.trainer_spec.bos_id))
        bos = (TokenId)proto.trainer_spec.bos_id;
    if (inRange(proto.trainer_spec.eos_id))
        eos = (TokenId)proto.trainer_spec.eos_id;
    if (inRange(proto.trainer_spec.unk_id))
        unk = (TokenId)proto.trainer_spec.unk_id;

    set_special_tokens(pad, bos, eos, unk);
    return true;
}

bool SentencePieceTokenizer::load_from_file(const std::string& model_path)
{
    // Official SentencePiece `.model` files are protobuf-encoded ModelProto.
    // Try protobuf first; fall back to the repo's simplified format.
    {
        sentencepiece::ModelProto proto;
        if (proto.parseFromFile(model_path))
        {
            std::vector<std::string> tokens;
            std::vector<float> scores;
            std::vector<int32_t> types;
            tokens.reserve(proto.pieces.size());
            scores.reserve(proto.pieces.size());
            types.reserve(proto.pieces.size());

            for (const auto& p : proto.pieces)
            {
                tokens.push_back(p.piece);
                scores.push_back(p.score);
                types.push_back((int32_t)p.type);
            }

            if (!load_from_token_list(tokens, scores, types))
                return false;

            // Prefer explicit ids from TrainerSpec when sane.
            const auto inRange = [&](int32_t id) -> bool { return id >= 0 && (size_t)id < m_vocab.size(); };
            TokenId pad = m_pad;
            TokenId bos = m_bos;
            TokenId eos = m_eos;
            TokenId unk = m_unk;

            if (inRange(proto.trainer_spec.pad_id))
                pad = (TokenId)proto.trainer_spec.pad_id;
            if (inRange(proto.trainer_spec.bos_id))
                bos = (TokenId)proto.trainer_spec.bos_id;
            if (inRange(proto.trainer_spec.eos_id))
                eos = (TokenId)proto.trainer_spec.eos_id;
            if (inRange(proto.trainer_spec.unk_id))
                unk = (TokenId)proto.trainer_spec.unk_id;

            set_special_tokens(pad, bos, eos, unk);
            return true;
        }
    }

    std::ifstream file(model_path, std::ios::binary);
    if (!file.is_open())
        return false;

    // Matches the existing in-repo "simplified model" reader:
    // - skip 16-byte header
    // - int32 piece_count
    // - repeated: u32 len, bytes piece, float score
    file.seekg(16, std::ios::beg);
    if (!file.good())
        return false;

    int32_t numPieces = 0;
    file.read(reinterpret_cast<char*>(&numPieces), 4);
    if (!file.good() || numPieces <= 0 || numPieces > 500000)
        return false;

    std::vector<std::string> pieces;
    std::vector<float> scores;
    pieces.reserve((size_t)numPieces);
    scores.reserve((size_t)numPieces);

    for (int32_t i = 0; i < numPieces; ++i)
    {
        uint32_t pieceLen = 0;
        file.read(reinterpret_cast<char*>(&pieceLen), 4);
        if (!file.good() || pieceLen > 4096)
            return false;

        std::string pieceStr(pieceLen, '\0');
        if (pieceLen > 0)
            file.read(&pieceStr[0], pieceLen);
        if (!file.good())
            return false;

        float score = 0.0f;
        file.read(reinterpret_cast<char*>(&score), 4);
        if (!file.good())
            return false;

        pieces.push_back(std::move(pieceStr));
        scores.push_back(score);
    }

    return load_from_token_list(pieces, scores, {});
}

bool SentencePieceTokenizer::load_from_gguf(const std::string& gguf_path)
{
    RawrXD::StreamingGGUFLoader loader;
    if (!loader.Open(gguf_path))
        return false;
    if (!loader.ParseHeader())
        return false;
    if (!loader.ParseMetadata())
        return false;

    const auto meta = loader.GetMetadata();
    if (!meta.tokens.empty())
    {
        return load_from_token_list(meta.tokens, meta.token_scores, meta.token_types);
    }
    return false;
}

bool SentencePieceTokenizer::load_from_token_list(const std::vector<std::string>& tokens,
                                                  const std::vector<float>& scores, const std::vector<int32_t>& types)
{
    m_vocab.clear();
    m_token_to_id.clear();

    m_vocab.reserve(tokens.size());

    TokenId pad = INVALID_TOKEN;
    TokenId bos = INVALID_TOKEN;
    TokenId eos = INVALID_TOKEN;
    TokenId unk = INVALID_TOKEN;

    for (size_t i = 0; i < tokens.size(); ++i)
    {
        TokenInfo ti;
        ti.id = (TokenId)i;
        ti.text = tokens[i];
        ti.score = (i < scores.size()) ? scores[i] : 0.0f;
        ti.type = (i < types.size()) ? (uint32_t)types[i] : 0;

        // Special token detection (covers common GGUF variants)
        if (ti.text == "<pad>")
            pad = ti.id;
        else if (ti.text == "<s>" || ti.text == "<|begin_of_text|>")
            bos = ti.id;
        else if (ti.text == "</s>" || ti.text == "<|end_of_text|>")
            eos = ti.id;
        else if (ti.text == "<unk>")
            unk = ti.id;

        m_token_to_id[ti.text] = ti.id;
        m_vocab.push_back(std::move(ti));
    }

    // Defaults if not present.
    if (unk == INVALID_TOKEN)
        unk = 0;

    m_pad = pad;
    m_bos = bos;
    m_eos = eos;
    m_unk = unk;

    build_trie_();
    return !m_vocab.empty();
}

void SentencePieceTokenizer::build_trie_()
{
    m_trie = std::make_unique<TrieNode>();
    for (const auto& ti : m_vocab)
    {
        TrieNode* cur = m_trie.get();
        for (char c : ti.text)
        {
            auto& next = cur->next[c];
            if (!next)
                next = std::make_unique<TrieNode>();
            cur = next.get();
        }
        cur->token_id = ti.id;
    }
}

std::vector<TokenId> SentencePieceTokenizer::encode_greedy_(const std::string& text) const
{
    // Greedy longest-match over trie.
    // This is a correctness-first fallback; it matches the current repo tokenizer behavior.
    std::vector<TokenId> out;
    size_t i = 0;
    while (i < text.size())
    {
        const TrieNode* cur = m_trie.get();
        TokenId best = INVALID_TOKEN;
        size_t best_len = 0;

        size_t j = i;
        while (j < text.size())
        {
            auto it = cur->next.find(text[j]);
            if (it == cur->next.end())
                break;
            cur = it->second.get();
            ++j;
            if (cur->token_id != INVALID_TOKEN)
            {
                best = cur->token_id;
                best_len = j - i;
            }
        }

        if (best == INVALID_TOKEN || best_len == 0)
        {
            // Unknown byte — fall back to unk.
            out.push_back(m_unk != INVALID_TOKEN ? m_unk : 0);
            ++i;
            continue;
        }

        out.push_back(best);
        i += best_len;
    }
    return out;
}

Encoding SentencePieceTokenizer::encode(const std::string& text, bool add_special_tokens) const
{
    Encoding enc;
    if (m_vocab.empty())
        return enc;

    std::string t = text;

    // SentencePiece typically treats whitespace via "▁" marker, but GGUF token lists already encode that.
    // We don't synthesize ▁ here; we just tokenize against existing pieces.
    auto ids = encode_greedy_(t);

    if (add_special_tokens)
    {
        if (m_add_bos && m_bos != INVALID_TOKEN)
            enc.token_ids.push_back(m_bos);
        enc.token_ids.insert(enc.token_ids.end(), ids.begin(), ids.end());
        if (m_add_eos && m_eos != INVALID_TOKEN)
            enc.token_ids.push_back(m_eos);
    }
    else
    {
        enc.token_ids = std::move(ids);
    }

    enc.attention_mask.assign(enc.token_ids.size(), 1);
    return enc;
}

std::string SentencePieceTokenizer::decode(const std::vector<TokenId>& tokens, const DecodeOptions& options) const
{
    if (m_vocab.empty())
        return "";

    std::string out;
    out.reserve(tokens.size() * 4);

    for (TokenId id : tokens)
    {
        if (id < 0 || (size_t)id >= m_vocab.size())
            continue;
        if (options.skip_special_tokens && is_special_token(id))
            continue;
        out += m_vocab[(size_t)id].text;
        if (options.spaces_between_tokens)
            out.push_back(' ');
    }

    if (options.clean_up_tokenization && options.spaces_between_tokens && !out.empty() && out.back() == ' ')
        out.pop_back();

    return out;
}

TokenId SentencePieceTokenizer::token_to_id(const std::string& token) const
{
    auto it = m_token_to_id.find(token);
    if (it == m_token_to_id.end())
        return m_unk != INVALID_TOKEN ? m_unk : 0;
    return it->second;
}

std::string SentencePieceTokenizer::id_to_token(TokenId id) const
{
    if (id < 0 || (size_t)id >= m_vocab.size())
        return "";
    return m_vocab[(size_t)id].text;
}

const TokenInfo& SentencePieceTokenizer::get_token_info(TokenId id) const
{
    static TokenInfo kEmpty;
    if (id < 0 || (size_t)id >= m_vocab.size())
        return kEmpty;
    return m_vocab[(size_t)id];
}

bool SentencePieceTokenizer::is_special_token(TokenId id) const
{
    return (id == m_pad) || (id == m_bos) || (id == m_eos) || (id == m_unk);
}

bool SentencePieceTokenizer::is_control_token(TokenId id) const
{
    // Control tokens are model-specific; treat "special" as control here.
    return is_special_token(id);
}

}  // namespace tokenizer
