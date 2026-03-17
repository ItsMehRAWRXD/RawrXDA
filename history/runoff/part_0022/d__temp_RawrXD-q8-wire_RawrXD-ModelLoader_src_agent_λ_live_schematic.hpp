/**
 * @file λ_live_schematic.hpp
 * @brief Live schematic approval system - Production
 * @author RawrXD Agentic Infrastructure
 * @version 1.0.0
 * 
 * Real-time visual IDE layout approval system with:
 * - Drag/drop node positioning
 * - Approve/reject with visual feedback
 * - User marking system (check/X/arrow/question)
 * - Live preview in target IDE
 * - Commit with instant application
 */

#pragma once

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef LAMBDA_LIVE_SCHEMATIC_HPP
#define LAMBDA_LIVE_SCHEMATIC_HPP

#include <windows.h>
#include <vector>
#include <string>
#include <map>
#include <functional>
#include <memory>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

enum class ApprovalState { PENDING, APPROVED, REJECTED, MODIFIED };
enum class MarkType { NONE, CHECK, X_MARK, ARROW, QUESTION };

struct LiveNode {
    RECT bounds;
    COLORREF color;
    uint32_t caps;
    std::string label;
    std::string identifier;      // Internal ID
    ApprovalState state;
    std::vector<std::pair<POINT, MarkType>> marks;
    bool is_dragging;
    POINT drag_offset;
    bool is_selected;
    int original_x, original_y;
    HWND target_hwnd;            // Which IDE window to control
    int control_id;              // Win32 control ID
};

struct LiveWire {
    POINT from, to;
    COLORREF color;
    uint32_t cap_type;
    ApprovalState state;
    bool is_proposed;
};

class LiveSchematic {
    std::vector<LiveNode> nodes_;
    std::vector<LiveWire> wires_;
    HDC mem_dc_;
    HBITMAP mem_bmp_;
    int width_, height_;
    HWND hwnd_;
    POINT cursor_;
    bool show_ghost_;
    LiveNode ghost_node_;
    
    // Callbacks (private)
    std::function<void(const LiveNode&)> on_approved_;
    std::function<void(const LiveNode&)> on_rejected_;
    std::function<void(const LiveNode&, int, int)> on_moved_;
    std::function<void()> on_committed_;
    
public:
    LiveSchematic(int w, int h) : width_(w), height_(h), show_ghost_(false), hwnd_(nullptr) {
        HDC screen = ::GetDC(nullptr);
        mem_dc_ = CreateCompatibleDC(screen);
        mem_bmp_ = CreateCompatibleBitmap(screen, w, h);
        SelectObject(mem_dc_, mem_bmp_);
        ::ReleaseDC(nullptr, screen);
        
        ghost_node_.state = ApprovalState::PENDING;
        ghost_node_.caps = 0;
        ghost_node_.is_dragging = false;
        ghost_node_.is_selected = false;
    }
    
    ~LiveSchematic() {
        if (mem_bmp_) DeleteObject(mem_bmp_);
        if (mem_dc_) DeleteDC(mem_dc_);
    }
    
    void SetCallbacks(
        std::function<void(const LiveNode&)> approve,
        std::function<void(const LiveNode&)> reject,
        std::function<void(const LiveNode&, int, int)> move,
        std::function<void()> commit) {
        on_approved_ = approve;
        on_rejected_ = reject;
        on_moved_ = move;
        on_committed_ = commit;
    }
    
    size_t ProposeNode(const RECT& rc, COLORREF col, uint32_t caps, 
                       const char* label, const char* id, int ctrl_id = -1) {
        LiveNode n;
        n.bounds = rc;
        n.color = col;
        n.caps = caps;
        n.label = label;
        n.identifier = id;
        n.state = ApprovalState::PENDING;
        n.is_dragging = false;
        n.is_selected = false;
        n.original_x = rc.left;
        n.original_y = rc.top;
        n.target_hwnd = nullptr;
        n.control_id = ctrl_id;
        n.drag_offset = {0, 0};
        
        nodes_.push_back(n);
        return nodes_.size() - 1;
    }
    
    void ProposeWire(size_t from_node, size_t to_node, COLORREF col, uint32_t cap) {
        if (from_node >= nodes_.size() || to_node >= nodes_.size()) return;
        
        LiveWire w;
        w.from = {
            (nodes_[from_node].bounds.left + nodes_[from_node].bounds.right) / 2,
            (nodes_[from_node].bounds.top + nodes_[from_node].bounds.bottom) / 2
        };
        w.to = {
            (nodes_[to_node].bounds.left + nodes_[to_node].bounds.right) / 2,
            (nodes_[to_node].bounds.top + nodes_[to_node].bounds.bottom) / 2
        };
        w.color = col;
        w.cap_type = cap;
        w.state = ApprovalState::PENDING;
        w.is_proposed = true;
        wires_.push_back(w);
    }
    
    void SetGhostNode(const RECT& rc, COLORREF col, uint32_t caps, const char* lbl) {
        ghost_node_.bounds = rc;
        ghost_node_.color = col;
        ghost_node_.caps = caps;
        ghost_node_.label = lbl;
        show_ghost_ = true;
    }
    
    void Render();
    
    void OnMouseMove(int x, int y);
    void OnLButtonDown(int x, int y);
    void OnLButtonUp(int x, int y);
    void OnRButtonDown(int x, int y);
    void OnMButtonDown(int x, int y);
    void OnKeyDown(WPARAM key);
    
    void Commit();
    void SetHWND(HWND h) { hwnd_ = h; }
    void SetIDEWindow(HWND h, size_t node_idx) {
        if (node_idx < nodes_.size()) {
            nodes_[node_idx].target_hwnd = h;
        }
    }
    
    HDC GetDC() const { return mem_dc_; }
    bool IsGhostMode() const { return show_ghost_; }
    
    std::vector<LiveNode> GetApprovedNodes() const {
        std::vector<LiveNode> result;
        for (const auto& n : nodes_) {
            if (n.state == ApprovalState::APPROVED || n.state == ApprovalState::MODIFIED) {
                result.push_back(n);
            }
        }
        return result;
    }
    
    const std::vector<LiveNode>& GetAllNodes() const { return nodes_; }
    size_t GetNodeCount() const { return nodes_.size(); }
    
    int GetApprovedCount() const {
        int count = 0;
        for (const auto& n : nodes_) {
            if (n.state == ApprovalState::APPROVED) count++;
        }
        return count;
    }
    
    int GetRejectedCount() const {
        int count = 0;
        for (const auto& n : nodes_) {
            if (n.state == ApprovalState::REJECTED) count++;
        }
        return count;
    }
    
private:
    void DrawCheck(int x, int y);
    void DrawX(int x, int y);
    void DrawArrow(int x, int y);
    void DrawQuestion(int x, int y);
};

#endif // LAMBDA_LIVE_SCHEMATIC_HPP
