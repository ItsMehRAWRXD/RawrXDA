#include "../codec/compression.h"
#include "Win32IDE.h"
#include "video/tubi_backend.h"

#include <commctrl.h>
#include <shellapi.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

namespace
{
constexpr UINT WM_RAWR_VIDEO_STUDIO_REFRESH = WM_APP + 440;
constexpr int IDC_VIDEO_PROMPT = 16001;
constexpr int IDC_VIDEO_STORYBOARD = 16002;
constexpr int IDC_VIDEO_PROVIDER = 16003;
constexpr int IDC_VIDEO_LOCAL_MODEL = 16004;
constexpr int IDC_VIDEO_STYLE = 16005;
constexpr int IDC_VIDEO_DURATION = 16006;
constexpr int IDC_VIDEO_ASPECT = 16007;
constexpr int IDC_VIDEO_RESOLUTION = 16008;
constexpr int IDC_VIDEO_GENERATE = 16009;
constexpr int IDC_VIDEO_USE_CHAT = 16010;
constexpr int IDC_VIDEO_OPEN_FOLDER = 16011;
constexpr int IDC_VIDEO_CANCEL = 16012;
constexpr int IDC_VIDEO_JOBS = 16013;
constexpr int IDC_VIDEO_STATUS = 16014;

std::wstring utf8ToWideLocal(const std::string& utf8)
{
    if (utf8.empty())
        return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), &out[0], size);
    return out;
}

std::string wideToUtf8Local(const std::wstring& wide)
{
    if (wide.empty())
        return {};
    const int size =
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), &out[0], size, nullptr, nullptr);
    return out;
}

std::string getWindowTextUtf8(HWND hwnd)
{
    if (!hwnd || !IsWindow(hwnd))
        return {};
    const int len = GetWindowTextLengthW(hwnd);
    std::wstring buffer(static_cast<size_t>(len) + 1, L'\0');
    GetWindowTextW(hwnd, &buffer[0], len + 1);
    buffer.resize(static_cast<size_t>(len));
    return wideToUtf8Local(buffer);
}

std::string getComboTextUtf8(HWND hwnd)
{
    wchar_t buffer[256] = {};
    GetWindowTextW(hwnd, buffer, static_cast<int>(std::size(buffer)));
    return wideToUtf8Local(buffer);
}

std::string escapeJson(const std::string& value)
{
    std::string out;
    out.reserve(value.size() + 16);
    for (char ch : value)
    {
        switch (ch)
        {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out.push_back(ch);
                break;
        }
    }
    return out;
}

void addComboItems(HWND hwnd, const std::initializer_list<const wchar_t*>& items)
{
    for (const wchar_t* item : items)
        SendMessageW(hwnd, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
    SendMessageW(hwnd, CB_SETCURSEL, 0, 0);
}

std::vector<uint8_t> toBytes(const std::string& text)
{
    return std::vector<uint8_t>(text.begin(), text.end());
}

std::string psQuote(const std::string& value)
{
    std::string out = "'";
    for (char ch : value)
    {
        if (ch == '\'')
            out += "''";
        else
            out.push_back(ch);
    }
    out += "'";
    return out;
}

bool tryStageBundledTubiBackend(const std::filesystem::path& artifactDir, std::filesystem::path* stagedPath)
{
    std::vector<std::filesystem::path> candidates;

    wchar_t moduleBuf[MAX_PATH] = {};
    const DWORD moduleLen = GetModuleFileNameW(nullptr, moduleBuf, MAX_PATH);
    if (moduleLen > 0 && moduleLen < MAX_PATH)
    {
        const std::filesystem::path modulePath(moduleBuf);
        const std::filesystem::path moduleDir = modulePath.parent_path();
        candidates.push_back(moduleDir / "rawrxd-tubi-backend.exe");
        candidates.push_back(moduleDir.parent_path() / "bin" / "rawrxd-tubi-backend.exe");
    }

    const std::filesystem::path workspaceBase = std::filesystem::current_path();
    candidates.push_back(workspaceBase / "build-win32" / "bin" / "rawrxd-tubi-backend.exe");
    candidates.push_back(workspaceBase / "build-ninja" / "bin" / "rawrxd-tubi-backend.exe");

    std::error_code ec;
    for (const auto& candidate : candidates)
    {
        if (candidate.empty() || !std::filesystem::exists(candidate, ec) || ec)
        {
            ec.clear();
            continue;
        }

        const std::filesystem::path destination = artifactDir / "rawrxd-tubi-backend.exe";
        std::filesystem::copy_file(candidate, destination, std::filesystem::copy_options::overwrite_existing, ec);
        if (!ec)
        {
            if (stagedPath)
                *stagedPath = destination;
            return true;
        }
        ec.clear();
    }

    return false;
}

}  // namespace

void Win32IDE::layoutVideoStudioControls()
{
    if (!m_hwndVideoStudio)
        return;

    RECT rc{};
    GetClientRect(m_hwndVideoStudio, &rc);
    const int margin = 12;
    const int labelH = 18;
    const int comboH = 24;
    const int btnH = 28;
    const int leftW = std::max<int>(280, (rc.right - (margin * 3)) / 2);
    const int rightX = margin + leftW + margin;
    const int rightW = std::max<int>(260, rc.right - rightX - margin);

    int y = margin;
    MoveWindow(m_hwndVideoPromptLabel, margin, y, leftW, labelH, TRUE);
    y += labelH + 2;
    MoveWindow(m_hwndVideoPrompt, margin, y, leftW, 110, TRUE);
    y += 118;

    MoveWindow(m_hwndVideoStoryboardLabel, margin, y, leftW, labelH, TRUE);
    y += labelH + 2;
    MoveWindow(m_hwndVideoStoryboard, margin, y, leftW, rc.bottom - y - 120, TRUE);

    int ry = margin;
    MoveWindow(m_hwndVideoProviderLabel, rightX, ry, 90, labelH, TRUE);
    MoveWindow(m_hwndVideoProvider, rightX + 90, ry - 2, rightW - 90, comboH + 4, TRUE);
    ry += 30;

    MoveWindow(m_hwndVideoLocalModelLabel, rightX, ry, 90, labelH, TRUE);
    MoveWindow(m_hwndVideoLocalModel, rightX + 90, ry - 2, rightW - 90, comboH + 160, TRUE);
    ry += 30;

    MoveWindow(m_hwndVideoStyleLabel, rightX, ry, 90, labelH, TRUE);
    MoveWindow(m_hwndVideoStyle, rightX + 90, ry - 2, rightW - 90, comboH + 4, TRUE);
    ry += 30;

    MoveWindow(m_hwndVideoDurationLabel, rightX, ry, 90, labelH, TRUE);
    MoveWindow(m_hwndVideoDuration, rightX + 90, ry - 2, rightW - 90, comboH + 4, TRUE);
    ry += 30;

    MoveWindow(m_hwndVideoAspectLabel, rightX, ry, 90, labelH, TRUE);
    MoveWindow(m_hwndVideoAspect, rightX + 90, ry - 2, rightW - 90, comboH + 4, TRUE);
    ry += 30;

    MoveWindow(m_hwndVideoResolutionLabel, rightX, ry, 90, labelH, TRUE);
    MoveWindow(m_hwndVideoResolution, rightX + 90, ry - 2, rightW - 90, comboH + 4, TRUE);
    ry += 36;

    MoveWindow(m_hwndVideoGenerateBtn, rightX, ry, 100, btnH, TRUE);
    MoveWindow(m_hwndVideoUseChatBtn, rightX + 108, ry, 110, btnH, TRUE);
    MoveWindow(m_hwndVideoOpenFolderBtn, rightX + 226, ry, 110, btnH, TRUE);
    MoveWindow(m_hwndVideoCancelBtn, rightX + 344, ry, 90, btnH, TRUE);
    ry += 40;

    MoveWindow(m_hwndVideoJobsLabel, rightX, ry, rightW, labelH, TRUE);
    ry += labelH + 2;
    const int jobsH = std::max<int>(130, (rc.bottom - ry - 120) / 2);
    MoveWindow(m_hwndVideoJobList, rightX, ry, rightW, jobsH, TRUE);
    ry += jobsH + 10;

    MoveWindow(m_hwndVideoDetailsLabel, rightX, ry, rightW, labelH, TRUE);
    ry += labelH + 2;
    MoveWindow(m_hwndVideoStatus, rightX, ry, rightW, rc.bottom - ry - margin, TRUE);
}

Win32IDE::VideoGenerationJob* Win32IDE::findVideoJobById(const std::string& jobId)
{
    for (auto& job : m_videoJobs)
    {
        if (job.id == jobId)
            return &job;
    }
    return nullptr;
}

const Win32IDE::VideoGenerationJob* Win32IDE::findVideoJobById(const std::string& jobId) const
{
    for (const auto& job : m_videoJobs)
    {
        if (job.id == jobId)
            return &job;
    }
    return nullptr;
}

void Win32IDE::populateVideoStudioLocalModels()
{
    if (!m_hwndVideoLocalModel || !IsWindow(m_hwndVideoLocalModel))
        return;

    SendMessageW(m_hwndVideoLocalModel, CB_RESETCONTENT, 0, 0);
    m_videoDiscoveredModels.clear();

    std::vector<std::string> modelDirs;
    auto addDir = [&modelDirs](const std::string& raw)
    {
        if (raw.empty())
            return;
        std::error_code ec;
        const std::filesystem::path p(raw);
        if (!std::filesystem::exists(p, ec))
            return;
        const std::string norm = p.lexically_normal().string();
        if (std::find(modelDirs.begin(), modelDirs.end(), norm) == modelDirs.end())
            modelDirs.push_back(norm);
    };
    for (const auto& dir : m_userModelDirectories)
        addDir(dir);
    addDir("D:\\OllamaModels");
    addDir("F:\\OllamaModels");
    addDir("D:\\models");
    addDir("F:\\models");

    auto addEnvDirs = [&addDir](const char* envName)
    {
        const DWORD envLen = GetEnvironmentVariableA(envName, nullptr, 0);
        if (envLen <= 1)
            return;
        std::string envValue(static_cast<size_t>(envLen), '\0');
        const DWORD copied = GetEnvironmentVariableA(envName, envValue.data(), envLen);
        if (copied == 0)
            return;
        envValue.resize(copied);
        size_t start = 0;
        while (start <= envValue.size())
        {
            const size_t end = envValue.find(';', start);
            addDir(envValue.substr(start, end == std::string::npos ? std::string::npos : end - start));
            if (end == std::string::npos)
                break;
            start = end + 1;
        }
    };
    addEnvDirs("OLLAMA_MODELS");
    addEnvDirs("RAWRXD_MODELS_PATH");

    for (const auto& dir : modelDirs)
    {
        std::error_code ec;
        for (std::filesystem::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        {
            if (ec)
                break;
            if (!it->is_regular_file(ec))
                continue;
            const std::filesystem::path path = it->path();
            const std::string ext = path.extension().string();
            if (_stricmp(ext.c_str(), ".gguf") == 0 || _stricmp(ext.c_str(), ".safetensors") == 0 ||
                _stricmp(ext.c_str(), ".ckpt") == 0 || _stricmp(ext.c_str(), ".bin") == 0)
            {
                m_videoDiscoveredModels.push_back(path.string());
                if (m_videoDiscoveredModels.size() >= 128)
                    break;
            }
        }
        if (m_videoDiscoveredModels.size() >= 128)
            break;
    }

    if (m_videoDiscoveredModels.empty())
    {
        SendMessageW(m_hwndVideoLocalModel, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"No local models discovered"));
        SendMessageW(m_hwndVideoLocalModel, CB_SETCURSEL, 0, 0);
        return;
    }

    for (const auto& model : m_videoDiscoveredModels)
    {
        const std::wstring w = utf8ToWideLocal(model);
        SendMessageW(m_hwndVideoLocalModel, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(w.c_str()));
    }
    SendMessageW(m_hwndVideoLocalModel, CB_SETCURSEL, 0, 0);
}

void Win32IDE::toggleVideoStudioWindow()
{
    if (m_hwndVideoStudio && IsWindow(m_hwndVideoStudio))
    {
        if (IsWindowVisible(m_hwndVideoStudio))
            ShowWindow(m_hwndVideoStudio, SW_HIDE);
        else
            ShowWindow(m_hwndVideoStudio, SW_SHOW);
        return;
    }
    showVideoStudioWindow();
}

void Win32IDE::showVideoStudioWindow()
{
    if (m_hwndVideoStudio && IsWindow(m_hwndVideoStudio))
    {
        ShowWindow(m_hwndVideoStudio, SW_SHOW);
        SetForegroundWindow(m_hwndVideoStudio);
        refreshVideoStudioUi();
        return;
    }

    static bool registered = false;
    if (!registered)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = Win32IDE::VideoStudioProc;
        wc.hInstance = m_hInstance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = L"RawrXD_VideoStudioWindow";
        RegisterClassW(&wc);
        registered = true;
    }

    m_hwndVideoStudio = CreateWindowExW(WS_EX_TOOLWINDOW, L"RawrXD_VideoStudioWindow", L"tubi Video Studio",
                                        WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 920, 680,
                                        m_hwndMain, nullptr, m_hInstance, this);
    if (!m_hwndVideoStudio)
        return;

    m_hwndVideoPrompt = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL, 0, 0, 0, 0,
        m_hwndVideoStudio, reinterpret_cast<HMENU>(IDC_VIDEO_PROMPT), m_hInstance, nullptr);
    m_hwndVideoStoryboard = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"Shot 1: Establishing shot\nShot 2: Main action\nShot 3: Close-up / payoff",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL, 0, 0, 0, 0, m_hwndVideoStudio,
        reinterpret_cast<HMENU>(IDC_VIDEO_STORYBOARD), m_hInstance, nullptr);
    m_hwndVideoProvider =
        CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 0, 300,
                        m_hwndVideoStudio, reinterpret_cast<HMENU>(IDC_VIDEO_PROVIDER), m_hInstance, nullptr);
    m_hwndVideoLocalModel =
        CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 0, 300,
                        m_hwndVideoStudio, reinterpret_cast<HMENU>(IDC_VIDEO_LOCAL_MODEL), m_hInstance, nullptr);
    m_hwndVideoStyle =
        CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 0, 300,
                        m_hwndVideoStudio, reinterpret_cast<HMENU>(IDC_VIDEO_STYLE), m_hInstance, nullptr);
    m_hwndVideoDuration =
        CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 0, 300,
                        m_hwndVideoStudio, reinterpret_cast<HMENU>(IDC_VIDEO_DURATION), m_hInstance, nullptr);
    m_hwndVideoAspect =
        CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 0, 300,
                        m_hwndVideoStudio, reinterpret_cast<HMENU>(IDC_VIDEO_ASPECT), m_hInstance, nullptr);
    m_hwndVideoResolution =
        CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 0, 300,
                        m_hwndVideoStudio, reinterpret_cast<HMENU>(IDC_VIDEO_RESOLUTION), m_hInstance, nullptr);
    m_hwndVideoGenerateBtn =
        CreateWindowExW(0, L"BUTTON", L"Generate", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, m_hwndVideoStudio,
                        reinterpret_cast<HMENU>(IDC_VIDEO_GENERATE), m_hInstance, nullptr);
    m_hwndVideoUseChatBtn =
        CreateWindowExW(0, L"BUTTON", L"Use Chat Prompt", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
                        m_hwndVideoStudio, reinterpret_cast<HMENU>(IDC_VIDEO_USE_CHAT), m_hInstance, nullptr);
    m_hwndVideoOpenFolderBtn =
        CreateWindowExW(0, L"BUTTON", L"Open Folder", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
                        m_hwndVideoStudio, reinterpret_cast<HMENU>(IDC_VIDEO_OPEN_FOLDER), m_hInstance, nullptr);
    m_hwndVideoCancelBtn =
        CreateWindowExW(0, L"BUTTON", L"Cancel Job", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
                        m_hwndVideoStudio, reinterpret_cast<HMENU>(IDC_VIDEO_CANCEL), m_hInstance, nullptr);
    m_hwndVideoJobList =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL, 0, 0, 0, 0,
                        m_hwndVideoStudio, reinterpret_cast<HMENU>(IDC_VIDEO_JOBS), m_hInstance, nullptr);
    m_hwndVideoStatus =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 0, 0, 0, 0,
                        m_hwndVideoStudio, reinterpret_cast<HMENU>(IDC_VIDEO_STATUS), m_hInstance, nullptr);
    m_hwndVideoPromptLabel = CreateWindowExW(0, L"STATIC", L"Prompt", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
                                             m_hwndVideoStudio, nullptr, m_hInstance, nullptr);
    m_hwndVideoStoryboardLabel = CreateWindowExW(0, L"STATIC", L"Storyboard / Shot List", WS_CHILD | WS_VISIBLE, 0, 0,
                                                 0, 0, m_hwndVideoStudio, nullptr, m_hInstance, nullptr);
    m_hwndVideoProviderLabel = CreateWindowExW(0, L"STATIC", L"Provider", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
                                               m_hwndVideoStudio, nullptr, m_hInstance, nullptr);
    m_hwndVideoLocalModelLabel = CreateWindowExW(0, L"STATIC", L"Local Model", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
                                                 m_hwndVideoStudio, nullptr, m_hInstance, nullptr);
    m_hwndVideoStyleLabel = CreateWindowExW(0, L"STATIC", L"Style", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
                                            m_hwndVideoStudio, nullptr, m_hInstance, nullptr);
    m_hwndVideoDurationLabel = CreateWindowExW(0, L"STATIC", L"Duration", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
                                               m_hwndVideoStudio, nullptr, m_hInstance, nullptr);
    m_hwndVideoAspectLabel = CreateWindowExW(0, L"STATIC", L"Aspect", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
                                             m_hwndVideoStudio, nullptr, m_hInstance, nullptr);
    m_hwndVideoResolutionLabel = CreateWindowExW(0, L"STATIC", L"Resolution", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
                                                 m_hwndVideoStudio, nullptr, m_hInstance, nullptr);
    m_hwndVideoJobsLabel = CreateWindowExW(0, L"STATIC", L"Render Queue", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
                                           m_hwndVideoStudio, nullptr, m_hInstance, nullptr);
    m_hwndVideoDetailsLabel = CreateWindowExW(0, L"STATIC", L"Job Details", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
                                              m_hwndVideoStudio, nullptr, m_hInstance, nullptr);

    addComboItems(m_hwndVideoProvider,
                  {L"Local GGUF / diffusion bridge", L"OpenAI-compatible API", L"Custom bridge stub"});
    addComboItems(m_hwndVideoStyle, {L"Cinematic", L"Anime", L"Photoreal", L"UI design", L"Pixel art"});
    addComboItems(m_hwndVideoDuration,
                  {L"5s", L"10s", L"15s", L"30s", L"1m", L"5m", L"10m", L"30m", L"1h", L"2h", L"3h"});
    addComboItems(m_hwndVideoAspect, {L"16:9", L"9:16", L"1:1", L"21:9"});
    addComboItems(m_hwndVideoResolution, {L"320p", L"480p", L"720p", L"1080p", L"1440p", L"4K"});
    populateVideoStudioLocalModels();

    layoutVideoStudioControls();
    refreshVideoStudioUi();
    appendToOutput("tubi Video Studio opened.\n", "Output", OutputSeverity::Info);
}

void Win32IDE::refreshVideoStudioUi()
{
    if (!m_hwndVideoStudio || !IsWindow(m_hwndVideoStudio))
        return;

    std::lock_guard<std::mutex> lock(m_videoJobsMutex);

    if (m_hwndVideoJobList && IsWindow(m_hwndVideoJobList))
    {
        const int selected = static_cast<int>(SendMessageW(m_hwndVideoJobList, LB_GETCURSEL, 0, 0));
        SendMessageW(m_hwndVideoJobList, LB_RESETCONTENT, 0, 0);
        for (const auto& job : m_videoJobs)
        {
            std::ostringstream row;
            row << job.id << " | " << job.style << " | " << job.duration << " | " << job.status;
            const std::wstring w = utf8ToWideLocal(row.str());
            SendMessageW(m_hwndVideoJobList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(w.c_str()));
        }
        if (!m_videoJobs.empty())
        {
            const int safeSel = (selected >= 0 && selected < static_cast<int>(m_videoJobs.size()))
                                    ? selected
                                    : static_cast<int>(m_videoJobs.size()) - 1;
            SendMessageW(m_hwndVideoJobList, LB_SETCURSEL, safeSel, 0);
        }
    }

    std::ostringstream detail;
    if (m_videoJobs.empty())
    {
        detail << "tubi ready.\r\n\r\n"
               << "Engine: tubi\r\n"
               << "Mode: MASM x64 native renderer\r\n"
               << "Compression: brutal package output enabled\r\n\r\n"
               << "This implementation discovers local models, renders a real frame sequence,\r\n"
               << "writes a backend manifest, and muxes an MP4 when ffmpeg is available.\r\n";
    }
    else
    {
        int selected = 0;
        if (m_hwndVideoJobList && IsWindow(m_hwndVideoJobList))
        {
            const LRESULT curSel = SendMessageW(m_hwndVideoJobList, LB_GETCURSEL, 0, 0);
            if (curSel != LB_ERR)
                selected = static_cast<int>(curSel);
        }
        if (selected < 0 || selected >= static_cast<int>(m_videoJobs.size()))
            selected = static_cast<int>(m_videoJobs.size()) - 1;
        const auto& job = m_videoJobs[static_cast<size_t>(selected)];
        detail << "Job ID: " << job.id << "\r\n"
               << "Engine: " << job.engineName << "\r\n"
               << "Provider: " << job.provider << "\r\n"
               << "Local model: " << (job.localModel.empty() ? "(none)" : job.localModel) << "\r\n"
               << "Style: " << job.style << "\r\n"
               << "Camera: " << (job.cameraMode.empty() ? "cinematic-pan" : job.cameraMode) << "\r\n"
               << "Seed: " << job.seed << "\r\n"
               << "Duration: " << job.duration << "\r\n"
               << "Duration seconds: " << job.renderedDurationSeconds << "\r\n"
               << "Aspect: " << job.aspectRatio << "\r\n"
               << "Resolution: " << job.resolution << "\r\n"
               << "Tags: " << (job.extractedTags.empty() ? "(pending)" : job.extractedTags) << "\r\n"
               << "Status: " << job.status << "\r\n"
               << "Artifacts: " << job.artifactDir << "\r\n"
               << "Brutal package: " << (job.compressedPackagePath.empty() ? "(pending)" : job.compressedPackagePath)
               << "\r\n"
               << "Backend manifest: " << (job.backendManifestPath.empty() ? "(pending)" : job.backendManifestPath)
               << "\r\n"
               << "Progress file: " << (job.progressPath.empty() ? "(pending)" : job.progressPath) << "\r\n"
               << "Shot plan: " << (job.shotPlanPath.empty() ? "(pending)" : job.shotPlanPath) << "\r\n"
               << "Contact sheet: " << (job.contactSheetPath.empty() ? "(pending)" : job.contactSheetPath) << "\r\n"
               << "Preview start: " << (job.previewStartPath.empty() ? "(pending)" : job.previewStartPath) << "\r\n"
               << "Preview mid: " << (job.previewMidPath.empty() ? "(pending)" : job.previewMidPath) << "\r\n"
               << "Preview end: " << (job.previewEndPath.empty() ? "(pending)" : job.previewEndPath) << "\r\n"
               << "Rendered video: " << (job.renderedVideoPath.empty() ? "(pending)" : job.renderedVideoPath) << "\r\n"
               << "Rendered size: "
               << (job.renderedWidth > 0 ? std::to_string(job.renderedWidth) + "x" + std::to_string(job.renderedHeight)
                                         : std::string("(pending)"))
               << "\r\n"
               << "Rendered cadence: "
               << (job.renderedFps > 0
                       ? std::to_string(job.renderedFps) + " fps / " + std::to_string(job.renderedFrames) + " frames"
                       : std::string("(pending)"))
               << "\r\n"
               << "Shot count: " << job.shotCount << "\r\n"
               << "Encoder: " << (job.encoderDiagnostics.empty() ? "(pending)" : job.encoderDiagnostics) << "\r\n"
               << "Brutal compression: " << (job.brutalCompressionUsed ? "enabled" : "disabled") << "\r\n\r\n"
               << "Prompt:\r\n"
               << job.prompt << "\r\n\r\n"
               << "Negative Prompt:\r\n"
               << (job.negativePrompt.empty() ? "(none)" : job.negativePrompt) << "\r\n\r\n"
               << "Storyboard:\r\n"
               << job.storyboard << "\r\n";
    }

    if (m_hwndVideoStatus && IsWindow(m_hwndVideoStatus))
        SetWindowTextW(m_hwndVideoStatus, utf8ToWideLocal(detail.str()).c_str());
}

void Win32IDE::useChatPromptForVideoStudio()
{
    if (!m_hwndVideoPrompt || !IsWindow(m_hwndVideoPrompt))
        return;
    const std::string chatPrompt = m_hwndCopilotChatInput ? getWindowTextUtf8(m_hwndCopilotChatInput) : "";
    if (chatPrompt.empty())
        return;
    SetWindowTextW(m_hwndVideoPrompt, utf8ToWideLocal(chatPrompt).c_str());
}

void Win32IDE::openVideoStudioOutputFolder()
{
    std::filesystem::path target = resolveRawrxdWorkspaceBase() / "video-studio";
    {
        std::lock_guard<std::mutex> lock(m_videoJobsMutex);
        if (!m_videoJobs.empty())
        {
            int selected = static_cast<int>(m_videoJobs.size()) - 1;
            if (m_hwndVideoJobList && IsWindow(m_hwndVideoJobList))
            {
                const LRESULT curSel = SendMessageW(m_hwndVideoJobList, LB_GETCURSEL, 0, 0);
                if (curSel != LB_ERR)
                    selected = static_cast<int>(curSel);
            }
            if (selected >= 0 && selected < static_cast<int>(m_videoJobs.size()) &&
                !m_videoJobs[static_cast<size_t>(selected)].artifactDir.empty())
            {
                target = std::filesystem::path(m_videoJobs[static_cast<size_t>(selected)].artifactDir);
            }
        }
    }
    std::error_code ec;
    std::filesystem::create_directories(target, ec);
    const std::wstring targetWide = target.wstring();
    ShellExecuteW(m_hwndMain, L"open", targetWide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void Win32IDE::cancelSelectedVideoStudioJob()
{
    std::lock_guard<std::mutex> lock(m_videoJobsMutex);
    if (m_videoJobs.empty())
        return;
    int selected = static_cast<int>(m_videoJobs.size()) - 1;
    if (m_hwndVideoJobList && IsWindow(m_hwndVideoJobList))
    {
        const LRESULT curSel = SendMessageW(m_hwndVideoJobList, LB_GETCURSEL, 0, 0);
        if (curSel != LB_ERR)
            selected = static_cast<int>(curSel);
    }
    if (selected < 0 || selected >= static_cast<int>(m_videoJobs.size()))
        return;
    auto& job = m_videoJobs[static_cast<size_t>(selected)];
    job.cancelled = true;
    job.running = false;
    job.status = "Cancelled";
    if (m_hwndStatusBar)
        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(L"Video render cancelled"));
    if (m_hwndVideoStudio)
        PostMessageW(m_hwndVideoStudio, WM_RAWR_VIDEO_STUDIO_REFRESH, 0, 0);
}

void Win32IDE::startVideoStudioRender()
{
    if (!m_hwndVideoPrompt || !IsWindow(m_hwndVideoPrompt))
        return;

    VideoGenerationJob job;
    job.engineName = "tubi";
    job.prompt = getWindowTextUtf8(m_hwndVideoPrompt);
    job.storyboard = getWindowTextUtf8(m_hwndVideoStoryboard);
    job.provider = getComboTextUtf8(m_hwndVideoProvider);
    job.localModel = getComboTextUtf8(m_hwndVideoLocalModel);
    job.style = getComboTextUtf8(m_hwndVideoStyle);
    job.duration = getComboTextUtf8(m_hwndVideoDuration);
    job.aspectRatio = getComboTextUtf8(m_hwndVideoAspect);
    job.resolution = getComboTextUtf8(m_hwndVideoResolution);
    job.negativePrompt = "blurry, deformed, washed out, flat lighting";
    job.cameraMode = "cinematic-pan";

    if (job.prompt.empty())
    {
        MessageBoxW(m_hwndVideoStudio, L"Enter a prompt first.", L"Video Studio", MB_OK | MB_ICONINFORMATION);
        return;
    }

    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    job.id = "video-" + std::to_string(millis);
    job.seed = static_cast<int>(millis & 0x7fffffff);
    job.status = "Queued";
    job.running = true;

    const std::filesystem::path artifactDir = resolveRawrxdWorkspaceBase() / "video-studio" / job.id;
    std::error_code ec;
    std::filesystem::create_directories(artifactDir, ec);
    job.artifactDir = artifactDir.string();

    {
        std::ofstream req(artifactDir / "render_request.json", std::ios::binary);
        req << "{\n"
            << "  \"job_id\": \"" << escapeJson(job.id) << "\",\n"
            << "  \"engine\": \"" << escapeJson(job.engineName) << "\",\n"
            << "  \"provider\": \"" << escapeJson(job.provider) << "\",\n"
            << "  \"local_model\": \"" << escapeJson(job.localModel) << "\",\n"
            << "  \"style\": \"" << escapeJson(job.style) << "\",\n"
            << "  \"camera_mode\": \"" << escapeJson(job.cameraMode) << "\",\n"
            << "  \"negative_prompt\": \"" << escapeJson(job.negativePrompt) << "\",\n"
            << "  \"seed\": " << job.seed << ",\n"
            << "  \"duration\": \"" << escapeJson(job.duration) << "\",\n"
            << "  \"aspect_ratio\": \"" << escapeJson(job.aspectRatio) << "\",\n"
            << "  \"resolution\": \"" << escapeJson(job.resolution) << "\",\n"
            << "  \"prompt\": \"" << escapeJson(job.prompt) << "\",\n"
            << "  \"storyboard\": \"" << escapeJson(job.storyboard) << "\"\n"
            << "}\n";
    }
    {
        std::ofstream sb(artifactDir / "storyboard.txt", std::ios::binary);
        sb << job.storyboard << "\n";
    }
    {
        std::ofstream readme(artifactDir / "README.txt", std::ios::binary);
        readme << "tubi local-first video job\n\n"
               << "This folder contains a tubi render package, local model selection,\n"
               << "a MASM x64 backend manifest, and a brutal-compressed payload for handoff\n"
               << "to a local or hosted backend.\n";
    }
    std::filesystem::path stagedBackendPath;
    const bool stagedBackend = tryStageBundledTubiBackend(artifactDir, &stagedBackendPath);
    {
        std::ofstream runner(artifactDir / "run_tubi_local.ps1", std::ios::binary);
        runner << "$ErrorActionPreference = 'Stop'\n"
               << "$runner = $env:RAWRXD_TUBI_LOCAL_RUNNER\n"
               << "$fallback = Join-Path $PSScriptRoot 'rawrxd-tubi-backend.exe'\n"
               << "if ([string]::IsNullOrWhiteSpace($runner) -and (Test-Path $fallback)) {\n"
               << "  $runner = $fallback\n"
               << "}\n"
               << "if ([string]::IsNullOrWhiteSpace($runner)) {\n"
               << "  Write-Host 'Set RAWRXD_TUBI_LOCAL_RUNNER or place rawrxd-tubi-backend.exe beside this script.'\n"
               << "  exit 1\n"
               << "}\n"
               << "$argsList = @(\n"
               << "  '--job-id', " << psQuote(job.id) << ",\n"
               << "  '--engine', 'tubi',\n"
               << "  '--provider', " << psQuote(job.provider) << ",\n"
               << "  '--model', " << psQuote(job.localModel) << ",\n"
               << "  '--style', " << psQuote(job.style) << ",\n"
               << "  '--camera', " << psQuote(job.cameraMode) << ",\n"
               << "  '--negative-prompt', " << psQuote(job.negativePrompt) << ",\n"
               << "  '--seed', " << psQuote(std::to_string(job.seed)) << ",\n"
               << "  '--resolution', " << psQuote(job.resolution) << ",\n"
               << "  '--duration', " << psQuote(job.duration) << ",\n"
               << "  '--aspect', " << psQuote(job.aspectRatio) << ",\n"
               << "  '--prompt', " << psQuote(job.prompt) << ",\n"
               << "  '--storyboard', " << psQuote(job.storyboard) << ",\n"
               << "  '--out-dir', " << psQuote(artifactDir.string()) << "\n"
               << ")\n"
               << "& $runner @argsList\n";
    }
    {
        const std::string packageText =
            "engine=tubi\nprovider=" + job.provider + "\nlocal_model=" + job.localModel + "\nstyle=" + job.style +
            "\ncamera=" + job.cameraMode + "\nnegative_prompt=" + job.negativePrompt +
            "\nseed=" + std::to_string(job.seed) + "\nduration=" + job.duration + "\naspect=" + job.aspectRatio +
            "\nresolution=" + job.resolution + "\nprompt=" + job.prompt + "\n\nstoryboard=\n" + job.storyboard + "\n";
        bool compressedOk = false;
        const std::vector<uint8_t> compressed = codec::deflate(toBytes(packageText), &compressedOk);
        if (compressedOk && !compressed.empty())
        {
            const std::filesystem::path pkg = artifactDir / (job.id + ".tubi.brutal");
            std::ofstream out(pkg, std::ios::binary);
            out.write(reinterpret_cast<const char*>(compressed.data()),
                      static_cast<std::streamsize>(compressed.size()));
            job.compressedPackagePath = pkg.string();
            job.brutalCompressionUsed = true;
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_videoJobsMutex);
        m_videoJobs.push_back(job);
    }

    appendToOutput("[tubi] Queued " + job.id + " (" + job.style + ", " + job.duration + ", " + job.aspectRatio + ", " +
                       job.resolution + ", model=" + (job.localModel.empty() ? std::string("none") : job.localModel) +
                       ")\n",
                   "Output", OutputSeverity::Info);
    if (stagedBackend)
    {
        appendToOutput("[tubi] Staged native backend at " + stagedBackendPath.string() + "\n", "Output",
                       OutputSeverity::Info);
    }
    else
    {
        appendToOutput("[tubi] Native backend was not staged into the job folder; runner fallback may require "
                       "RAWRXD_TUBI_LOCAL_RUNNER.\n",
                       "Output", OutputSeverity::Warning);
    }
    if (m_hwndStatusBar)
        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(L"tubi render queued"));
    refreshVideoStudioUi();

    const std::string jobId = job.id;
    const HWND videoWnd = m_hwndVideoStudio;
    std::thread(
        [this, job, jobId, videoWnd, artifactDir]()
        {
            const char* phases[] = {"Resolving local model", "Writing brutal-compressed package",
                                    "Preparing tubi backend request", "Rendering MASM x64 frame sequence"};
            for (const char* phase : phases)
            {
                {
                    std::lock_guard<std::mutex> lock(m_videoJobsMutex);
                    auto* queuedJob = findVideoJobById(jobId);
                    if (!queuedJob || queuedJob->cancelled)
                        return;
                    queuedJob->status = phase;
                }
                if (videoWnd)
                    PostMessageW(videoWnd, WM_RAWR_VIDEO_STUDIO_REFRESH, 0, 0);
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }

            rawrxd::video::TubiRenderRequest request;
            request.jobId = job.id;
            request.engineName = job.engineName;
            request.provider = job.provider;
            request.localModel = job.localModel;
            request.prompt = job.prompt;
            request.storyboard = job.storyboard;
            request.style = job.style;
            request.duration = job.duration;
            request.aspectRatio = job.aspectRatio;
            request.resolution = job.resolution;
            request.negativePrompt = job.negativePrompt;
            request.cameraMode = job.cameraMode;
            request.seed = job.seed;
            request.outputDir = artifactDir;

            const auto renderResult = rawrxd::video::renderVideoClip(request);
            if (!renderResult)
            {
                {
                    std::lock_guard<std::mutex> lock(m_videoJobsMutex);
                    auto* failedJob = findVideoJobById(jobId);
                    if (!failedJob || failedJob->cancelled)
                        return;
                    failedJob->running = false;
                    failedJob->completed = false;
                    failedJob->status = "render failed: " + renderResult.error();
                }
                appendToOutput("[tubi] Render failed for " + jobId + ": " + renderResult.error() + "\n", "Output",
                               OutputSeverity::Error);
                if (videoWnd)
                    PostMessageW(videoWnd, WM_RAWR_VIDEO_STUDIO_REFRESH, 0, 0);
                return;
            }

            {
                std::lock_guard<std::mutex> lock(m_videoJobsMutex);
                auto* finishedJob = findVideoJobById(jobId);
                if (!finishedJob || finishedJob->cancelled)
                    return;
                finishedJob->running = false;
                finishedJob->completed = true;
                finishedJob->status =
                    renderResult->mp4Created ? "render complete (mp4 ready)" : "render complete (frames ready)";
                finishedJob->backendManifestPath = renderResult->manifestPath.string();
                finishedJob->renderedVideoPath = renderResult->mp4Created ? renderResult->mp4Path.string() : "";
                finishedJob->progressPath = renderResult->progressPath.string();
                finishedJob->shotPlanPath = renderResult->shotPlanPath.string();
                finishedJob->contactSheetPath = renderResult->contactSheetPath.string();
                finishedJob->previewStartPath = renderResult->previewStartPath.string();
                finishedJob->previewMidPath = renderResult->previewMidPath.string();
                finishedJob->previewEndPath = renderResult->previewEndPath.string();
                finishedJob->extractedTags = renderResult->extractedTags;
                finishedJob->encoderDiagnostics = renderResult->encoderDiagnostics;
                finishedJob->renderedWidth = renderResult->width;
                finishedJob->renderedHeight = renderResult->height;
                finishedJob->renderedFps = renderResult->fps;
                finishedJob->renderedFrames = renderResult->totalFrames;
                finishedJob->renderedDurationSeconds = renderResult->durationSeconds;
                finishedJob->shotCount = renderResult->shotCount;
            }

            appendToOutput("[tubi] Render complete for " + jobId +
                               (renderResult->mp4Created ? " (mp4 ready)\n" : " (frame sequence ready)\n"),
                           "Output", OutputSeverity::Success);
            appendToOutput(
                "[tubi] contact sheet=" + renderResult->contactSheetPath.string() + " | tags=" +
                    (renderResult->extractedTags.empty() ? std::string("none") : renderResult->extractedTags) + "\n",
                "Output", OutputSeverity::Info);
            if (videoWnd)
                PostMessageW(videoWnd, WM_RAWR_VIDEO_STUDIO_REFRESH, 0, 0);
        })
        .detach();
}

LRESULT CALLBACK Win32IDE::VideoStudioProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* ide = reinterpret_cast<Win32IDE*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (uMsg == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    switch (uMsg)
    {
        case WM_SIZE:
            if (ide)
                ide->layoutVideoStudioControls();
            return 0;
        case WM_COMMAND:
            if (!ide)
                break;
            switch (LOWORD(wParam))
            {
                case IDC_VIDEO_GENERATE:
                    ide->startVideoStudioRender();
                    return 0;
                case IDC_VIDEO_USE_CHAT:
                    ide->useChatPromptForVideoStudio();
                    return 0;
                case IDC_VIDEO_OPEN_FOLDER:
                    ide->openVideoStudioOutputFolder();
                    return 0;
                case IDC_VIDEO_CANCEL:
                    ide->cancelSelectedVideoStudioJob();
                    return 0;
                case IDC_VIDEO_JOBS:
                    if (HIWORD(wParam) == LBN_SELCHANGE)
                    {
                        ide->refreshVideoStudioUi();
                        return 0;
                    }
                    break;
                case IDC_VIDEO_PROVIDER:
                case IDC_VIDEO_LOCAL_MODEL:
                case IDC_VIDEO_RESOLUTION:
                    if (HIWORD(wParam) == CBN_SELCHANGE)
                    {
                        ide->refreshVideoStudioUi();
                        return 0;
                    }
                    break;
                default:
                    break;
            }
            break;
        case WM_RAWR_VIDEO_STUDIO_REFRESH:
            if (ide)
                ide->refreshVideoStudioUi();
            return 0;
        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        case WM_DESTROY:
            if (ide)
                ide->m_hwndVideoStudio = nullptr;
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}
