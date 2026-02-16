// problems_panel_bridge.cpp — C API for Problems Panel (P0)
// Exports ProblemsPanel_AddDiagnostic / ProblemsPanel_ClearSource for SAST, SCA, Secrets, Build.
// Forwards to ProblemsAggregator so the unified Problems view shows all sources.

#include "problems_aggregator.hpp"
#include <string>
#include <vector>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

namespace RawrXD {

namespace {

std::string wideToUtf8(const wchar_t* wstr) {
    if (!wstr) return {};
#ifdef _WIN32
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &out[0], len, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
#else
    (void)wstr;
    return {};
#endif
}

} // namespace

} // namespace RawrXD

extern "C" {

#ifdef _WIN32
__declspec(dllexport)
#endif
void ProblemsPanel_AddDiagnostic(
    const wchar_t* file,
    int line,
    int col,
    const wchar_t* message,
    const wchar_t* code,
    int severity,
    const wchar_t* source)
{
    if (!message || !source) return;
    std::string path = RawrXD::wideToUtf8(file);
    std::string msg  = RawrXD::wideToUtf8(message);
    std::string cd   = RawrXD::wideToUtf8(code ? code : L"");
    std::string src  = RawrXD::wideToUtf8(source);
    // Map spec severity 0=Error, 1=Warning, 2=Info, 3=Hint -> aggregator 1=Error, 2=Warning, 3=Info, 4=Hint
    int aggSev = (severity >= 0 && severity <= 3) ? (severity + 1) : 2;
    RawrXD::ProblemsAggregator::instance().add(src, path, line, col, aggSev, cd, msg, cd);
}

#ifdef _WIN32
__declspec(dllexport)
#endif
void ProblemsPanel_ClearSource(const wchar_t* source)
{
    if (!source) return;
    std::string src = RawrXD::wideToUtf8(source);
    RawrXD::ProblemsAggregator::instance().clear(src);
}

} // extern "C"
