#if !defined(_MSC_VER)

#include <cstdint>
#include <cstring>

extern "C" {

struct RawrXD_Emit_Buffer {
    void* base_ptr;
    void* current_ptr;
    uint64_t capacity;
};

static uint64_t rawrxdEmitBytes(RawrXD_Emit_Buffer* buf, const void* src, uint64_t n) {
    if (!buf || !buf->base_ptr || !buf->current_ptr || !src) {
        return 0;
    }
    uint8_t* base = static_cast<uint8_t*>(buf->base_ptr);
    uint8_t* cur = static_cast<uint8_t*>(buf->current_ptr);
    if (cur < base) {
        return 0;
    }
    const uint64_t used = static_cast<uint64_t>(cur - base);
    if (used > buf->capacity || n > (buf->capacity - used)) {
        return 0;
    }
    std::memcpy(cur, src, static_cast<size_t>(n));
    buf->current_ptr = cur + n;
    return 1;
}

void RawrXD_Emit_Reset(RawrXD_Emit_Buffer* buf) {
    if (!buf) {
        return;
    }
    buf->current_ptr = buf->base_ptr;
}

uint64_t Emit_Byte(RawrXD_Emit_Buffer* buf, uint8_t val) {
    return rawrxdEmitBytes(buf, &val, sizeof(val));
}

uint64_t Emit_Word(RawrXD_Emit_Buffer* buf, uint16_t val) {
    return rawrxdEmitBytes(buf, &val, sizeof(val));
}

uint64_t Emit_Dword(RawrXD_Emit_Buffer* buf, uint32_t val) {
    return rawrxdEmitBytes(buf, &val, sizeof(val));
}

uint64_t Emit_Qword(RawrXD_Emit_Buffer* buf, uint64_t val) {
    return rawrxdEmitBytes(buf, &val, sizeof(val));
}

uint64_t Emit_Mov_Rax_Imm64(RawrXD_Emit_Buffer* buf, uint64_t imm) {
    const uint8_t opcodes[2] = {0x48, 0xB8};
    if (!rawrxdEmitBytes(buf, opcodes, sizeof(opcodes))) {
        return 0;
    }
    return Emit_Qword(buf, imm);
}

uint64_t Emit_Int3(RawrXD_Emit_Buffer* buf) {
    return Emit_Byte(buf, 0xCC);
}

uint64_t Emit_Ret(RawrXD_Emit_Buffer* buf) {
    return Emit_Byte(buf, 0xC3);
}

uint64_t Emit_Nop(RawrXD_Emit_Buffer* buf) {
    return Emit_Byte(buf, 0x90);
}

}  // extern "C"

#endif  // !defined(_MSC_VER)
