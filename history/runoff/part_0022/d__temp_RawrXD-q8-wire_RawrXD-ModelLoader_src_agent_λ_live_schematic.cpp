/**
 * @file λ_live_schematic.cpp
 * @brief Live schematic approval system implementation - Production
 */

#include "λ_live_schematic.hpp"
#include "license_enforcement.h"
#include <cmath>

void LiveSchematic::DrawCheck(int x, int y) {
    HPEN pen = CreatePen(PS_SOLID, 3, RGB(0, 200, 0));
    HPEN old = (HPEN)SelectObject(mem_dc_, pen);
    MoveToEx(mem_dc_, x, y + 5, nullptr);
    LineTo(mem_dc_, x + 5, y + 15);
    LineTo(mem_dc_, x + 15, y - 5);
    SelectObject(mem_dc_, old);
    DeleteObject(pen);
}

void LiveSchematic::DrawX(int x, int y) {
    HPEN pen = CreatePen(PS_SOLID, 3, RGB(200, 0, 0));
    HPEN old = (HPEN)SelectObject(mem_dc_, pen);
    MoveToEx(mem_dc_, x, y, nullptr);
    LineTo(mem_dc_, x + 15, y + 15);
    MoveToEx(mem_dc_, x + 15, y, nullptr);
    LineTo(mem_dc_, x, y + 15);
    SelectObject(mem_dc_, old);
    DeleteObject(pen);
}

void LiveSchematic::DrawArrow(int x, int y) {
    HPEN pen = CreatePen(PS_SOLID, 2, RGB(0, 0, 200));
    HPEN old = (HPEN)SelectObject(mem_dc_, pen);
    MoveToEx(mem_dc_, x, y, nullptr);
    LineTo(mem_dc_, x + 15, y);
    LineTo(mem_dc_, x + 10, y - 5);
    MoveToEx(mem_dc_, x + 15, y, nullptr);
    LineTo(mem_dc_, x + 10, y + 5);
    SelectObject(mem_dc_, old);
    DeleteObject(pen);
}

void LiveSchematic::DrawQuestion(int x, int y) {
    SetTextColor(mem_dc_, RGB(200, 150, 0));
    TextOutA(mem_dc_, x, y, "?", 1);
}

void LiveSchematic::Render() {
    // Enterprise license enforcement gate for Schematic Studio
    if (!LicenseEnforcementGate::getInstance().gateSchematicStudio().success) {
        // Draw "License Required" overlay instead
        RECT bg = {0, 0, width_, height_};
        FillRect(mem_dc_, &bg, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
        SetTextColor(mem_dc_, RGB(180, 0, 0));
        TextOutA(mem_dc_, 20, height_/2, "[Enterprise License Required]", 30);
        return;
    }
    // Clear background
    RECT bg = {0, 0, width_, height_};
    FillRect(mem_dc_, &bg, (HBRUSH)GetStockObject(WHITE_BRUSH));
    
    // Draw grid
    HPEN grid_pen = CreatePen(PS_DOT, 1, RGB(240, 240, 240));
    HPEN old_pen = (HPEN)SelectObject(mem_dc_, grid_pen);
    for (int x = 0; x < width_; x += 20) {
        MoveToEx(mem_dc_, x, 0, nullptr);
        LineTo(mem_dc_, x, height_);
    }
    for (int y = 0; y < height_; y += 20) {
        MoveToEx(mem_dc_, 0, y, nullptr);
        LineTo(mem_dc_, width_, y);
    }
    SelectObject(mem_dc_, old_pen);
    DeleteObject(grid_pen);
    
    // Draw wires
    for (auto& w : wires_) {
        HPEN pen;
        int width = 2;
        int style = PS_SOLID;
        
        if (w.state == ApprovalState::APPROVED) {
            width = 3;
        } else if (w.state == ApprovalState::REJECTED) {
            style = PS_DASH;
            width = 1;
        } else {
            style = PS_DASHDOT;
        }
        
        pen = CreatePen(style, width, w.color);
        SelectObject(mem_dc_, pen);
        MoveToEx(mem_dc_, w.from.x, w.from.y, nullptr);
        LineTo(mem_dc_, w.to.x, w.to.y);
        DeleteObject(pen);
        
        // Arrowhead
        if (w.state != ApprovalState::REJECTED) {
            double dx = w.to.x - w.from.x;
            double dy = w.to.y - w.from.y;
            double angle = atan2(dy, dx);
            
            POINT arrow[3];
            arrow[0] = w.to;
            arrow[1].x = w.to.x - 12 * cos(angle - 0.5);
            arrow[1].y = w.to.y - 12 * sin(angle - 0.5);
            arrow[2].x = w.to.x - 12 * cos(angle + 0.5);
            arrow[2].y = w.to.y - 12 * sin(angle + 0.5);
            
            HBRUSH arrow_brush = CreateSolidBrush(w.color);
            HBRUSH old_brush = (HBRUSH)SelectObject(mem_dc_, arrow_brush);
            Polygon(mem_dc_, arrow, 3);
            SelectObject(mem_dc_, old_brush);
            DeleteObject(arrow_brush);
        }
    }
    
    // Draw ghost node (follows cursor)
    if (show_ghost_) {
        RECT ghost = ghost_node_.bounds;
        OffsetRect(&ghost, cursor_.x - ghost.left - 50, cursor_.y - ghost.top - 25);
        
        HBRUSH ghost_brush = CreateHatchBrush(HS_DIAGCROSS, RGB(200, 200, 200));
        HBRUSH old_brush = (HBRUSH)SelectObject(mem_dc_, ghost_brush);
        FillRect(mem_dc_, &ghost, ghost_brush);
        SelectObject(mem_dc_, old_brush);
        DeleteObject(ghost_brush);
        
        SelectObject(mem_dc_, GetStockObject(GRAY_BRUSH));
        FrameRect(mem_dc_, &ghost, (HBRUSH)GetStockObject(GRAY_BRUSH));
        
        SetBkMode(mem_dc_, TRANSPARENT);
        SetTextColor(mem_dc_, RGB(150, 150, 150));
        DrawTextA(mem_dc_, ghost_node_.label.c_str(), -1, &ghost, 
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    
    // Draw nodes
    for (size_t i = 0; i < nodes_.size(); i++) {
        auto& n = nodes_[i];
        
        // Selection highlight
        if (n.is_selected) {
            RECT sel = n.bounds;
            InflateRect(&sel, 4, 4);
            HBRUSH sel_brush = CreateSolidBrush(RGB(255, 255, 0));
            SelectObject(mem_dc_, sel_brush);
            FrameRect(mem_dc_, &sel, sel_brush);
            DeleteObject(sel_brush);
        }
        
        // Background by state
        COLORREF fill_color = n.color;
        if (n.state == ApprovalState::REJECTED) {
            HBRUSH hatch = CreateHatchBrush(HS_DIAGCROSS, RGB(200, 200, 200));
            FillRect(mem_dc_, &n.bounds, hatch);
            DeleteObject(hatch);
        } else if (n.state == ApprovalState::PENDING) {
            fill_color = RGB(
                (GetRValue(n.color) + 40 > 255) ? 255 : GetRValue(n.color) + 40,
                (GetGValue(n.color) + 40 > 255) ? 255 : GetGValue(n.color) + 40,
                (GetBValue(n.color) + 40 > 255) ? 255 : GetBValue(n.color) + 40);
            HBRUSH brush = CreateSolidBrush(fill_color);
            FillRect(mem_dc_, &n.bounds, brush);
            DeleteObject(brush);
        } else {
            HBRUSH brush = CreateSolidBrush(fill_color);
            FillRect(mem_dc_, &n.bounds, brush);
            DeleteObject(brush);
        }
        
        // Border
        COLORREF border_color = RGB(100, 100, 100);
        int border_style = PS_DASH;
        
        if (n.state == ApprovalState::APPROVED) {
            border_color = RGB(0, 150, 0);
            border_style = PS_SOLID;
        } else if (n.state == ApprovalState::REJECTED) {
            border_color = RGB(200, 0, 0);
            border_style = PS_SOLID;
        }
        
        HPEN border_pen = CreatePen(border_style, 2, border_color);
        SelectObject(mem_dc_, border_pen);
        SelectObject(mem_dc_, GetStockObject(NULL_BRUSH));
        Rectangle(mem_dc_, n.bounds.left, n.bounds.top, n.bounds.right, n.bounds.bottom);
        DeleteObject(border_pen);
        
        // Label
        SetBkMode(mem_dc_, TRANSPARENT);
        SetTextColor(mem_dc_, RGB(0, 0, 0));
        HFONT hfont = (HFONT)SelectObject(mem_dc_, GetStockObject(DEFAULT_GUI_FONT));
        DrawTextA(mem_dc_, n.label.c_str(), -1, (RECT*)&n.bounds, 
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(mem_dc_, hfont);
        
        // Approval indicators (checkmark/X in corner)
        if (n.state == ApprovalState::APPROVED) {
            DrawCheck(n.bounds.right - 20, n.bounds.top + 5);
        } else if (n.state == ApprovalState::REJECTED) {
            DrawX(n.bounds.right - 20, n.bounds.top + 5);
        }
        
        // User-placed marks
        for (auto& mark : n.marks) {
            switch (mark.second) {
                case MarkType::CHECK: DrawCheck(mark.first.x, mark.first.y); break;
                case MarkType::X_MARK: DrawX(mark.first.x, mark.first.y); break;
                case MarkType::ARROW: DrawArrow(mark.first.x, mark.first.y); break;
                case MarkType::QUESTION: DrawQuestion(mark.first.x, mark.first.y); break;
                default: break;
            }
        }
    }
    
    // Draw instructions
    SetTextColor(mem_dc_, RGB(0, 0, 0));
    HFONT font = (HFONT)SelectObject(mem_dc_, GetStockObject(SYSTEM_FONT));
    
    char instructions[] = "Click=Select | Drag=Move | R-Click=Approve/Reject | M-Click=Mark | A=Approve | S=Reject | X=Check | Z=X Mark | Enter=Commit";
    TextOutA(mem_dc_, 10, 10, instructions, sizeof(instructions));
    
    char keys[] = "G=Ghost Mode | Arrow Keys=Fine Move | Ctrl+Z=Undo | Del=Delete Node | Esc=Cancel Ghost";
    TextOutA(mem_dc_, 10, 30, keys, sizeof(keys));
    
    SelectObject(mem_dc_, font);
    
    // Status bar
    int approved = GetApprovedCount();
    int rejected = GetRejectedCount();
    int pending = nodes_.size() - approved - rejected;
    
    char status[256];
    sprintf(status, "Nodes: %zu | Approved: %d | Rejected: %d | Pending: %d | Marks: %zu", 
        nodes_.size(), approved, rejected, pending, wires_.size());
    
    RECT status_rc = {10, height_ - 30, width_ - 10, height_ - 10};
    FillRect(mem_dc_, &status_rc, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
    SetTextColor(mem_dc_, RGB(0, 0, 0));
    TextOutA(mem_dc_, 15, height_ - 25, status, strlen(status));
    
    // Legend
    RECT legend_rc = {width_ - 220, 60, width_ - 10, 200};
    FillRect(mem_dc_, &legend_rc, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
    FrameRect(mem_dc_, &legend_rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
    
    SetTextColor(mem_dc_, RGB(0, 0, 0));
    TextOutA(mem_dc_, width_ - 210, 70, "STATES:", 7);
    
    // Legend items
    int leg_y = 90;
    struct { COLORREF c; const char* n; } legend[] = {
        {RGB(100, 149, 237), "Container"},
        {RGB(144, 238, 144), "Navigate"},
        {RGB(255, 182, 193), "Display"},
        {RGB(255, 255, 224), "Manipulate"},
        {RGB(221, 160, 221), "Route"},
        {RGB(255, 218, 185), "Persist"}
    };
    
    for (int i = 0; i < 6; i++) {
        RECT r = {width_ - 210, leg_y, width_ - 190, leg_y + 16};
        HBRUSH leg_brush = CreateSolidBrush(legend[i].c);
        FillRect(mem_dc_, &r, leg_brush);
        DeleteObject(leg_brush);
        FrameRect(mem_dc_, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));
        TextOutA(mem_dc_, width_ - 180, leg_y + 1, legend[i].n, strlen(legend[i].n));
        leg_y += 20;
    }
}

void LiveSchematic::OnMouseMove(int x, int y) {
    cursor_.x = x;
    cursor_.y = y;
    
    for (auto& n : nodes_) {
        if (n.is_dragging) {
            int dx = x - n.drag_offset.x;
            int dy = y - n.drag_offset.y;
            int w = n.bounds.right - n.bounds.left;
            int h = n.bounds.bottom - n.bounds.top;
            
            // Snap to grid
            dx = (dx / 10) * 10;
            dy = (dy / 10) * 10;
            
            n.bounds.left = dx;
            n.bounds.top = dy;
            n.bounds.right = dx + w;
            n.bounds.bottom = dy + h;
            
            if (on_moved_) on_moved_(n, dx, dy);
        }
    }
}

void LiveSchematic::OnLButtonDown(int x, int y) {
    for (auto& n : nodes_) n.is_selected = false;
    
    for (int i = nodes_.size() - 1; i >= 0; i--) {
        auto& n = nodes_[i];
        if (x >= n.bounds.left && x < n.bounds.right &&
            y >= n.bounds.top && y < n.bounds.bottom) {
            
            n.is_selected = true;
            n.is_dragging = true;
            n.drag_offset.x = x - n.bounds.left;
            n.drag_offset.y = y - n.bounds.top;
            break;
        }
    }
}

void LiveSchematic::OnLButtonUp(int x, int y) {
    for (auto& n : nodes_) {
        if (n.is_dragging) {
            n.is_dragging = false;
            if (n.state == ApprovalState::PENDING) {
                n.state = ApprovalState::MODIFIED;
            }
        }
    }
}

void LiveSchematic::OnRButtonDown(int x, int y) {
    for (size_t i = 0; i < nodes_.size(); i++) {
        auto& n = nodes_[i];
        if (x >= n.bounds.left && x < n.bounds.right &&
            y >= n.bounds.top && y < n.bounds.bottom) {
            
            HMENU menu = CreatePopupMenu();
            AppendMenuA(menu, MF_STRING, 1, "\xE2\x9C\x93 Approve");
            AppendMenuA(menu, MF_STRING, 2, "\xE2\x9C\x97 Reject");
            AppendMenuA(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuA(menu, MF_STRING, 3, "Reset to Original");
            AppendMenuA(menu, MF_STRING, 4, "Delete Node");
            AppendMenuA(menu, MF_STRING, 5, "Duplicate");
            
            POINT pt = {x, y};
            ClientToScreen(hwnd_, &pt);
            
            int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd_, nullptr);
            DestroyMenu(menu);
            
            if (cmd == 1) {
                n.state = ApprovalState::APPROVED;
                if (on_approved_) on_approved_(n);
            }
            else if (cmd == 2) {
                n.state = ApprovalState::REJECTED;
                if (on_rejected_) on_rejected_(n);
            }
            else if (cmd == 3) {
                int w = n.bounds.right - n.bounds.left;
                int h = n.bounds.bottom - n.bounds.top;
                n.bounds.left = n.original_x;
                n.bounds.top = n.original_y;
                n.bounds.right = n.original_x + w;
                n.bounds.bottom = n.original_y + h;
            }
            else if (cmd == 4) {
                nodes_.erase(nodes_.begin() + i);
            }
            else if (cmd == 5) {
                LiveNode dup = n;
                dup.bounds.left += 30;
                dup.bounds.top += 30;
                dup.bounds.right += 30;
                dup.bounds.bottom += 30;
                dup.state = ApprovalState::PENDING;
                nodes_.push_back(dup);
            }
            break;
        }
    }
}

void LiveSchematic::OnMButtonDown(int x, int y) {
    for (auto& n : nodes_) {
        if (x >= n.bounds.left && x < n.bounds.right &&
            y >= n.bounds.top && y < n.bounds.bottom) {
            
            HMENU menu = CreatePopupMenu();
            AppendMenuA(menu, MF_STRING, 1, "\xE2\x9C\x93 Check Mark");
            AppendMenuA(menu, MF_STRING, 2, "\xE2\x9C\x97 X Mark");
            AppendMenuA(menu, MF_STRING, 3, "\xE2\x86\x92 Arrow");
            AppendMenuA(menu, MF_STRING, 4, "? Question");
            AppendMenuA(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuA(menu, MF_STRING, 5, "Clear Marks");
            
            POINT pt = {x, y};
            ClientToScreen(hwnd_, &pt);
            
            int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd_, nullptr);
            DestroyMenu(menu);
            
            POINT mark_pt = {x - n.bounds.left, y - n.bounds.top};
            
            if (cmd == 1) n.marks.push_back({mark_pt, MarkType::CHECK});
            else if (cmd == 2) n.marks.push_back({mark_pt, MarkType::X_MARK});
            else if (cmd == 3) n.marks.push_back({mark_pt, MarkType::ARROW});
            else if (cmd == 4) n.marks.push_back({mark_pt, MarkType::QUESTION});
            else if (cmd == 5) n.marks.clear();
            
            break;
        }
    }
}

void LiveSchematic::OnKeyDown(WPARAM key) {
    if (key == 'A') {
        for (auto& n : nodes_) {
            if (n.is_selected) {
                n.state = ApprovalState::APPROVED;
                if (on_approved_) on_approved_(n);
            }
        }
    }
    else if (key == 'S') {
        for (auto& n : nodes_) {
            if (n.is_selected) {
                n.state = ApprovalState::REJECTED;
                if (on_rejected_) on_rejected_(n);
            }
        }
    }
    else if (key == 'X') {
        for (auto& n : nodes_) {
            if (n.is_selected) {
                POINT pt = {10, 10};
                n.marks.push_back({pt, MarkType::CHECK});
            }
        }
    }
    else if (key == 'Z') {
        for (auto& n : nodes_) {
            if (n.is_selected) {
                POINT pt = {10, 10};
                n.marks.push_back({pt, MarkType::X_MARK});
            }
        }
    }
    else if (key == VK_DELETE) {
        for (int i = nodes_.size() - 1; i >= 0; i--) {
            if (nodes_[i].is_selected) {
                nodes_.erase(nodes_.begin() + i);
            }
        }
    }
    else if (key == VK_ESCAPE) {
        show_ghost_ = false;
    }
    else if (key == VK_RETURN) {
        Commit();
    }
}

void LiveSchematic::Commit() {
    // Apply all approved and modified nodes to actual IDE
    for (const auto& n : nodes_) {
        if (n.state == ApprovalState::APPROVED || n.state == ApprovalState::MODIFIED) {
            char buf[512];
            sprintf(buf, "[COMMIT] %s at (%d,%d) size %dx%d caps 0x%x id=%s",
                n.label.c_str(), 
                n.bounds.left, n.bounds.top,
                n.bounds.right - n.bounds.left, 
                n.bounds.bottom - n.bounds.top,
                n.caps,
                n.identifier.c_str());
            OutputDebugStringA(buf);
            
            // Apply to target window if specified
            if (n.target_hwnd && n.control_id >= 0) {
                SetWindowPos(GetDlgItem(n.target_hwnd, n.control_id), nullptr,
                    n.bounds.left, n.bounds.top,
                    n.bounds.right - n.bounds.left,
                    n.bounds.bottom - n.bounds.top,
                    SWP_NOZORDER | SWP_SHOWWINDOW);
            }
        }
    }
    
    if (on_committed_) on_committed_();
}
