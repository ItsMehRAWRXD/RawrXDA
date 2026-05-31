#pragma once

#include <windows.h>
#include <richedit.h>

#include <cstdint>
#include <string>

namespace RawrXD::Review {

constexpr UINT WM_USER_EDIT_REVIEW_REQUIRED = WM_APP + 612;

enum class EditType : uint32_t {
    Insert = 0,
    Replace = 1,
    Delete = 2,
    Rename = 3,
    Create = 4,
};

enum class EditSource : uint32_t {
    Agent = 0,
    GhostText = 1,
    Lsp = 2,
    User = 3,
    Unknown = 4,
};

enum class EditState : uint32_t {
    Pending = 0,
    Approved = 1,
    Applied = 2,
    Committed = 3,
    Declined = 4,
    Discarded = 5,
    Stale = 6,
    AutoDeclined = 7,
};

struct PendingEditRequest {
    EditType type = EditType::Replace;
    EditSource source = EditSource::Agent;
    HWND targetHandle = nullptr;
    CHARRANGE oldRange{0, 0};
    std::string file;
    std::string oldText;
    std::string newText;
    std::string newPath;
    bool replaceAll = false;
    uint64_t createdTickMs = 0;
};

}  // namespace RawrXD::Review