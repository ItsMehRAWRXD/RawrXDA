/**
 * @file μ_live_controller.hpp
 * @brief Live IDE controller - integrates schematic with actual IDE - Production
 */

#pragma once

#include "λ_live_schematic.hpp"
#include <windows.h>
#include <vector>
#include <string>
#include <atomic>

class LiveIDEController {
    LiveSchematic schematic_;
    HWND viewer_hwnd_;
    HWND ide_hwnd_;
    std::atomic<bool> committed_;
    std::atomic<bool> waiting_for_approval_;
    std::vector<HWND> provisioned_windows_;
    
public:
    LiveIDEController() : schematic_(1220, 820), viewer_hwnd_(nullptr), 
                          ide_hwnd_(nullptr), committed_(false), waiting_for_approval_(true) {}
    
    void Initialize(HWND ide_window);
    void ShowViewer();
    
    static LRESULT CALLBACK ViewerProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    void OnApproved(const LiveNode& n);
    void OnRejected(const LiveNode& n);
    void OnMoved(const LiveNode& n, int x, int y);
    void OnCommitted();
    
    bool IsCommitted() const { return committed_.load(); }
    bool IsWaiting() const { return waiting_for_approval_.load(); }
    
    void Finalize();
    
    std::vector<LiveNode> GetApprovedLayout() const {
        return schematic_.GetApprovedNodes();
    }
    
    HWND GetViewerHWND() const { return viewer_hwnd_; }
};
