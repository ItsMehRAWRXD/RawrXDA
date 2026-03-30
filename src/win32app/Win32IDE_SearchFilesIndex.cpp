// Win32IDE_SearchFilesIndex.cpp — mmap-backed inverted index + optional Vulkan VRAM staging for /api/search-files
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "Win32IDE_SearchFilesIndex.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstring>
#include <functional>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#if RAWR_HAS_VULKAN
#include <vulkan/vulkan.h>
#endif

namespace rawrxd::local_server_search
{
namespace
{

std::mutex g_searchCacheMutex;
std::atomic<uint64_t> g_index_generation{0};

static std::string normalizeRoot(std::string p)
{
    for (auto& c : p)
    {
        if (c == '/')
            c = '\\';
    }
    while (!p.empty() && (p.back() == '\\' || p.back() == '/'))
        p.pop_back();
    return p;
}

static bool nameMatchesPattern(const std::string& name, const std::string& pattern)
{
    if (pattern.empty() || pattern == "*")
        return true;
    if (pattern.size() >= 2 && pattern[0] == '*' && pattern[pattern.size() - 1] == '*')
    {
        std::string sub = pattern.substr(1, pattern.size() - 2);
        std::string lowerName = name;
        std::string lowerSub = sub;
        for (auto& c : lowerName)
            c = (char)std::tolower((unsigned char)c);
        for (auto& c : lowerSub)
            c = (char)std::tolower((unsigned char)c);
        return lowerName.find(lowerSub) != std::string::npos;
    }
    if (!pattern.empty() && pattern[0] == '*')
    {
        std::string ext = pattern.substr(1);
        std::string lowerName = name;
        std::string lowerExt = ext;
        for (auto& c : lowerName)
            c = (char)std::tolower((unsigned char)c);
        for (auto& c : lowerExt)
            c = (char)std::tolower((unsigned char)c);
        return (lowerName.size() >= lowerExt.size() &&
                lowerName.substr(lowerName.size() - lowerExt.size()) == lowerExt);
    }
    std::string lowerName = name;
    std::string lowerPattern = pattern;
    for (auto& c : lowerName)
        c = (char)std::tolower((unsigned char)c);
    for (auto& c : lowerPattern)
        c = (char)std::tolower((unsigned char)c);
    return lowerName.find(lowerPattern) != std::string::npos;
}

static bool shouldSkipDir(const std::string& name, const std::unordered_set<std::string>& extra)
{
    if (name == "." || name == "..")
        return true;
    if (name[0] == '.')
        return true;
    static const char* kBuiltin[] = {"node_modules", ".git", "__pycache__", ".vs", "Debug", "Release"};
    for (auto* s : kBuiltin)
    {
        if (name == s)
            return true;
    }
    std::string lower = name;
    for (auto& c : lower)
        c = (char)std::tolower((unsigned char)c);
    return extra.find(lower) != extra.end();
}

static void appendTokenFromLine(const char* line, size_t len, bool case_sensitive, std::vector<std::string>& out)
{
    std::string cur;
    auto flush = [&]
    {
        if (cur.size() >= 2)
        {
            if (!case_sensitive)
            {
                for (auto& c : cur)
                    c = (char)std::tolower((unsigned char)c);
            }
            out.push_back(cur);
        }
        cur.clear();
    };
    for (size_t i = 0; i < len; ++i)
    {
        unsigned char ch = (unsigned char)line[i];
        if (std::isalnum(ch) || ch == '_')
            cur.push_back((char)ch);
        else
            flush();
    }
    flush();
}

static std::vector<std::string> splitQueryTokens(const std::string& q)
{
    std::vector<std::string> toks;
    std::string cur;
    for (unsigned char ch : q)
    {
        if (std::isspace(ch))
        {
            if (!cur.empty())
            {
                toks.push_back(cur);
                cur.clear();
            }
        }
        else
            cur.push_back((char)ch);
    }
    if (!cur.empty())
        toks.push_back(cur);
    return toks;
}

static bool readFileMmap(const std::string& path, int maxBytes, std::string& out)
{
    out.clear();
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                           nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    LARGE_INTEGER li{};
    if (!GetFileSizeEx(h, &li) || li.QuadPart <= 0)
    {
        CloseHandle(h);
        return false;
    }
    const DWORD cap = (DWORD)std::min<int64_t>(li.QuadPart, maxBytes);
    HANDLE map = CreateFileMappingA(h, nullptr, PAGE_READONLY, 0, 0, nullptr);
    CloseHandle(h);
    if (!map)
        return false;
    const void* view = MapViewOfFile(map, FILE_MAP_READ, 0, 0, cap);
    if (!view)
    {
        CloseHandle(map);
        return false;
    }
    out.assign((const char*)view, cap);
    UnmapViewOfFile(view);
    CloseHandle(map);
    return true;
}

static void searchRecursiveScan(const SearchRequest& req, std::vector<FileSearchResult>& results)
{
    const std::unordered_set<std::string> extra(req.exclude_dirs_extra.begin(), req.exclude_dirs_extra.end());

    std::function<void(const std::string&)> walk = [&](const std::string& dir)
    {
        if ((int)results.size() >= req.max_results)
            return;
        std::string searchPath = dir + "\\*";
        WIN32_FIND_DATAA fd{};
        HANDLE hFind = FindFirstFileA(searchPath.c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE)
            return;
        do
        {
            if ((int)results.size() >= req.max_results)
                break;
            std::string name = fd.cFileName;
            std::string fullPath = dir + "\\" + name;

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (!shouldSkipDir(name, extra))
                    walk(fullPath);
            }
            else
            {
                if (!nameMatchesPattern(name, req.pattern))
                    continue;
                if (!req.search_content || req.query.empty())
                {
                    FileSearchResult r;
                    r.path = fullPath;
                    r.line = 0;
                    results.push_back(std::move(r));
                    continue;
                }
                if (fd.nFileSizeLow > (DWORD)req.max_file_bytes || fd.nFileSizeHigh != 0)
                    continue;
                std::string content;
                if (!readFileMmap(fullPath, req.max_file_bytes, content))
                    continue;

                std::string hay = content;
                std::string needle = req.query;
                if (!req.case_sensitive)
                {
                    for (auto& c : hay)
                        c = (char)std::tolower((unsigned char)c);
                    for (auto& c : needle)
                        c = (char)std::tolower((unsigned char)c);
                }
                size_t pos = 0;
                int lineNum = 1;
                size_t lineStart = 0;
                while (pos < hay.size() && (int)results.size() < req.max_results)
                {
                    size_t found = hay.find(needle, pos);
                    if (found == std::string::npos)
                        break;
                    while (lineStart < found)
                    {
                        if (content[lineStart] == '\n')
                            lineNum++;
                        lineStart++;
                    }
                    size_t lineEnd = content.find('\n', found);
                    size_t prevNewline = content.rfind('\n', found);
                    size_t ls = (prevNewline == std::string::npos) ? 0 : prevNewline + 1;
                    size_t le = (lineEnd == std::string::npos) ? content.size() : lineEnd;
                    std::string lineText = content.substr(ls, std::min(le - ls, (size_t)req.snippet_max_chars));
                    FileSearchResult r;
                    r.path = fullPath;
                    r.line = lineNum;
                    r.lineText = std::move(lineText);
                    results.push_back(std::move(r));
                    pos = found + std::max<size_t>(needle.size(), 1);
                }
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    };

    walk(req.root);
}

struct Hit
{
    uint32_t file_idx = 0;
    uint32_t line = 0;
    std::string text;
};

struct IndexData
{
    uint64_t generation = 0;
    std::vector<std::string> files;
    std::unordered_map<std::string, std::vector<Hit>> postings;
};

static void buildInvertedIndex(const SearchRequest& req, IndexData& out, double& build_ms)
{
    const auto t0 = std::chrono::steady_clock::now();
    const std::unordered_set<std::string> extra(req.exclude_dirs_extra.begin(), req.exclude_dirs_extra.end());
    out.files.clear();
    out.postings.clear();
    out.generation = ++g_index_generation;

    constexpr int kMaxIndexedFiles = 12000;
    int filesIndexed = 0;

    std::function<void(const std::string&)> walk = [&](const std::string& dir)
    {
        if (filesIndexed >= kMaxIndexedFiles)
            return;
        std::string searchPath = dir + "\\*";
        WIN32_FIND_DATAA fd{};
        HANDLE hFind = FindFirstFileA(searchPath.c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE)
            return;
        do
        {
            std::string name = fd.cFileName;
            std::string fullPath = dir + "\\" + name;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (!shouldSkipDir(name, extra))
                    walk(fullPath);
                continue;
            }
            if (!nameMatchesPattern(name, req.pattern))
                continue;
            if (fd.nFileSizeLow > (DWORD)req.max_file_bytes || fd.nFileSizeHigh != 0)
                continue;

            std::string content;
            if (!readFileMmap(fullPath, req.max_file_bytes, content))
                continue;

            uint32_t file_idx = (uint32_t)out.files.size();
            out.files.push_back(fullPath);
            filesIndexed++;

            int lineNum = 1;
            size_t lineStart = 0;
            for (size_t i = 0; i <= content.size(); ++i)
            {
                if (i < content.size() && content[i] != '\n')
                    continue;
                const char* linePtr = content.data() + lineStart;
                size_t lineLen = i - lineStart;
                std::vector<std::string> toks;
                appendTokenFromLine(linePtr, lineLen, req.case_sensitive, toks);
                std::unordered_set<std::string> uniq(toks.begin(), toks.end());
                size_t le = i;
                size_t ls = lineStart;
                std::string snippet = content.substr(ls, std::min(le - ls, (size_t)req.snippet_max_chars));
                for (const auto& tok : uniq)
                {
                    if (tok.size() < 2)
                        continue;
                    out.postings[tok].push_back(Hit{file_idx, (uint32_t)lineNum, snippet});
                }
                lineNum++;
                lineStart = i + 1;
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    };

    walk(req.root);
    const auto t1 = std::chrono::steady_clock::now();
    build_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
}

static std::string cacheFingerprint(const SearchRequest& req)
{
    std::ostringstream oss;
    oss << req.pattern << "|" << req.max_file_bytes << "|" << (req.case_sensitive ? "1" : "0") << "|";
    auto ex = req.exclude_dirs_extra;
    std::sort(ex.begin(), ex.end());
    for (const auto& e : ex)
        oss << e << ";";
    return oss.str();
}

struct CacheEntry
{
    std::string root;
    std::string fingerprint;
    std::chrono::steady_clock::time_point built_at{};
    int ttl_sec = 30;
    IndexData index;
    double last_build_ms = 0;
#if RAWR_HAS_VULKAN
    VkDevice vk_dev = VK_NULL_HANDLE;
    VkBuffer vram_buf = VK_NULL_HANDLE;
    VkDeviceMemory vram_mem = VK_NULL_HANDLE;
    size_t vram_size = 0;
#endif
};

static CacheEntry g_cache;

#if RAWR_HAS_VULKAN

struct VkUploadCtx
{
    VkInstance inst = VK_NULL_HANDLE;
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    VkDevice dev = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t qfam = 0;
    VkCommandPool pool = VK_NULL_HANDLE;
    bool ready = false;
};

static VkUploadCtx& vkUpload()
{
    static VkUploadCtx c;
    return c;
}

static void destroyVramEntry(CacheEntry& e)
{
    if (e.vram_buf != VK_NULL_HANDLE && e.vk_dev != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(e.vk_dev, e.vram_buf, nullptr);
        e.vram_buf = VK_NULL_HANDLE;
    }
    if (e.vram_mem != VK_NULL_HANDLE && e.vk_dev != VK_NULL_HANDLE)
    {
        vkFreeMemory(e.vk_dev, e.vram_mem, nullptr);
        e.vram_mem = VK_NULL_HANDLE;
    }
    e.vk_dev = VK_NULL_HANDLE;
    e.vram_size = 0;
}

static bool ensureVkUploadDevice()
{
    auto& c = vkUpload();
    if (c.ready)
        return true;

    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "RawrXDSearchIndex";
    app.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;

    if (vkCreateInstance(&ici, nullptr, &c.inst) != VK_SUCCESS)
        return false;

    uint32_t n = 0;
    vkEnumeratePhysicalDevices(c.inst, &n, nullptr);
    if (n == 0)
    {
        vkDestroyInstance(c.inst, nullptr);
        c.inst = VK_NULL_HANDLE;
        return false;
    }
    std::vector<VkPhysicalDevice> devs(n);
    vkEnumeratePhysicalDevices(c.inst, &n, devs.data());
    c.phys = devs[0];

    uint32_t famCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(c.phys, &famCount, nullptr);
    std::vector<VkQueueFamilyProperties> fams(famCount);
    vkGetPhysicalDeviceQueueFamilyProperties(c.phys, &famCount, fams.data());
    c.qfam = UINT32_MAX;
    for (uint32_t i = 0; i < famCount; ++i)
    {
        if (fams[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            c.qfam = i;
            break;
        }
    }
    if (c.qfam == UINT32_MAX)
    {
        c.qfam = 0;
    }

    float qp = 1.f;
    VkDeviceQueueCreateInfo dq{};
    dq.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    dq.queueFamilyIndex = c.qfam;
    dq.queueCount = 1;
    dq.pQueuePriorities = &qp;

    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &dq;

    if (vkCreateDevice(c.phys, &dci, nullptr, &c.dev) != VK_SUCCESS)
    {
        vkDestroyInstance(c.inst, nullptr);
        c.inst = VK_NULL_HANDLE;
        return false;
    }
    vkGetDeviceQueue(c.dev, c.qfam, 0, &c.queue);

    VkCommandPoolCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pci.queueFamilyIndex = c.qfam;
    if (vkCreateCommandPool(c.dev, &pci, nullptr, &c.pool) != VK_SUCCESS)
    {
        vkDestroyDevice(c.dev, nullptr);
        vkDestroyInstance(c.inst, nullptr);
        c.dev = VK_NULL_HANDLE;
        c.inst = VK_NULL_HANDLE;
        return false;
    }

    c.ready = true;
    return true;
}

static bool stageBlobToVram(const void* data, size_t size, double& upload_ms, uint64_t& out_bytes, VkDevice& out_dev,
                            VkBuffer& out_buf, VkDeviceMemory& out_mem)
{
    upload_ms = 0;
    out_bytes = 0;
    out_buf = VK_NULL_HANDLE;
    out_mem = VK_NULL_HANDLE;
    out_dev = VK_NULL_HANDLE;
    if (!data || size == 0 || !ensureVkUploadDevice())
        return false;

    auto& c = vkUpload();
    const auto t0 = std::chrono::steady_clock::now();

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = size;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer devBuf = VK_NULL_HANDLE;
    if (vkCreateBuffer(c.dev, &bci, nullptr, &devBuf) != VK_SUCCESS)
        return false;

    VkMemoryRequirements mr{};
    vkGetBufferMemoryRequirements(c.dev, devBuf, &mr);

    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(c.phys, &mp);

    uint32_t memIndex = UINT32_MAX;
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
    {
        if ((mr.memoryTypeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            memIndex = i;
            break;
        }
    }
    if (memIndex == UINT32_MAX)
    {
        for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        {
            if (mr.memoryTypeBits & (1u << i))
            {
                memIndex = i;
                break;
            }
        }
    }
    if (memIndex == UINT32_MAX)
    {
        vkDestroyBuffer(c.dev, devBuf, nullptr);
        return false;
    }

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = mr.size;
    mai.memoryTypeIndex = memIndex;

    VkDeviceMemory devMem = VK_NULL_HANDLE;
    if (vkAllocateMemory(c.dev, &mai, nullptr, &devMem) != VK_SUCCESS)
    {
        vkDestroyBuffer(c.dev, devBuf, nullptr);
        return false;
    }
    vkBindBufferMemory(c.dev, devBuf, devMem, 0);

    VkBufferCreateInfo sbci = bci;
    sbci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkBuffer stBuf = VK_NULL_HANDLE;
    if (vkCreateBuffer(c.dev, &sbci, nullptr, &stBuf) != VK_SUCCESS)
    {
        vkFreeMemory(c.dev, devMem, nullptr);
        vkDestroyBuffer(c.dev, devBuf, nullptr);
        return false;
    }

    VkMemoryRequirements smr{};
    vkGetBufferMemoryRequirements(c.dev, stBuf, &smr);
    uint32_t stIndex = UINT32_MAX;
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
    {
        if ((smr.memoryTypeBits & (1u << i)) &&
            (mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
            (mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            stIndex = i;
            break;
        }
    }
    if (stIndex == UINT32_MAX)
    {
        vkDestroyBuffer(c.dev, stBuf, nullptr);
        vkFreeMemory(c.dev, devMem, nullptr);
        vkDestroyBuffer(c.dev, devBuf, nullptr);
        return false;
    }

    VkMemoryAllocateInfo smai{};
    smai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    smai.allocationSize = smr.size;
    smai.memoryTypeIndex = stIndex;

    VkDeviceMemory stMem = VK_NULL_HANDLE;
    if (vkAllocateMemory(c.dev, &smai, nullptr, &stMem) != VK_SUCCESS)
    {
        vkDestroyBuffer(c.dev, stBuf, nullptr);
        vkFreeMemory(c.dev, devMem, nullptr);
        vkDestroyBuffer(c.dev, devBuf, nullptr);
        return false;
    }
    vkBindBufferMemory(c.dev, stBuf, stMem, 0);

    void* mapped = nullptr;
    if (vkMapMemory(c.dev, stMem, 0, size, 0, &mapped) != VK_SUCCESS)
    {
        vkFreeMemory(c.dev, stMem, nullptr);
        vkDestroyBuffer(c.dev, stBuf, nullptr);
        vkFreeMemory(c.dev, devMem, nullptr);
        vkDestroyBuffer(c.dev, devBuf, nullptr);
        return false;
    }
    std::memcpy(mapped, data, size);
    vkUnmapMemory(c.dev, stMem);

    VkCommandBufferAllocateInfo cbai{};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = c.pool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(c.dev, &cbai, &cmd) != VK_SUCCESS)
    {
        vkFreeMemory(c.dev, stMem, nullptr);
        vkDestroyBuffer(c.dev, stBuf, nullptr);
        vkFreeMemory(c.dev, devMem, nullptr);
        vkDestroyBuffer(c.dev, devBuf, nullptr);
        return false;
    }

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    VkBufferCopy region{};
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size = size;
    vkCmdCopyBuffer(cmd, stBuf, devBuf, 1, &region);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    if (vkQueueSubmit(c.queue, 1, &si, VK_NULL_HANDLE) != VK_SUCCESS)
    {
        vkFreeCommandBuffers(c.dev, c.pool, 1, &cmd);
        vkFreeMemory(c.dev, stMem, nullptr);
        vkDestroyBuffer(c.dev, stBuf, nullptr);
        vkFreeMemory(c.dev, devMem, nullptr);
        vkDestroyBuffer(c.dev, devBuf, nullptr);
        return false;
    }
    vkQueueWaitIdle(c.queue);
    vkFreeCommandBuffers(c.dev, c.pool, 1, &cmd);

    vkFreeMemory(c.dev, stMem, nullptr);
    vkDestroyBuffer(c.dev, stBuf, nullptr);

    const auto t1 = std::chrono::steady_clock::now();
    upload_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    out_bytes = (uint64_t)size;
    out_buf = devBuf;
    out_mem = devMem;
    out_dev = c.dev;
    return true;
}

static std::vector<uint8_t> packIndexBlob(const IndexData& idx)
{
    std::vector<uint8_t> blob;
    auto appendU32 = [&](uint32_t v)
    {
        blob.push_back((uint8_t)(v & 0xff));
        blob.push_back((uint8_t)((v >> 8) & 0xff));
        blob.push_back((uint8_t)((v >> 16) & 0xff));
        blob.push_back((uint8_t)((v >> 24) & 0xff));
    };
    auto appendU64 = [&](uint64_t v)
    {
        for (int i = 0; i < 8; ++i)
            blob.push_back((uint8_t)((v >> (8 * i)) & 0xff));
    };
    appendU32(0x52415258u);  // RARX magic
    appendU64(idx.generation);
    appendU32((uint32_t)std::min<size_t>(idx.files.size(), 0xffffffffu));
    for (const auto& f : idx.files)
    {
        for (unsigned char ch : f)
            blob.push_back(ch);
        blob.push_back(0);
    }
    uint32_t totalCells = 0;
    for (const auto& kv : idx.postings)
        totalCells += (uint32_t)std::min<size_t>(kv.second.size(), 64);
    appendU32(totalCells);
    for (const auto& kv : idx.postings)
    {
        for (unsigned char ch : kv.first)
            blob.push_back(ch);
        blob.push_back(0);
        uint32_t n = (uint32_t)std::min<size_t>(kv.second.size(), 64);
        appendU32(n);
        for (uint32_t i = 0; i < n; ++i)
        {
            appendU32(kv.second[i].file_idx);
            appendU32(kv.second[i].line);
        }
    }
    constexpr size_t kMaxVram = 16u * 1024u * 1024u;
    if (blob.size() > kMaxVram)
        blob.resize(kMaxVram);
    return blob;
}

#else

static void destroyVramEntry(CacheEntry&) {}

static std::vector<uint8_t> packIndexBlob(const IndexData& idx)
{
    (void)idx;
    return {};
}

#endif

static void queryInvertedIndex(const SearchRequest& req, const IndexData& idx, std::vector<FileSearchResult>& results,
                               double& qms)
{
    const auto t0 = std::chrono::steady_clock::now();
    auto tokens = splitQueryTokens(req.query);
    if (tokens.empty())
    {
        qms = 0;
        return;
    }
    if (!req.case_sensitive)
    {
        for (auto& t : tokens)
            for (auto& c : t)
                c = (char)std::tolower((unsigned char)c);
    }

    std::vector<const std::vector<Hit>*> lists;
    lists.reserve(tokens.size());
    for (const auto& tok : tokens)
    {
        if (tok.size() < 2)
            continue;
        auto it = idx.postings.find(tok);
        if (it == idx.postings.end())
        {
            lists.clear();
            break;
        }
        lists.push_back(&it->second);
    }
    if (lists.empty())
    {
        qms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        return;
    }

    std::unordered_map<uint64_t, FileSearchResult> acc;
    auto keyOf = [](uint32_t f, uint32_t ln) -> uint64_t { return (uint64_t)f << 32 | (uint64_t)ln; };

    if (lists.size() == 1)
    {
        for (const Hit& h : *lists[0])
        {
            if (h.file_idx >= idx.files.size())
                continue;
            uint64_t k = keyOf(h.file_idx, h.line);
            if (acc.size() >= (size_t)req.max_results)
                break;
            FileSearchResult fr;
            fr.path = idx.files[h.file_idx];
            fr.line = (int)h.line;
            fr.lineText = h.text;
            acc.emplace(k, std::move(fr));
        }
    }
    else
    {
        std::unordered_map<uint64_t, Hit> first;
        for (const Hit& h : *lists[0])
        {
            if (h.file_idx >= idx.files.size())
                continue;
            first[keyOf(h.file_idx, h.line)] = h;
        }
        for (size_t li = 1; li < lists.size(); ++li)
        {
            std::unordered_set<uint64_t> present;
            for (const Hit& h : *lists[li])
            {
                if (h.file_idx >= idx.files.size())
                    continue;
                present.insert(keyOf(h.file_idx, h.line));
            }
            for (auto it = first.begin(); it != first.end();)
            {
                if (present.find(it->first) == present.end())
                    it = first.erase(it);
                else
                    ++it;
            }
        }
        for (const auto& kv : first)
        {
            if ((int)acc.size() >= req.max_results)
                break;
            const Hit& h = kv.second;
            FileSearchResult fr;
            fr.path = idx.files[h.file_idx];
            fr.line = (int)h.line;
            fr.lineText = h.text;
            acc.emplace(kv.first, std::move(fr));
        }
    }

    for (const auto& kv : acc)
        results.push_back(kv.second);

    qms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

static std::string toLowerAscii(std::string s)
{
    for (auto& c : s)
        c = (char)std::tolower((unsigned char)c);
    return s;
}

}  // namespace

void executeFileSearch(const SearchRequest& req, std::vector<FileSearchResult>& out, SearchResponseMetrics& met)
{
    out.clear();
    met = SearchResponseMetrics{};
    const auto tTotal0 = std::chrono::steady_clock::now();

    SearchRequest r = req;
    r.root = normalizeRoot(r.root);
    std::string backend = toLowerAscii(r.backend);
    if (backend.empty())
        backend = "auto";

    if (backend == "recursive-scan")
    {
        searchRecursiveScan(r, out);
        met.backend = "recursive-scan";
        met.total_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tTotal0).count();
        met.query_ms = met.total_ms;
        return;
    }

    if (!r.search_content || r.query.empty())
    {
        searchRecursiveScan(r, out);
        met.backend = "recursive-scan";
        met.total_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tTotal0).count();
        met.query_ms = met.total_ms;
        return;
    }

    const bool want_mmap = (backend == "mmap-cpu-index" || backend == "vram-inverted-index");
    bool use_index = false;
    if (backend == "auto")
    {
        auto toks = splitQueryTokens(r.query);
        use_index = !toks.empty();
        for (const auto& t : toks)
        {
            if (t.size() < 2)
            {
                use_index = false;
                break;
            }
        }
        if (use_index)
        {
            for (unsigned char ch : r.query)
            {
                if (ch == ':' || ch == '/' || ch == '\\' || ch == '"' || ch == '\'' || ch == '(' || ch == ')')
                {
                    use_index = false;
                    met.fallback_reason = "query_not_index_friendly";
                    break;
                }
            }
        }
    }
    else if (want_mmap)
        use_index = true;

    if (!use_index)
    {
        searchRecursiveScan(r, out);
        met.backend = "recursive-scan";
        met.total_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tTotal0).count();
        met.query_ms = met.total_ms;
        if (!met.fallback_reason.empty())
        {
        }
        return;
    }

    std::lock_guard<std::mutex> lock(g_searchCacheMutex);
    const std::string fp = cacheFingerprint(r);
    const auto now = std::chrono::steady_clock::now();
    bool cache_hit =
        (!r.force_reindex && g_cache.root == r.root && g_cache.fingerprint == fp &&
         g_cache.ttl_sec == r.index_ttl_sec &&
         std::chrono::duration_cast<std::chrono::seconds>(now - g_cache.built_at).count() < r.index_ttl_sec);

    double build_ms = 0;
    if (!cache_hit)
    {
        destroyVramEntry(g_cache);
        g_cache = CacheEntry{};
        g_cache.root = r.root;
        g_cache.fingerprint = fp;
        g_cache.ttl_sec = r.index_ttl_sec;
        buildInvertedIndex(r, g_cache.index, build_ms);
        g_cache.last_build_ms = build_ms;
        g_cache.built_at = now;
        met.cached_index = false;
        met.index_generation = g_cache.index.generation;
    }
    else
    {
        met.cached_index = true;
        met.index_generation = g_cache.index.generation;
        build_ms = g_cache.last_build_ms;
    }

    met.index_build_ms = build_ms;

    double qms = 0;
    queryInvertedIndex(r, g_cache.index, out, qms);
    met.query_ms = qms;

#if RAWR_HAS_VULKAN
    if (g_cache.vram_buf == VK_NULL_HANDLE && (backend == "vram-inverted-index" || (backend == "auto" && !cache_hit)))
    {
        std::vector<uint8_t> blob = packIndexBlob(g_cache.index);
        if (!blob.empty())
        {
            double up = 0;
            uint64_t bytes = 0;
            if (stageBlobToVram(blob.data(), blob.size(), up, bytes, g_cache.vk_dev, g_cache.vram_buf,
                                g_cache.vram_mem))
            {
                g_cache.vram_size = blob.size();
                met.gpu_upload_ms = up;
                met.vram_resident_bytes = bytes;
            }
            else if (backend == "vram-inverted-index")
            {
                met.fallback_reason =
                    met.fallback_reason.empty() ? std::string("vulkan_stage_failed") : met.fallback_reason;
            }
        }
    }
    if (backend == "mmap-cpu-index")
        met.backend = "mmap-cpu-index";
    else if (g_cache.vram_buf != VK_NULL_HANDLE)
    {
        met.backend = "vram-inverted-index";
        if (met.vram_resident_bytes == 0)
            met.vram_resident_bytes = g_cache.vram_size;
    }
    else
        met.backend = "mmap-cpu-index";
#else
    met.backend = "mmap-cpu-index";
    if (backend == "vram-inverted-index")
        met.fallback_reason = "vulkan_disabled";
#endif

    met.total_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tTotal0).count();
}

}  // namespace rawrxd::local_server_search
