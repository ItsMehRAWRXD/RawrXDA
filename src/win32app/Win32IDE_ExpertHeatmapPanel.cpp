// Win32 expert heatmap: virtualized grid + swarm stats HUD (GDI, Rose Pine palette).
#include "../cpu_inference_engine.h"
#include "Win32IDE.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <commctrl.h>
#include <windows.h>
#include <windowsx.h>

namespace
{
constexpr UINT_PTR kHeatmapTimerId = 1;
constexpr UINT WM_RAWR_HEATMAP_FRAME = WM_APP + 107;
constexpr int kCellPx = 8;
constexpr int kHudTopH = 56;
constexpr int kSparkH = 24;
constexpr int kToolbarH = 26;
constexpr UINT_PTR kIdStarve = 2001;
constexpr UINT_PTR kIdCold = 2002;
constexpr UINT_PTR kIdDense = 2003;
constexpr UINT_PTR kIdCapture = 2004;

struct CellKey
{
    std::uint32_t modelIndex = 0;
    std::uint32_t layerStart = 0;
    std::uint32_t expertIndex = 0;

    bool operator<(const CellKey& o) const
    {
        if (modelIndex != o.modelIndex)
            return modelIndex < o.modelIndex;
        if (layerStart != o.layerStart)
            return layerStart < o.layerStart;
        return expertIndex < o.expertIndex;
    }
};

struct AggCell
{
    std::uint32_t holdMax = 0;
    bool resident = false;
    bool prefetchInFlight = false;
    std::uint64_t bytesMax = 0;
    std::uint64_t touchMax = 0;
    std::uint32_t layerEnd = 0;
    std::size_t planRowIndex = 0;
};

struct FrameSnapshot
{
    std::uint64_t snapshotId = 0;
    std::uint64_t planGeneration = 0;
    bool stale = true;
    RawrXD::Swarm::SwarmRuntimeStats stats{};
    std::map<CellKey, AggCell> aggs;
    std::vector<std::uint32_t> layerOrder;
    std::vector<std::uint32_t> expertOrder;
};

struct HeatmapPanelCtx
{
    HWND hwnd = nullptr;
    std::atomic<bool> captureBusy{false};
    std::atomic<bool> destroyed{false};
    std::mutex frameMu;
    FrameSnapshot shown;
    HWND cbStarve = nullptr;
    HWND cbCold = nullptr;
    HWND cbDense = nullptr;
    HWND btnCapture = nullptr;
    int scrollX = 0;
    int scrollY = 0;
    int hoverRow = -1;
    int hoverCol = -1;
    std::array<std::uint16_t, 64> sparkInUse{};
    std::array<std::uint16_t, 64> sparkPinMs{};
    std::size_t sparkIdx = 0;
};

static const wchar_t kHeatmapClass[] = L"RawrXDExpertHeatmapPanel";

static void updateScrollbars(HWND hwnd, HeatmapPanelCtx* ctx, const FrameSnapshot& snap)
{
    if (!ctx || !hwnd)
        return;

    RECT cr{};
    GetClientRect(hwnd, &cr);

    const int gridTop = kHudTopH + kSparkH + kToolbarH;
    const int viewH = std::max(0, static_cast<int>(cr.bottom) - gridTop);
    const int viewW = static_cast<int>(cr.right);
    const int gridW = static_cast<int>(snap.expertOrder.size()) * kCellPx;
    const int gridH = static_cast<int>(snap.layerOrder.size()) * kCellPx;

    SCROLLINFO si = {sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS};
    si.nMin = 0;
    si.nMax = std::max(0, gridH - 1);
    si.nPage = static_cast<UINT>(std::max(1, viewH));
    si.nPos = std::min<int>(ctx->scrollY, std::max(0, gridH - viewH));
    ctx->scrollY = si.nPos;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

    si.nMax = std::max(0, gridW - 1);
    si.nPage = static_cast<UINT>(std::max(1, viewW));
    si.nPos = std::min<int>(ctx->scrollX, std::max(0, gridW - viewW));
    ctx->scrollX = si.nPos;
    SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
}

static void aggregateSnapshot(const RawrXD::Swarm::ExpertHeatmapSnapshot& snap, FrameSnapshot& out)
{
    out.snapshotId = snap.snapshotId;
    out.planGeneration = snap.planGeneration;
    out.stats = snap.statsAtSample;
    out.aggs.clear();
    // Fail-closed: cap cell iteration at 100k entries to prevent unbounded aggregation
    static constexpr size_t kMaxCells = 100000;
    size_t cellCount = 0;
    for (const RawrXD::Swarm::ExpertHeatmapCell& c : snap.cells)
    {
        if (cellCount++ >= kMaxCells)
            break;
        // Validate indices are in reasonable ranges
        if (c.modelIndex > 255 || c.layerStart > 10000 || c.expertIndex > 65535)
            continue;
        CellKey k{c.modelIndex, c.layerStart, c.expertIndex};
        AggCell& a = out.aggs[k];
        a.holdMax = std::max(a.holdMax, c.holdCount);
        a.resident = a.resident || c.resident;
        a.prefetchInFlight = a.prefetchInFlight || c.prefetchInFlight;
        a.bytesMax = std::max(a.bytesMax, c.residentBytes);
        if (c.resident)
            a.touchMax = std::max(a.touchMax, c.lastTouchSequence);
        a.layerEnd = c.layerEnd;
        a.planRowIndex = c.planRowIndex;
    }
    std::unordered_map<std::uint32_t, bool> seenL;
    std::unordered_map<std::uint32_t, bool> seenE;
    for (const auto& kv : out.aggs)
    {
        seenL[kv.first.layerStart] = true;
        seenE[kv.first.expertIndex] = true;
    }
    out.layerOrder.clear();
    for (const auto& p : seenL)
        out.layerOrder.push_back(p.first);
    std::sort(out.layerOrder.begin(), out.layerOrder.end());
    out.expertOrder.clear();
    for (const auto& p : seenE)
        out.expertOrder.push_back(p.first);
    std::sort(out.expertOrder.begin(), out.expertOrder.end(),
              [](std::uint32_t a, std::uint32_t b)
              {
                  const bool da = (a == 0xFFFFFFFFu);
                  const bool db = (b == 0xFFFFFFFFu);
                  if (da != db)
                      return !da && db;
                  return a < b;
              });
}

static void runHeatmapCapture(HWND hwnd, HeatmapPanelCtx* ctx)
{
    if (!ctx || ctx->destroyed.load(std::memory_order_acquire))
        return;
    auto eng = RawrXD::CPUInferenceEngine::GetSharedInstance();
    FrameSnapshot frame;
    frame.stale = true;
    if (!eng || !eng->IsModelLoaded())
    {
        std::lock_guard<std::mutex> lk(ctx->frameMu);
        ctx->shown = frame;
        PostMessageW(hwnd, WM_RAWR_HEATMAP_FRAME, 0, 0);
        ctx->captureBusy.store(false, std::memory_order_release);
        return;
    }
    RawrXD::Swarm::ExpertHeatmapCaptureParams cap;
    cap.modelIndex = 0;
    cap.planRowStride = 8;
    cap.maxCells = 4000;
    cap.expertsOnly = true;
    RawrXD::Swarm::ExpertHeatmapSnapshot snap;
    if (!eng->CaptureSwarmExpertHeatmap(cap, snap))
    {
        std::lock_guard<std::mutex> lk(ctx->frameMu);
        ctx->shown = frame;
        PostMessageW(hwnd, WM_RAWR_HEATMAP_FRAME, 0, 0);
        ctx->captureBusy.store(false, std::memory_order_release);
        return;
    }
    const std::uint64_t liveGen = eng->SwarmPlanGeneration();
    aggregateSnapshot(snap, frame);
    frame.stale = (snap.planGeneration != liveGen);
    {
        std::lock_guard<std::mutex> lk(ctx->frameMu);
        ctx->shown = std::move(frame);
        const std::uint32_t iu = ctx->shown.stats.inUseSliceCount;
        const std::uint64_t pinA = ctx->shown.stats.pinBlockAttempts;
        const std::uint64_t pinMs = ctx->shown.stats.pinBlockLatencyMsTotal;
        const std::uint16_t pinAvg =
            static_cast<std::uint16_t>(pinA ? std::min<std::uint64_t>(pinMs / pinA, 65535ull) : 0);
        ctx->sparkInUse[ctx->sparkIdx % ctx->sparkInUse.size()] =
            static_cast<std::uint16_t>(std::min<std::uint32_t>(iu, 200u));
        ctx->sparkPinMs[ctx->sparkIdx % ctx->sparkPinMs.size()] = pinAvg;
        ++ctx->sparkIdx;
    }
    PostMessageW(hwnd, WM_RAWR_HEATMAP_FRAME, 0, 0);
    ctx->captureBusy.store(false, std::memory_order_release);
}

static bool filterPass(const AggCell& a, std::uint32_t expert, bool fStarve, bool fCold, bool fDense)
{
    if (!fStarve && !fCold && !fDense)
        return true;
    bool ok = false;
    if (fStarve && a.holdMax > 0 && !a.resident)
        ok = true;
    if (fCold && !a.resident && !a.prefetchInFlight)
        ok = true;
    if (fDense && expert == 0xFFFFFFFFu)
        ok = true;
    return ok;
}

static LRESULT CALLBACK HeatmapPanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* ctx = reinterpret_cast<HeatmapPanelCtx*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg)
    {
        case WM_CREATE:
        {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            auto* nctx = new HeatmapPanelCtx();
            nctx->hwnd = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(nctx));
            ctx = nctx;
            CreateWindowExW(0, L"BUTTON", L"Starvation", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 4, kHudTopH + kSparkH,
                            100, 22, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kIdStarve)), cs->hInstance,
                            nullptr);
            CreateWindowExW(0, L"BUTTON", L"Cold miss", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 110,
                            kHudTopH + kSparkH, 90, 22, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kIdCold)),
                            cs->hInstance, nullptr);
            CreateWindowExW(0, L"BUTTON", L"Dense only", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 206,
                            kHudTopH + kSparkH, 95, 22, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kIdDense)),
                            cs->hInstance, nullptr);
            CreateWindowExW(0, L"BUTTON", L"Copy capture id", WS_CHILD | WS_VISIBLE, 310, kHudTopH + kSparkH, 120, 22,
                            hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kIdCapture)), cs->hInstance, nullptr);
            ctx->cbStarve = GetDlgItem(hwnd, static_cast<int>(kIdStarve));
            ctx->cbCold = GetDlgItem(hwnd, static_cast<int>(kIdCold));
            ctx->cbDense = GetDlgItem(hwnd, static_cast<int>(kIdDense));
            ctx->btnCapture = GetDlgItem(hwnd, static_cast<int>(kIdCapture));
            SetTimer(hwnd, kHeatmapTimerId, 1000, nullptr);
            return 0;
        }
        case WM_DESTROY:
            if (ctx)
            {
                ctx->destroyed.store(true, std::memory_order_release);
                KillTimer(hwnd, kHeatmapTimerId);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                delete ctx;
            }
            return 0;
        case WM_TIMER:
            if (wParam == kHeatmapTimerId && ctx && !ctx->captureBusy.exchange(true, std::memory_order_acq_rel))
            {
                std::thread([hwnd, ctx]() { runHeatmapCapture(hwnd, ctx); }).detach();
            }
            return 0;
        case WM_RAWR_HEATMAP_FRAME:
            if (ctx)
            {
                FrameSnapshot local;
                {
                    std::lock_guard<std::mutex> lk(ctx->frameMu);
                    local = ctx->shown;
                }
                updateScrollbars(hwnd, ctx, local);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == kIdCapture && ctx)
            {
                std::uint64_t sid = 0;
                std::uint64_t pg = 0;
                {
                    std::lock_guard<std::mutex> lk(ctx->frameMu);
                    sid = ctx->shown.snapshotId;
                    pg = ctx->shown.planGeneration;
                }
                wchar_t clip[128];
                swprintf_s(clip, L"snapshotId=%llu planGeneration=%llu", static_cast<unsigned long long>(sid),
                           static_cast<unsigned long long>(pg));
                if (OpenClipboard(hwnd))
                {
                    EmptyClipboard();
                    const SIZE_T nbytes = (wcslen(clip) + 1) * sizeof(wchar_t);
                    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, nbytes);
                    if (h)
                    {
                        void* p = GlobalLock(h);
                        if (p)
                        {
                            memcpy(p, clip, nbytes);
                            GlobalUnlock(h);
                            SetClipboardData(CF_UNICODETEXT, h);
                        }
                    }
                    CloseClipboard();
                }
            }
            else if (HIWORD(wParam) == BN_CLICKED)
                InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_MOUSEMOVE:
        {
            if (!ctx)
                break;
            int mx = GET_X_LPARAM(lParam);
            int my = GET_Y_LPARAM(lParam);
            const int gridTop = kHudTopH + kSparkH + kToolbarH;
            if (my < gridTop)
            {
                if (ctx->hoverRow != -1 || ctx->hoverCol != -1)
                {
                    ctx->hoverRow = ctx->hoverCol = -1;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                break;
            }
            FrameSnapshot local;
            {
                std::lock_guard<std::mutex> lk(ctx->frameMu);
                local = ctx->shown;
            }
            const int ncols = static_cast<int>(local.expertOrder.size());
            const int nrows = static_cast<int>(local.layerOrder.size());
            if (ncols <= 0 || nrows <= 0)
                break;
            const int gx = mx + ctx->scrollX;
            const int gy = my - gridTop + ctx->scrollY;
            const int col = gx / kCellPx;
            const int row = gy / kCellPx;
            if (col >= 0 && col < ncols && row >= 0 && row < nrows)
            {
                if (row != ctx->hoverRow || col != ctx->hoverCol)
                {
                    ctx->hoverRow = row;
                    ctx->hoverCol = col;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
            else if (ctx->hoverRow != -1)
            {
                ctx->hoverRow = ctx->hoverCol = -1;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            break;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (!ctx)
            {
                EndPaint(hwnd, &ps);
                return 0;
            }
            RECT cr;
            GetClientRect(hwnd, &cr);
            const COLORREF bg = RGB(0x19, 0x17, 0x24);
            const COLORREF cellCold = RGB(0x26, 0x23, 0x3a);
            const COLORREF cellRes = RGB(0x31, 0x74, 0x8f);
            const COLORREF cellDense = RGB(0xc4, 0xa7, 0xe7);
            const COLORREF gold = RGB(0xf6, 0xc1, 0x77);
            const COLORREF love = RGB(0xeb, 0x6f, 0x92);
            HBRUSH brBg = CreateSolidBrush(bg);
            FillRect(hdc, &cr, brBg);
            DeleteObject(brBg);

            FrameSnapshot local;
            std::uint64_t sparkPos = 0;
            std::array<std::uint16_t, 64> spI{};
            std::array<std::uint16_t, 64> spP{};
            {
                std::lock_guard<std::mutex> lk(ctx->frameMu);
                local = ctx->shown;
                sparkPos = ctx->sparkIdx;
                spI = ctx->sparkInUse;
                spP = ctx->sparkPinMs;
            }

            const std::uint64_t pinA = local.stats.pinBlockAttempts;
            const double pinMsAvg =
                pinA ? static_cast<double>(local.stats.pinBlockLatencyMsTotal) / static_cast<double>(pinA) : 0.0;
            // Cap pinMsAvg at 999999.99 to prevent buffer overflow in swprintf_s
            const double safePinMsAvg = std::min(pinMsAvg, 999999.99);
            // Cap stat values to prevent display overflow
            const unsigned int safeInUse = std::min(local.stats.inUseSliceCount, 999999u);
            wchar_t hud[512];
            swprintf_s(hud,
                       L"Expert heatmap  %ls  snap=%llu gen=%llu  in_use=%u  ev_rej=%llu ev_starve=%llu  "
                       L"pin_blk=%llu to=%llu pin_ms_avg=%.2f  pf_enq_dup=%llu jit_cold=%llu",
                       local.stale ? L"[STALE]" : L"[live]", static_cast<unsigned long long>(local.snapshotId),
                       static_cast<unsigned long long>(local.planGeneration), safeInUse,
                       static_cast<unsigned long long>(local.stats.evictionRejectedInUse),
                       static_cast<unsigned long long>(local.stats.evictStarvation),
                       static_cast<unsigned long long>(local.stats.pinBlockAttempts),
                       static_cast<unsigned long long>(local.stats.pinBlockTimeouts), safePinMsAvg,
                       static_cast<unsigned long long>(local.stats.prefetchEnqueueSkippedDuplicate),
                       static_cast<unsigned long long>(local.stats.jitPinNonResident));
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(0xe0, 0xde, 0xf4));
            RECT hudRc{4, 4, cr.right - 4, kHudTopH};
            DrawTextW(hdc, hud, -1, &hudRc, DT_LEFT | DT_WORDBREAK);

            HPEN penGold = CreatePen(PS_SOLID, 1, gold);
            HPEN penLove = CreatePen(PS_SOLID, 2, love);
            HPEN penOld = static_cast<HPEN>(SelectObject(hdc, penGold));
            const int sparkY = kHudTopH + 4;
            const int sparkW = (cr.right - 8) / 2;
            // Validate sparkPos to ensure modulo arithmetic doesn't overflow
            const std::size_t safeSparkPos = sparkPos % 64;
            for (int i = 1; i < 64; ++i)
            {
                int x0 = 4 + (i - 1) * sparkW / 63;
                int x1 = 4 + i * sparkW / 63;
                const std::size_t idx0 = (safeSparkPos + static_cast<std::size_t>(i - 1)) % 64;
                const std::size_t idx1 = (safeSparkPos + static_cast<std::size_t>(i)) % 64;
                const int y0 = std::min(18, static_cast<int>(spI[idx0]));
                const int y1 = std::min(18, static_cast<int>(spI[idx1]));
                MoveToEx(hdc, x0, sparkY + 20 - y0, nullptr);
                LineTo(hdc, x1, sparkY + 20 - y1);
            }
            SelectObject(hdc, penLove);
            const int xOff = 4 + sparkW + 8;
            for (int i = 1; i < 64; ++i)
            {
                int x0 = xOff + (i - 1) * sparkW / 63;
                int x1 = xOff + i * sparkW / 63;
                const std::size_t idx0 = (safeSparkPos + static_cast<std::size_t>(i - 1)) % 64;
                const std::size_t idx1 = (safeSparkPos + static_cast<std::size_t>(i)) % 64;
                const int py0 = std::min(20, static_cast<int>(spP[idx0]));
                const int py1 = std::min(20, static_cast<int>(spP[idx1]));
                MoveToEx(hdc, x0, sparkY + 20 - py0, nullptr);
                LineTo(hdc, x1, sparkY + 20 - py1);
            }
            SelectObject(hdc, penOld);
            DeleteObject(penGold);
            DeleteObject(penLove);

            const bool fStarve =
                ctx && ctx->cbStarve && (SendMessageW(ctx->cbStarve, BM_GETCHECK, 0, 0) == BST_CHECKED);
            const bool fCold = ctx && ctx->cbCold && (SendMessageW(ctx->cbCold, BM_GETCHECK, 0, 0) == BST_CHECKED);
            const bool fDense = ctx && ctx->cbDense && (SendMessageW(ctx->cbDense, BM_GETCHECK, 0, 0) == BST_CHECKED);

            const int gridTop = kHudTopH + kSparkH + kToolbarH;
            std::unordered_map<std::uint32_t, int> layerRow;
            std::unordered_map<std::uint32_t, int> expertCol;
            for (std::size_t i = 0; i < local.layerOrder.size(); ++i)
                layerRow[local.layerOrder[i]] = static_cast<int>(i);
            for (std::size_t i = 0; i < local.expertOrder.size(); ++i)
                expertCol[local.expertOrder[i]] = static_cast<int>(i);

            const int ncols = static_cast<int>(local.expertOrder.size());
            const int nrows = static_cast<int>(local.layerOrder.size());
            const int gridW = ncols * kCellPx;
            const int gridH = nrows * kCellPx;
            SetWindowOrgEx(hdc, -ctx->scrollX, -ctx->scrollY - gridTop, nullptr);

            for (const auto& kv : local.aggs)
            {
                const auto lr = layerRow.find(kv.first.layerStart);
                const auto ec = expertCol.find(kv.first.expertIndex);
                if (lr == layerRow.end() || ec == expertCol.end())
                    continue;
                const AggCell& ac = kv.second;
                if (!filterPass(ac, kv.first.expertIndex, fStarve, fCold, fDense))
                    continue;
                RECT rc{ec->second * kCellPx, lr->second * kCellPx, (ec->second + 1) * kCellPx,
                        (lr->second + 1) * kCellPx};
                COLORREF fill = ac.resident ? cellRes : cellCold;
                if (kv.first.expertIndex == 0xFFFFFFFFu)
                    fill = cellDense;
                HBRUSH br = CreateSolidBrush(fill);
                FillRect(hdc, &rc, br);
                DeleteObject(br);
                if (ac.prefetchInFlight)
                {
                    HBRUSH bnull = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
                    HGDIOBJ old = SelectObject(hdc, bnull);
                    HPEN pf = CreatePen(PS_SOLID, 2, gold);
                    SelectObject(hdc, pf);
                    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
                    SelectObject(hdc, old);
                    DeleteObject(pf);
                }
                if (ac.holdMax > 0)
                {
                    RECT dot{rc.left + 1, rc.top + 1, rc.left + 4, rc.top + 4};
                    HBRUSH brL = CreateSolidBrush(love);
                    FillRect(hdc, &dot, brL);
                    DeleteObject(brL);
                }
            }
            SetWindowOrgEx(hdc, 0, 0, nullptr);

            if (ctx && ctx->hoverRow >= 0 && ctx->hoverCol >= 0 && ctx->hoverRow < nrows && ctx->hoverCol < ncols)
            {
                const std::uint32_t lay = local.layerOrder[static_cast<std::size_t>(ctx->hoverRow)];
                const std::uint32_t exp = local.expertOrder[static_cast<std::size_t>(ctx->hoverCol)];
                CellKey hk{0, lay, exp};
                auto it = local.aggs.find(hk);
                if (it != local.aggs.end())
                {
                    const AggCell& ac = it->second;
                    wchar_t tip[384];
                    swprintf_s(tip,
                               L"L%u-%u expert=%u row=%zu  res=%d hold=%u pf=%d bytes=%llu touch=%llu  "
                               L"planGen=%llu snap=%llu",
                               lay, ac.layerEnd, exp, ac.planRowIndex, ac.resident ? 1 : 0, ac.holdMax,
                               ac.prefetchInFlight ? 1 : 0, static_cast<unsigned long long>(ac.bytesMax),
                               static_cast<unsigned long long>(ac.touchMax),
                               static_cast<unsigned long long>(local.planGeneration),
                               static_cast<unsigned long long>(local.snapshotId));
                    RECT tr{4, cr.bottom - 22, cr.right - 4, cr.bottom - 2};
                    DrawTextW(hdc, tip, -1, &tr, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
                }
            }

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_VSCROLL:
            if (ctx)
            {
                SCROLLINFO si = {sizeof(si), SIF_ALL};
                GetScrollInfo(hwnd, SB_VERT, &si);
                switch (LOWORD(wParam))
                {
                    case SB_LINEUP:
                        ctx->scrollY -= kCellPx;
                        break;
                    case SB_LINEDOWN:
                        ctx->scrollY += kCellPx;
                        break;
                    case SB_PAGEUP:
                        ctx->scrollY -= static_cast<int>(si.nPage);
                        break;
                    case SB_PAGEDOWN:
                        ctx->scrollY += static_cast<int>(si.nPage);
                        break;
                    case SB_THUMBTRACK:
                    case SB_THUMBPOSITION:
                        ctx->scrollY = static_cast<int>(HIWORD(wParam));
                        break;
                    default:
                        break;
                }
                ctx->scrollY = std::max(0, ctx->scrollY);
                si.fMask = SIF_POS;
                si.nPos = ctx->scrollY;
                SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_HSCROLL:
            if (ctx)
            {
                SCROLLINFO si = {sizeof(si), SIF_ALL};
                GetScrollInfo(hwnd, SB_HORZ, &si);
                switch (LOWORD(wParam))
                {
                    case SB_LINELEFT:
                        ctx->scrollX -= kCellPx;
                        break;
                    case SB_LINERIGHT:
                        ctx->scrollX += kCellPx;
                        break;
                    case SB_PAGELEFT:
                        ctx->scrollX -= static_cast<int>(si.nPage);
                        break;
                    case SB_PAGERIGHT:
                        ctx->scrollX += static_cast<int>(si.nPage);
                        break;
                    case SB_THUMBTRACK:
                    case SB_THUMBPOSITION:
                        ctx->scrollX = static_cast<int>(HIWORD(wParam));
                        break;
                    default:
                        break;
                }
                ctx->scrollX = std::max(0, ctx->scrollX);
                si.fMask = SIF_POS;
                si.nPos = ctx->scrollX;
                SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_SIZE:
            if (ctx)
            {
                FrameSnapshot local;
                {
                    std::lock_guard<std::mutex> lk(ctx->frameMu);
                    local = ctx->shown;
                }
                updateScrollbars(hwnd, ctx, local);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
}  // namespace

void RawrXD_RegisterExpertHeatmapWindowClass(HINSTANCE hi)
{
    static std::once_flag s_once;
    std::call_once(s_once,
                   [hi]()
                   {
                       WNDCLASSEXW wc = {sizeof(wc)};
                       wc.lpfnWndProc = HeatmapPanelProc;
                       wc.hInstance = hi;
                       wc.lpszClassName = kHeatmapClass;
                       wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
                       wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
                       RegisterClassExW(&wc);
                   });
}

void Win32IDE::toggleExpertHeatmapPanel()
{
    if (m_hwndExpertHeatmapPanel && IsWindow(m_hwndExpertHeatmapPanel))
    {
        DestroyWindow(m_hwndExpertHeatmapPanel);
        m_hwndExpertHeatmapPanel = nullptr;
        return;
    }
    RawrXD_RegisterExpertHeatmapWindowClass(m_hInstance);
    RECT r{};
    if (m_hwndMain && IsWindow(m_hwndMain))
        GetWindowRect(m_hwndMain, &r);
    const int w = 920;
    const int h = 640;
    m_hwndExpertHeatmapPanel = CreateWindowExW(WS_EX_TOOLWINDOW, kHeatmapClass, L"RawrXD — Expert heatmap (swarm)",
                                               WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL, r.left + 40, r.top + 40,
                                               w, h, m_hwndMain, nullptr, m_hInstance, nullptr);
    if (m_hwndExpertHeatmapPanel)
        ShowWindow(m_hwndExpertHeatmapPanel, SW_SHOW);
}
