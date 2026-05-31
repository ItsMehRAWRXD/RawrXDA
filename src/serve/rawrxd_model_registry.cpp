// ============================================================================
// rawrxd_model_registry.cpp — GGUF model scanner implementation
// ============================================================================
#include "rawrxd_model_registry.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <functional>

namespace RawrXD
{
namespace Serve
{

static std::string joinPath(const std::string& dir, const char* fileName)
{
    if (dir.empty())
        return fileName ? std::string(fileName) : std::string();

    const char tail = dir.back();
    if (tail == '\\' || tail == '/')
        return dir + (fileName ? fileName : "");

    return dir + "\\" + (fileName ? fileName : "");
}

// GGUF magic: "GGUF" in little-endian
static constexpr uint32_t GGUF_MAGIC = 0x46554747u;  // 'G','G','U','F'

// ============================================================================
// ModelRegistry
// ============================================================================

ModelRegistry::ModelRegistry() = default;
ModelRegistry::~ModelRegistry() = default;

void ModelRegistry::addSearchPath(const std::string& dir)
{
    std::lock_guard<std::mutex> lk(m_mu);
    m_searchPaths.push_back(dir);
}

size_t ModelRegistry::scan()
{
    std::lock_guard<std::mutex> lk(m_mu);
    m_models.clear();

    for (auto& dir : m_searchPaths)
        scanDirectory(dir);

    // Sort alphabetically by name
    std::sort(m_models.begin(), m_models.end(),
              [](const ModelEntry& a, const ModelEntry& b) { return a.name < b.name; });
    return m_models.size();
}

const ModelEntry* ModelRegistry::find(const std::string& nameOrPath) const
{
    std::lock_guard<std::mutex> lk(m_mu);

    // Exact match on path
    for (auto& m : m_models)
        if (m.path == nameOrPath)
            return &m;

    // Case-insensitive name prefix match
    std::string lower;
    lower.resize(nameOrPath.size());
    std::transform(nameOrPath.begin(), nameOrPath.end(), lower.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });

    for (auto& m : m_models)
    {
        std::string mn;
        mn.resize(m.name.size());
        std::transform(m.name.begin(), m.name.end(), mn.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        if (mn == lower || mn.find(lower) == 0)
            return &m;
    }
    return nullptr;
}

bool ModelRegistry::remove(const std::string& nameOrPath)
{
    std::lock_guard<std::mutex> lk(m_mu);
    for (auto it = m_models.begin(); it != m_models.end(); ++it)
    {
        if (it->path == nameOrPath || it->name == nameOrPath)
        {
            if (!DeleteFileA(it->path.c_str()))
                return false;
            m_models.erase(it);
            return true;
        }
    }
    return false;
}

std::string ModelRegistry::defaultModelDir()
{
    char buf[MAX_PATH];
    if (GetEnvironmentVariableA("USERPROFILE", buf, MAX_PATH) == 0)
        return "C:\\.rawrxd\\models";
    std::string dir = std::string(buf) + "\\.rawrxd\\models";
    CreateDirectoryA((std::string(buf) + "\\.rawrxd").c_str(), nullptr);
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir;
}

// ============================================================================
// Internal
// ============================================================================

void ModelRegistry::scanDirectory(const std::string& dir)
{
    WIN32_FIND_DATAA fd;
    std::string pattern = joinPath(dir, "*.gguf");
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;

    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        std::string fullPath = joinPath(dir, fd.cFileName);
        ModelEntry entry;
        if (probeGGUFHeader(fullPath, entry))
        {
            entry.path = fullPath;
            if (entry.name.empty())
                entry.name = inferName(fd.cFileName);

            // File size from find data
            ULARGE_INTEGER sz;
            sz.HighPart = fd.nFileSizeHigh;
            sz.LowPart = fd.nFileSizeLow;
            entry.fileSizeBytes = sz.QuadPart;

            // Modified time as Unix epoch (seconds)
            ULARGE_INTEGER ft;
            ft.HighPart = fd.ftLastWriteTime.dwHighDateTime;
            ft.LowPart = fd.ftLastWriteTime.dwLowDateTime;
            // FILETIME epoch = 1601-01-01; Unix epoch = 1970-01-01
            // Diff = 116444736000000000 in 100-ns intervals
            entry.modifiedAt = (ft.QuadPart - 116444736000000000ULL) / 10000000ULL;

            m_models.push_back(std::move(entry));
        }
    } while (FindNextFileA(h, &fd));

    FindClose(h);
}

bool ModelRegistry::probeGGUFHeader(const std::string& path, ModelEntry& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return false;

    // Read magic + version
    uint32_t magic = 0, version = 0;
    f.read(reinterpret_cast<char*>(&magic), 4);
    f.read(reinterpret_cast<char*>(&version), 4);
    if (magic != GGUF_MAGIC || version < 2 || version > 3)
        return false;

    // Read tensor_count + metadata_kv_count (uint64 in GGUFv3, uint32 in v2)
    uint64_t tensorCount = 0, kvCount = 0;
    if (version == 3)
    {
        f.read(reinterpret_cast<char*>(&tensorCount), 8);
        f.read(reinterpret_cast<char*>(&kvCount), 8);
    }
    else
    {
        uint32_t tc32 = 0, kv32 = 0;
        f.read(reinterpret_cast<char*>(&tc32), 4);
        f.read(reinterpret_cast<char*>(&kv32), 4);
        tensorCount = tc32;
        kvCount = kv32;
    }

    // Sanity bounds
    if (kvCount > 100000 || tensorCount > 100000)
        return false;

    // Read KV pairs to extract arch, context_length, vocab_size
    // GGUF KV format: key_len(u64/u32) + key_str + value_type(u32) + value

    auto readLen = [&](uint64_t& len) -> bool
    {
        if (version == 3)
        {
            return !!f.read(reinterpret_cast<char*>(&len), 8);
        }
        else
        {
            uint32_t l32 = 0;
            if (!f.read(reinterpret_cast<char*>(&l32), 4))
                return false;
            len = l32;
            return true;
        }
    };

    auto readStr = [&](std::string& s) -> bool
    {
        uint64_t len = 0;
        if (!readLen(len))
            return false;
        if (len > 65536)
            return false;  // sanity
        s.resize(static_cast<size_t>(len));
        return !!f.read(s.data(), static_cast<std::streamsize>(len));
    };

    // GGUF value types
    enum GGUFType : uint32_t
    {
        GGUF_TYPE_UINT8 = 0,
        GGUF_TYPE_INT8 = 1,
        GGUF_TYPE_UINT16 = 2,
        GGUF_TYPE_INT16 = 3,
        GGUF_TYPE_UINT32 = 4,
        GGUF_TYPE_INT32 = 5,
        GGUF_TYPE_FLOAT32 = 6,
        GGUF_TYPE_BOOL = 7,
        GGUF_TYPE_STRING = 8,
        GGUF_TYPE_ARRAY = 9,
        GGUF_TYPE_UINT64 = 10,
        GGUF_TYPE_INT64 = 11,
        GGUF_TYPE_FLOAT64 = 12,
    };

    std::function<bool(uint32_t)> skipValue;
    skipValue = [&](uint32_t vtype) -> bool
    {
        switch (vtype)
        {
            case GGUF_TYPE_UINT8:
            case GGUF_TYPE_INT8:
            case GGUF_TYPE_BOOL:
                f.seekg(1, std::ios::cur);
                return !!f;
            case GGUF_TYPE_UINT16:
            case GGUF_TYPE_INT16:
                f.seekg(2, std::ios::cur);
                return !!f;
            case GGUF_TYPE_UINT32:
            case GGUF_TYPE_INT32:
            case GGUF_TYPE_FLOAT32:
                f.seekg(4, std::ios::cur);
                return !!f;
            case GGUF_TYPE_UINT64:
            case GGUF_TYPE_INT64:
            case GGUF_TYPE_FLOAT64:
                f.seekg(8, std::ios::cur);
                return !!f;
            case GGUF_TYPE_STRING:
            {
                std::string tmp;
                return readStr(tmp);
            }
            case GGUF_TYPE_ARRAY:
            {
                uint32_t arrType = 0;
                uint64_t arrLen = 0;
                f.read(reinterpret_cast<char*>(&arrType), 4);
                if (!readLen(arrLen))
                    return false;
                if (arrLen > 10000000)
                    return false;
                for (uint64_t i = 0; i < arrLen; i++)
                    if (!skipValue(arrType))
                        return false;
                return true;
            }
            default:
                return false;
        }
    };

    // Cap iteration to prevent DoS on malformed files
    uint64_t maxKv = (kvCount < 2000) ? kvCount : 2000;
    for (uint64_t i = 0; i < maxKv && f; i++)
    {
        std::string key;
        if (!readStr(key))
            break;

        uint32_t vtype = 0;
        if (!f.read(reinterpret_cast<char*>(&vtype), 4))
            break;

        // Extract known metadata keys
        if (key == "general.architecture" && vtype == GGUF_TYPE_STRING)
        {
            readStr(out.architecture);
        }
        else if (key == "general.name" && vtype == GGUF_TYPE_STRING)
        {
            readStr(out.name);
        }
        else if ((key.find(".context_length") != std::string::npos) && vtype == GGUF_TYPE_UINT32)
        {
            f.read(reinterpret_cast<char*>(&out.contextLength), 4);
        }
        else if ((key.find(".vocab_size") != std::string::npos) && vtype == GGUF_TYPE_UINT32)
        {
            f.read(reinterpret_cast<char*>(&out.vocabSize), 4);
        }
        else
        {
            if (!skipValue(vtype))
                break;
        }
    }

    out.quantization = inferQuant(path);
    return true;
}

std::string ModelRegistry::inferName(const std::string& filename) const
{
    // "phi3mini.gguf" -> "phi3mini"
    // "codestral-22b-Q4_K_M.gguf" -> "codestral:22b-Q4_K_M"
    std::string base = filename;
    auto dotPos = base.rfind('.');
    if (dotPos != std::string::npos)
        base = base.substr(0, dotPos);

    // Try to split on common separators to form "model:tag" style
    auto dashPos = base.find('-');
    if (dashPos != std::string::npos && dashPos < base.size() - 1)
    {
        return base.substr(0, dashPos) + ":" + base.substr(dashPos + 1);
    }

    return base;
}

std::string ModelRegistry::inferQuant(const std::string& filename) const
{
    // Common quantization tags in GGUF filenames
    static const char* quantTags[] = {
        "Q2_K",   "Q3_K_S", "Q3_K_M", "Q3_K_L", "Q4_0", "Q4_1",    "Q4_K_S", "Q4_K_M",  "Q5_0",  "Q5_1",   "Q5_K_S",
        "Q5_K_M", "Q6_K",   "Q8_0",   "F16",    "F32",  "IQ2_XXS", "IQ2_XS", "IQ3_XXS", "IQ3_S", "IQ4_NL", "IQ4_XS",
    };

    std::string upper;
    upper.resize(filename.size());
    std::transform(filename.begin(), filename.end(), upper.begin(),
                   [](unsigned char c) { return (char)std::toupper(c); });

    for (auto tag : quantTags)
    {
        if (upper.find(tag) != std::string::npos)
            return tag;
    }
    return "unknown";
}

}  // namespace Serve
}  // namespace RawrXD
