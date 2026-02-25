// ============================================================================
// tier1_headless_stubs.cpp — Tier 1 Cosmetic Handlers (stub holder; see UNFINISHED_FEATURES.md)
// ============================================================================
// RawrEngine/RawrXD_Gold use auto_feature_registry; ssot_handlers (full Tier1) is Win32 IDE only.
// This file provides headless implementations for handleTier1* so command_registry links.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "ssot_handlers.h"
#include "shared_feature_dispatch.h"
#include <cstdio>

namespace {

static CommandResult headlessTier1(const CommandContext& ctx, const char* name) {
    if (ctx.outputFn) {
        char buf[192];
        snprintf(buf, sizeof(buf), "[%s] GUI-only. Start Win32 IDE for visual features.\n", name);
        ctx.output(buf);
    }
    return CommandResult::ok(name);
}

} // namespace

CommandResult handleTier1SmoothScrollToggle(const CommandContext& ctx) {
    return headlessTier1(ctx, "tier1.smoothScroll");
}
CommandResult handleTier1MinimapEnhanced(const CommandContext& ctx) {
    return headlessTier1(ctx, "tier1.minimapEnhanced");
}
CommandResult handleTier1BreadcrumbsToggle(const CommandContext& ctx) {
    return headlessTier1(ctx, "tier1.breadcrumbs");
}
CommandResult handleTier1FuzzyPalette(const CommandContext& ctx) {
    return headlessTier1(ctx, "tier1.fuzzyPalette");
}
CommandResult handleTier1SettingsGUI(const CommandContext& ctx) {
    return headlessTier1(ctx, "tier1.settingsGUI");
}
CommandResult handleTier1WelcomePage(const CommandContext& ctx) {
    return headlessTier1(ctx, "tier1.welcomePage");
}
CommandResult handleTier1FileIconTheme(const CommandContext& ctx) {
    return headlessTier1(ctx, "tier1.fileIcons");
}
CommandResult handleTier1TabDragToggle(const CommandContext& ctx) {
    return headlessTier1(ctx, "tier1.tabDrag");
}
CommandResult handleTier1SplitVertical(const CommandContext& ctx) {
    return headlessTier1(ctx, "tier1.splitVertical");
}
CommandResult handleTier1SplitHorizontal(const CommandContext& ctx) {
    return headlessTier1(ctx, "tier1.splitHorizontal");
}
CommandResult handleTier1SplitGrid(const CommandContext& ctx) {
    return headlessTier1(ctx, "tier1.splitGrid");
}
CommandResult handleTier1SplitClose(const CommandContext& ctx) {
    return headlessTier1(ctx, "tier1.splitClose");
}
CommandResult handleTier1SplitFocusNext(const CommandContext& ctx) {
    return headlessTier1(ctx, "tier1.splitFocusNext");
}
CommandResult handleTier1AutoUpdateCheck(const CommandContext& ctx) {
    return headlessTier1(ctx, "tier1.autoUpdate");
}
CommandResult handleTier1UpdateDismiss(const CommandContext& ctx) {
    return headlessTier1(ctx, "tier1.updateDismiss");
}
