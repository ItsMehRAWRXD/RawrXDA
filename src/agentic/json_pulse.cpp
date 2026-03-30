#include "json_pulse.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <mutex>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

__declspec(align(64)) std::uint8_t g_pulse_lut_base[512] = {};

namespace {

bool cpu_has_vbmi2() {
#if defined(_MSC_VER) && defined(_M_X64)
    int regs[4] = {0, 0, 0, 0};
    __cpuidex(regs, 7, 0);
    // CPUID.(EAX=7,ECX=0):ECX[6] = AVX-512 VBMI2
    return (regs[2] & (1 << 6)) != 0;
#else
    return false;
#endif
}

void init_lut_once() {
    for (int i = 0; i < 256; ++i) {
        g_pulse_lut_base[i] = static_cast<std::uint8_t>(i);
        g_pulse_lut_base[i + 256] = 0;
    }

    auto mark_escape = [](unsigned char c, unsigned char mapped) {
        g_pulse_lut_base[c] = mapped;
        g_pulse_lut_base[c + 256] = 1;
    };

    mark_escape('"', '"');
    mark_escape('\\', '\\');
    mark_escape('\n', 'n');
    mark_escape('\r', 'r');
    mark_escape('\t', 't');
}

}  // namespace

extern "C" std::size_t rawrxd_json_pulse_scalar_bridge(const std::uint8_t* src, std::size_t len, std::uint8_t* dst) {
    if (!src || !dst) {
        return 0;
    }

    std::size_t out = 0;
    for (std::size_t i = 0; i < len; ++i) {
        const unsigned char c = src[i];
        switch (c) {
            case '"':
                dst[out++] = '\\';
                dst[out++] = '"';
                break;
            case '\\':
                dst[out++] = '\\';
                dst[out++] = '\\';
                break;
            case '\n':
                dst[out++] = '\\';
                dst[out++] = 'n';
                break;
            case '\r':
                dst[out++] = '\\';
                dst[out++] = 'r';
                break;
            case '\t':
                dst[out++] = '\\';
                dst[out++] = 't';
                break;
            default:
                if (c < 0x20) {
                    static constexpr char hex[] = "0123456789ABCDEF";
                    dst[out++] = '\\';
                    dst[out++] = 'u';
                    dst[out++] = '0';
                    dst[out++] = '0';
                    dst[out++] = static_cast<std::uint8_t>(hex[(c >> 4) & 0x0F]);
                    dst[out++] = static_cast<std::uint8_t>(hex[c & 0x0F]);
                } else {
                    dst[out++] = c;
                }
                break;
        }
    }

    return out;
}

namespace rawrxd::agentic::jsonpulse {

JsonPulse::JsonPulse() {
    static std::once_flag once;
    std::call_once(once, init_lut_once);
    m_hasVbmi2 = cpu_has_vbmi2();
}

std::size_t JsonPulse::Encode(const char* src, std::size_t len, char* dst, std::size_t dstCapacity) const {
    if (!src || !dst || len == 0 || dstCapacity == 0) {
        return 0;
    }

    // Worst-case JSON escape expansion is 6x (\u00XX).
    if (len > (static_cast<std::size_t>(-1) / 6)) {
        return 0;
    }
    const std::size_t worstCase = len * 6;
    if (dstCapacity < worstCase) {
        return 0;
    }

    const auto* in = reinterpret_cast<const std::uint8_t*>(src);
    auto* out = reinterpret_cast<std::uint8_t*>(dst);

    if (m_hasVbmi2) {
        std::size_t produced = 0;
        std::size_t consumed = 0;

        while ((len - consumed) >= 64) {
            produced += rawrxd_json_pulse_vbmi2(in + consumed, 64, out + produced);
            consumed += 64;
        }

        if (consumed < len) {
            produced += rawrxd_json_pulse_vbmi2(in + consumed, len - consumed, out + produced);
        }

        return produced;
    }
    return rawrxd_json_pulse_scalar_bridge(in, len, out);
}

}  // namespace rawrxd::agentic::jsonpulse
