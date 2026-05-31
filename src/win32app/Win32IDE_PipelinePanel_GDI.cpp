// Win32IDE_PipelinePanel_GDI.cpp — GDI+ DAG Visualization Implementation
// Hierarchical layout engine + retained-mode renderer for agentic execution traces
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#include "Win32IDE_PipelinePanel_GDI.h"
#include <windowsx.h>
#include <cmath>
#include <algorithm>

// ============================================================================
// Static GDI+ Initialization
// ============================================================================

ULONG_PTR Win32IDE_PipelinePanel::s_gdiplusToken = 0;
int Win32IDE_PipelinePanel::s_instanceCount = 0;

// ============================================================================
// DagLayoutEngine Implementation
// ============================================================================

void DagLayoutEngine::ComputeLayout(std::map<std::string, DagNode>& nodes)
{
    if (nodes.empty()) return;

    // Pass 1: Insert dummy nodes for edges that span multiple layers
    InsertDummyNodes(nodes);

    // Pass 2: Assign ranks (longest path from roots)
    AssignRanks(nodes);

    // Pass 3: Assign horizontal positions within each rank
    AssignPositions(nodes);

    // Pass 4: Map logical coordinates to screen pixels
    AssignCoordinates(nodes);
}

void DagLayoutEngine::InsertDummyNodes(std::map<std::string, DagNode>& nodes)
{
    // For each edge that spans more than one rank, insert intermediate
    // dummy nodes to create a proper layered graph structure
    std::vector<std::pair<std::string, std::string>> edgesToProcess;
    for (auto& [id, node] : nodes) {
        for (const auto& childId : node.children) {
            edgesToProcess.emplace_back(id, childId);
        }
    }

    for (const auto& [parentId, childId] : edgesToProcess) {
        auto parentIt = nodes.find(parentId);
        auto childIt = nodes.find(childId);
        if (parentIt == nodes.end() || childIt == nodes.end()) continue;

        int rankDiff = childIt->second.rank - parentIt->second.rank;
        if (rankDiff > 1) {
            // Insert dummy nodes for intermediate layers
            std::string prevId = parentId;
            for (int r = parentIt->second.rank + 1; r < childIt->second.rank; ++r) {
                std::string dummyId = "_dummy_" + parentId + "_" + childId + "_" + std::to_string(r);
                DagNode dummy;
                dummy.id = dummyId;
                dummy.label = "";
                dummy.rank = r;
                dummy.status = DagNodeStatus::Pending;
                nodes[dummyId] = std::move(dummy);

                // Wire: prev -> dummy
                nodes[prevId].children.push_back(dummyId);
                nodes[dummyId].parents.push_back(prevId);

                prevId = dummyId;
            }
            // Wire: last dummy -> child
            nodes[prevId].children.push_back(childId);
            // Remove direct parent->child edge, replace with parent->first_dummy
            auto& parentChildren = parentIt->second.children;
            parentChildren.erase(
                std::remove(parentChildren.begin(), parentChildren.end(), childId),
                parentChildren.end()
            );
        }
    }
}

void DagLayoutEngine::AssignRanks(std::map<std::string, DagNode>& nodes)
{
    // Initialize all ranks to 0
    for (auto& [id, node] : nodes) {
        node.rank = 0;
    }

    // Iterative longest-path algorithm
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& [id, node] : nodes) {
            for (const auto& childId : node.children) {
                auto it = nodes.find(childId);
                if (it != nodes.end() && it->second.rank < node.rank + 1) {
                    it->second.rank = node.rank + 1;
                    changed = true;
                }
            }
        }
    }
}

void DagLayoutEngine::AssignPositions(std::map<std::string, DagNode>& nodes)
{
    // Group nodes by rank
    std::map<int, std::vector<std::string>> rankGroups;
    for (auto& [id, node] : nodes) {
        rankGroups[node.rank].push_back(id);
    }

    // Barycenter heuristic: position nodes based on average parent position
    for (auto& [rank, nodeIds] : rankGroups) {
        if (rank == 0) {
            // Root layer: simple left-to-right ordering
            for (size_t i = 0; i < nodeIds.size(); ++i) {
                nodes[nodeIds[i]].position = static_cast<int>(i);
            }
            continue;
        }

        // Calculate barycenter for each node
        std::vector<std::pair<float, std::string>> barycenters;
        for (const auto& id : nodeIds) {
            auto& node = nodes[id];
            if (node.parents.empty()) {
                barycenters.emplace_back(0.0f, id);
                continue;
            }

            float sum = 0.0f;
            int count = 0;
            for (const auto& parentId : node.parents) {
                auto it = nodes.find(parentId);
                if (it != nodes.end()) {
                    sum += static_cast<float>(it->second.position);
                    ++count;
                }
            }
            barycenters.emplace_back(count > 0 ? sum / count : 0.0f, id);
        }

        // Sort by barycenter to minimize crossings
        std::sort(barycenters.begin(), barycenters.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

        // Assign positions
        for (size_t i = 0; i < barycenters.size(); ++i) {
            nodes[barycenters[i].second].position = static_cast<int>(i);
        }
    }
}

void DagLayoutEngine::AssignCoordinates(std::map<std::string, DagNode>& nodes)
{
    for (auto& [id, node] : nodes) {
        node.bounds = Gdiplus::RectF(
            node.position * (NODE_WIDTH + MARGIN_X) + 50.0f,
            node.rank * (NODE_HEIGHT + MARGIN_Y) + 50.0f,
            NODE_WIDTH,
            NODE_HEIGHT
        );
    }
}

// ============================================================================
// PipelineRenderer Implementation
// ============================================================================

void PipelineRenderer::DrawGraph(Gdiplus::Graphics* g,
    const std::map<std::string, DagNode>& graph,
    const std::string& selectedNodeId,
    const std::string& activeNodeId)
{
    // Draw edges first (behind nodes)
    for (const auto& [id, node] : graph) {
        for (const auto& childId : node.children) {
            auto childIt = graph.find(childId);
            if (childIt != graph.end()) {
                DrawEdge(g, node, childIt->second);
            }
        }
    }

    // Draw nodes
    for (const auto& [id, node] : graph) {
        bool isSelected = (id == selectedNodeId);
        bool isActive = (id == activeNodeId);
        DrawNode(g, node, isSelected, isActive);
    }
}

void PipelineRenderer::DrawNode(Gdiplus::Graphics* g, const DagNode& node,
    bool isSelected, bool isActive)
{
    // Skip dummy nodes (invisible)
    if (node.id.find("_dummy_") == 0) return;

    Gdiplus::GraphicsPath path;
    float r = 8.0f; // Corner radius

    path.AddArc(node.bounds.X, node.bounds.Y, r, r, 180, 90);
    path.AddArc(node.bounds.GetRight() - r, node.bounds.Y, r, r, 270, 90);
    path.AddArc(node.bounds.GetRight() - r, node.bounds.GetBottom() - r, r, r, 0, 90);
    path.AddArc(node.bounds.X, node.bounds.GetBottom() - r, r, r, 90, 90);
    path.CloseFigure();

    // Status-based gradient fill
    Gdiplus::Color topColor = StatusToGradientTop(node.status);
    Gdiplus::Color bottomColor = StatusToGradientBottom(node.status);
    Gdiplus::LinearGradientBrush brush(node.bounds, topColor, bottomColor,
        Gdiplus::LinearGradientModeVertical);
    g->FillPath(&brush, &path);

    // Selection highlight
    if (isSelected) {
        Gdiplus::Pen highlightPen(Gdiplus::Color(0, 255, 255), 3.0f);
        g->DrawPath(&highlightPen, &path);

        // Semi-transparent cyan fill overlay
        Gdiplus::SolidBrush highlightBrush(Gdiplus::Color(40, 0, 255, 255));
        g->FillPath(&highlightBrush, &path);
    }

    // Active node pulse effect
    if (isActive) {
        Gdiplus::Pen activePen(Gdiplus::Color(0, 200, 255), 2.0f);
        activePen.SetDashStyle(Gdiplus::DashStyleDash);
        g->DrawPath(&activePen, &path);
    }

    // Standard border
    Gdiplus::Pen borderPen(Gdiplus::Color(80, 80, 80), 1.0f);
    g->DrawPath(&borderPen, &path);

    // Node label
    DrawNodeLabel(g, node);
}

void PipelineRenderer::DrawEdge(Gdiplus::Graphics* g, const DagNode& parent, const DagNode& child)
{
    // Skip edges to/from dummy nodes (draw straight vertical lines for those)
    bool parentIsDummy = parent.id.find("_dummy_") == 0;
    bool childIsDummy = child.id.find("_dummy_") == 0;

    Gdiplus::PointF start(
        parent.bounds.X + parent.bounds.Width / 2,
        parent.bounds.GetBottom()
    );
    Gdiplus::PointF end(
        child.bounds.X + child.bounds.Width / 2,
        child.bounds.Y
    );

    if (parentIsDummy || childIsDummy) {
        // Straight vertical line for dummy connections
        Gdiplus::Pen edgePen(Gdiplus::Color(60, 60, 60), 1.0f);
        g->DrawLine(&edgePen, start, end);
        return;
    }

    // Cubic Bézier curve for real node connections
    float offset = (end.Y - start.Y) * 0.5f;
    Gdiplus::PointF ctrl1(start.X, start.Y + offset);
    Gdiplus::PointF ctrl2(end.X, end.Y - offset);

    Gdiplus::Pen edgePen(Gdiplus::Color(100, 100, 100), 1.5f);
    edgePen.SetStartCap(Gdiplus::LineCapRound);
    edgePen.SetEndCap(Gdiplus::LineCapArrowAnchor);

    g->DrawBezier(&edgePen, start, ctrl1, ctrl2, end);
}

void PipelineRenderer::DrawNodeLabel(Gdiplus::Graphics* g, const DagNode& node)
{
    if (node.label.empty()) return;

    Gdiplus::FontFamily family(L"Segoe UI");
    Gdiplus::Font font(&family, 10, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 255, 255));

    // Center text in node
    Gdiplus::RectF layoutRect = node.bounds;
    layoutRect.X += 5.0f;
    layoutRect.Y += 5.0f;
    layoutRect.Width -= 10.0f;
    layoutRect.Height -= 10.0f;

    Gdiplus::StringFormat format;
    format.SetAlignment(Gdiplus::StringAlignmentCenter);
    format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    format.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

    std::wstring wLabel(node.label.begin(), node.label.end());
    g->DrawString(wLabel.c_str(), -1, &font, layoutRect, &format, &textBrush);

    // Draw tool name below if different from label
    if (!node.toolName.empty() && node.toolName != node.label) {
        Gdiplus::Font toolFont(&family, 8, Gdiplus::FontStyleItalic, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush toolBrush(Gdiplus::Color(180, 180, 180));

        Gdiplus::RectF toolRect = layoutRect;
        toolRect.Y += 20.0f;

        std::wstring wTool(node.toolName.begin(), node.toolName.end());
        g->DrawString(wTool.c_str(), -1, &toolFont, toolRect, &format, &toolBrush);
    }
}

void PipelineRenderer::DrawMiniMap(Gdiplus::Graphics* g,
    const std::map<std::string, DagNode>& graph,
    const Gdiplus::RectF& viewport,
    float viewWidth, float viewHeight)
{
    Gdiplus::RectF miniMapRect(viewWidth - 160.0f, 10.0f, 150.0f, 100.0f);

    // Background
    Gdiplus::SolidBrush bgBrush(Gdiplus::Color(100, 0, 0, 0));
    g->FillRectangle(&bgBrush, miniMapRect);

    // Border
    Gdiplus::Pen borderPen(Gdiplus::Color(120, 120, 120), 1.0f);
    g->DrawRectangle(&borderPen, miniMapRect);

    // Save state
    Gdiplus::GraphicsState state = g->Save();

    // Scale entire graph to fit mini-map
    g->SetTransform(nullptr);
    g->TranslateTransform(miniMapRect.X, miniMapRect.Y);
    g->ScaleTransform(0.05f, 0.05f);

    // Draw simplified graph
    for (const auto& [id, node] : graph) {
        if (node.id.find("_dummy_") == 0) continue;

        Gdiplus::SolidBrush nodeBrush(Gdiplus::Color(150, 150, 150));
        g->FillRectangle(&nodeBrush, node.bounds);
    }

    // Viewport indicator
    Gdiplus::Pen viewportPen(Gdiplus::Color(0, 255, 0), 20.0f);
    g->DrawRectangle(&viewportPen, viewport);

    g->Restore(state);
}

Gdiplus::Color PipelineRenderer::StatusToColor(DagNodeStatus status)
{
    switch (status) {
        case DagNodeStatus::Pending:   return Gdiplus::Color(200, 200, 0);    // Yellow
        case DagNodeStatus::Running: return Gdiplus::Color(0, 200, 255);    // Cyan/Blue
        case DagNodeStatus::Success: return Gdiplus::Color(0, 200, 80);     // Green
        case DagNodeStatus::Failed:  return Gdiplus::Color(220, 50, 50);  // Red
        case DagNodeStatus::Cancelled: return Gdiplus::Color(150, 150, 150); // Gray
        default: return Gdiplus::Color(100, 100, 100);
    }
}

Gdiplus::Color PipelineRenderer::StatusToGradientTop(DagNodeStatus status)
{
    switch (status) {
        case DagNodeStatus::Pending:   return Gdiplus::Color(80, 80, 0);
        case DagNodeStatus::Running: return Gdiplus::Color(0, 80, 120);
        case DagNodeStatus::Success: return Gdiplus::Color(0, 100, 40);
        case DagNodeStatus::Failed:  return Gdiplus::Color(120, 20, 20);
        case DagNodeStatus::Cancelled: return Gdiplus::Color(80, 80, 80);
        default: return Gdiplus::Color(60, 60, 60);
    }
}

Gdiplus::Color PipelineRenderer::StatusToGradientBottom(DagNodeStatus status)
{
    switch (status) {
        case DagNodeStatus::Pending:   return Gdiplus::Color(40, 40, 0);
        case DagNodeStatus::Running: return Gdiplus::Color(0, 40, 80);
        case DagNodeStatus::Success: return Gdiplus::Color(0, 60, 20);
        case DagNodeStatus::Failed:  return Gdiplus::Color(60, 10, 10);
        case DagNodeStatus::Cancelled: return Gdiplus::Color(40, 40, 40);
        default: return Gdiplus::Color(30, 30, 30);
    }
}

// ============================================================================
// Win32IDE_PipelinePanel Implementation
// ============================================================================

Win32IDE_PipelinePanel::Win32IDE_PipelinePanel()
{
    if (s_instanceCount == 0) {
        Gdiplus::GdiplusStartupInput input;
        Gdiplus::GdiplusStartup(&s_gdiplusToken, &input, nullptr);
    }
    ++s_instanceCount;
}

Win32IDE_PipelinePanel::~Win32IDE_PipelinePanel()
{
    Destroy();

    --s_instanceCount;
    if (s_instanceCount == 0 && s_gdiplusToken != 0) {
        Gdiplus::GdiplusShutdown(s_gdiplusToken);
        s_gdiplusToken = 0;
    }
}

bool Win32IDE_PipelinePanel::Create(HWND hwndParent, HINSTANCE hInst,
    int x, int y, int width, int height)
{
    m_hwndParent = hwndParent;
    m_hInst = hInst;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.lpszClassName = L"RawrXD_PipelinePanel";

    RegisterClassExW(&wc);

    m_hWnd = CreateWindowExW(
        WS_EX_COMPOSITED,
        L"RawrXD_PipelinePanel",
        L"Pipeline",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        x, y, width, height,
        hwndParent, nullptr, hInst, this
    );

    if (!m_hWnd) return false;

    SetWindowLongPtr(m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    return true;
}

void Win32IDE_PipelinePanel::Destroy()
{
    if (m_hBackBuffer) {
        DeleteObject(m_hBackBuffer);
        m_hBackBuffer = nullptr;
    }
    if (m_hMemDC) {
        DeleteDC(m_hMemDC);
        m_hMemDC = nullptr;
    }
    if (m_hWnd) {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

// ============================================================================
// Graph Data Management
// ============================================================================

void Win32IDE_PipelinePanel::SetGraph(std::map<std::string, DagNode> graph)
{
    m_graph = std::move(graph);
    RecomputeLayout();
    Invalidate();
}

void Win32IDE_PipelinePanel::UpdateNodeStatus(const std::string& id, DagNodeStatus status)
{
    auto it = m_graph.find(id);
    if (it != m_graph.end()) {
        it->second.status = status;
        Invalidate();
    }
}

void Win32IDE_PipelinePanel::SetActiveNode(const std::string& id)
{
    m_activeNodeId = id;
    Invalidate();
}

void Win32IDE_PipelinePanel::ClearGraph()
{
    m_graph.clear();
    m_selectedNodeId.clear();
    m_activeNodeId.clear();
    Invalidate();
}

// ============================================================================
// Viewport Control
// ============================================================================

void Win32IDE_PipelinePanel::ScrollToNode(const std::string& id)
{
    auto it = m_graph.find(id);
    if (it == m_graph.end()) return;

    const Gdiplus::RectF& bounds = it->second.bounds;

    float viewCenterX = m_clientWidth / 2.0f;
    float viewCenterY = m_clientHeight / 2.0f;

    m_targetPanOffset.X = viewCenterX - (bounds.X + bounds.Width / 2.0f) * m_zoomLevel;
    m_targetPanOffset.Y = viewCenterY - (bounds.Y + bounds.Height / 2.0f) * m_zoomLevel;

    m_autoScrolling = true;
    Invalidate();
}

void Win32IDE_PipelinePanel::ResetView()
{
    m_zoomLevel = 1.0f;
    m_panOffset = { 0.0f, 0.0f };
    m_targetPanOffset = { 0.0f, 0.0f };
    m_autoScrolling = false;
    Invalidate();
}

// ============================================================================
// Layout
// ============================================================================

void Win32IDE_PipelinePanel::RecomputeLayout()
{
    m_layoutEngine.ComputeLayout(m_graph);
}

// ============================================================================
// Coordinate Transforms
// ============================================================================

Gdiplus::PointF Win32IDE_PipelinePanel::ScreenToWorld(int x, int y) const
{
    return Gdiplus::PointF(
        (x - m_panOffset.X) / m_zoomLevel,
        (y - m_panOffset.Y) / m_zoomLevel
    );
}

Gdiplus::PointF Win32IDE_PipelinePanel::WorldToScreen(float x, float y) const
{
    return Gdiplus::PointF(
        x * m_zoomLevel + m_panOffset.X,
        y * m_zoomLevel + m_panOffset.Y
    );
}

// ============================================================================
// Rendering
// ============================================================================

void Win32IDE_PipelinePanel::Invalidate()
{
    if (m_hWnd) {
        ::InvalidateRect(m_hWnd, nullptr, FALSE);
    }
}

void Win32IDE_PipelinePanel::OnPaint()
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hWnd, &ps);

    RECT rc;
    GetClientRect(m_hWnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    // --- Double Buffering Setup ---
    if (!m_hMemDC || width != m_bufferWidth || height != m_bufferHeight) {
        if (m_hBackBuffer) DeleteObject(m_hBackBuffer);
        if (m_hMemDC) DeleteDC(m_hMemDC);

        m_hMemDC = CreateCompatibleDC(hdc);
        m_hBackBuffer = CreateCompatibleBitmap(hdc, width, height);
        SelectObject(m_hMemDC, m_hBackBuffer);
        m_bufferWidth = width;
        m_bufferHeight = height;
    }

    // Initialize GDI+ on memory DC
    Gdiplus::Graphics g(m_hMemDC);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
    g.Clear(Gdiplus::Color(30, 30, 30));

    // --- Apply Viewport Transform ---
    Gdiplus::Matrix transform;
    transform.Translate(m_panOffset.X, m_panOffset.Y);
    transform.Scale(m_zoomLevel, m_zoomLevel);
    g.SetTransform(&transform);

    // --- Smooth Auto-Scroll ---
    if (m_autoScrolling) {
        m_panOffset.X += (m_targetPanOffset.X - m_panOffset.X) * 0.1f;
        m_panOffset.Y += (m_targetPanOffset.Y - m_panOffset.Y) * 0.1f;

        if (std::abs(m_targetPanOffset.X - m_panOffset.X) < 1.0f &&
            std::abs(m_targetPanOffset.Y - m_panOffset.Y) < 1.0f) {
            m_panOffset = m_targetPanOffset;
            m_autoScrolling = false;
        }
    }

    // --- Calculate Viewport for Culling ---
    Gdiplus::RectF viewport(
        -m_panOffset.X / m_zoomLevel,
        -m_panOffset.Y / m_zoomLevel,
        width / m_zoomLevel,
        height / m_zoomLevel
    );

    // --- Render Graph ---
    m_renderer.DrawGraph(
        &g, m_graph, m_selectedNodeId, m_activeNodeId);

    // --- Draw Mini-Map ---
    g.SetTransform(nullptr);
    m_renderer.DrawMiniMap(
        &g, m_graph, viewport, static_cast<float>(width), static_cast<float>(height));

    // --- Flip Buffer to Screen ---
    BitBlt(hdc, 0, 0, width, height, m_hMemDC, 0, 0, SRCCOPY);

    EndPaint(m_hWnd, &ps);
}

// ============================================================================
// Message Handlers
// ============================================================================

void Win32IDE_PipelinePanel::OnSize(int width, int height)
{
    m_clientWidth = width;
    m_clientHeight = height;

    if (m_hBackBuffer) {
        DeleteObject(m_hBackBuffer);
        m_hBackBuffer = nullptr;
    }

    Invalidate();
}

void Win32IDE_PipelinePanel::OnMouseMove(int x, int y)
{
    Gdiplus::PointF worldPt = ScreenToWorld(x, y);

    std::string prevHover = m_hoverNodeId;
    m_hoverNodeId.clear();

    for (const auto& [id, node] : m_graph) {
        if (node.bounds.Contains(worldPt)) {
            m_hoverNodeId = id;
            break;
        }
    }

    if (m_hoverNodeId != prevHover) {
        if (OnNodeHover && !m_hoverNodeId.empty()) {
            OnNodeHover(m_hoverNodeId);
        }
        Invalidate();
    }
}

void Win32IDE_PipelinePanel::OnLButtonDown(int x, int y)
{
    Gdiplus::PointF worldPt = ScreenToWorld(x, y);

    for (const auto& [id, node] : m_graph) {
        if (node.bounds.Contains(worldPt)) {
            m_selectedNodeId = id;

            if (OnNodeSelected) {
                OnNodeSelected(id);
            }

            Invalidate();
            return;
        }
    }

    m_selectedNodeId.clear();
    Invalidate();
}

void Win32IDE_PipelinePanel::OnMouseWheel(short delta, int x, int y)
{
    POINT pt = { x, y };
    ScreenToClient(m_hWnd, &pt);
    Gdiplus::PointF mousePt(static_cast<Gdiplus::REAL>(pt.x), static_cast<Gdiplus::REAL>(pt.y));

    float oldZoom = m_zoomLevel;
    float zoomFactor = (delta > 0) ? 1.1f : 0.9f;
    m_zoomLevel *= zoomFactor;

    m_zoomLevel = (std::max)(0.1f, (std::min)(m_zoomLevel, 5.0f));

    float actualFactor = m_zoomLevel / oldZoom;
    m_panOffset.X = mousePt.X - (mousePt.X - m_panOffset.X) * actualFactor;
    m_panOffset.Y = mousePt.Y - (mousePt.Y - m_panOffset.Y) * actualFactor;

    Invalidate();
}

void Win32IDE_PipelinePanel::OnEraseBkgnd()
{
    // Do nothing - we handle all rendering in OnPaint with double buffering
}

// ============================================================================
// Window Procedure
// ============================================================================

LRESULT CALLBACK Win32IDE_PipelinePanel::WndProc(HWND hWnd, UINT msg,
    WPARAM wParam, LPARAM lParam)
{
    Win32IDE_PipelinePanel* pThis = reinterpret_cast<Win32IDE_PipelinePanel*>(
        GetWindowLongPtr(hWnd, GWLP_USERDATA));

    if (pThis) {
        return pThis->HandleMessage(msg, wParam, lParam);
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT Win32IDE_PipelinePanel::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_PAINT:
            OnPaint();
            return 0;

        case WM_ERASEBKGND:
            OnEraseBkgnd();
            return 1;

        case WM_SIZE:
            OnSize(LOWORD(lParam), HIWORD(lParam));
            return 0;

        case WM_MOUSEMOVE:
            OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;

        case WM_LBUTTONDOWN:
            OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;

        case WM_MOUSEWHEEL: {
            short delta = GET_WHEEL_DELTA_WPARAM(wParam);
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);
            OnMouseWheel(delta, xPos, yPos);
            return 0;
        }

        case WM_DESTROY:
            m_hWnd = nullptr;
            return 0;

        default:
            return DefWindowProc(m_hWnd, msg, wParam, lParam);
    }
}