#include "sentencepiece_protobuf.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace tokenizer::sentencepiece
{

// Protobuf wire types
static constexpr uint32_t WIRE_VARINT = 0;
static constexpr uint32_t WIRE_FIXED64 = 1;
static constexpr uint32_t WIRE_LEN = 2;
static constexpr uint32_t WIRE_FIXED32 = 5;

ProtobufReader::ProtobufReader(const uint8_t* data, size_t size) : m_data(data), m_size(size), m_pos(0) {}

uint64_t ProtobufReader::readRawVarint_()
{
    uint64_t result = 0;
    int shift = 0;
    while (m_pos < m_size && shift < 64)
    {
        const uint8_t byte = m_data[m_pos++];
        result |= (uint64_t)(byte & 0x7Fu) << shift;
        if ((byte & 0x80u) == 0)
            break;
        shift += 7;
    }
    return result;
}

bool ProtobufReader::readTag(uint32_t& fieldNumber, uint32_t& wireType)
{
    if (m_pos >= m_size)
        return false;
    const uint64_t tag = readRawVarint_();
    fieldNumber = (uint32_t)(tag >> 3);
    wireType = (uint32_t)(tag & 0x7u);
    return fieldNumber != 0;  // field #0 is invalid in protobuf
}

uint64_t ProtobufReader::readVarint()
{
    return readRawVarint_();
}

int64_t ProtobufReader::readSignedVarint()
{
    const uint64_t v = readRawVarint_();
    return (int64_t)((v >> 1) ^ (uint64_t)(-(int64_t)(v & 1u)));
}

uint32_t ProtobufReader::readFixed32()
{
    if (!canRead_(4))
    {
        m_pos = m_size;
        return 0;
    }
    uint32_t value = 0;
    std::memcpy(&value, m_data + m_pos, 4);
    m_pos += 4;
    return value;
}

uint64_t ProtobufReader::readFixed64()
{
    if (!canRead_(8))
    {
        m_pos = m_size;
        return 0;
    }
    uint64_t value = 0;
    std::memcpy(&value, m_data + m_pos, 8);
    m_pos += 8;
    return value;
}

float ProtobufReader::readFloat()
{
    const uint32_t bits = readFixed32();
    float v = 0.0f;
    std::memcpy(&v, &bits, 4);
    return v;
}

double ProtobufReader::readDouble()
{
    const uint64_t bits = readFixed64();
    double v = 0.0;
    std::memcpy(&v, &bits, 8);
    return v;
}

std::string ProtobufReader::readString()
{
    const uint64_t len64 = readRawVarint_();
    const size_t len = (len64 > m_size) ? (m_size - std::min(m_pos, m_size)) : (size_t)len64;
    if (!canRead_(len))
    {
        m_pos = m_size;
        return {};
    }
    std::string out((const char*)m_data + m_pos, (const char*)m_data + m_pos + len);
    m_pos += len;
    return out;
}

std::vector<uint8_t> ProtobufReader::readBytes()
{
    const uint64_t len64 = readRawVarint_();
    const size_t len = (len64 > m_size) ? (m_size - std::min(m_pos, m_size)) : (size_t)len64;
    if (!canRead_(len))
    {
        m_pos = m_size;
        return {};
    }
    std::vector<uint8_t> out(m_data + m_pos, m_data + m_pos + len);
    m_pos += len;
    return out;
}

void ProtobufReader::skipField(uint32_t wireType)
{
    switch (wireType)
    {
        case WIRE_VARINT:
            (void)readRawVarint_();
            break;
        case WIRE_FIXED64:
            if (canRead_(8))
                m_pos += 8;
            else
                m_pos = m_size;
            break;
        case WIRE_LEN:
        {
            const uint64_t len64 = readRawVarint_();
            const size_t len = (len64 > m_size) ? (m_size - std::min(m_pos, m_size)) : (size_t)len64;
            if (canRead_(len))
                m_pos += len;
            else
                m_pos = m_size;
            break;
        }
        case WIRE_FIXED32:
            if (canRead_(4))
                m_pos += 4;
            else
                m_pos = m_size;
            break;
        default:
            // Unknown wire type; abort parse
            m_pos = m_size;
            break;
    }
}

static void parseTrainerSpec_(TrainerSpec& out, const uint8_t* data, size_t size)
{
    ProtobufReader r(data, size);
    while (r.hasMore())
    {
        uint32_t f = 0, wt = 0;
        if (!r.readTag(f, wt))
            break;

        switch (f)
        {
            case 1:  // model_type
                out.model_type = (ModelType)(int32_t)r.readVarint();
                break;
            case 2:  // vocab_size
                out.vocab_size = (int32_t)r.readVarint();
                break;
            case 10:  // unk_id
                out.unk_id = (int32_t)r.readVarint();
                break;
            case 11:  // bos_id
                out.bos_id = (int32_t)r.readVarint();
                break;
            case 12:  // eos_id
                out.eos_id = (int32_t)r.readVarint();
                break;
            case 13:  // pad_id
                out.pad_id = (int32_t)r.readVarint();
                break;
            case 18:  // unk_piece
                out.unk_piece = r.readString();
                break;
            case 19:  // bos_piece
                out.bos_piece = r.readString();
                break;
            case 20:  // eos_piece
                out.eos_piece = r.readString();
                break;
            case 21:  // pad_piece
                out.pad_piece = r.readString();
                break;
            default:
                r.skipField(wt);
                break;
        }
    }
}

static void parseNormalizerSpec_(NormalizerSpec& out, const uint8_t* data, size_t size)
{
    ProtobufReader r(data, size);
    while (r.hasMore())
    {
        uint32_t f = 0, wt = 0;
        if (!r.readTag(f, wt))
            break;

        switch (f)
        {
            case 1:  // name
                out.name = r.readString();
                break;
            case 2:  // precompiled_charsmap
                out.precompiled_charsmap = r.readString();
                break;
            case 3:  // add_dummy_prefix
                out.add_dummy_prefix = (int32_t)r.readVarint();
                break;
            case 4:  // remove_extra_whitespaces
                out.remove_extra_whitespaces = (int32_t)r.readVarint();
                break;
            case 5:  // escape_whitespaces
                out.escape_whitespaces = (int32_t)r.readVarint();
                break;
            default:
                r.skipField(wt);
                break;
        }
    }
}

static SentencePiece parseSentencePiece_(const uint8_t* data, size_t size)
{
    SentencePiece sp;
    ProtobufReader r(data, size);
    while (r.hasMore())
    {
        uint32_t f = 0, wt = 0;
        if (!r.readTag(f, wt))
            break;

        switch (f)
        {
            case 1:  // piece
                sp.piece = r.readString();
                break;
            case 2:  // score (fixed32)
                sp.score = (wt == WIRE_FIXED32) ? r.readFloat() : (float)r.readDouble();
                break;
            case 3:  // type
                sp.type = (PieceType)(int32_t)r.readVarint();
                break;
            default:
                r.skipField(wt);
                break;
        }
    }
    return sp;
}

bool ModelProto::parse(const uint8_t* data, size_t size)
{
    pieces.clear();
    vocab_size = 0;
    trainer_spec = TrainerSpec{};
    normalizer_spec = NormalizerSpec{};

    ProtobufReader r(data, size);
    while (r.hasMore())
    {
        uint32_t f = 0, wt = 0;
        if (!r.readTag(f, wt))
            break;

        // SentencePiece's ModelProto has had minor field-number variations across forks;
        // accept common layouts (trainer_spec=1, normalizer_spec=2, pieces=3 or 4).
        if (f == 1 && wt == WIRE_LEN)
        {
            const auto bytes = r.readBytes();
            if (!bytes.empty())
                parseTrainerSpec_(trainer_spec, bytes.data(), bytes.size());
            continue;
        }
        if (f == 2 && wt == WIRE_LEN)
        {
            const auto bytes = r.readBytes();
            if (!bytes.empty())
                parseNormalizerSpec_(normalizer_spec, bytes.data(), bytes.size());
            continue;
        }
        if ((f == 3 || f == 4) && wt == WIRE_LEN)
        {
            const auto bytes = r.readBytes();
            if (!bytes.empty())
            {
                SentencePiece sp = parseSentencePiece_(bytes.data(), bytes.size());
                if (!sp.piece.empty())
                    pieces.push_back(std::move(sp));
            }
            continue;
        }
        if (f == 5 && wt == WIRE_VARINT)
        {
            // Some variants store vocab_size here.
            vocab_size = (int32_t)r.readVarint();
            continue;
        }

        r.skipField(wt);
    }

    if (vocab_size <= 0)
        vocab_size = (int32_t)pieces.size();

    return !pieces.empty();
}

bool ModelProto::parseFromFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
        return false;

    file.seekg(0, std::ios::end);
    const std::streamoff end = file.tellg();
    if (end <= 0)
        return false;
    file.seekg(0, std::ios::beg);

    const size_t size = (size_t)end;
    std::vector<uint8_t> data(size);
    file.read((char*)data.data(), (std::streamsize)size);
    if (!file.good())
        return false;

    return parse(data.data(), data.size());
}

}  // namespace tokenizer::sentencepiece
