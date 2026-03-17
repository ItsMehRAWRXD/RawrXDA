#include "../../src/core/prompt_template_engine.h"

#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (!data || size == 0 || size > (1u << 16)) {
        return 0;
    }

    std::string input(reinterpret_cast<const char*>(data), size);

    auto& engine = RawrXD::Prompt::PromptTemplateEngine::Global();
    static bool builtinsRegistered = false;
    if (!builtinsRegistered) {
        engine.registerBuiltins();
        builtinsRegistered = true;
    }

    RawrXD::Prompt::TemplateContext ctx;
    ctx["user_message"] = input;
    ctx["code"] = input;
    ctx["language"] = std::string("cpp");
    ctx["instruction"] = std::string("refactor");
    ctx["task"] = input;

    engine.renderInline(input, ctx);
    engine.render("chat_chatml", ctx);
    engine.render("refactor", ctx);
    engine.render("agentic_plan", ctx);

    return 0;
}
