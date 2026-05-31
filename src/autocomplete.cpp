// autocomplete.cpp – LLM autocomplete engine for RawrXD
// Single-file, zero external dependencies (C++17 + OS APIs only).
//
// Build (MSVC x64):
//   cl /std:c++17 /O2 /arch:AVX2 /EHsc autocomplete.cpp /Fe:autocomplete.exe
// Build (GCC/MinGW x64):
//   g++ -std=c++17 -O3 -mavx2 -pthread -o autocomplete autocomplete.cpp
//
// Usage:
//   autocomplete.exe --model path\to\model.gguf [--ctx 2048] [--threads 4]
//
// Shared-memory segment name: "RawrXD_Autocomplete"   (Windows named mapping)
//                             "/RawrXD_Autocomplete"   (POSIX shm)

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cassert>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstdlib>
#include <iostream>

// ── SIMD ─────────────────────────────────────────────────────────────────────
#if defined(__AVX2__) || (defined(_MSC_VER) && defined(__AVX2__))
#  include <immintrin.h>
#  define HAVE_AVX2 1
#else
#  define HAVE_AVX2 0
#endif

// ── OS abstractions ───────────────────────────────────────────────────────────
#ifdef _WIN32
#  include <windows.h>
   using ShmHandle = HANDLE;
   inline ShmHandle shm_open_existing(const char* name) {
       return OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name);
   }
   inline ShmHandle shm_create(const char* name, size_t size) {
       return CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                 (DWORD)(size >> 32), (DWORD)(size & 0xFFFFFFFF), name);
   }
   inline void* shm_map(ShmHandle h, size_t /*size*/) {
       return MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, 0);
   }
   inline void shm_unmap(void* p, size_t /*size*/) { UnmapViewOfFile(p); }
   inline void shm_close(ShmHandle h) { CloseHandle(h); }
   inline int cpu_count() {
       SYSTEM_INFO si; GetSystemInfo(&si); return (int)si.dwNumberOfProcessors;
   }
#else
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#  include <unistd.h>
   using ShmHandle = int;
   inline ShmHandle shm_open_existing(const char* name) {
       return shm_open(name, O_RDWR, 0666);
   }
   inline ShmHandle shm_create(const char* name, size_t size) {
       shm_unlink(name);
       int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
       if (fd >= 0) ftruncate(fd, (off_t)size);
       return fd;
   }
   inline void* shm_map(ShmHandle fd, size_t size) {
       return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
   }
   inline void shm_unmap(void* p, size_t size) { munmap(p, size); }
   inline void shm_close(ShmHandle fd) { close(fd); }
   inline int cpu_count() { return (int)sysconf(_SC_NPROCESSORS_ONLN); }
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Shared-memory exchange protocol (lock-free, single-producer/consumer)
// Layout fits inside one 4 KB page.
// ─────────────────────────────────────────────────────────────────────────────
struct alignas(64) SharedExchange {
    // ── request fields (written by IDE, read by engine) ──────────────────────
    std::atomic<uint32_t> gate;          // 0=idle 1=request 2=response 3=shutdown
    uint32_t  n_prefix_tokens;           // number of prefix tokens
    uint32_t  n_suffix_tokens;           // FIM suffix tokens (0 = no FIM)
    uint32_t  max_new_tokens;            // cap on speculative output (1-64)
    uint32_t  prefix_tokens[512];        // prefix token ids
    uint32_t  suffix_tokens[128];        // suffix token ids (FIM)
    char      fim_mode;                  // 'p'=prefix-suffix-middle, 0=disabled
    char      _pad[63];
    // ── response fields (written by engine, read by IDE) ─────────────────────
    uint32_t  out_tokens[64];            // generated token ids
    uint32_t  n_out_tokens;              // how many were produced
    float     token_logprobs[64];        // per-token log probability
    char      text_buf[2048];            // decoded text (UTF-8, null-terminated)
};
static_assert(sizeof(SharedExchange) <= 16384, "SharedExchange too large");

// ─────────────────────────────────────────────────────────────────────────────
// GGML quantisation type IDs
// ─────────────────────────────────────────────────────────────────────────────
enum class GGMLType : uint32_t {
    F32  = 0,
    F16  = 1,
    Q4_0 = 2,
    Q4_1 = 3,
    Q5_0 = 6,
    Q5_1 = 7,
    Q8_0 = 8,
    UNKNOWN = 0xFFFFFFFFu
};

static const char* ggml_rxd_type_name(GGMLType t) {
    switch (t) {
        case GGMLType::F32:  return "F32";
        case GGMLType::F16:  return "F16";
        case GGMLType::Q4_0: return "Q4_0";
        case GGMLType::Q4_1: return "Q4_1";
        case GGMLType::Q5_0: return "Q5_0";
        case GGMLType::Q5_1: return "Q5_1";
        case GGMLType::Q8_0: return "Q8_0";
        default: return "?";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tensor descriptor
// ─────────────────────────────────────────────────────────────────────────────
struct Tensor {
    std::string           name;
    GGMLType              type  = GGMLType::F32;
    std::vector<uint64_t> dims;
    std::vector<uint8_t>  data;

    int ne(int i) const {
        return (i < (int)dims.size()) ? (int)dims[i] : 1;
    }
    // total element count
    int nelements() const {
        int n = 1;
        for (auto d : dims) n *= (int)d;
        return n;
    }
    // bytes for one row (dims[0] elements)
    size_t row_bytes() const {
        int n = ne(0);
        constexpr int QK = 32;
        int nb = (n + QK - 1) / QK;
        switch (type) {
            case GGMLType::F32:  return (size_t)n * 4;
            case GGMLType::F16:  return (size_t)n * 2;
            case GGMLType::Q4_0: return (size_t)nb * 18;   // 2(d) + 16(qs)
            case GGMLType::Q4_1: return (size_t)nb * 20;   // 2(d)+2(m)+16(qs)
            case GGMLType::Q5_0: return (size_t)nb * 22;   // 2(d)+4(qh)+16(qs)
            case GGMLType::Q5_1: return (size_t)nb * 24;   // 2(d)+2(m)+4(qh)+16(qs)
            case GGMLType::Q8_0: return (size_t)nb * 34;   // 2(d)+32(qs)
            default: return 0;
        }
    }
    size_t total_bytes() const {
        int rows = 1;
        for (int i = 1; i < (int)dims.size(); ++i) rows *= (int)dims[i];
        return row_bytes() * (size_t)rows;
    }
    bool empty() const { return data.empty(); }
};

// ─────────────────────────────────────────────────────────────────────────────
// GGUF file reader  (spec v3)
// ─────────────────────────────────────────────────────────────────────────────
struct GGUFReader {
    std::vector<Tensor> tensors;

    struct KV {
        std::string           key;
        uint32_t              type;
        std::vector<uint8_t>  raw; // raw bytes of value
    };
    std::vector<KV> kv_store;

    Tensor* find(const char* name) {
        for (auto& t : tensors) if (t.name == name) return &t;
        return nullptr;
    }

    // ── KV getters ────────────────────────────────────────────────────────────
    int32_t get_i32(const char* key, int32_t def = 0) const {
        for (auto& p : kv_store) if (p.key == key) {
            if (p.raw.size() >= 4) { int32_t v; memcpy(&v, p.raw.data(), 4); return v; }
        }
        return def;
    }
    uint32_t get_u32(const char* key, uint32_t def = 0) const {
        for (auto& p : kv_store) if (p.key == key) {
            if (p.raw.size() >= 4) { uint32_t v; memcpy(&v, p.raw.data(), 4); return v; }
        }
        return def;
    }
    float get_f32(const char* key, float def = 0.f) const {
        for (auto& p : kv_store) if (p.key == key) {
            if (p.raw.size() >= 4) { float v; memcpy(&v, p.raw.data(), 4); return v; }
        }
        return def;
    }
    std::string get_str(const char* key) const {
        for (auto& p : kv_store) if (p.key == key && p.type == 8) {
            if (p.raw.size() < 8) continue;
            uint64_t len; memcpy(&len, p.raw.data(), 8);
            if (p.raw.size() < 8 + len) continue;
            return std::string(reinterpret_cast<const char*>(p.raw.data()) + 8, (size_t)len);
        }
        return {};
    }
    std::vector<std::string> get_str_arr(const char* key) const {
        std::vector<std::string> out;
        for (auto& p : kv_store) if (p.key == key && p.type == 9) {
            const uint8_t* ptr = p.raw.data();
            if (p.raw.size() < 12) break;
            uint32_t elem_type; memcpy(&elem_type, ptr, 4);
            uint64_t count;     memcpy(&count, ptr + 4, 8);
            if (elem_type != 8) break; // not string
            const uint8_t* q = ptr + 12;
            size_t remain = p.raw.size() - 12;
            for (uint64_t i = 0; i < count; ++i) {
                if (remain < 8) break;
                uint64_t slen; memcpy(&slen, q, 8);
                q += 8; remain -= 8;
                if (remain < slen) break;
                out.emplace_back(reinterpret_cast<const char*>(q), (size_t)slen);
                q += slen; remain -= (size_t)slen;
            }
            break;
        }
        return out;
    }
    std::vector<int32_t> get_i32_arr(const char* key) const {
        std::vector<int32_t> out;
        for (auto& p : kv_store) if (p.key == key && p.type == 9) {
            const uint8_t* ptr = p.raw.data();
            if (p.raw.size() < 12) break;
            uint32_t elem_type; memcpy(&elem_type, ptr, 4);
            uint64_t count;     memcpy(&count, ptr + 4, 8);
            if (elem_type != 4) break; // not i32
            out.resize((size_t)count);
            memcpy(out.data(), ptr + 12, (size_t)count * 4);
            break;
        }
        return out;
    }

    // ── file I/O helpers ─────────────────────────────────────────────────────
private:
    static bool read_str(FILE* f, std::string& out) {
        uint64_t len;
        if (fread(&len, 8, 1, f) != 1) return false;
        out.resize((size_t)len);
        return fread(out.data(), 1, (size_t)len, f) == (size_t)len;
    }

    // Read a single KV value into raw bytes, given vtype
    static bool read_kv_value(FILE* f, uint32_t vtype, std::vector<uint8_t>& raw) {
        switch (vtype) {
            case 0: // UINT8
            case 1: { // INT8
                raw.resize(1);
                return fread(raw.data(), 1, 1, f) == 1;
            }
            case 2: // UINT16
            case 3: { // INT16
                raw.resize(2);
                return fread(raw.data(), 1, 2, f) == 2;
            }
            case 4: // UINT32
            case 5: // INT32
            case 10: { // BOOL
                raw.resize(4);
                return fread(raw.data(), 1, 4, f) == 4;
            }
            case 6: // FLOAT32
            {
                raw.resize(4);
                return fread(raw.data(), 1, 4, f) == 4;
            }
            case 11: // UINT64
            case 12: // INT64
            case 13: { // FLOAT64
                raw.resize(8);
                return fread(raw.data(), 1, 8, f) == 8;
            }
            case 8: { // STRING
                uint64_t slen;
                if (fread(&slen, 8, 1, f) != 1) return false;
                raw.resize(8 + (size_t)slen);
                memcpy(raw.data(), &slen, 8);
                return fread(raw.data() + 8, 1, (size_t)slen, f) == (size_t)slen;
            }
            case 9: { // ARRAY
                uint32_t etype;
                uint64_t count;
                if (fread(&etype, 4, 1, f) != 1) return false;
                if (fread(&count, 8, 1, f) != 1) return false;
                // For string array, each element is variable length.
                if (etype == 8) {
                    // read all strings, stitch into raw
                    std::vector<std::vector<uint8_t>> pieces;
                    size_t total = 12; // header: etype(4) + count(8)
                    for (uint64_t i = 0; i < count; ++i) {
                        uint64_t slen;
                        if (fread(&slen, 8, 1, f) != 1) return false;
                        std::vector<uint8_t> piece(8 + (size_t)slen);
                        memcpy(piece.data(), &slen, 8);
                        if (fread(piece.data() + 8, 1, (size_t)slen, f) != (size_t)slen) return false;
                        total += piece.size();
                        pieces.push_back(std::move(piece));
                    }
                    raw.resize(total);
                    uint8_t* p = raw.data();
                    memcpy(p, &etype, 4); p += 4;
                    memcpy(p, &count, 8); p += 8;
                    for (auto& piece : pieces) {
                        memcpy(p, piece.data(), piece.size());
                        p += piece.size();
                    }
                    return true;
                }
                // Fixed-size array
                uint32_t elem_bytes = 0;
                switch (etype) {
                    case 0: case 1: case 10: elem_bytes = 1; break;
                    case 2: case 3: elem_bytes = 2; break;
                    case 4: case 5: case 6:  elem_bytes = 4; break;
                    case 11: case 12: case 13: elem_bytes = 8; break;
                    default: elem_bytes = 4;
                }
                raw.resize(12 + (size_t)count * elem_bytes);
                memcpy(raw.data(), &etype, 4);
                memcpy(raw.data() + 4, &count, 8);
                return fread(raw.data() + 12, elem_bytes, (size_t)count, f) == (size_t)count;
            }
            default:
                return false;
        }
    }

public:
    bool load(const char* path) {
        FILE* f = fopen(path, "rb");
        if (!f) { perror("fopen"); return false; }
        bool ok = load_file(f);
        fclose(f);
        return ok;
    }

private:
    bool load_file(FILE* f) {
        // Magic + version
        uint32_t magic, ver;
        if (fread(&magic, 4, 1, f) != 1 || magic != 0x46554747u) {
            fprintf(stderr, "Not a GGUF file (magic=0x%08X)\n", magic);
            return false;
        }
        if (fread(&ver, 4, 1, f) != 1 || (ver < 2 || ver > 3)) {
            fprintf(stderr, "Unsupported GGUF version %u\n", ver);
            return false;
        }
        uint64_t n_tensors, n_kv;
        fread(&n_tensors, 8, 1, f);
        fread(&n_kv,      8, 1, f);

        // Tensor info (name, ndim, dims, type, offset)
        struct TensorInfo {
            std::string name;
            GGMLType    type;
            std::vector<uint64_t> dims;
            uint64_t    offset; // byte offset from data section start
        };
        std::vector<TensorInfo> infos(n_tensors);
        for (uint64_t i = 0; i < n_tensors; ++i) {
            if (!read_str(f, infos[i].name)) return false;
            uint32_t ndim;
            fread(&ndim, 4, 1, f);
            infos[i].dims.resize(ndim);
            for (uint32_t d = 0; d < ndim; ++d)
                fread(&infos[i].dims[d], 8, 1, f);
            uint32_t ttype;
            fread(&ttype, 4, 1, f);
            infos[i].type = static_cast<GGMLType>(ttype);
            fread(&infos[i].offset, 8, 1, f);
        }

        // KV metadata
        for (uint64_t i = 0; i < n_kv; ++i) {
            KV entry;
            if (!read_str(f, entry.key)) return false;
            fread(&entry.type, 4, 1, f);
            if (!read_kv_value(f, entry.type, entry.raw)) return false;
            kv_store.push_back(std::move(entry));
        }

        // Alignment to data section (GGUF spec: align to 32 bytes by default,
        // overridable by "general.alignment" key)
        uint32_t align = get_u32("general.alignment", 32);
        if (align == 0) align = 32;
        long pos = ftell(f);
        long pad = (long)(align - ((size_t)pos % align)) % align;
        if (pad) fseek(f, pad, SEEK_CUR);
        long data_start = ftell(f);

        // Read tensors at their recorded offsets
        tensors.resize(n_tensors);
        for (uint64_t i = 0; i < n_tensors; ++i) {
            tensors[i].name = infos[i].name;
            tensors[i].type = infos[i].type;
            tensors[i].dims = infos[i].dims;
            size_t nbytes = tensors[i].total_bytes();
            if (nbytes == 0) continue;
            tensors[i].data.resize(nbytes);
            long target = data_start + (long)infos[i].offset;
            fseek(f, target, SEEK_SET);
            if (fread(tensors[i].data.data(), 1, nbytes, f) != nbytes) {
                fprintf(stderr, "Short read for tensor '%s'\n", infos[i].name.c_str());
                return false;
            }
        }
        return true;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// FP16 ↔ FP32
// ─────────────────────────────────────────────────────────────────────────────
static inline float fp16_to_f32(uint16_t h) noexcept {
    // IEEE 754-2008 half precision -> single
    uint32_t sign = (h >> 15) & 1u;
    uint32_t exp  = (h >> 10) & 0x1Fu;
    uint32_t mant =  h        & 0x3FFu;
    uint32_t bits;
    if (exp == 0) {
        if (mant == 0) { bits = sign << 31; }
        else {
            // denormal: normalise
            int e = -1;
            uint32_t m = mant;
            do { e++; m <<= 1; } while (!(m & 0x400));
            bits = (sign << 31) | ((uint32_t)(112 - e) << 23) | ((m & 0x3FF) << 13);
        }
    } else if (exp == 0x1F) {
        bits = (sign << 31) | 0x7F800000u | (mant << 13);
    } else {
        bits = (sign << 31) | ((exp + 112) << 23) | (mant << 13);
    }
    float v; memcpy(&v, &bits, 4); return v;
}

// ─────────────────────────────────────────────────────────────────────────────
// Quantised dot products
// Each returns  sum_i  dequant(w[i]) * x[i]  over  n  elements.
// n must be a multiple of QK=32.
// ─────────────────────────────────────────────────────────────────────────────
static constexpr int QK = 32;

static float dot_q4_0(const uint8_t* __restrict w,
                      const float*   __restrict x, int n) noexcept {
#if HAVE_AVX2
    // AVX2 path: 32 weights per block, process lo+hi nibbles with bias-8
    // Block: 2B fp16 scale + 16B qs (4-bit pairs), stride=18B
    __m256 acc0 = _mm256_setzero_ps();
    __m256 acc1 = _mm256_setzero_ps();
    __m256 acc2 = _mm256_setzero_ps();
    __m256 acc3 = _mm256_setzero_ps();
    const __m128i nibMask = _mm_set1_epi8(0x0F);
    const __m128i bias8   = _mm_set1_epi8(8);
    int nb = n / QK;
    for (int j = 0; j < nb; ++j) {
        const __m256 vd = _mm256_set1_ps(fp16_to_f32(*reinterpret_cast<const uint16_t*>(w)));
        const __m128i pk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(w + 2));
        const float* xi = x + j * QK;
        // lo nibbles: w[0..15]; hi nibbles: w[16..31]
        __m128i lo = _mm_sub_epi8(_mm_and_si128(pk, nibMask), bias8);
        __m128i hi = _mm_sub_epi8(_mm_and_si128(_mm_srli_epi16(pk, 4), nibMask), bias8);
        // extend 16×int8 → 8+8×int32, convert to float, scale by d, dot with x
        acc0 = _mm256_fmadd_ps(
            _mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_cvtepi8_epi32(lo)),          vd),
            _mm256_loadu_ps(xi     ), acc0);
        acc1 = _mm256_fmadd_ps(
            _mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_cvtepi8_epi32(_mm_bsrli_si128(lo, 8))), vd),
            _mm256_loadu_ps(xi +  8), acc1);
        acc2 = _mm256_fmadd_ps(
            _mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_cvtepi8_epi32(hi)),          vd),
            _mm256_loadu_ps(xi + 16), acc2);
        acc3 = _mm256_fmadd_ps(
            _mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_cvtepi8_epi32(_mm_bsrli_si128(hi, 8))), vd),
            _mm256_loadu_ps(xi + 24), acc3);
        w += 18;
    }
    __m256 sum4 = _mm256_add_ps(_mm256_add_ps(acc0, acc1), _mm256_add_ps(acc2, acc3));
    __m128 hi128 = _mm256_extractf128_ps(sum4, 1);
    __m128 lo128 = _mm256_castps256_ps128(sum4);
    lo128 = _mm_add_ps(lo128, hi128);
    lo128 = _mm_hadd_ps(lo128, lo128);
    lo128 = _mm_hadd_ps(lo128, lo128);
    return _mm_cvtss_f32(lo128);
#else
    float sum = 0.f;
    int nb = n / QK;
    for (int j = 0; j < nb; ++j) {
        float d = fp16_to_f32(*reinterpret_cast<const uint16_t*>(w));
        const uint8_t* qs = w + 2;
        float s = 0.f;
        for (int k = 0; k < QK / 2; ++k) {
            s += (float)((int)(qs[k] & 0x0F) - 8) * x[j * QK + k];
            s += (float)((int)(qs[k] >>    4) - 8) * x[j * QK + k + QK / 2];
        }
        sum += d * s;
        w += 18;
    }
    return sum;
#endif
}

static float dot_q4_1(const uint8_t* __restrict w,
                      const float*   __restrict x, int n) noexcept {
    // Block layout: 2 bytes d (f16), 2 bytes m (f16), 16 bytes qs
    float sum = 0.f;
    int nb = n / QK;
    for (int j = 0; j < nb; ++j) {
        float d = fp16_to_f32(*reinterpret_cast<const uint16_t*>(w));
        float m = fp16_to_f32(*reinterpret_cast<const uint16_t*>(w + 2));
        const uint8_t* qs = w + 4;
        float s = 0.f, sx = 0.f;
        for (int k = 0; k < QK / 2; ++k) {
            int v0 = (int)(qs[k] & 0x0F); // 0..15
            int v1 = (int)(qs[k] >>    4);
            float xlo = x[j * QK + k];
            float xhi = x[j * QK + k + QK / 2];
            s  += (float)v0 * xlo + (float)v1 * xhi;
            sx += xlo + xhi;
        }
        sum += d * s + m * sx;
        w += 20;
    }
    return sum;
}

static float dot_q5_0(const uint8_t* __restrict w,
                      const float*   __restrict x, int n) noexcept {
    // Block layout: 2 bytes d (f16), 4 bytes qh (high bits), 16 bytes qs (low 4 bits each)
    float sum = 0.f;
    int nb = n / QK;
    for (int j = 0; j < nb; ++j) {
        float d = fp16_to_f32(*reinterpret_cast<const uint16_t*>(w));
        uint32_t qh; memcpy(&qh, w + 2, 4);
        const uint8_t* qs = w + 6;
        float s = 0.f;
        for (int k = 0; k < QK / 2; ++k) {
            // bits k and k+16 of qh are the 5th bits
            int bit0 = (int)((qh >> (    k)) & 1u);
            int bit1 = (int)((qh >> (k + 16)) & 1u);
            int v0 = (int)(((qs[k] & 0x0F) | (bit0 << 4))) - 16;
            int v1 = (int)(((qs[k] >>    4) | (bit1 << 4))) - 16;
            s += (float)v0 * x[j * QK + k];
            s += (float)v1 * x[j * QK + k + QK / 2];
        }
        sum += d * s;
        w += 22;
    }
    return sum;
}

static float dot_q5_1(const uint8_t* __restrict w,
                      const float*   __restrict x, int n) noexcept {
    // Block layout: 2 bytes d, 2 bytes m, 4 bytes qh, 16 bytes qs
    float sum = 0.f;
    int nb = n / QK;
    for (int j = 0; j < nb; ++j) {
        float d = fp16_to_f32(*reinterpret_cast<const uint16_t*>(w));
        float m = fp16_to_f32(*reinterpret_cast<const uint16_t*>(w + 2));
        uint32_t qh; memcpy(&qh, w + 4, 4);
        const uint8_t* qs = w + 8;
        float s = 0.f, sx = 0.f;
        for (int k = 0; k < QK / 2; ++k) {
            int bit0 = (int)((qh >> (    k)) & 1u);
            int bit1 = (int)((qh >> (k + 16)) & 1u);
            int v0 = (int)((qs[k] & 0x0F) | (bit0 << 4)); // 0..31
            int v1 = (int)((qs[k] >>    4) | (bit1 << 4));
            float xlo = x[j * QK + k];
            float xhi = x[j * QK + k + QK / 2];
            s  += (float)v0 * xlo + (float)v1 * xhi;
            sx += xlo + xhi;
        }
        sum += d * s + m * sx;
        w += 24;
    }
    return sum;
}

static float dot_q8_0(const uint8_t* __restrict w,
                      const float*   __restrict x, int n) noexcept {
    // Block layout: 2 bytes d (f16), 32 bytes i8 quants
    float sum = 0.f;
    int nb = n / QK;
    for (int j = 0; j < nb; ++j) {
        float d = fp16_to_f32(*reinterpret_cast<const uint16_t*>(w));
        const int8_t* qs = reinterpret_cast<const int8_t*>(w + 2);
        float s = 0.f;
#if HAVE_AVX2
        // AVX2 path: process 8 floats at a time
        __m256 acc = _mm256_setzero_ps();
        for (int k = 0; k < QK; k += 8) {
            // load 8 int8 -> convert to float via __m256i
            __m128i vi8 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(qs + k));
            __m256i vi32 = _mm256_cvtepi8_epi32(vi8);
            __m256 vf = _mm256_cvtepi32_ps(vi32);
            __m256 vx = _mm256_loadu_ps(x + j * QK + k);
            acc = _mm256_fmadd_ps(vf, vx, acc);
        }
        // horizontal sum
        __m128 hi = _mm256_extractf128_ps(acc, 1);
        __m128 lo = _mm256_castps256_ps128(acc);
        lo = _mm_add_ps(lo, hi);
        lo = _mm_hadd_ps(lo, lo);
        lo = _mm_hadd_ps(lo, lo);
        s = _mm_cvtss_f32(lo);
#else
        for (int k = 0; k < QK; ++k)
            s += (float)qs[k] * x[j * QK + k];
#endif
        sum += d * s;
        w += 34;
    }
    return sum;
}

static float dot_f16(const uint8_t* __restrict w,
                     const float*   __restrict x, int n) noexcept {
    float sum = 0.f;
    const uint16_t* wh = reinterpret_cast<const uint16_t*>(w);
    for (int i = 0; i < n; ++i)
        sum += fp16_to_f32(wh[i]) * x[i];
    return sum;
}

static float dot_f32(const uint8_t* __restrict w,
                     const float*   __restrict x, int n) noexcept {
    const float* wf = reinterpret_cast<const float*>(w);
    float sum = 0.f;
#if HAVE_AVX2
    __m256 acc = _mm256_setzero_ps();
    int i = 0;
    for (; i + 8 <= n; i += 8)
        acc = _mm256_fmadd_ps(_mm256_loadu_ps(wf + i),
                              _mm256_loadu_ps(x + i), acc);
    __m128 hi = _mm256_extractf128_ps(acc, 1);
    __m128 lo = _mm256_castps256_ps128(acc);
    lo = _mm_add_ps(lo, hi);
    lo = _mm_hadd_ps(lo, lo);
    lo = _mm_hadd_ps(lo, lo);
    sum = _mm_cvtss_f32(lo);
    for (; i < n; ++i) sum += wf[i] * x[i];
#else
    for (int i = 0; i < n; ++i) sum += wf[i] * x[i];
#endif
    return sum;
}

// Dispatch by type for one row of a weight matrix
static float dot_row(const Tensor& t, const float* __restrict x, int row) noexcept {
    int n = t.ne(0);
    const uint8_t* row_ptr = t.data.data() + (size_t)row * t.row_bytes();
    switch (t.type) {
        case GGMLType::F32:  return dot_f32 (row_ptr, x, n);
        case GGMLType::F16:  return dot_f16 (row_ptr, x, n);
        case GGMLType::Q4_0: return dot_q4_0(row_ptr, x, n);
        case GGMLType::Q4_1: return dot_q4_1(row_ptr, x, n);
        case GGMLType::Q5_0: return dot_q5_0(row_ptr, x, n);
        case GGMLType::Q5_1: return dot_q5_1(row_ptr, x, n);
        case GGMLType::Q8_0: return dot_q8_0(row_ptr, x, n);
        default: return 0.f;
    }
}

// Full matrix multiply  y[d] = W(d x n) · x[n]
static void matmul(float* __restrict y,
                   const Tensor& W,
                   const float* __restrict x,
                   int d) noexcept {
    for (int i = 0; i < d; ++i)
        y[i] = dot_row(W, x, i);
}

// ---------------------------------------------------------------------------
// Persistent worker pool — eliminates OS thread spawn/join per matmul_par call.
// On Phi-3-mini Q4_0 (32 layers × 7 matmuls × 8 threads = 1792 spawns/token),
// each Windows std::thread spawn costs ~50-100µs → ~90-180ms/token overhead.
// The pool keeps workers alive between calls, cutting this to ~2µs wakeup.
// ---------------------------------------------------------------------------
struct MatmulPool {
    explicit MatmulPool(int nw) {
        workers.reserve((size_t)nw);
        for (int i = 0; i < nw; ++i)
            workers.emplace_back([this]{ worker_fn(); });
    }
    ~MatmulPool() {
        { std::lock_guard<std::mutex> lk(mtx); shutdown = true; }
        work_cv.notify_all();
        for (auto& t : workers) t.join();
    }

    // Submit tasks and block until all complete.
    void run(std::function<void(int)> fn, int count) {
        if (count <= 0) return;
        {
            std::lock_guard<std::mutex> lk(mtx);
            pending = count;
            task_fn = std::move(fn);
            task_count = count;
            next_task = 0;
        }
        work_cv.notify_all();
        std::unique_lock<std::mutex> lk(mtx);
        idle_cv.wait(lk, [this]{ return pending == 0; });
    }

private:
    std::vector<std::thread> workers;
    std::mutex               mtx;
    std::condition_variable  work_cv, idle_cv;
    std::function<void(int)> task_fn;
    int task_count = 0;
    int next_task  = 0;
    int pending    = 0;
    bool shutdown  = false;

    void worker_fn() {
        while (true) {
            int idx = -1;
            {
                std::unique_lock<std::mutex> lk(mtx);
                work_cv.wait(lk, [this]{ return shutdown || next_task < task_count; });
                if (shutdown && next_task >= task_count) return;
                idx = next_task++;
            }
            task_fn(idx);
            {
                std::lock_guard<std::mutex> lk(mtx);
                if (--pending == 0)
                    idle_cv.notify_one();
            }
        }
    }
};

static MatmulPool* g_matmul_pool   = nullptr;
static int         g_pool_nthreads = 0;

static MatmulPool& get_matmul_pool(int n) {
    if (!g_matmul_pool || g_pool_nthreads != n) {
        delete g_matmul_pool;
        g_matmul_pool   = new MatmulPool(n);
        g_pool_nthreads = n;
    }
    return *g_matmul_pool;
}

// ---------------------------------------------------------------------------
// Parallel matmul using the persistent pool.
// Partition output rows across workers; each worker calls dot_row per row.
// ---------------------------------------------------------------------------
static void matmul_par(float* __restrict y,
                       const Tensor& W,
                       const float* __restrict x,
                       int d, int n_threads) {
    if (n_threads <= 1 || d < 64) { matmul(y, W, x, d); return; }
    // clamp workers to actual row count
    int nw = std::min(n_threads, d);
    int chunk = (d + nw - 1) / nw;
    MatmulPool& pool = get_matmul_pool(nw);
    pool.run([&](int t) {
        int beg = t * chunk;
        int end = std::min(beg + chunk, d);
        for (int i = beg; i < end; ++i)
            y[i] = dot_row(W, x, i);
    }, nw);
}

// ─────────────────────────────────────────────────────────────────────────────
// Transformer ops
// ─────────────────────────────────────────────────────────────────────────────
static inline void rmsnorm(float* __restrict o,
                           const float* __restrict x,
                           const float* __restrict w,
                           int n, float eps) noexcept {
    float ss = 0.f;
    for (int i = 0; i < n; ++i) ss += x[i] * x[i];
    float scale = 1.f / sqrtf(ss / (float)n + eps);
    for (int i = 0; i < n; ++i) o[i] = w[i] * (x[i] * scale);
}

static inline void softmax(float* __restrict x, int n) noexcept {
    float maxv = x[0];
    for (int i = 1; i < n; ++i) if (x[i] > maxv) maxv = x[i];
    float sum = 0.f;
    for (int i = 0; i < n; ++i) { x[i] = expf(x[i] - maxv); sum += x[i]; }
    float inv = 1.f / sum;
    for (int i = 0; i < n; ++i) x[i] *= inv;
}

static inline void rope_inplace(float* q, float* k,
                                int pos, int head_dim,
                                float theta,
                                int n_q_heads, int n_kv_heads) noexcept {
    auto apply = [&](float* h, int n_heads_local) {
        for (int hi = 0; hi < n_heads_local; ++hi) {
            float* hptr = h + hi * head_dim;
            for (int i = 0; i < head_dim; i += 2) {
                float freq = 1.f / powf(theta, (float)i / (float)head_dim);
                float angle = (float)pos * freq;
                float c = cosf(angle), s = sinf(angle);
                float a = hptr[i], b = hptr[i + 1];
                hptr[i]     = a * c - b * s;
                hptr[i + 1] = a * s + b * c;
            }
        }
    };
    apply(q, n_q_heads);
    apply(k, n_kv_heads);
}

static inline void silu_inplace(float* x, int n) noexcept {
    for (int i = 0; i < n; ++i)
        x[i] = x[i] / (1.f + expf(-x[i]));
}

static inline void accum(float* __restrict o,
                         const float* __restrict a, int n) noexcept {
    for (int i = 0; i < n; ++i) o[i] += a[i];
}

// ─────────────────────────────────────────────────────────────────────────────
// Tokenizer (greedy longest-match + BPE merges)
// ─────────────────────────────────────────────────────────────────────────────
struct Tokenizer {
    std::vector<std::string>            vocab;
    std::unordered_map<std::string,int> id_map;
    std::vector<std::pair<int,int>>     merges;   // (left_id, right_id) -> merged_id
    std::unordered_map<uint64_t,int>    merge_map; // key=(left<<32|right)

    int bos        = -1;
    int eos        = -1;
    int unk        = 0;
    int fim_prefix = -1;
    int fim_suffix = -1;
    int fim_middle = -1;
    int nl_token   = -1;

    bool load(const GGUFReader& g) {
        vocab = g.get_str_arr("tokenizer.ggml.tokens");
        if (vocab.empty()) return false;
        for (int i = 0; i < (int)vocab.size(); ++i)
            id_map[vocab[i]] = i;

        // Build merge table
        auto mstrs = g.get_str_arr("tokenizer.ggml.merges");
        for (auto& ms : mstrs) {
            size_t sp = ms.find(' ');
            if (sp == std::string::npos) continue;
            std::string a = ms.substr(0, sp);
            std::string b = ms.substr(sp + 1);
            std::string ab = a + b;
            auto ia = id_map.find(a);
            auto ib = id_map.find(b);
            auto iab = id_map.find(ab);
            if (ia == id_map.end() || ib == id_map.end() || iab == id_map.end()) continue;
            uint64_t key = ((uint64_t)(uint32_t)ia->second << 32) | (uint32_t)ib->second;
            merge_map[key] = iab->second;
        }

        bos = g.get_i32("tokenizer.ggml.bos_token_id", -1);
        eos = g.get_i32("tokenizer.ggml.eos_token_id", -1);
        unk = g.get_i32("tokenizer.ggml.unknown_token_id", 0);

        // FIM tokens – try both naming conventions
        struct { const char* a; const char* b; int* dst; } fim_pairs[] = {
            {"<fim_prefix>",  "<|fim_prefix|>",  &fim_prefix},
            {"<fim_suffix>",  "<|fim_suffix|>",  &fim_suffix},
            {"<fim_middle>",  "<|fim_middle|>",  &fim_middle},
        };
        for (auto& fp : fim_pairs) {
            *fp.dst = -1;
            auto it = id_map.find(fp.a);
            if (it != id_map.end()) { *fp.dst = it->second; continue; }
            it = id_map.find(fp.b);
            if (it != id_map.end()) { *fp.dst = it->second; }
        }
        // Newline token
        {
            auto it = id_map.find("\n");
            if (it != id_map.end()) nl_token = it->second;
        }
        return true;
    }

    // Greedy longest-match byte tokenisation
    std::vector<int> encode_greedy(const std::string& text) const {
        std::vector<int> out;
        size_t i = 0;
        while (i < text.size()) {
            size_t best_len = 0;
            int best_id = unk;
            // try lengths from longest possible down to 1
            size_t max_try = std::min(text.size() - i, (size_t)64);
            for (size_t l = max_try; l >= 1; --l) {
                auto it = id_map.find(text.substr(i, l));
                if (it != id_map.end()) {
                    best_len = l;
                    best_id  = it->second;
                    break;
                }
            }
            out.push_back(best_id);
            i += best_len ? best_len : 1;
        }
        return out;
    }

    // BPE merge pass (O(n * merges), fast enough for short prompts)
    void apply_merges(std::vector<int>& ids) const {
        if (merge_map.empty()) return;
        bool changed;
        do {
            changed = false;
            for (size_t i = 0; i + 1 < ids.size(); ++i) {
                uint64_t key = ((uint64_t)(uint32_t)ids[i] << 32) | (uint32_t)ids[i + 1];
                auto it = merge_map.find(key);
                if (it != merge_map.end()) {
                    ids[i] = it->second;
                    ids.erase(ids.begin() + (ptrdiff_t)i + 1);
                    changed = true;
                    // restart from previous position to catch cascading merges
                    if (i > 0) --i;
                }
            }
        } while (changed);
    }

    std::vector<int> encode(const std::string& text, bool add_bos = true) const {
        std::vector<int> ids = encode_greedy(text);
        apply_merges(ids);
        if (add_bos && bos >= 0) ids.insert(ids.begin(), bos);
        return ids;
    }

    std::string decode(const std::vector<int>& ids) const {
        std::string out;
        for (int id : ids) {
            if (id < 0 || id >= (int)vocab.size()) continue;
            const std::string& tok = vocab[id];
            // handle ▁ (U+2581 = 0xE2 0x96 0x81) as space
            if (tok.size() >= 3 &&
                (uint8_t)tok[0] == 0xE2 && (uint8_t)tok[1] == 0x96 && (uint8_t)tok[2] == 0x81)
                out += ' ' + tok.substr(3);
            else
                out += tok;
        }
        return out;
    }

    // Build FIM prompt:  <fim_prefix> prefix <fim_suffix> suffix <fim_middle>
    std::vector<int> encode_fim(const std::string& prefix,
                                const std::string& suffix) const {
        std::vector<int> ids;
        if (fim_prefix >= 0) ids.push_back(fim_prefix);
        auto p = encode(prefix, false); ids.insert(ids.end(), p.begin(), p.end());
        if (fim_suffix >= 0) ids.push_back(fim_suffix);
        auto s = encode(suffix, false); ids.insert(ids.end(), s.begin(), s.end());
        if (fim_middle >= 0) ids.push_back(fim_middle);
        return ids;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Transformer model state
// ─────────────────────────────────────────────────────────────────────────────
struct Config {
    int   dim         = 0;
    int   hidden_dim  = 0;
    int   n_layers    = 0;
    int   n_heads     = 0;
    int   n_kv_heads  = 0;
    int   head_dim    = 0;
    int   vocab_size  = 0;
    int   max_seq_len = 2048;
    float norm_eps    = 1e-5f;
    float rope_theta  = 10000.f;
};

struct Transformer {
    Config cfg;
    int    n_threads = 1;

    // Model weights (borrowed from GGUF tensors)
    Tensor tok_embd;
    Tensor output_norm;
    Tensor output;
    std::vector<Tensor> attn_norm, attn_q, attn_k, attn_v, attn_o;
    std::vector<Tensor> ffn_norm, ffn_gate, ffn_up, ffn_down;

    // Runtime buffers
    std::vector<float> kv_cache;  // [2, n_layers, n_kv_heads, max_seq_len, head_dim]
    std::vector<float> x, xb, xb2;
    std::vector<float> q, k, v;
    std::vector<float> att_score; // [n_heads, max_seq_len]
    std::vector<float> att_out;   // [n_heads * head_dim]
    std::vector<float> hb, hb2;
    std::vector<float> logits;

    bool init(GGUFReader& g, int max_ctx, int threads) {
        cfg.dim         = g.get_i32("llama.embedding_length");
        cfg.hidden_dim  = g.get_i32("llama.feed_forward_length");
        cfg.n_layers    = g.get_i32("llama.block_count");
        cfg.n_heads     = g.get_i32("llama.attention.head_count");
        cfg.n_kv_heads  = g.get_i32("llama.attention.head_count_kv", cfg.n_heads);
        cfg.vocab_size  = g.get_i32("llama.vocab_size");
        cfg.max_seq_len = std::min(g.get_i32("llama.context_length", 2048), max_ctx);
        cfg.norm_eps    = g.get_f32("llama.attention.layer_norm_rms_epsilon", 1e-5f);
        cfg.rope_theta  = g.get_f32("llama.rope.freq_base", 10000.f);
        cfg.head_dim    = cfg.dim / cfg.n_heads;
        n_threads       = threads;

        if (cfg.dim <= 0 || cfg.n_layers <= 0 || cfg.n_heads <= 0 || cfg.vocab_size <= 0) {
            fprintf(stderr, "Config parse failed\n"); return false;
        }

        auto get = [&](const char* name) -> Tensor {
            Tensor* tp = g.find(name);
            return tp ? *tp : Tensor{};
        };
        tok_embd    = get("token_embd.weight");
        output_norm = get("output_norm.weight");
        output      = get("output.weight");

        if (tok_embd.empty()) { fprintf(stderr, "Missing token_embd.weight\n"); return false; }
        if (output_norm.empty()) { fprintf(stderr, "Missing output_norm.weight\n"); return false; }
        // output.weight may be absent (tied to embedding)
        if (output.empty()) output = tok_embd;

        attn_norm.resize(cfg.n_layers); attn_q.resize(cfg.n_layers);
        attn_k.resize(cfg.n_layers);    attn_v.resize(cfg.n_layers);
        attn_o.resize(cfg.n_layers);    ffn_norm.resize(cfg.n_layers);
        ffn_gate.resize(cfg.n_layers);  ffn_up.resize(cfg.n_layers);
        ffn_down.resize(cfg.n_layers);

        char buf[128];
        for (int l = 0; l < cfg.n_layers; ++l) {
#define GET_L(field, fmt) snprintf(buf,sizeof(buf),fmt,l); field[l]=get(buf)
            GET_L(attn_norm, "blk.%d.attn_norm.weight");
            GET_L(attn_q,    "blk.%d.attn_q.weight");
            GET_L(attn_k,    "blk.%d.attn_k.weight");
            GET_L(attn_v,    "blk.%d.attn_v.weight");
            GET_L(attn_o,    "blk.%d.attn_output.weight");
            GET_L(ffn_norm,  "blk.%d.ffn_norm.weight");
            GET_L(ffn_gate,  "blk.%d.ffn_gate.weight");
            GET_L(ffn_up,    "blk.%d.ffn_up.weight");
            GET_L(ffn_down,  "blk.%d.ffn_down.weight");
#undef GET_L
        }

        // Allocate KV cache: K and V each [n_layers][n_kv_heads][max_seq_len][head_dim]
        size_t kv_per_layer = (size_t)cfg.n_kv_heads * cfg.max_seq_len * cfg.head_dim;
        kv_cache.assign(2 * (size_t)cfg.n_layers * kv_per_layer, 0.f);

        x.resize((size_t)cfg.dim);
        xb.resize((size_t)cfg.dim);
        xb2.resize((size_t)cfg.dim);
        q.resize((size_t)cfg.n_heads    * cfg.head_dim);
        k.resize((size_t)cfg.n_kv_heads * cfg.head_dim);
        v.resize((size_t)cfg.n_kv_heads * cfg.head_dim);
        att_score.resize((size_t)cfg.n_heads * cfg.max_seq_len);
        att_out.resize((size_t)cfg.n_heads * cfg.head_dim);
        hb.resize((size_t)cfg.hidden_dim);
        hb2.resize((size_t)cfg.hidden_dim);
        logits.resize((size_t)cfg.vocab_size);

        printf("[autocomplete] Model: dim=%d hidden=%d layers=%d heads=%d kv_heads=%d "
               "vocab=%d ctx=%d eps=%.2e rope_theta=%.0f\n",
               cfg.dim, cfg.hidden_dim, cfg.n_layers, cfg.n_heads, cfg.n_kv_heads,
               cfg.vocab_size, cfg.max_seq_len, (double)cfg.norm_eps, (double)cfg.rope_theta);
        return true;
    }

    void reset_cache() {
        std::fill(kv_cache.begin(), kv_cache.end(), 0.f);
    }

    // Embed token (handles F32, F16, and quantised embedding tables)
    void embed(int token) {
        if (token < 0 || token >= cfg.vocab_size) {
            std::fill(x.begin(), x.end(), 0.f); return;
        }
        if (tok_embd.type == GGMLType::F32) {
            const float* src = reinterpret_cast<const float*>(tok_embd.data.data())
                               + (size_t)token * cfg.dim;
            memcpy(x.data(), src, (size_t)cfg.dim * sizeof(float));
        } else if (tok_embd.type == GGMLType::F16) {
            const uint16_t* src = reinterpret_cast<const uint16_t*>(tok_embd.data.data())
                                  + (size_t)token * cfg.dim;
            for (int i = 0; i < cfg.dim; ++i)
                x[i] = fp16_to_f32(src[i]);
        } else {
            // Quantised embedding: use dot_row as dequant row decoder
            // Each "row" in the embedding table is one token's embedding.
            // dot_row computes W[row] · x, but we want to dequant W[row].
            // Dequant by using a unit vector that selects one element at a time
            // would be O(dim^2) – too slow. Instead, dequant a full row directly.
            // For Q4_0 / Q8_0 we iterate blocks and write into x.
            const uint8_t* row_ptr = tok_embd.data.data()
                                     + (size_t)token * tok_embd.row_bytes();
            int n = cfg.dim;
            int nb = n / QK;
            if (tok_embd.type == GGMLType::Q4_0) {
                for (int j = 0; j < nb; ++j) {
                    float d = fp16_to_f32(*reinterpret_cast<const uint16_t*>(row_ptr));
                    const uint8_t* qs = row_ptr + 2;
                    for (int k = 0; k < QK / 2; ++k) {
                        x[j * QK + k]           = (float)((int)(qs[k] & 0x0F) - 8) * d;
                        x[j * QK + k + QK / 2]  = (float)((int)(qs[k] >>    4) - 8) * d;
                    }
                    row_ptr += 18;
                }
            } else if (tok_embd.type == GGMLType::Q8_0) {
                for (int j = 0; j < nb; ++j) {
                    float d = fp16_to_f32(*reinterpret_cast<const uint16_t*>(row_ptr));
                    const int8_t* qs = reinterpret_cast<const int8_t*>(row_ptr + 2);
                    for (int k = 0; k < QK; ++k)
                        x[j * QK + k] = (float)qs[k] * d;
                    row_ptr += 34;
                }
            } else {
                // Fallback: zero (unsupported quantised embedding type)
                std::fill(x.begin(), x.end(), 0.f);
            }
        }
    }

    // Single forward pass for token at sequence position pos
    // Leaves result in logits[].
    void forward(int token, int pos) {
        embed(token);

        const float inv_sqrt_hd = 1.f / sqrtf((float)cfg.head_dim);
        size_t kv_per_layer = (size_t)cfg.n_kv_heads * cfg.max_seq_len * cfg.head_dim;

        for (int l = 0; l < cfg.n_layers; ++l) {
            // ── Attention pre-norm ─────────────────────────────────────────
            if (attn_norm[l].empty()) continue;
            rmsnorm(xb.data(), x.data(),
                    reinterpret_cast<const float*>(attn_norm[l].data.data()),
                    cfg.dim, cfg.norm_eps);

            // ── Q / K / V projections ──────────────────────────────────────
            matmul_par(q.data(), attn_q[l], xb.data(), cfg.n_heads    * cfg.head_dim, n_threads);
            matmul_par(k.data(), attn_k[l], xb.data(), cfg.n_kv_heads * cfg.head_dim, n_threads);
            matmul_par(v.data(), attn_v[l], xb.data(), cfg.n_kv_heads * cfg.head_dim, n_threads);

            // ── RoPE ──────────────────────────────────────────────────────
            rope_inplace(q.data(), k.data(),
                         pos, cfg.head_dim, cfg.rope_theta,
                         cfg.n_heads, cfg.n_kv_heads);

            // ── Store K,V into cache ───────────────────────────────────────
            float* k_layer = kv_cache.data()
                             + (size_t)l * kv_per_layer;
            float* v_layer = kv_cache.data()
                             + ((size_t)cfg.n_layers + l) * kv_per_layer;

            for (int h = 0; h < cfg.n_kv_heads; ++h) {
                float* kh_slot = k_layer + ((size_t)h * cfg.max_seq_len + pos) * cfg.head_dim;
                float* vh_slot = v_layer + ((size_t)h * cfg.max_seq_len + pos) * cfg.head_dim;
                memcpy(kh_slot, k.data() + h * cfg.head_dim, (size_t)cfg.head_dim * sizeof(float));
                memcpy(vh_slot, v.data() + h * cfg.head_dim, (size_t)cfg.head_dim * sizeof(float));
            }

            // ── Multi-head attention ──────────────────────────────────────
            std::fill(att_out.begin(), att_out.end(), 0.f);

            for (int h = 0; h < cfg.n_heads; ++h) {
                int kvh = (cfg.n_kv_heads == cfg.n_heads) ? h : h / (cfg.n_heads / cfg.n_kv_heads);
                const float* qh  = q.data() + h * cfg.head_dim;
                float* score_row = att_score.data() + h * cfg.max_seq_len;

                // Dot Q·K for all cached positions
                for (int p = 0; p <= pos; ++p) {
                    const float* kh = k_layer + ((size_t)kvh * cfg.max_seq_len + p) * cfg.head_dim;
                    float s = 0.f;
                    for (int i = 0; i < cfg.head_dim; ++i) s += qh[i] * kh[i];
                    score_row[p] = s * inv_sqrt_hd;
                }

                softmax(score_row, pos + 1);

                float* oh = att_out.data() + h * cfg.head_dim;
                for (int p = 0; p <= pos; ++p) {
                    const float* vh = v_layer + ((size_t)kvh * cfg.max_seq_len + p) * cfg.head_dim;
                    float w = score_row[p];
                    for (int i = 0; i < cfg.head_dim; ++i) oh[i] += w * vh[i];
                }
            }

            // ── Output projection + residual ──────────────────────────────
            matmul_par(xb2.data(), attn_o[l], att_out.data(), cfg.dim, n_threads);
            accum(x.data(), xb2.data(), cfg.dim);

            // ── FFN (SwiGLU) ───────────────────────────────────────────────
            if (ffn_norm[l].empty()) continue;
            rmsnorm(xb.data(), x.data(),
                    reinterpret_cast<const float*>(ffn_norm[l].data.data()),
                    cfg.dim, cfg.norm_eps);

            matmul_par(hb.data(),  ffn_gate[l], xb.data(), cfg.hidden_dim, n_threads);
            matmul_par(hb2.data(), ffn_up[l],   xb.data(), cfg.hidden_dim, n_threads);
            silu_inplace(hb.data(), cfg.hidden_dim);
            for (int i = 0; i < cfg.hidden_dim; ++i) hb[i] *= hb2[i];
            matmul_par(xb2.data(), ffn_down[l], hb.data(), cfg.dim, n_threads);
            accum(x.data(), xb2.data(), cfg.dim);
        }

        // ── Final norm + lm_head ─────────────────────────────────────────
        rmsnorm(x.data(), x.data(),
                reinterpret_cast<const float*>(output_norm.data.data()),
                cfg.dim, cfg.norm_eps);
        matmul_par(logits.data(), output, x.data(), cfg.vocab_size, n_threads);
    }

    // Argmax over logits (greedy)
    int greedy_token() const {
        int best = 0;
        float best_v = logits[0];
        for (int i = 1; i < cfg.vocab_size; ++i)
            if (logits[i] > best_v) { best_v = logits[i]; best = i; }
        return best;
    }

    // Log-prob of a specific token (stable softmax)
    float log_prob(int token) const {
        float maxv = *std::max_element(logits.begin(), logits.end());
        float sum = 0.f;
        for (float v : logits) sum += expf(v - maxv);
        return (logits[token] - maxv) - logf(sum);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Inference engine (wraps Tokenizer + Transformer, handles FIM + sampling)
// ─────────────────────────────────────────────────────────────────────────────
struct Engine {
    GGUFReader  gguf;
    Tokenizer   tok;
    Transformer tf;
    std::mutex  mu; // serialise concurrent requests

    bool load(const char* path, int max_ctx, int threads) {
        printf("[autocomplete] Loading %s …\n", path);
        if (!gguf.load(path)) return false;
        if (!tok.load(gguf))  { fprintf(stderr, "Tokenizer load failed\n"); return false; }
        if (!tf.init(gguf, max_ctx, threads)) return false;

        // Print quantisation summary
        size_t total_bytes = 0;
        for (auto& t : gguf.tensors) {
            if (!t.data.empty()) total_bytes += t.data.size();
        }
        printf("[autocomplete] Loaded %.1f MB  vocab=%zu  threads=%d\n",
               total_bytes / 1048576.0, tok.vocab.size(), threads);
        return true;
    }

    struct Request {
        std::vector<int> prompt_ids;  // already-encoded tokens
        int              max_new     = 8;
        int              stop_token  = -1;
    };

    struct Response {
        std::vector<int>   tokens;
        std::vector<float> logprobs;
    };

    Response generate(const Request& req) {
        std::lock_guard<std::mutex> lg(mu);
        Response resp;

        tf.reset_cache();
        const auto& ids = req.prompt_ids;
        if (ids.empty()) return resp;

        // Prefill: run all prompt tokens
        for (int i = 0; i < (int)ids.size(); ++i)
            tf.forward(ids[i], i);

        // Speculative decode: produce up to max_new tokens
        int pos = (int)ids.size();
        for (int step = 0; step < req.max_new && pos < tf.cfg.max_seq_len; ++step) {
            int next = tf.greedy_token();
            float lp = tf.log_prob(next);
            resp.tokens.push_back(next);
            resp.logprobs.push_back(lp);
            if (next == req.stop_token ||
                next == tok.eos ||
                next == tok.nl_token) break; // end of line / EOS → stop
            tf.forward(next, pos++);
        }
        return resp;
    }

    // Convenience: text-in, text-out (for testing)
    std::string complete_text(const std::string& prefix,
                              const std::string& suffix,
                              int max_new = 32) {
        Request req;
        if (!suffix.empty() && tok.fim_prefix >= 0)
            req.prompt_ids = tok.encode_fim(prefix, suffix);
        else
            req.prompt_ids = tok.encode(prefix, true);
        req.max_new = max_new;
        auto resp = generate(req);
        return tok.decode(resp.tokens);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Shared-memory service loop
// ─────────────────────────────────────────────────────────────────────────────
static volatile bool g_running = true;

#ifdef _WIN32
static BOOL WINAPI console_ctrl(DWORD) { g_running = false; return TRUE; }
#else
#include <signal.h>
static void sig_handler(int) { g_running = false; }
#endif

static void serve(Engine& engine, SharedExchange* ex) {
    printf("[autocomplete] Serving on shared memory. Waiting for requests…\n");
    while (g_running) {
        uint32_t gate = ex->gate.load(std::memory_order_acquire);
        if (gate == 3) break; // shutdown signal from IDE
        if (gate != 1) {
            std::this_thread::sleep_for(std::chrono::microseconds(200));
            continue;
        }

        // ── Decode request ────────────────────────────────────────────────
        Engine::Request req;
        uint32_t n_pre = std::min(ex->n_prefix_tokens, (uint32_t)512);
        uint32_t n_suf = std::min(ex->n_suffix_tokens, (uint32_t)128);
        req.max_new    = std::min(std::max(ex->max_new_tokens, 1u), 64u);

        if (ex->fim_mode == 'p' && n_suf > 0 && engine.tok.fim_prefix >= 0) {
            // FIM mode: build fim prompt from raw token ids
            if (engine.tok.fim_prefix >= 0) req.prompt_ids.push_back(engine.tok.fim_prefix);
            for (uint32_t i = 0; i < n_pre; ++i) req.prompt_ids.push_back((int)ex->prefix_tokens[i]);
            if (engine.tok.fim_suffix >= 0) req.prompt_ids.push_back(engine.tok.fim_suffix);
            for (uint32_t i = 0; i < n_suf; ++i) req.prompt_ids.push_back((int)ex->suffix_tokens[i]);
            if (engine.tok.fim_middle >= 0) req.prompt_ids.push_back(engine.tok.fim_middle);
        } else {
            for (uint32_t i = 0; i < n_pre; ++i) req.prompt_ids.push_back((int)ex->prefix_tokens[i]);
        }

        // ── Run inference ─────────────────────────────────────────────────
        auto t0 = std::chrono::high_resolution_clock::now();
        auto resp = engine.generate(req);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double,std::milli>(t1 - t0).count();

        // ── Write response ────────────────────────────────────────────────
        uint32_t n_out = (uint32_t)std::min(resp.tokens.size(), (size_t)64);
        ex->n_out_tokens = n_out;
        for (uint32_t i = 0; i < n_out; ++i) {
            ex->out_tokens[i]    = (uint32_t)resp.tokens[i];
            ex->token_logprobs[i] = resp.logprobs[i];
        }

        // Decode generated text into text_buf
        {
            std::string text = engine.tok.decode(resp.tokens);
            size_t copy_len = std::min(text.size(), sizeof(ex->text_buf) - 1);
            memcpy(ex->text_buf, text.c_str(), copy_len);
            ex->text_buf[copy_len] = '\0';
        }

        // Signal response ready
        ex->gate.store(2, std::memory_order_release);
        printf("[autocomplete] Responded %u tokens in %.1f ms: %s\n",
               n_out, ms, ex->text_buf);
    }
    printf("[autocomplete] Shutdown.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
// CLI self-test (--test flag)
// ─────────────────────────────────────────────────────────────────────────────
static int run_test(Engine& engine) {
    printf("[test] Prefix completion:\n");
    std::string out = engine.complete_text("int main(", "", 16);
    printf("  >> %s\n", out.c_str());

    if (engine.tok.fim_prefix >= 0) {
        printf("[test] FIM completion (fill between prefix/suffix):\n");
        out = engine.complete_text("void sort(int* arr", ") { /* sort */ }", 16);
        printf("  >> %s\n", out.c_str());
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
    const char* model_path = nullptr;
    int max_ctx   = 2048;
    int threads   = 0; // 0 = auto
    bool do_test  = false;
    bool daemon   = true;

    for (int i = 1; i < argc; ++i) {
        if      (!strcmp(argv[i], "--model")   && i+1 < argc) model_path = argv[++i];
        else if (!strcmp(argv[i], "--ctx")     && i+1 < argc) max_ctx  = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--threads") && i+1 < argc) threads  = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--test"))                   do_test  = true;
        else if (!strcmp(argv[i], "--no-daemon"))              daemon   = false;
    }

    if (!model_path) {
        fprintf(stderr,
            "Usage: %s --model <path.gguf> [--ctx N] [--threads N] [--test] [--no-daemon]\n",
            argv[0]);
        return 1;
    }
    if (threads <= 0) threads = std::min(cpu_count(), 8);

    Engine engine;
    if (!engine.load(model_path, max_ctx, threads)) return 1;

    if (do_test) return run_test(engine);

    if (!daemon) {
        // Headless: read prompt from stdin, print completion to stdout
        std::string prompt;
        if (std::getline(std::cin, prompt)) {
            printf("%s", engine.complete_text(prompt, "", 64).c_str());
        }
        return 0;
    }

    // ── Create shared memory and serve ────────────────────────────────────
#ifdef _WIN32
    SetConsoleCtrlHandler(console_ctrl, TRUE);
    ShmHandle shm_h = shm_create("RawrXD_Autocomplete", sizeof(SharedExchange));
    if (!shm_h) { fprintf(stderr, "CreateFileMapping failed: %lu\n", GetLastError()); return 1; }
#else
    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);
    ShmHandle shm_h = shm_create("/RawrXD_Autocomplete", sizeof(SharedExchange));
    if (shm_h < 0) { perror("shm_open"); return 1; }
#endif

    auto* ex = static_cast<SharedExchange*>(shm_map(shm_h, sizeof(SharedExchange)));
    if (!ex) { perror("shm_map"); shm_close(shm_h); return 1; }
    memset(ex, 0, sizeof(SharedExchange));
    ex->gate.store(0, std::memory_order_relaxed);

    serve(engine, ex);

    shm_unmap(ex, sizeof(SharedExchange));
    shm_close(shm_h);
    return 0;
}
