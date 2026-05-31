#include "gguf_writer_minimal.hpp"

#include <cstring>

namespace RawrXD::GGUFFactory
{

static constexpr uint32_t GGUF_MAGIC = 0x46554747u;  // "GGUF" LE

void GGUFWriterMinimal::addString(const std::string& key, const std::string& value)
{
    m_strings[key] = value;
}

void GGUFWriterMinimal::addInt32(const std::string& key, int32_t value)
{
    m_ints[key] = value;
}

void GGUFWriterMinimal::addUInt32(const std::string& key, uint32_t value)
{
    m_uints[key] = value;
}

void GGUFWriterMinimal::addFloat32(const std::string& key, float value)
{
    m_floats[key] = value;
}

void GGUFWriterMinimal::addBool(const std::string& key, bool value)
{
    m_bools[key] = value;
}

void GGUFWriterMinimal::addTensorMeta(const std::string& name, const std::vector<int64_t>& shape, uint32_t type)
{
    GGUFTensorInfoMinimal t;
    t.name = name;
    t.shape = shape;
    t.type = type;
    t.offset = 0;
    m_tensors.push_back(std::move(t));
}

bool GGUFWriterMinimal::writeMetadataOnly(const std::string& path)
{
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        m_lastError = "Failed to open output file";
        return false;
    }

    if (!writeHeader_(file))
        return false;
    if (!writeKvPairs_(file))
        return false;
    if (!writeTensorInfo_(file))
        return false;

    return file.good();
}

bool GGUFWriterMinimal::writeComplete(const std::string& path,
                                      const std::vector<std::pair<std::string, std::vector<char>>>& tensorData)
{
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        m_lastError = "Failed to open output file";
        return false;
    }

    if (!writeHeader_(file))
        return false;
    if (!writeKvPairs_(file))
        return false;
    if (!writeTensorInfo_(file))
        return false;

    padToAlignment_(file, 32);
    if (!writeTensorData_(file, tensorData))
        return false;

    return file.good();
}

bool GGUFWriterMinimal::writeHeader_(std::ofstream& file)
{
    const uint32_t magic = GGUF_MAGIC;
    file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    file.write(reinterpret_cast<const char*>(&m_version), sizeof(m_version));

    const uint64_t tensorCount = static_cast<uint64_t>(m_tensors.size());
    file.write(reinterpret_cast<const char*>(&tensorCount), sizeof(tensorCount));

    const uint64_t kvCount =
        static_cast<uint64_t>(m_strings.size() + m_ints.size() + m_uints.size() + m_floats.size() + m_bools.size());
    file.write(reinterpret_cast<const char*>(&kvCount), sizeof(kvCount));

    if (!file.good())
    {
        m_lastError = "Failed to write GGUF header";
        return false;
    }
    return true;
}

bool GGUFWriterMinimal::writeKvPairs_(std::ofstream& file)
{
    for (const auto& [k, v] : m_strings)
    {
        if (!writeKvPair_(file, k, GGUFValueType::STRING, &v))
            return false;
    }
    for (const auto& [k, v] : m_ints)
    {
        if (!writeKvPair_(file, k, GGUFValueType::INT32, &v))
            return false;
    }
    for (const auto& [k, v] : m_uints)
    {
        if (!writeKvPair_(file, k, GGUFValueType::UINT32, &v))
            return false;
    }
    for (const auto& [k, v] : m_floats)
    {
        if (!writeKvPair_(file, k, GGUFValueType::FLOAT32, &v))
            return false;
    }
    for (const auto& [k, v] : m_bools)
    {
        if (!writeKvPair_(file, k, GGUFValueType::BOOL, &v))
            return false;
    }
    return true;
}

bool GGUFWriterMinimal::writeTensorInfo_(std::ofstream& file)
{
    for (const auto& t : m_tensors)
    {
        const uint64_t nameLen = static_cast<uint64_t>(t.name.size());
        file.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
        file.write(t.name.data(), (std::streamsize)nameLen);

        const uint32_t nDims = static_cast<uint32_t>(t.shape.size());
        file.write(reinterpret_cast<const char*>(&nDims), sizeof(nDims));
        for (int64_t d : t.shape)
        {
            file.write(reinterpret_cast<const char*>(&d), sizeof(d));
        }

        file.write(reinterpret_cast<const char*>(&t.type), sizeof(t.type));

        // offset placeholder (GGUF stores absolute file offsets in many implementations;
        // our reader uses the value but tolerates 0 for metadata-only overlays).
        const uint64_t off = t.offset;
        file.write(reinterpret_cast<const char*>(&off), sizeof(off));
    }

    if (!file.good())
    {
        m_lastError = "Failed to write GGUF tensor metadata";
        return false;
    }
    return true;
}

bool GGUFWriterMinimal::writeTensorData_(std::ofstream& file,
                                         const std::vector<std::pair<std::string, std::vector<char>>>& tensorData)
{
    // This minimal writer assumes tensorData order matches addTensorMeta order.
    // Caller can pass an empty list for metadata-only.
    for (size_t i = 0; i < tensorData.size(); ++i)
    {
        const auto& blob = tensorData[i].second;
        if (!blob.empty())
            file.write(blob.data(), (std::streamsize)blob.size());
        padToAlignment_(file, 32);
    }

    if (!file.good())
    {
        m_lastError = "Failed to write GGUF tensor data";
        return false;
    }
    return true;
}

bool GGUFWriterMinimal::writeKvPair_(std::ofstream& file, const std::string& key, GGUFValueType type, const void* value)
{
    const uint64_t keyLen = static_cast<uint64_t>(key.size());
    file.write(reinterpret_cast<const char*>(&keyLen), sizeof(keyLen));
    file.write(key.data(), (std::streamsize)keyLen);

    const uint32_t typeVal = static_cast<uint32_t>(type);
    file.write(reinterpret_cast<const char*>(&typeVal), sizeof(typeVal));

    switch (type)
    {
        case GGUFValueType::STRING:
        {
            const auto* s = reinterpret_cast<const std::string*>(value);
            const uint64_t len = static_cast<uint64_t>(s->size());
            file.write(reinterpret_cast<const char*>(&len), sizeof(len));
            file.write(s->data(), (std::streamsize)len);
        }
        break;
        case GGUFValueType::INT32:
        {
            const auto* v = reinterpret_cast<const int32_t*>(value);
            file.write(reinterpret_cast<const char*>(v), sizeof(*v));
        }
        break;
        case GGUFValueType::UINT32:
        {
            const auto* v = reinterpret_cast<const uint32_t*>(value);
            file.write(reinterpret_cast<const char*>(v), sizeof(*v));
        }
        break;
        case GGUFValueType::FLOAT32:
        {
            const auto* v = reinterpret_cast<const float*>(value);
            file.write(reinterpret_cast<const char*>(v), sizeof(*v));
        }
        break;
        case GGUFValueType::BOOL:
        {
            const auto* v = reinterpret_cast<const bool*>(value);
            const uint8_t b = (*v) ? 1 : 0;
            file.write(reinterpret_cast<const char*>(&b), sizeof(b));
        }
        break;
        default:
            m_lastError = "Unsupported GGUF KV value type";
            return false;
    }

    if (!file.good())
    {
        m_lastError = "Failed to write GGUF KV pair";
        return false;
    }
    return true;
}

void GGUFWriterMinimal::padToAlignment_(std::ofstream& file, size_t alignment)
{
    const std::streamoff pos = file.tellp();
    if (pos < 0)
        return;
    const size_t p = (size_t)pos;
    const size_t aligned = (p + (alignment - 1)) & ~(alignment - 1);
    if (aligned > p)
    {
        const size_t n = aligned - p;
        std::vector<char> zeros(n, 0);
        file.write(zeros.data(), (std::streamsize)zeros.size());
    }
}

}  // namespace RawrXD::GGUFFactory
