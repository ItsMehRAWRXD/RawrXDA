#pragma once

// ============================================================================
// PaintCanvas / PaintApp — C++20, Win32. No Qt. (QWidget, QUndoStack removed)
// ============================================================================

#include <deque>
#include <memory>
#include <string>

#include "image_generator/image_generator.h"

class PaintCanvas {
public:
    explicit PaintCanvas(int width, int height, void* parent = nullptr);
    ~PaintCanvas() = default;

    enum Tool {
        PENCIL,
        BRUSH,
        ERASER,
        LINE,
        RECTANGLE,
        CIRCLE,
        ELLIPSE,
        FILL,
        PICKER,
        TEXT
    };

    void clear_canvas(const ig::Color& color = ig::Color::white());
    void set_tool(Tool tool) { m_current_tool = tool; }
    void set_foreground_color(const ig::Color& color) { m_foreground_color = color; }
    void set_background_color(const ig::Color& color) { m_background_color = color; }
    void set_brush_size(float size) { m_brush_size = std::max(1.0f, size); }
    void set_opacity(float opacity) { m_opacity = ig::clamp01(opacity); }

    ig::Color get_foreground_color() const { return m_foreground_color; }
    ig::Color get_background_color() const { return m_background_color; }
    float get_brush_size() const { return m_brush_size; }
    float get_opacity() const { return m_opacity; }

    bool save_as_bmp(const std::string& path);
    bool save_as_png(const std::string& path);
    bool load_from_file(const std::string& path);

    void undo();
    void redo();

    void set_zoom(float zoom) { m_zoom = std::max(0.1f, zoom); }
    void pan(int dx, int dy);
    float get_zoom() const { return m_zoom; }

    /** Win32: call from WM_PAINT to render canvas into HDC */
    void paint(void* hdc);
    /** Win32: call from WM_LBUTTONDOWN, WM_MOUSEMOVE, WM_LBUTTONUP, WM_MOUSEWHEEL */
    void mousePress(int x, int y);
    void mouseMove(int x, int y);
    void mouseRelease(int x, int y);
    void wheel(int delta);

private:
    void save_state_for_undo();
    void draw_brush_stroke(float x, float y, float prev_x, float prev_y);
    void draw_line_preview(float x, float y);
    void draw_shape_preview(float x, float y);
    void finalize_shape(float x, float y);

    ig::Canvas m_canvas;
    ig::Canvas m_temp_preview;
    std::deque<ig::Canvas> m_undo_stack;
    std::deque<ig::Canvas> m_redo_stack;
    std::deque<ig::Canvas> m_history;

    Tool m_current_tool = PENCIL;
    ig::Color m_foreground_color = ig::Color::black();
    ig::Color m_background_color = ig::Color::white();
    float m_brush_size = 3.0f;
    float m_opacity = 1.0f;
    float m_zoom = 1.0f;
    int m_pan_x = 0;
    int m_pan_y = 0;

    float m_shape_start_x = 0, m_shape_start_y = 0;
    bool m_is_drawing = false;
    float m_last_x = 0, m_last_y = 0;
};

class PaintApp {
public:
    explicit PaintApp(void* parent = nullptr);
    ~PaintApp() = default;

    void on_new_file();
    void on_open_file();
    void on_save_as_bmp();
    void on_save_as_png();
    void on_foreground_color_clicked();
    void on_background_color_clicked();
    void on_tool_changed(int index);
    void on_brush_size_changed(int value);
    void on_opacity_changed(int value);
    void on_undo();
    void on_redo();
    void on_clear_canvas();
    void on_zoom_in();
    void on_zoom_out();
    void on_zoom_reset();

private:
    void create_menu_bar();
    void* create_tool_panel();
    void update_color_buttons();

    PaintCanvas* m_canvas = nullptr;
    void* m_foreground_button = nullptr;
    void* m_background_button = nullptr;
    void* m_tool_combo = nullptr;
    void* m_brush_size_slider = nullptr;
    void* m_opacity_slider = nullptr;
    void* m_brush_size_spin = nullptr;
    void* m_zoom_label = nullptr;
};
