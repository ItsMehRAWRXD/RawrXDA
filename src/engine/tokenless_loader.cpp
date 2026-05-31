#include "tokenless_loader.hpp"

#include "streaming_gguf_loader.h"

namespace engine
{

bool TokenlessLoader::load_weights_from_gguf(const std::string& path, CPUEngine* engine)
{
    if (!engine)
    {
        m_last_error = "Null engine pointer";
        return false;
    }

    if (!engine->load_model(path))
    {
        m_last_error = engine->last_error();
        if (m_last_error.empty())
            m_last_error = "Failed to load GGUF weights";
        return false;
    }

    TokenizerConfig detected;
    if (detect_tokenizer_from_gguf_(path, detected))
        m_cfg = detected;

    m_last_gguf_path = path;
    return true;
}

bool TokenlessLoader::detect_tokenizer_from_gguf_(const std::string& path, TokenizerConfig& config)
{
    RawrXD::StreamingGGUFLoader loader;
    if (!loader.Open(path))
        return false;
    if (!loader.ParseHeader())
        return false;
    if (!loader.ParseMetadata())
        return false;

    const auto meta = loader.GetMetadata();
    const auto& kv = meta.kv_pairs;

    auto it = kv.find("tokenizer.ggml.model");
    if (it != kv.end())
    {
        const std::string& model = it->second;
        if (model == "gpt2" || model == "bpe")
            config.type = TokenizerType::BPE;
        else if (model == "sentencepiece" || model == "llama")
            config.type = TokenizerType::SentencePiece;
        else if (model == "hf" || model == "huggingface" || model == "tokenizer.json")
            config.type = TokenizerType::HuggingFace;
    }

    const auto get_i32 = [&](const char* key, int32_t def) -> int32_t
    {
        auto it2 = kv.find(key);
        if (it2 == kv.end())
            return def;
        try
        {
            return (int32_t)std::stoi(it2->second);
        }
        catch (...)
        {
            return def;
        }
    };

    const auto get_bool = [&](const char* key, bool def) -> bool
    {
        auto it2 = kv.find(key);
        if (it2 == kv.end())
            return def;
        const std::string& v = it2->second;
        return (v == "true" || v == "1");
    };

    config.bos_token_id = get_i32("tokenizer.ggml.bos_token_id", 1);
    config.eos_token_id = get_i32("tokenizer.ggml.eos_token_id", 2);
    config.unk_token_id = get_i32("tokenizer.ggml.unknown_token_id", 0);
    config.pad_token_id = get_i32("tokenizer.ggml.padding_token_id", -1);

    config.add_bos = get_bool("tokenizer.ggml.add_bos_token", true);
    config.add_eos = get_bool("tokenizer.ggml.add_eos_token", false);

    // If GGUF already carries the decoded token list, we can infer SentencePiece-style tokenization.
    if (config.type == TokenizerType::Unknown && !meta.tokens.empty())
        config.type = TokenizerType::SentencePiece;

    return config.type != TokenizerType::Unknown;
}

bool TokenlessLoader::load_bpe_tokenizer_(const TokenizerConfig& config)
{
    if (config.vocab_path.empty() || config.merges_path.empty())
    {
        m_last_error = "BPE tokenizer requires vocab_path and merges_path";
        return false;
    }

    auto bpe = std::make_shared<::tokenizer::BPETokenizer>();
    if (!bpe->load_from_file(config.vocab_path, config.merges_path))
    {
        m_last_error = "Failed to load BPE tokenizer from files";
        return false;
    }

    bpe->set_special_tokens(config.pad_token_id, config.bos_token_id, config.eos_token_id, config.unk_token_id);
    bpe->set_add_bos_token(config.add_bos);
    bpe->set_add_eos_token(config.add_eos);
    bpe->set_add_prefix_space(config.add_prefix_space);

    m_tokenizer = bpe;
    return true;
}

bool TokenlessLoader::load_sentencepiece_tokenizer_(const TokenizerConfig& config)
{
    auto sp = std::make_shared<::tokenizer::SentencePieceTokenizer>();

    // Prefer explicit .model when given.
    if (!config.model_path.empty())
    {
        if (!sp->load_from_file(config.model_path))
        {
            m_last_error = "Failed to load SentencePiece tokenizer from model file";
            return false;
        }
    }

    // If no model path, caller can still use `load_from_gguf` directly via config detection + path.
    // TokenlessLoader itself doesn't keep the GGUF path, so require model_path unless it is already
    // loaded elsewhere.
    if (config.model_path.empty())
    {
        if (m_last_gguf_path.empty() || !sp->load_from_gguf(m_last_gguf_path))
        {
            m_last_error = "SentencePiece tokenizer requires model_path (or a GGUF with metadata.tokens)";
            return false;
        }
    }

    sp->set_special_tokens(config.pad_token_id, config.bos_token_id, config.eos_token_id, config.unk_token_id);
    sp->set_add_bos_token(config.add_bos);
    sp->set_add_eos_token(config.add_eos);

    m_tokenizer = sp;
    return true;
}

bool TokenlessLoader::load_huggingface_tokenizer_(const TokenizerConfig& config)
{
    if (config.tokenizer_json.empty())
    {
        m_last_error = "HuggingFace tokenizer requires tokenizer_json path";
        return false;
    }

    auto hf = std::make_shared<::tokenizer::HuggingFaceTokenizer>();
    if (!hf->load_from_file(config.tokenizer_json))
    {
        m_last_error = "Failed to load HuggingFace tokenizer from tokenizer.json";
        return false;
    }

    // Reuse common flags where applicable (HF BPE generally wants prefix space).
    // Specials are embedded in vocab/added_tokens; config overrides are applied if provided.
    // We only override if ids are non-negative.
    const auto inRange = [&](int32_t id) -> bool { return id >= 0 && (size_t)id < hf->vocab_size(); };

    int32_t pad = hf->pad_token_id();
    int32_t bos = hf->bos_token_id();
    int32_t eos = hf->eos_token_id();
    int32_t unk = hf->unk_token_id();

    if (inRange(config.pad_token_id))
        pad = config.pad_token_id;
    if (inRange(config.bos_token_id))
        bos = config.bos_token_id;
    if (inRange(config.eos_token_id))
        eos = config.eos_token_id;
    if (inRange(config.unk_token_id))
        unk = config.unk_token_id;

    hf->set_special_tokens(pad, bos, eos, unk);
    hf->set_add_bos_token(config.add_bos);
    hf->set_add_eos_token(config.add_eos);
    hf->set_add_prefix_space(config.add_prefix_space);

    m_tokenizer = hf;
    return true;
}

bool TokenlessLoader::load_tokenizer(const TokenizerConfig& config, CPUEngine* engine)
{
    (void)engine;
    m_cfg = config;

    switch (config.type)
    {
        case TokenizerType::BPE:
            return load_bpe_tokenizer_(config);
        case TokenizerType::SentencePiece:
            return load_sentencepiece_tokenizer_(config);
        case TokenizerType::HuggingFace:
            return load_huggingface_tokenizer_(config);
        case TokenizerType::Tiktoken:
            m_last_error = "Tiktoken tokenizer not implemented";
            return false;
        case TokenizerType::Custom:
            if (!m_tokenizer)
            {
                m_last_error = "Custom tokenizer not set";
                return false;
            }
            return true;
        default:
            m_last_error = "Unknown tokenizer type";
            return false;
    }
}

bool TokenlessLoader::load_from_files(const std::string& weights_path, const TokenizerConfig& tokenizer_config,
                                      CPUEngine* engine)
{
    if (!load_weights_from_gguf(weights_path, engine))
        return false;
    if (!load_tokenizer(tokenizer_config, engine))
        return false;

    // Hand tokenizer ownership into engine for text streaming.
    if (engine && m_tokenizer)
        engine->set_tokenizer(m_tokenizer);
    return true;
}

std::vector<int32_t> TokenlessLoader::encode(const std::string& text, bool add_special) const
{
    if (!m_tokenizer)
        return {};
    auto enc = m_tokenizer->encode(text, add_special);
    return enc.token_ids;
}

std::string TokenlessLoader::decode(const std::vector<int32_t>& tokens, bool skip_special) const
{
    if (!m_tokenizer)
        return "";
    ::tokenizer::DecodeOptions opt;
    opt.skip_special_tokens = skip_special;
    return m_tokenizer->decode(tokens, opt);
}

int32_t TokenlessLoader::bos_token_id() const
{
    if (!m_tokenizer)
        return m_cfg.bos_token_id;
    return m_tokenizer->bos_token_id();
}

int32_t TokenlessLoader::eos_token_id() const
{
    if (!m_tokenizer)
        return m_cfg.eos_token_id;
    return m_tokenizer->eos_token_id();
}

int32_t TokenlessLoader::unk_token_id() const
{
    if (!m_tokenizer)
        return m_cfg.unk_token_id;
    return m_tokenizer->unk_token_id();
}

}  // namespace engine
