#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QColorDialog>
#include <QUndoStack>
#include <QUndoCommand>
#include <memory>
#include <deque>

#include "image_generator/image_generator.h"

class PaintCanvas : public QWidget {
    Q_OBJECT

public:
    explicit PaintCanvas(int width, int height, QWidget* parent = nullptr);
    ~PaintCanvas() override = default;

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
    void set_zoom(float zoom) { m_zoom = std::max(0.1f, zoom); update(); }
    void pan(int dx, int dy);
    float get_zoom() const { return m_zoom; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    QSize sizeHint() const override { return QSize(800, 600); }

private:
    struct DrawCommand : public QUndoCommand {
        ig::Canvas before_state;
        ig::Canvas after_state;

        DrawCommand(const ig::Canvas& before, const ig::Canvas& after, QUndoCommand* parent = nullptr);
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
    std::unique_ptr<QUndoStack> m_undo_stack;
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

class PaintApp : public QMainWindow {
    Q_OBJECT

public:
    explicit PaintApp(QWidget* parent = nullptr);
    ~PaintApp() override = default;

private slots:
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
    QLayout* create_tool_panel();
    void update_color_buttons();

    PaintCanvas* m_canvas = nullptr;
    QPushButton* m_foreground_button = nullptr;
    QPushButton* m_background_button = nullptr;
    QComboBox* m_tool_combo = nullptr;
    QSlider* m_brush_size_slider = nullptr;
    QSlider* m_opacity_slider = nullptr;
    QSpinBox* m_brush_size_spin = nullptr;
    QLabel* m_zoom_label = nullptr;
};
