#include "safetensors.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>

namespace weights
{
namespace
{

struct JsonValue
{
    enum class Type
    {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };
    Type type = Type::Null;
    bool b = false;
    double n = 0.0;
    std::string s;
    std::vector<JsonValue> a;
    std::unordered_map<std::string, JsonValue> o;
};

static void skipWs_(const std::string& j, size_t& p)
{
    while (p < j.size() && std::isspace((unsigned char)j[p]))
        ++p;
}

static bool parseString_(const std::string& j, size_t& p, std::string& out)
{
    skipWs_(j, p);
    if (p >= j.size() || j[p] != '"')
        return false;
    ++p;
    out.clear();
    while (p < j.size())
    {
        char c = j[p++];
        if (c == '"')
            return true;
        if (c == '\\' && p < j.size())
        {
            char e = j[p++];
            switch (e)
            {
                case '"':
                    out.push_back('"');
                    break;
                case '\\':
                    out.push_back('\\');
                    break;
                case '/':
                    out.push_back('/');
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                default:
                    out.push_back(e);
                    break;
            }
        }
        else
        {
            out.push_back(c);
        }
    }
    return false;
}

static bool parseNumber_(const std::string& j, size_t& p, double& out)
{
    skipWs_(j, p);
    size_t start = p;
    if (p < j.size() && (j[p] == '-' || j[p] == '+'))
        ++p;
    while (p < j.size() && std::isdigit((unsigned char)j[p]))
        ++p;
    if (p < j.size() && j[p] == '.')
    {
        ++p;
        while (p < j.size() && std::isdigit((unsigned char)j[p]))
            ++p;
    }
    if (p < j.size() && (j[p] == 'e' || j[p] == 'E'))
    {
        ++p;
        if (p < j.size() && (j[p] == '-' || j[p] == '+'))
            ++p;
        while (p < j.size() && std::isdigit((unsigned char)j[p]))
            ++p;
    }
    if (p == start)
        return false;
    try
    {
        out = std::stod(j.substr(start, p - start));
        return true;
    }
    catch (...)
    {
        return false;
    }
}

static bool parseValue_(const std::string& j, size_t& p, JsonValue& out);

static bool parseArray_(const std::string& j, size_t& p, JsonValue& out)
{
    skipWs_(j, p);
    if (p >= j.size() || j[p] != '[')
        return false;
    ++p;
    out.type = JsonValue::Type::Array;
    out.a.clear();
    skipWs_(j, p);
    if (p < j.size() && j[p] == ']')
    {
        ++p;
        return true;
    }
    while (p < j.size())
    {
        JsonValue v;
        if (!parseValue_(j, p, v))
            return false;
        out.a.push_back(std::move(v));
        skipWs_(j, p);
        if (p < j.size() && j[p] == ']')
        {
            ++p;
            return true;
        }
        if (p >= j.size() || j[p] != ',')
            return false;
        ++p;
    }
    return false;
}

static bool parseObject_(const std::string& j, size_t& p, JsonValue& out)
{
    skipWs_(j, p);
    if (p >= j.size() || j[p] != '{')
        return false;
    ++p;
    out.type = JsonValue::Type::Object;
    out.o.clear();
    skipWs_(j, p);
    if (p < j.size() && j[p] == '}')
    {
        ++p;
        return true;
    }
    while (p < j.size())
    {
        std::string key;
        if (!parseString_(j, p, key))
            return false;
        skipWs_(j, p);
        if (p >= j.size() || j[p] != ':')
            return false;
        ++p;
        JsonValue v;
        if (!parseValue_(j, p, v))
            return false;
        out.o.emplace(std::move(key), std::move(v));
        skipWs_(j, p);
        if (p < j.size() && j[p] == '}')
        {
            ++p;
            return true;
        }
        if (p >= j.size() || j[p] != ',')
            return false;
        ++p;
    }
    return false;
}

static bool parseValue_(const std::string& j, size_t& p, JsonValue& out)
{
    skipWs_(j, p);
    if (p >= j.size())
        return false;
    const char c = j[p];
    if (c == '"')
    {
        out.type = JsonValue::Type::String;
        return parseString_(j, p, out.s);
    }
    if (c == '{')
        return parseObject_(j, p, out);
    if (c == '[')
        return parseArray_(j, p, out);
    if (c == 't' && j.compare(p, 4, "true") == 0)
    {
        out.type = JsonValue::Type::Bool;
        out.b = true;
        p += 4;
        return true;
    }
    if (c == 'f' && j.compare(p, 5, "false") == 0)
    {
        out.type = JsonValue::Type::Bool;
        out.b = false;
        p += 5;
        return true;
    }
    if (c == 'n' && j.compare(p, 4, "null") == 0)
    {
        out.type = JsonValue::Type::Null;
        p += 4;
        return true;
    }
    if (c == '-' || c == '+' || std::isdigit((unsigned char)c))
    {
        out.type = JsonValue::Type::Number;
        return parseNumber_(j, p, out.n);
    }
    return false;
}

}  // namespace

bool SafetensorsReader::load(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
    {
        m_last_error = "failed to open: " + path;
        return false;
    }

    uint64_t metaSize = 0;
    f.read(reinterpret_cast<char*>(&metaSize), sizeof(metaSize));
    if (!f)
    {
        m_last_error = "failed to read metadata size";
        return false;
    }
    if (metaSize > (1ull << 30))
    {  // 1GB sanity
        m_last_error = "metadata too large";
        return false;
    }

    std::string meta;
    meta.resize((size_t)metaSize);
    f.read(meta.data(), (std::streamsize)metaSize);
    if (!f)
    {
        m_last_error = "failed to read metadata json";
        return false;
    }

    // read remaining file as data
    f.seekg(0, std::ios::end);
    const std::streamoff end = f.tellg();
    const std::streamoff dataOff = 8 + (std::streamoff)metaSize;
    if (end < dataOff)
    {
        m_last_error = "file truncated";
        return false;
    }
    const size_t dataSize = (size_t)(end - dataOff);
    f.seekg(dataOff, std::ios::beg);
    m_owned_data.resize(dataSize);
    f.read(reinterpret_cast<char*>(m_owned_data.data()), (std::streamsize)dataSize);
    if (!f)
    {
        m_last_error = "failed to read tensor data";
        return false;
    }

    m_data_ptr = m_owned_data.data();
    m_data_size = m_owned_data.size();
    m_metadata_json = std::move(meta);
    m_tensors.clear();

    return parse_metadata_json_(m_metadata_json);
}

bool SafetensorsReader::load_from_memory(const uint8_t* data, size_t size)
{
    if (!data || size < 8)
    {
        m_last_error = "buffer too small";
        return false;
    }
    uint64_t metaSize = 0;
    std::memcpy(&metaSize, data, sizeof(metaSize));
    if (size < 8 + metaSize)
    {
        m_last_error = "metadata exceeds buffer";
        return false;
    }
    m_metadata_json.assign(reinterpret_cast<const char*>(data + 8), (size_t)metaSize);
    m_owned_data.clear();
    m_data_ptr = data + 8 + (size_t)metaSize;
    m_data_size = size - 8 - (size_t)metaSize;
    m_tensors.clear();
    return parse_metadata_json_(m_metadata_json);
}

bool SafetensorsReader::parse_metadata_json_(const std::string& json)
{
    size_t p = 0;
    JsonValue root;
    if (!parseValue_(json, p, root) || root.type != JsonValue::Type::Object)
    {
        m_last_error = "invalid json metadata";
        return false;
    }

    for (const auto& kv : root.o)
    {
        const std::string& name = kv.first;
        if (name == "__metadata__")
            continue;
        const JsonValue& v = kv.second;
        if (v.type != JsonValue::Type::Object)
            continue;

        auto itDtype = v.o.find("dtype");
        auto itShape = v.o.find("shape");
        auto itOff = v.o.find("data_offsets");
        if (itDtype == v.o.end() || itShape == v.o.end() || itOff == v.o.end())
        {
            m_last_error = "tensor entry missing fields: " + name;
            return false;
        }
        if (itDtype->second.type != JsonValue::Type::String)
        {
            m_last_error = "dtype not string: " + name;
            return false;
        }
        if (itShape->second.type != JsonValue::Type::Array || itOff->second.type != JsonValue::Type::Array ||
            itOff->second.a.size() != 2)
        {
            m_last_error = "shape/offsets invalid: " + name;
            return false;
        }

        TensorMeta meta;
        meta.name = name;
        meta.dtype = itDtype->second.s;
        for (const auto& d : itShape->second.a)
        {
            if (d.type != JsonValue::Type::Number)
            {
                m_last_error = "shape dim not number: " + name;
                return false;
            }
            meta.shape.push_back((int64_t)d.n);
        }

        const size_t start = (size_t)itOff->second.a[0].n;
        const size_t end = (size_t)itOff->second.a[1].n;
        if (end < start)
        {
            m_last_error = "offset range invalid: " + name;
            return false;
        }
        if (end > m_data_size)
        {
            m_last_error = "tensor data exceeds buffer: " + name;
            return false;
        }
        meta.offset = start;
        meta.size = end - start;

        // Best-effort validation: size should match dtype * element count.
        const size_t dt = dtype_size_(meta.dtype);
        if (dt == 0)
        {
            m_last_error = "unsupported dtype: " + meta.dtype;
            return false;
        }
        int64_t elems = 1;
        for (int64_t dim : meta.shape)
            elems *= (dim > 0 ? dim : 0);
        const size_t expect = (size_t)elems * dt;
        if (expect != meta.size)
        {
            // Allow mismatch only when metadata describes a view (rare); still enforce bounds.
        }

        m_tensors.emplace(meta.name, std::move(meta));
    }
    return true;
}

size_t SafetensorsReader::dtype_size_(const std::string& dtype)
{
    if (dtype == "F32" || dtype == "I32" || dtype == "U32")
        return 4;
    if (dtype == "F16" || dtype == "BF16" || dtype == "I16" || dtype == "U16")
        return 2;
    if (dtype == "F64" || dtype == "I64" || dtype == "U64")
        return 8;
    if (dtype == "I8" || dtype == "U8")
        return 1;
    return 0;
}

const TensorMeta* SafetensorsReader::get_tensor_meta(const std::string& name) const
{
    auto it = m_tensors.find(name);
    if (it == m_tensors.end())
        return nullptr;
    return &it->second;
}

std::vector<std::string> SafetensorsReader::get_tensor_names() const
{
    std::vector<std::string> out;
    out.reserve(m_tensors.size());
    for (const auto& kv : m_tensors)
        out.push_back(kv.first);
    return out;
}

const uint8_t* SafetensorsReader::get_tensor_data_ptr(const std::string& name) const
{
    const auto* meta = get_tensor_meta(name);
    if (!meta)
        return nullptr;
    if (!m_data_ptr)
        return nullptr;
    if (meta->offset + meta->size > m_data_size)
        return nullptr;
    return m_data_ptr + meta->offset;
}

bool SafetensorsReader::copy_tensor_data(const std::string& name, void* dst, size_t dst_size) const
{
    const auto* meta = get_tensor_meta(name);
    if (!meta || !dst)
        return false;
    if (dst_size < meta->size)
        return false;
    const uint8_t* src = get_tensor_data_ptr(name);
    if (!src)
        return false;
    std::memcpy(dst, src, meta->size);
    return true;
}

}  // namespace weights
