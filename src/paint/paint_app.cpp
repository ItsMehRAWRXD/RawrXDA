#include "paint_app.h"
#include "image_generator/image_generator.h"
#include "win32_file_dialog.h"
#include <cstring>

// ======================== PaintCanvas Implementation (Qt-free) ========================

PaintCanvas::PaintCanvas(int width, int height, void* parent) : m_canvas(width, height), m_temp_preview(width, height)
{
    (void)parent;
    m_canvas.clear(m_background_color);
    m_temp_preview.clear(ig::Color::transparent());
}

void PaintCanvas::save_state_for_undo()
{
    if (m_history.size() >= 50)
    {
        m_history.pop_front();
    }
    m_history.push_back(m_canvas);
    m_redo_stack.clear();
}

void PaintCanvas::clear_canvas(const ig::Color& color)
{
    save_state_for_undo();
    m_canvas.clear(color);
}

void PaintCanvas::draw_brush_stroke(float x, float y, float prev_x, float prev_y)
{
    if (m_current_tool == PENCIL)
    {
        ig::Color brush_color = m_foreground_color;
        brush_color.a = m_opacity;
        ig::line_aa(m_canvas, prev_x, prev_y, x, y, brush_color);
    }
    else if (m_current_tool == BRUSH)
    {
        ig::Color brush_color = m_foreground_color;
        brush_color.a = m_opacity;
        ig::line_thick(m_canvas, prev_x, prev_y, x, y, m_brush_size, brush_color);
    }
    else if (m_current_tool == ERASER)
    {
        ig::Color erase_color = ig::Color::transparent();
        ig::line_thick(m_canvas, prev_x, prev_y, x, y, m_brush_size, erase_color);
    }
}

void PaintCanvas::draw_line_preview(float x, float y)
{
    m_temp_preview = m_canvas;
    if (m_current_tool == LINE)
    {
        ig::Color line_color = m_foreground_color;
        line_color.a = m_opacity;
        ig::line_aa(m_temp_preview, m_shape_start_x, m_shape_start_y, x, y, line_color);
    }
}

void PaintCanvas::draw_shape_preview(float x, float y)
{
    m_temp_preview = m_canvas;
    ig::Color shape_color = m_foreground_color;
    shape_color.a = m_opacity;

    switch (m_current_tool)
    {
        case RECTANGLE:
        {
            int x1 = static_cast<int>(std::min(m_shape_start_x, x));
            int y1 = static_cast<int>(std::min(m_shape_start_y, y));
            int x2 = static_cast<int>(std::max(m_shape_start_x, x));
            int y2 = static_cast<int>(std::max(m_shape_start_y, y));
            ig::fill_rect(m_temp_preview, x1, y1, x2 - x1, y2 - y1, shape_color);
            break;
        }
        case CIRCLE:
        {
            float r = std::sqrt((x - m_shape_start_x) * (x - m_shape_start_x) +
                                (y - m_shape_start_y) * (y - m_shape_start_y));
            ig::fill_circle(m_temp_preview, m_shape_start_x, m_shape_start_y, r, shape_color);
            break;
        }
        case ELLIPSE:
        {
            float rx = std::abs(x - m_shape_start_x);
            float ry = std::abs(y - m_shape_start_y);
            ig::fill_ellipse(m_temp_preview, m_shape_start_x, m_shape_start_y, rx, ry, shape_color);
            break;
        }
        case LINE:
            ig::line_aa(m_temp_preview, m_shape_start_x, m_shape_start_y, x, y, shape_color);
            break;
        default:
            break;
    }
}

void PaintCanvas::finalize_shape(float x, float y)
{
    ig::Color shape_color = m_foreground_color;
    shape_color.a = m_opacity;

    switch (m_current_tool)
    {
        case RECTANGLE:
        {
            int x1 = static_cast<int>(std::min(m_shape_start_x, x));
            int y1 = static_cast<int>(std::min(m_shape_start_y, y));
            int x2 = static_cast<int>(std::max(m_shape_start_x, x));
            int y2 = static_cast<int>(std::max(m_shape_start_y, y));
            ig::fill_rect(m_canvas, x1, y1, x2 - x1, y2 - y1, shape_color);
            break;
        }
        case CIRCLE:
        {
            float r = std::sqrt((x - m_shape_start_x) * (x - m_shape_start_x) +
                                (y - m_shape_start_y) * (y - m_shape_start_y));
            ig::fill_circle(m_canvas, m_shape_start_x, m_shape_start_y, r, shape_color);
            break;
        }
        case ELLIPSE:
        {
            float rx = std::abs(x - m_shape_start_x);
            float ry = std::abs(y - m_shape_start_y);
            ig::fill_ellipse(m_canvas, m_shape_start_x, m_shape_start_y, rx, ry, shape_color);
            break;
        }
        case LINE:
            ig::line_aa(m_canvas, m_shape_start_x, m_shape_start_y, x, y, shape_color);
            break;
        case FILL:
        {
            int ix = static_cast<int>(x);
            int iy = static_cast<int>(y);
            if (m_canvas.in_bounds(ix, iy))
            {
                ig::flood_fill(m_canvas, ix, iy, shape_color);
            }
            break;
        }
        case PICKER:
        {
            int ix = static_cast<int>(x);
            int iy = static_cast<int>(y);
            if (m_canvas.in_bounds(ix, iy))
            {
                m_foreground_color = m_canvas.get(ix, iy);
            }
            break;
        }
        default:
            break;
    }
}

#ifdef _WIN32
#include <windows.h>
#endif

void PaintCanvas::paint(void* hdc)
{
#ifdef _WIN32
    HDC dc = static_cast<HDC>(hdc);
    if (!dc || m_canvas.data.empty())
        return;
    int w = m_canvas.width, h = m_canvas.height;
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP hb = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (hb && bits)
    {
        std::memcpy(bits, m_canvas.data.data(), static_cast<size_t>(w * h * 4));
        HDC mem = CreateCompatibleDC(dc);
        HGDIOBJ old = SelectObject(mem, hb);
        StretchBlt(dc, m_pan_x, m_pan_y, static_cast<int>(w * m_zoom), static_cast<int>(h * m_zoom), mem, 0, 0, w, h,
                   SRCCOPY);
        SelectObject(mem, old);
        DeleteDC(mem);
        DeleteObject(hb);
    }
#else
    (void)hdc;
#endif
}

void PaintCanvas::mousePress(int x, int y)
{
    float canvas_x = (x - m_pan_x) / m_zoom;
    float canvas_y = (y - m_pan_y) / m_zoom;
    m_last_x = canvas_x;
    m_last_y = canvas_y;
    m_shape_start_x = canvas_x;
    m_shape_start_y = canvas_y;
    m_is_drawing = true;
    if (m_current_tool == FILL || m_current_tool == PICKER)
    {
        finalize_shape(canvas_x, canvas_y);
        m_is_drawing = false;
    }
    else
    {
        save_state_for_undo();
    }
}

void PaintCanvas::mouseMove(int x, int y)
{
    float canvas_x = (x - m_pan_x) / m_zoom;
    float canvas_y = (y - m_pan_y) / m_zoom;
    if (m_is_drawing)
    {
        if (m_current_tool == PENCIL || m_current_tool == BRUSH || m_current_tool == ERASER)
        {
            draw_brush_stroke(canvas_x, canvas_y, m_last_x, m_last_y);
            m_last_x = canvas_x;
            m_last_y = canvas_y;
        }
        else if (m_current_tool == LINE || m_current_tool == RECTANGLE || m_current_tool == CIRCLE ||
                 m_current_tool == ELLIPSE)
        {
            draw_shape_preview(canvas_x, canvas_y);
            m_canvas = m_temp_preview;
        }
    }
}

void PaintCanvas::mouseRelease(int x, int y)
{
    if (m_is_drawing)
    {
        float canvas_x = (x - m_pan_x) / m_zoom;
        float canvas_y = (y - m_pan_y) / m_zoom;
        if (m_current_tool == LINE || m_current_tool == RECTANGLE || m_current_tool == CIRCLE ||
            m_current_tool == ELLIPSE)
            finalize_shape(canvas_x, canvas_y);
        m_is_drawing = false;
    }
}

void PaintCanvas::wheel(int delta)
{
    float zoom_factor = delta > 0 ? 1.1f : 0.9f;
    set_zoom(m_zoom * zoom_factor);
}

void PaintCanvas::pan(int dx, int dy)
{
    m_pan_x += dx;
    m_pan_y += dy;
}

bool PaintCanvas::save_as_bmp(const std::string& path)
{
    return ig::write_bmp(m_canvas, path);
}

bool PaintCanvas::save_as_png(const std::string& path)
{
    return ig::write_png(m_canvas, path);
}

bool PaintCanvas::load_from_file(const std::string& path)
{
#ifdef _WIN32
    std::string fileName = path;
    if (fileName.empty())
    {
        const char* filter = "Images (*.png;*.xpm;*.jpg)\0*.png;*.xpm;*.jpg\0PNG (*.png)\0*.png\0All (*.*)\0*.*\0";
        fileName = RawrXD::getOpenFileName(this, "Open Image", filter);
    }
    if (!fileName.empty())
    {
        ig::Canvas loaded = ig::load_image(fileName);
        if (loaded.width > 0 && loaded.height > 0)
        {
            m_canvas = std::move(loaded);
            return true;
        }
    }
#endif
    return false;
}

void PaintCanvas::undo()
{
    if (!m_history.empty())
    {
        m_redo_stack.push_back(m_canvas);
        m_canvas = m_history.back();
        m_history.pop_back();
    }
}

void PaintCanvas::redo()
{
    if (!m_redo_stack.empty())
    {
        m_canvas = m_redo_stack.back();
        m_redo_stack.pop_back();
    }
}

// ======================== PaintApp Implementation ========================

PaintApp::PaintApp(void* parent)
{
    (void)parent;
    m_canvas = new PaintCanvas(1024, 768, nullptr);
}

void PaintApp::create_menu_bar() {}

void* PaintApp::create_tool_panel()
{
    return nullptr;
}
void PaintApp::update_color_buttons() {}

void PaintApp::on_new_file()
{
    if (m_canvas)
        m_canvas->clear_canvas(ig::Color::white());
}

void PaintApp::on_open_file()
{
#ifdef _WIN32
    if (!m_canvas)
        return;
    const char* filter = "Images (*.png;*.bmp;*.jpg)\0*.png;*.bmp;*.jpg\0All (*.*)\0*.*\0";
    std::string file_path = RawrXD::getOpenFileName(nullptr, "Open Image", filter);
    if (!file_path.empty())
        m_canvas->load_from_file(file_path);
#endif
}

void PaintApp::on_save_as_bmp()
{
#ifdef _WIN32
    if (!m_canvas)
        return;
    const char* filter = "BMP (*.bmp)\0*.bmp\0All (*.*)\0*.*\0";
    std::string file_path = RawrXD::getSaveFileName(nullptr, "Save as BMP", filter, "bmp");
    if (!file_path.empty())
        m_canvas->save_as_bmp(file_path);
#endif
}

void PaintApp::on_save_as_png()
{
#ifdef _WIN32
    if (!m_canvas)
        return;
    const char* filter = "PNG (*.png)\0*.png\0All (*.*)\0*.*\0";
    std::string file_path = RawrXD::getSaveFileName(nullptr, "Save as PNG", filter, "png");
    if (!file_path.empty())
        m_canvas->save_as_png(file_path);
#endif
}

void PaintApp::on_foreground_color_clicked() {}

void PaintApp::on_background_color_clicked() {}

void PaintApp::on_tool_changed(int index)
{
    if (m_canvas)
        m_canvas->set_tool(static_cast<PaintCanvas::Tool>(index));
}

void PaintApp::on_brush_size_changed(int value)
{
    if (m_canvas)
        m_canvas->set_brush_size(static_cast<float>(value));
}

void PaintApp::on_opacity_changed(int value)
{
    if (m_canvas)
        m_canvas->set_opacity(value / 100.0f);
}

void PaintApp::on_undo()
{
    if (m_canvas)
        m_canvas->undo();
}

void PaintApp::on_redo()
{
    if (m_canvas)
        m_canvas->redo();
}

void PaintApp::on_clear_canvas()
{
    on_new_file();
}

void PaintApp::on_zoom_in()
{
    if (m_canvas)
        m_canvas->set_zoom(m_canvas->get_zoom() * 1.2f);
}

void PaintApp::on_zoom_out()
{
    if (m_canvas)
        m_canvas->set_zoom(m_canvas->get_zoom() / 1.2f);
}

void PaintApp::on_zoom_reset()
{
    if (m_canvas)
        m_canvas->set_zoom(1.0f);
}
