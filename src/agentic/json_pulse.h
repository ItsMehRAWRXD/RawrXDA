#pragma once

#include <cstddef>
#include <cstdint>

namespace rawrxd::agentic::jsonpulse {

class JsonPulse {
public:
    JsonPulse();

    bool hasVbmi2() const { return m_hasVbmi2; }

    // Encode one chunk into JSON-safe form. Returns bytes written.
    // Caller must provide sufficient destination space.
    std::size_t Encode(const char* src, std::size_t len, char* dst, std::size_t dstCapacity) const;

private:
    bool m_hasVbmi2 = false;
};

}  // namespace rawrxd::agentic::jsonpulse

extern "C" {
// Primary vector kernel entry (uses scalar bridge fallback internally for escape-heavy paths).
std::size_t rawrxd_json_pulse_vbmi2(const std::uint8_t* src, std::size_t len, std::uint8_t* dst);

// Scalar bridge called by ASM fallback path.
std::size_t rawrxd_json_pulse_scalar_bridge(const std::uint8_t* src, std::size_t len, std::uint8_t* dst);

// LUT base consumed by the kernel path.
extern __declspec(align(64)) std::uint8_t g_pulse_lut_base[512];
}
