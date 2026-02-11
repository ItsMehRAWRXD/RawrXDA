#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <map>

// Layout info
struct LayoutPreview {
    std::map<std::wstring, POINT> positions;
    float zoom = 1.0f;
    POINT pan = {0,0};
};

struct DeletedLayout {
    std::wstring name;
    std::wstring path;
    std::wstring tempBackup;
    SYSTEMTIME deletedAt;
};

enum TrashSortMode {
    SORT_BY_NAME,
    SORT_BY_TIME
};

// API
void ShowLayoutManager(HWND parent);
std::vector<std::wstring> ListSavedLayouts(const std::wstring& solutionName);
void RefreshTrashList(HWND hwnd);