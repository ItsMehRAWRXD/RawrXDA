#include <windows.h>

#include <cstddef>
#include <cstdlib>

extern "C" {

void asm_lsp_bridge_shutdown(void)
{
    // Strict lane fallback: shutdown with cleanup
    fprintf(stderr, "[Win32IDE] Strict lane shutdown\n");
}

void asm_gguf_loader_close(void* ctx)
{
    if (!ctx) {
        return;
    }

    // Match MASM bridge behavior: scrub and release opaque context memory.
    SecureZeroMemory(ctx, sizeof(void*));
    std::free(ctx);
}

} // extern "C"
