#pragma once

#include <memory>
#include <deque>
#include "native_widgets.h"
#include "native_undo_stack.h"

#include "image_generator/image_generator.h"

class PaintCanvas {
public:
    explicit PaintCanvas(int width, int height);
    ~PaintCanvas() = default;

    // Tool types
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

    // Canvas operations
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

    // File operations
    bool save_as_bmp(const std::string& path);
    bool save_as_png(const std::string& path);
    bool load_from_file(const std::string& path);

    // Undo/Redo
    void undo();
    void redo();

    // Zoom and pan
    void set_zoom(float zoom) { m_zoom = std::max(0.1f, zoom); }
    void pan(int dx, int dy);
    float get_zoom() const { return m_zoom; }

    // Headless / platform-agnostic event hooks
    void paint_to_canvas(); // invoke painting logic and update internal image
    void on_mouse_press(float x, float y, int button);
    void on_mouse_move(float x, float y, int buttons);
    void on_mouse_release(float x, float y, int button);
    void on_wheel(float delta);

private:
    struct DrawCommand : public NativeUndoCommand {
        ig::Canvas before_state;
        ig::Canvas after_state;

        DrawCommand(const ig::Canvas& before, const ig::Canvas& after, NativeUndoCommand* parent = nullptr);
        void undo() override;
        void redo() override;
    };

    void save_state_for_undo();
    void draw_brush_stroke(float x, float y, float prev_x, float prev_y);
    void draw_line_preview(float x, float y);
    void draw_shape_preview(float x, float y);
    void finalize_shape(float x, float y);

    ig::Canvas m_canvas;
    ig::Canvas m_temp_preview;  // For previewing shapes
    std::unique_ptr<NativeUndoStack> m_undo_stack;
    std::deque<ig::Canvas> m_history;

    Tool m_current_tool = PENCIL;
    ig::Color m_foreground_color = ig::Color::black();
    ig::Color m_background_color = ig::Color::white();
    float m_brush_size = 3.0f;
    float m_opacity = 1.0f;
    float m_zoom = 1.0f;
    int m_pan_x = 0;
    int m_pan_y = 0;

    // For shape drawing
    float m_shape_start_x = 0, m_shape_start_y = 0;
    bool m_is_drawing = false;
    float m_last_x = 0, m_last_y = 0;
};
class PaintApp {
public:
    PaintApp();
    ~PaintApp() = default;

    // High-level actions that would be wired to a GUI in a full implementation
    void new_file();
    bool open_file(const std::string& path);
    bool save_as_bmp(const std::string& path);
    bool save_as_png(const std::string& path);
    void set_foreground_color(const ig::Color& color);
    void set_background_color(const ig::Color& color);
    void set_tool(int index);
    void set_brush_size(int value);
    void set_opacity(int value);
    void undo();
    void redo();
    void clear_canvas();
    void zoom_in();
    void zoom_out();
    void zoom_reset();

private:
    void create_tool_panel();
    void update_color_buttons();

    std::unique_ptr<PaintCanvas> m_canvas;
    // UI state (headless representation)
    int m_tool_index = 0;
    int m_brush_size = 3;
    int m_opacity = 100; // percent
    float m_zoom = 1.0f;
};
