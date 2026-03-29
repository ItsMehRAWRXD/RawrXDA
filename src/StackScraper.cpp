#include "StackScraper.hpp"

#include <cstring>

extern "C" void RtlCaptureContext(PCONTEXT contextRecord);

namespace rawrxd {

void fillCpuContextLiteFromContext(RawrxdCpuContextLite* out, const CONTEXT& ctx)
{
    if (!out) {
        return;
    }
    out->rip = ctx.Rip;
    out->rsp = ctx.Rsp;
    out->rbp = ctx.Rbp;
    out->rax = ctx.Rax;
    out->rbx = ctx.Rbx;
    out->rcx = ctx.Rcx;
    out->rdx = ctx.Rdx;
    out->rsi = ctx.Rsi;
    out->rdi = ctx.Rdi;
    out->r8 = ctx.R8;
    out->r9 = ctx.R9;
    out->r10 = ctx.R10;
    out->r11 = ctx.R11;
    out->r12 = ctx.R12;
    out->r13 = ctx.R13;
    out->r14 = ctx.R14;
    out->r15 = ctx.R15;
}

bool tryCopyStackPeek(std::uint64_t rsp, std::uint64_t outQwords[8])
{
    if (!outQwords || rsp == 0) {
        return false;
    }
#if defined(_MSC_VER)
    __try {
        const auto* p = reinterpret_cast<const std::uint64_t*>(rsp);
        for (int i = 0; i < 8; ++i) {
            outQwords[i] = p[i];
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        std::memset(outQwords, 0, 8 * sizeof(std::uint64_t));
        return false;
    }
#else
    (void)rsp;
    std::memset(outQwords, 0, 8 * sizeof(std::uint64_t));
    return false;
#endif
}

void captureCpuContextLiteAccurate(RawrxdCpuContextLite* out)
{
    if (!out) {
        return;
    }
    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_FULL;
    RtlCaptureContext(&ctx);
    fillCpuContextLiteFromContext(out, ctx);
    (void)tryCopyStackPeek(out->rsp, out->stackPeek);
}

} // namespace rawrxd
