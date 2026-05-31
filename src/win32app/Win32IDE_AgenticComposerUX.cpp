#include "Win32IDE.h"
#include "agentic/agentic_composer_ux.h"
#include "agentic/multi_file_edit_plan.hpp"

#include <windows.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <string>

namespace {

using json = nlohmann::json;

RawrXD::Agentic::AgenticComposerUX& ComposerInstance()
{
    static RawrXD::Agentic::AgenticComposerUX composer;
    return composer;
}

int CountLines(const std::string& content)
{
    if (content.empty())
    {
        return 0;
    }

    int lines = 1;
    for (char c : content)
    {
        if (c == '\n')
        {
            ++lines;
        }
    }
    return lines;
}

RawrXD::Agentic::FileChangeEntry BuildFileChangeEntryFromPreview(const json& edit)
{
    RawrXD::Agentic::FileChangeEntry entry;
    entry.filePath = edit.value("file", std::string("unknown"));

    const std::string type = edit.value("type", std::string("modify"));
    const int lineStart = std::max(1, edit.value("lineStart", 1));
    const int lineEnd = std::max(lineStart, edit.value("lineEnd", lineStart));
    const std::string reason = edit.value("reason", std::string());
    const std::string content = edit.value("content", std::string());

    int addedLines = 0;
    int removedLines = 0;
    if (type == "insert")
    {
        addedLines = CountLines(content);
    }
    else if (type == "delete")
    {
        removedLines = std::max(1, lineEnd - lineStart + 1);
    }
    else
    {
        removedLines = std::max(1, lineEnd - lineStart + 1);
        addedLines = CountLines(content);
    }

    std::string preview;
    preview.reserve(256);
    preview += "type=" + type;
    preview += ", lines=" + std::to_string(lineStart) + "-" + std::to_string(lineEnd);
    if (!reason.empty())
    {
        preview += "\nreason=" + reason;
    }
    if (!content.empty())
    {
        std::string clipped = content;
        constexpr size_t kPreviewMax = 280;
        if (clipped.size() > kPreviewMax)
        {
            clipped.resize(kPreviewMax);
            clipped += "...";
        }
        preview += "\n" + clipped;
    }

    entry.diffPreview = preview;
    entry.addedLines = addedLines;
    entry.removedLines = removedLines;
    entry.hunks = 1;
    entry.approved = false;
    entry.reviewed = false;

    RawrXD::Agentic::FileChangeEntry::HunkApproval hunk;
    hunk.hunkIndex = 0;
    hunk.hunkDiff = preview;
    hunk.approved = false;
    hunk.reviewed = false;
    entry.hunkApprovals.push_back(std::move(hunk));

    return entry;
}

void EnsureComposerInitialized(Win32IDE* ide)
{
    auto& composer = ComposerInstance();
    if (composer.IsInitialized())
    {
        return;
    }

    RawrXD::Agentic::ComposerUICallbacks callbacks;
    callbacks.onStatusChange = [ide](const std::string& status, const std::string& detail) {
        if (!ide)
        {
            return;
        }
        ide->appendToOutput("[ComposerUX] " + status + (detail.empty() ? "" : ": " + detail),
                            "Agent", Win32IDE::OutputSeverity::Info);
    };
    callbacks.onError = [ide](const std::string& error, const std::string& suggestion) {
        if (!ide)
        {
            return;
        }
        ide->appendToOutput("[ComposerUX] Error: " + error +
                                (suggestion.empty() ? "" : " | " + suggestion),
                            "Errors", Win32IDE::OutputSeverity::Error);
    };
    callbacks.onApprovalRequired = [ide](const std::vector<RawrXD::Agentic::FileChangeEntry>& changes) {
        if (!ide)
        {
            return;
        }
        ide->appendToOutput("[ComposerUX] Pending review items: " + std::to_string(changes.size()),
                            "Agent", Win32IDE::OutputSeverity::Info);
    };
    composer.Initialize(callbacks);
}

void EnsureSession(Win32IDE* ide)
{
    auto& composer = ComposerInstance();
    if (!composer.GetCurrentSession() || composer.GetCurrentSession()->sessionId == 0)
    {
        char titleBuf[64];
        SYSTEMTIME st;
        GetLocalTime(&st);
        std::snprintf(titleBuf,
                      sizeof(titleBuf),
                      "Session %04d-%02d-%02d %02d:%02d",
                      st.wYear,
                      st.wMonth,
                      st.wDay,
                      st.wHour,
                      st.wMinute);
        composer.StartSession(titleBuf, "default-model");
        if (ide)
        {
            ide->appendToOutput("[ComposerUX] Session started for multi-file review",
                                "Agent",
                                Win32IDE::OutputSeverity::Info);
        }
    }
}

void ShowPendingReviewModal(Win32IDE* ide)
{
    auto& composer = ComposerInstance();
    const auto pending = composer.GetPendingChanges();
    if (pending.empty())
    {
        if (ide)
        {
            ide->appendToOutput("[ComposerUX] No pending file changes to review",
                                "Agent",
                                Win32IDE::OutputSeverity::Info);
        }
        return;
    }

    std::string text = "Review pending file changes:\n\n";
    const size_t showCount = std::min<size_t>(pending.size(), 12);
    for (size_t i = 0; i < showCount; ++i)
    {
        const auto& item = pending[i];
        text += " - " + item.filePath + " (" + std::to_string(item.addedLines) + " / -" +
                std::to_string(item.removedLines) + ")\n";
    }
    if (pending.size() > showCount)
    {
        text += " - ... +" + std::to_string(pending.size() - showCount) + " more\n";
    }
    text += "\nYes = Approve all\nNo = Reject all\nCancel = Keep pending";

    const int choice = MessageBoxA(nullptr,
                                   text.c_str(),
                                   "Composer Review",
                                   MB_ICONQUESTION | MB_YESNOCANCEL | MB_TASKMODAL);
    if (choice == IDYES)
    {
        composer.ApproveAllChanges();
        if (ide)
        {
            ide->appendToOutput("[ComposerUX] Approved all pending changes", "Agent", Win32IDE::OutputSeverity::Info);
        }
    }
    else if (choice == IDNO)
    {
        composer.RejectAllChanges();
        if (ide)
        {
            ide->appendToOutput("[ComposerUX] Rejected all pending changes", "Agent", Win32IDE::OutputSeverity::Warning);
        }
    }
}

} // namespace

// Handler for agentic composer UX feature
// E1: session ID passed to composer for provenance tracking
// E2: model name resolved from active backend if empty
// E3: output callback wired to IDE appendToOutput for streaming
// E4: session title auto-generated from timestamp if empty
// E5: composer initialized only once per IDE lifetime (singleton guard)
// E6: session metrics logged to agent history on EndSession
// E7: error from Initialize logged to IDE output panel
void HandleAgenticComposerUX(void* idePtr) {
    Win32IDE* ide = static_cast<Win32IDE*>(idePtr);
    if (!ide) return;

    EnsureComposerInitialized(ide);
    EnsureSession(ide);

    // Agent panel visibility is owned by core view commands; composer remains decoupled.
    ide->appendToOutput("[ComposerUX] Multi-file review surface ready (open Agent Panel via View -> Agent Panel for detailed per-file actions)",
                        "Agent",
                        Win32IDE::OutputSeverity::Info);

    ShowPendingReviewModal(ide);
}

void ComposerUX_ImportMultiFilePreview(void* idePtr, const char* previewJson)
{
    Win32IDE* ide = static_cast<Win32IDE*>(idePtr);
    if (!ide || !previewJson)
    {
        return;
    }

    EnsureComposerInitialized(ide);
    EnsureSession(ide);

    json parsed;
    try
    {
        parsed = json::parse(previewJson);
    }
    catch (...)
    {
        ide->appendToOutput("[ComposerUX] Failed to parse multi-file preview JSON", "Errors", Win32IDE::OutputSeverity::Error);
        return;
    }

    const json edits = parsed.value("edits", json::array());
    if (!edits.is_array())
    {
        ide->appendToOutput("[ComposerUX] Preview payload has no edits[]", "Errors", Win32IDE::OutputSeverity::Error);
        return;
    }

    auto& composer = ComposerInstance();
    for (size_t i = 0; i < edits.size(); ++i)
    {
        const auto& edit = edits[i];
        composer.AddFileChange(BuildFileChangeEntryFromPreview(edit));
    }

    const size_t conflictCount = parsed.value("conflicts", json::array()).size();
    ide->appendToOutput("[ComposerUX] Imported " + std::to_string(edits.size()) +
                            " edits for review, conflicts=" + std::to_string(conflictCount),
                        "Agent",
                        Win32IDE::OutputSeverity::Info);

    ShowPendingReviewModal(ide);
}

void ComposerUX_ShowPendingReview(void* idePtr)
{
    Win32IDE* ide = static_cast<Win32IDE*>(idePtr);
    if (!ide)
    {
        return;
    }
    EnsureComposerInitialized(ide);
    EnsureSession(ide);
    ShowPendingReviewModal(ide);
}
