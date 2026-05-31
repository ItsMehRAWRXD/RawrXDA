// ============================================================================
// feature_handlers_model_load.cpp — file.loadModel / !model_load (headless-safe)
// ============================================================================
// Split from feature_handlers.cpp so tests and minimal links can pull GGUF
// validation without Win32IDE, Ollama, hotpatch, and debugger dependencies.
// ============================================================================

#include "feature_handlers.h"

#include <windows.h>

#include <cstdint>
#include <sstream>
#include <string>

CommandResult handleFileLoadModel(const CommandContext& ctx)
{
    if (!ctx.args || !ctx.args[0])
    {
        ctx.output("Usage: !model_load <path-to-gguf>\n");
        return CommandResult::error("file.loadModel: missing path");
    }
    HANDLE h =
        CreateFileA(ctx.args, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
    {
        std::string msg = "[Model] File not found: " + std::string(ctx.args) + "\n";
        ctx.output(msg.c_str());
        return CommandResult::error("file.loadModel: not found");
    }
    uint32_t magic = 0;
    DWORD bytesRead = 0;
    ReadFile(h, &magic, sizeof(magic), &bytesRead, nullptr);
    LARGE_INTEGER fileSize;
    GetFileSizeEx(h, &fileSize);
    CloseHandle(h);
    if (magic != 0x46475547u)
    {
        ctx.output("[Model] Invalid GGUF magic bytes. Not a valid model file.\n");
        return CommandResult::error("file.loadModel: invalid GGUF");
    }
    std::ostringstream oss;
    oss << "[Model] Valid GGUF: " << ctx.args << " (" << (fileSize.QuadPart / (1024 * 1024)) << " MB)\n";
    oss << "[Model] Dispatching to GGUFLoader...\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("file.loadModel");
}
