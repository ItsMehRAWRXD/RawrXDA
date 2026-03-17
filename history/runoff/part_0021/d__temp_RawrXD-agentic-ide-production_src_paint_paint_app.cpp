#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "image_generator/stb_image_write.h"
#include "paint/paint_app.h"
#include <cstring>
#include <fstream>

// ======================== PaintCanvas Implementation ========================

PaintCanvas::PaintCanvas(int width, int height)
        : m_canvas(width, height), m_temp_preview(width, height), m_undo_stack(std::make_unique<NativeUndoStack>()) {
        m_canvas.clear(m_background_color);
        m_temp_preview.clear(ig::Color::transparent());
}

PaintCanvas::DrawCommand::DrawCommand(const ig::Canvas& before, const ig::Canvas& after, NativeUndoCommand* /*parent*/)
    : NativeUndoCommand("Draw"), before_state(before), after_state(after) {}

void PaintCanvas::DrawCommand::undo() {
    // Implement undo functionality using native APIs
}

void PaintCanvas::DrawCommand::redo() {
    // Implement redo functionality using native APIs
}

void PaintCanvas::save_state_for_undo() {
    if (m_history.size() >= 50) {
        m_history.pop_front();
    }
    m_history.push_back(m_canvas);
}

void PaintCanvas::clear_canvas(const ig::Color& color) {
    save_state_for_undo();
    m_canvas.clear(color);
    paint_to_canvas();
}

void PaintCanvas::draw_brush_stroke(float x, float y, float prev_x, float prev_y) {
    if (m_current_tool == PENCIL) {
        ig::Color brush_color = m_foreground_color;
        brush_color.a = m_opacity;
        ig::line_aa(m_canvas, prev_x, prev_y, x, y, brush_color);
    } else if (m_current_tool == BRUSH) {
        ig::Color brush_color = m_foreground_color;
        brush_color.a = m_opacity;
        ig::line_thick(m_canvas, prev_x, prev_y, x, y, m_brush_size, brush_color);
    } else if (m_current_tool == ERASER) {
        ig::Color erase_color = ig::Color::transparent();
        ig::line_thick(m_canvas, prev_x, prev_y, x, y, m_brush_size, erase_color);
    }
}

void PaintCanvas::draw_line_preview(float x, float y) {
    m_temp_preview = m_canvas;
    if (m_current_tool == LINE) {
        ig::Color line_color = m_foreground_color;
        line_color.a = m_opacity;
        ig::line_aa(m_temp_preview, m_shape_start_x, m_shape_start_y, x, y, line_color);
    }
}

void PaintCanvas::draw_shape_preview(float x, float y) {
    m_temp_preview = m_canvas;
    ig::Color shape_color = m_foreground_color;
    shape_color.a = m_opacity;

    switch (m_current_tool) {
        case RECTANGLE: {
            int x1 = static_cast<int>(std::min(m_shape_start_x, x));
            int y1 = static_cast<int>(std::min(m_shape_start_y, y));
            int x2 = static_cast<int>(std::max(m_shape_start_x, x));
            int y2 = static_cast<int>(std::max(m_shape_start_y, y));
            ig::fill_rect(m_temp_preview, x1, y1, x2 - x1, y2 - y1, shape_color);
            break;
        }
        case CIRCLE: {
            float r = std::sqrt((x - m_shape_start_x) * (x - m_shape_start_x) +
                                (y - m_shape_start_y) * (y - m_shape_start_y));
            ig::fill_circle(m_temp_preview, m_shape_start_x, m_shape_start_y, r, shape_color);
            break;
        }
        case ELLIPSE: {
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

void PaintCanvas::finalize_shape(float x, float y) {
    ig::Color shape_color = m_foreground_color;
    shape_color.a = m_opacity;

    switch (m_current_tool) {
        case RECTANGLE: {
            int x1 = static_cast<int>(std::min(m_shape_start_x, x));
            int y1 = static_cast<int>(std::min(m_shape_start_y, y));
            int x2 = static_cast<int>(std::max(m_shape_start_x, x));
            int y2 = static_cast<int>(std::max(m_shape_start_y, y));
            ig::fill_rect(m_canvas, x1, y1, x2 - x1, y2 - y1, shape_color);
            break;
        }
        case CIRCLE: {
            float r = std::sqrt((x - m_shape_start_x) * (x - m_shape_start_x) +
                                (y - m_shape_start_y) * (y - m_shape_start_y));
            ig::fill_circle(m_canvas, m_shape_start_x, m_shape_start_y, r, shape_color);
            break;
        }
        case ELLIPSE: {
            float rx = std::abs(x - m_shape_start_x);
            float ry = std::abs(y - m_shape_start_y);
            ig::fill_ellipse(m_canvas, m_shape_start_x, m_shape_start_y, rx, ry, shape_color);
            break;
        }
        case LINE:
            ig::line_aa(m_canvas, m_shape_start_x, m_shape_start_y, x, y, shape_color);
            break;
        case FILL: {
            int ix = static_cast<int>(x);
            int iy = static_cast<int>(y);
            if (m_canvas.in_bounds(ix, iy)) {
                ig::flood_fill(m_canvas, ix, iy, shape_color);
            }
            break;
        }
        case PICKER: {
            int ix = static_cast<int>(x);
            int iy = static_cast<int>(y);
            if (m_canvas.in_bounds(ix, iy)) {
                m_foreground_color = m_canvas.get(ix, iy);
            }
            break;
        }
        default:
            break;
    }
}

void PaintCanvas::paint_to_canvas() {
    // For headless rendering we don't draw to a GUI. This function composes
    // the temp preview into the main canvas and can be extended to write
    // rendered images to disk if needed.
    if (m_temp_preview.data.size() == m_canvas.data.size()) {
        // If zoom level or grid overlays were requested, apply them here.
        if (m_zoom > 2.0f) {
            int cell_size = static_cast<int>(16 * m_zoom);
            for (int x = 0; x < static_cast<int>(m_canvas.width * m_zoom); x += cell_size) {
                // Simple grid overlay: draw light pixels at grid lines
                for (int gy = 0; gy < m_canvas.height; ++gy) {
                    int ix = std::min(m_canvas.width - 1, (x + m_pan_x));
                    if (ix >= 0) {
                        size_t idx = (static_cast<size_t>(gy) * m_canvas.width + ix) * 4;
                        if (idx + 3 < m_canvas.data.size()) {
                            // Blend a faint gray into pixel
                            m_canvas.data[idx + 0] = (m_canvas.data[idx + 0] + 200) / 2;
                            m_canvas.data[idx + 1] = (m_canvas.data[idx + 1] + 200) / 2;
                            m_canvas.data[idx + 2] = (m_canvas.data[idx + 2] + 200) / 2;
                        }
                    }
                }
            }
        }
        // Merge temp preview if it has content
        bool preview_nonempty = false;
        for (auto b : m_temp_preview.data) { if (b != 0) { preview_nonempty = true; break; } }
        if (preview_nonempty) {
            m_canvas = m_temp_preview;
            m_temp_preview.clear(ig::Color::transparent());
        }
    }
}

void PaintCanvas::on_mouse_press(float x, float y, int button) {
    float canvas_x = (x - m_pan_x) / m_zoom;
    float canvas_y = (y - m_pan_y) / m_zoom;

    m_last_x = canvas_x;
    m_last_y = canvas_y;
    m_shape_start_x = canvas_x;
    m_shape_start_y = canvas_y;
    m_is_drawing = true;

    const int LEFT = 1;
    const int RIGHT = 2;
    if (button == LEFT) {
        if (m_current_tool == FILL || m_current_tool == PICKER) {
            finalize_shape(canvas_x, canvas_y);
            m_is_drawing = false;
        } else {
            save_state_for_undo();
        }
    } else if (button == RIGHT) {
        // Pan mode - store state
    }

    paint_to_canvas();
}

void PaintCanvas::on_mouse_move(float x, float y, int buttons) {
    float canvas_x = (x - m_pan_x) / m_zoom;
    float canvas_y = (y - m_pan_y) / m_zoom;

    const int LEFT = 1;
    const int RIGHT = 2;
    if ((buttons & LEFT) && m_is_drawing) {
        if (m_current_tool == PENCIL || m_current_tool == BRUSH || m_current_tool == ERASER) {
            draw_brush_stroke(canvas_x, canvas_y, m_last_x, m_last_y);
            m_last_x = canvas_x;
            m_last_y = canvas_y;
        } else if (m_current_tool == LINE || m_current_tool == RECTANGLE || 
                   m_current_tool == CIRCLE || m_current_tool == ELLIPSE) {
            draw_shape_preview(canvas_x, canvas_y);
            m_canvas = m_temp_preview;
        }
        paint_to_canvas();
    } else if ((buttons & RIGHT)) {
        // Pan
        pan(static_cast<int>(x - m_last_x), static_cast<int>(y - m_last_y));
        m_last_x = x;
        m_last_y = y;
    }
}

void PaintCanvas::on_mouse_release(float x, float y, int button) {
    const int LEFT = 1;
    const int RIGHT = 2;
    if (button == LEFT && m_is_drawing) {
        float canvas_x = (x - m_pan_x) / m_zoom;
        float canvas_y = (y - m_pan_y) / m_zoom;

        if (m_current_tool == LINE || m_current_tool == RECTANGLE || 
            m_current_tool == CIRCLE || m_current_tool == ELLIPSE) {
            finalize_shape(canvas_x, canvas_y);
        }
        m_is_drawing = false;
        paint_to_canvas();
    } else if (button == RIGHT) {
        // restore cursor state if applicable
    }
}

void PaintCanvas::on_wheel(float delta) {
    float zoom_factor = delta > 0 ? 1.1f : 0.9f;
    set_zoom(m_zoom * zoom_factor);
}

void PaintCanvas::pan(int dx, int dy) {
    m_pan_x += dx;
    m_pan_y += dy;
    paint_to_canvas();
}

bool PaintCanvas::save_as_bmp(const std::string& path) {
    return ig::write_bmp(m_canvas, path);
}

bool PaintCanvas::save_as_png(const std::string& path) {
    return ig::write_png(m_canvas, path);
}

bool PaintCanvas::load_from_file(const std::string& path) {
    // Check for BMP format
    if (path.length() >= 4 && path.substr(path.length() - 4) == ".bmp") {
        // Simple BMP loading: read BMP file header and data
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        
        // Read BMP file header (14 bytes)
        char bm[2];
        file.read(bm, 2);
        if (bm[0] != 'B' || bm[1] != 'M') return false;
        
        // Skip rest of header and read image data
        file.seekg(54);  // Skip to pixel data
        
        std::vector<uint8_t> pixel_data(static_cast<size_t>(m_canvas.width * m_canvas.height * 4));
        // Read BGRA format, convert to RGBA
        for (int y = m_canvas.height - 1; y >= 0; --y) {
            for (int x = 0; x < m_canvas.width; ++x) {
                size_t idx = static_cast<size_t>((y * m_canvas.width + x) * 4);
                uint8_t b, g, r, a;
                file.read(reinterpret_cast<char*>(&b), 1);
                file.read(reinterpret_cast<char*>(&g), 1);
                file.read(reinterpret_cast<char*>(&r), 1);
                file.read(reinterpret_cast<char*>(&a), 1);
                pixel_data[idx + 0] = r;
                pixel_data[idx + 1] = g;
                pixel_data[idx + 2] = b;
                pixel_data[idx + 3] = a;
            }
        }
        
        // Save current state to history and load new canvas
        m_history.push_back(m_canvas);
        m_canvas.data = pixel_data;
        paint_to_canvas();
        return true;
    }
    
    // For PNG and other formats, would need stb_image library
    // For now, return false for unsupported formats
    return false;
}

void PaintCanvas::undo() {
    if (!m_history.empty()) {
        m_canvas = m_history.back();
        m_history.pop_back();
        paint_to_canvas();
    }
}

void PaintCanvas::redo() {
    // Note: Full redo requires maintaining a separate redo stack.
    // For now, we implement a simple version using the undo history.
    // A full implementation would track redo states separately.
    if (!m_history.empty()) {
        // Simple redo: restore from history (would need a dedicated redo_history in full implementation)
        // For now, this is a placeholder that maintains consistency
        paint_to_canvas();
    }
}

// ======================== PaintApp Implementation ========================

PaintApp::PaintApp() {
    // Headless PaintApp initialization
    m_canvas = std::make_unique<PaintCanvas>(1024, 768);
    create_tool_panel();
}

void PaintApp::create_tool_panel() {
    // Headless tool panel: initialize default values and populate tool list
    m_tool_index = 0;
    m_brush_size = 3;
    m_opacity = 100;
    m_zoom = 1.0f;

        // Headless tool panel: initialize default values and populate tool list
        m_tool_index = 0;
        m_brush_size = 3;
        m_opacity = 100;
        m_zoom = 1.0f;
        // In a full native GUI implementation this would create UI widgets and hook up callbacks.
}

void PaintApp::update_color_buttons() {
    // Headless: nothing to update visually. This can log or notify UI hooks.
    (void)m_canvas;
}

void PaintApp::new_file() {
    m_canvas->clear_canvas(ig::Color::white());
}

bool PaintApp::open_file(const std::string& path) {
    return m_canvas->load_from_file(path);
}

bool PaintApp::save_as_bmp(const std::string& path) {
    return m_canvas->save_as_bmp(path);
}

bool PaintApp::save_as_png(const std::string& path) {
    return m_canvas->save_as_png(path);
}

void PaintApp::set_foreground_color(const ig::Color& color) {
    m_canvas->set_foreground_color(color);
}

void PaintApp::set_background_color(const ig::Color& color) {
    m_canvas->set_background_color(color);
}

void PaintApp::set_tool(int index) {
    m_canvas->set_tool(static_cast<PaintCanvas::Tool>(index));
    m_tool_index = index;
}

void PaintApp::set_brush_size(int value) {
    m_canvas->set_brush_size(static_cast<float>(value));
    m_brush_size = value;
}

void PaintApp::set_opacity(int value) {
    m_canvas->set_opacity(value / 100.0f);
    m_opacity = value;
}

void PaintApp::undo() { m_canvas->undo(); }
void PaintApp::redo() { m_canvas->redo(); }
void PaintApp::clear_canvas() { m_canvas->clear_canvas(ig::Color::white()); }

void PaintApp::zoom_in() {
    m_zoom *= 1.2f;
    m_canvas->set_zoom(m_zoom);
}
void PaintApp::zoom_out() {
    m_zoom /= 1.2f;
    m_canvas->set_zoom(m_zoom);
}
void PaintApp::zoom_reset() { m_zoom = 1.0f; m_canvas->set_zoom(1.0f); }
