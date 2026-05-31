#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tokenizer::sentencepiece
{

// Minimal protobuf decoder (no external dependencies).
// Supports a subset of protobuf wire types used by SentencePiece ModelProto.
class ProtobufReader
{
  public:
    ProtobufReader(const uint8_t* data, size_t size);

    bool hasMore() const { return m_pos < m_size; }
    size_t remaining() const { return (m_pos <= m_size) ? (m_size - m_pos) : 0; }

    bool readTag(uint32_t& fieldNumber, uint32_t& wireType);

    uint64_t readVarint();
    int64_t readSignedVarint();  // zigzag
    uint32_t readFixed32();
    uint64_t readFixed64();
    float readFloat();                 // fixed32
    double readDouble();               // fixed64
    std::string readString();          // length-delimited
    std::vector<uint8_t> readBytes();  // length-delimited

    void skipField(uint32_t wireType);

  private:
    uint64_t readRawVarint_();
    bool canRead_(size_t n) const { return m_pos + n <= m_size; }

  private:
    const uint8_t* m_data = nullptr;
    size_t m_size = 0;
    size_t m_pos = 0;
};

enum class ModelType : int32_t
{
    UNIGRAM = 1,
    BPE = 2,
    WORD = 3,
    CHAR = 4
};

enum class PieceType : int32_t
{
    NORMAL = 1,
    UNKNOWN = 2,
    CONTROL = 3,
    USER_DEFINED = 4,
    UNUSED = 5,
    BYTE = 6
};

struct SentencePiece
{
    std::string piece;
    float score = 0.0f;
    PieceType type = PieceType::NORMAL;
};

struct TrainerSpec
{
    ModelType model_type = ModelType::UNIGRAM;
    int32_t vocab_size = 0;
    int32_t unk_id = 0;
    int32_t bos_id = 1;
    int32_t eos_id = 2;
    int32_t pad_id = -1;
    std::string unk_piece = "<unk>";
    std::string bos_piece = "<s>";
    std::string eos_piece = "</s>";
    std::string pad_piece = "<pad>";
};

struct NormalizerSpec
{
    std::string name;
    std::string precompiled_charsmap;
    int32_t add_dummy_prefix = 1;
    int32_t remove_extra_whitespaces = 1;
    int32_t escape_whitespaces = 1;
};

struct ModelProto
{
    TrainerSpec trainer_spec;
    NormalizerSpec normalizer_spec;
    std::vector<SentencePiece> pieces;
    int32_t vocab_size = 0;

    bool parse(const uint8_t* data, size_t size);
    bool parseFromFile(const std::string& path);
};

}  // namespace tokenizer::sentencepiece
