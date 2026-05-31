#pragma once

#include <windows.h>
#include <vector>

struct RXDControl
{
    HWND hwnd;
    float x_pct;
    float y_pct;
    float w_pct;
    float h_pct;
    int off_x;
    int off_y;
    int off_w;
    int off_h;
};

class RXDLayoutEngine
{
  public:
    void Clear()
    {
        controls_.clear();
    }

    void Reserve(size_t count)
    {
        controls_.reserve(count);
    }

    void AddControl(HWND h, float xp, float yp, float wp, float hp, int ox = 0, int oy = 0, int ow = 0, int oh = 0)
    {
        if (!h)
            return;
        controls_.push_back({h, xp, yp, wp, hp, ox, oy, ow, oh});
    }

    bool Update(HWND parent)
    {
        if (!parent || controls_.empty())
            return false;

        RECT r{};
        if (!GetClientRect(parent, &r))
            return false;

        const int pw = r.right - r.left;
        const int ph = r.bottom - r.top;
        if (pw <= 0 || ph <= 0)
            return false;

        HDWP hdwp = BeginDeferWindowPos((int)controls_.size());
        if (!hdwp)
            return false;

        for (const auto& c : controls_)
        {
            int x = (int)(pw * c.x_pct) + c.off_x;
            int y = (int)(ph * c.y_pct) + c.off_y;
            int w = (int)(pw * c.w_pct) + c.off_w;
            int h = (int)(ph * c.h_pct) + c.off_h;

            if (w < 0)
                w = 0;
            if (h < 0)
                h = 0;

            hdwp = DeferWindowPos(hdwp, c.hwnd, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
            if (!hdwp)
                return false;
        }

        return EndDeferWindowPos(hdwp) != FALSE;
    }

  private:
    std::vector<RXDControl> controls_;
};
