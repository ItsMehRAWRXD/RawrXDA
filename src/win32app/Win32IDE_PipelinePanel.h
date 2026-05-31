#pragma once

// Win32IDE_PipelinePanel.h — GDI+ DAG Visualization Panel
// Hierarchical layout engine + retained-mode renderer for agentic execution traces
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED

#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>

#pragma comment(lib, "gdiplus.lib")

// ============================================================================
// Data Structures
// ============================================================================

enum class DagNodeStatus
{
    Pending    = 0,
    Running    = 1,
    Success    = 2,
    Failed     = 3,
    Cancelled  = 4
};

struct DagNode
{
    std::string id;
    std::string label;
    std::string toolName;
    int rank = 0;       // Vertical layer (Sugiyama rank)
    int position = 0;   // Horizontal order within layer
    Gdiplus::RectF bounds;
    DagNodeStatus status = DagNodeStatus::Pending;
    std::vector<std::string> children;
    std::vector<std::string> parents;
    std::string jsonMetadata;  // Raw trace data for inspector
};

// ============================================================================
// Layout Engine
// ============================================================================

class DagLayoutEngine
{
public:
    static constexpr float NODE_WIDTH  = 180.0f;
    static constexpr float NODE_HEIGHT = 60.0f;
    static constexpr float MARGIN_X    = 40.0f;
    static constexpr float MARGIN_Y    = 80.0f;

    void ComputeLayout(std::map<std::string, DagNode>& nodes);

private:
    void AssignRanks(std::map<std::string, DagNode>& nodes);
    void AssignPositions(std::map<std::string, DagNode>& nodes);
    void AssignCoordinates(std::map<std::string, DagNode>& nodes);
    void InsertDummyNodes(std::map<std::string, DagNode>& nodes);
};

// ============================================================================
// GDI+ Renderer
// ============================================================================

class PipelineRenderer
{
public:
    void DrawGraph(Gdiplus::Graphics* g, const std::map<std::string, DagNode>& graph,
                   const std::string& selectedNodeId, const std::string& activeNodeId);

    void DrawMiniMap(Gdiplus::Graphics* g, const std::map<std::string, DagNode>& graph,
                     const Gdiplus::RectF& viewport, float viewWidth, float viewHeight);

private:
    void DrawNode(Gdiplus::Graphics* g, const DagNode& node, bool isSelected, bool isActive);
    void DrawEdge(Gdiplus::Graphics* g, const DagNode& parent, const DagNode& child);
    void DrawNodeLabel(Gdiplus::Graphics* g, const DagNode& node);
    Gdiplus::Color StatusToColor(DagNodeStatus status);
    Gdiplus::Color StatusToGradientTop(DagNodeStatus status);
    Gdiplus::Color StatusToGradientBottom(DagNodeStatus status);
};

// ============================================================================
// Pipeline Panel Window
// ============================================================================

class Win32IDE_PipelinePanel
{
public:
    Win32IDE_PipelinePanel();
    ~Win32IDE_PipelinePanel();

    bool Create(HWND hwndParent, HINSTANCE hInst, int x, int y, int width, int height);
    void Destroy();

    HWND GetHwnd() const { return m_hWnd; }

    // Graph data management
    void SetGraph(std::map<std::string, DagNode> graph);
    void UpdateNodeStatus(const std::string& id, DagNodeStatus status);
    void SetActiveNode(const std::string& id);
    void ClearGraph();

    // Notification callback when a node is clicked
    std::function<void(const std::string&)> OnNodeSelected;
    std::function<void(const std::string&)> OnNodeHover;

    // Force redraw
    void Invalidate();

    // Viewport control
    void ScrollToNode(const std::string& id);
    void ResetView();

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // Message handlers
    void OnPaint();
    void OnSize(int width, int height);
    void OnMouseMove(int x, int y);
    void OnLButtonDown(int x, int y);
    void OnMouseWheel(short delta, int x, int y);
    void OnEraseBkgnd();

    // Coordinate transforms
    Gdiplus::PointF ScreenToWorld(int x, int y) const;
    Gdiplus::PointF WorldToScreen(float x, float y) const;

    // Layout
    void RecomputeLayout();

    // Window handle
    HWND m_hWnd = nullptr;
    HWND m_hwndParent = nullptr;
    HINSTANCE m_hInst = nullptr;

    // Graph data
    std::map<std::string, DagNode> m_graph;
    std::string m_selectedNodeId;
    std::string m_activeNodeId;
    std::string m_hoverNodeId;

    // Layout engine
    DagLayoutEngine m_layoutEngine;
    PipelineRenderer m_renderer;

    // Viewport state
    float m_zoomLevel = 1.0f;
    Gdiplus::PointF m_panOffset = { 0.0f, 0.0f };
    int m_clientWidth = 0;
    int m_clientHeight = 0;

    // Smooth scroll target
    Gdiplus::PointF m_targetPanOffset = { 0.0f, 0.0f };
    bool m_autoScrolling = false;

    // Double-buffering bitmap cache
    HBITMAP m_hBackBuffer = nullptr;
    HDC m_hMemDC = nullptr;
    int m_bufferWidth = 0;
    int m_bufferHeight = 0;

    // GDI+ token (shared across all instances)
    static ULONG_PTR s_gdiplusToken;
    static int s_instanceCount;
};
