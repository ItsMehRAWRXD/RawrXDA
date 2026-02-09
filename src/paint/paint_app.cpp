#include "paint_app.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QMenuBar>
#include <QMenu>
#include <QFileDialog>
#include <QColorDialog>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QSlider>
#include <QSpinBox>
#include <QUndoStack>
#include <QUndoCommand>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMessageBox>
#include <cstring>

// ======================== PaintCanvas Implementation ========================

PaintCanvas::PaintCanvas(int width, int height, QWidget* parent)
    : QWidget(parent), m_canvas(width, height), m_temp_preview(width, height),
      m_undo_stack(std::make_unique<QUndoStack>()) {
    m_canvas.clear(m_background_color);
    m_temp_preview.clear(ig::Color::transparent());
    setFocusPolicy(Qt::StrongFocus);
}

PaintCanvas::DrawCommand::DrawCommand(const ig::Canvas& before, const ig::Canvas& after, QUndoCommand* parent)
    : QUndoCommand("Draw", parent), before_state(before), after_state(after) {}

void PaintCanvas::DrawCommand::undo() {
    // Note: This needs access to the parent canvas, typically handled through a callback
}

void PaintCanvas::DrawCommand::redo() {
    // Note: This needs access to the parent canvas, typically handled through a callback
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
    update();
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

void PaintCanvas::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    
    // Create QImage from canvas data
    QImage qimage(m_canvas.width, m_canvas.height, QImage::Format_RGBA8888);
    std::memcpy(qimage.bits(), m_canvas.data.data(), m_canvas.data.size());

    // Draw on window
    QRect target_rect(m_pan_x, m_pan_y, 
                      static_cast<int>(m_canvas.width * m_zoom), 
                      static_cast<int>(m_canvas.height * m_zoom));
    painter.drawImage(target_rect, qimage);

    // Draw grid if zoomed in
    if (m_zoom > 2.0f) {
        painter.setPen(QPen(QColor(200, 200, 200, 100), 1, Qt::DashLine));
        int cell_size = static_cast<int>(16 * m_zoom);
        for (int x = 0; x < m_canvas.width * m_zoom; x += cell_size) {
            painter.drawLine(m_pan_x + x, m_pan_y, m_pan_x + x, 
                           m_pan_y + static_cast<int>(m_canvas.height * m_zoom));
        }
        for (int y = 0; y < m_canvas.height * m_zoom; y += cell_size) {
            painter.drawLine(m_pan_x, m_pan_y + y, 
                           m_pan_x + static_cast<int>(m_canvas.width * m_zoom), m_pan_y + y);
        }
    }
}

void PaintCanvas::mousePressEvent(QMouseEvent* event) {
    float canvas_x = (event->x() - m_pan_x) / m_zoom;
    float canvas_y = (event->y() - m_pan_y) / m_zoom;

    m_last_x = canvas_x;
    m_last_y = canvas_y;
    m_shape_start_x = canvas_x;
    m_shape_start_y = canvas_y;
    m_is_drawing = true;

    if (event->button() == Qt::LeftButton) {
        if (m_current_tool == FILL || m_current_tool == PICKER) {
            finalize_shape(canvas_x, canvas_y);
            m_is_drawing = false;
        } else {
            save_state_for_undo();
        }
    } else if (event->button() == Qt::RightButton) {
        // Pan mode
        setCursor(Qt::ClosedHandCursor);
    }

    update();
}

void PaintCanvas::mouseMoveEvent(QMouseEvent* event) {
    float canvas_x = (event->x() - m_pan_x) / m_zoom;
    float canvas_y = (event->y() - m_pan_y) / m_zoom;

    if (event->buttons() & Qt::LeftButton && m_is_drawing) {
        if (m_current_tool == PENCIL || m_current_tool == BRUSH || m_current_tool == ERASER) {
            draw_brush_stroke(canvas_x, canvas_y, m_last_x, m_last_y);
            m_last_x = canvas_x;
            m_last_y = canvas_y;
        } else if (m_current_tool == LINE || m_current_tool == RECTANGLE || 
                   m_current_tool == CIRCLE || m_current_tool == ELLIPSE) {
            draw_shape_preview(canvas_x, canvas_y);
            m_canvas = m_temp_preview;
        }
        update();
    } else if (event->buttons() & Qt::RightButton) {
        // Pan
        pan(event->x() - m_last_x, event->y() - m_last_y);
        m_last_x = event->x();
        m_last_y = event->y();
    }
}

void PaintCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_is_drawing) {
        float canvas_x = (event->x() - m_pan_x) / m_zoom;
        float canvas_y = (event->y() - m_pan_y) / m_zoom;

        if (m_current_tool == LINE || m_current_tool == RECTANGLE || 
            m_current_tool == CIRCLE || m_current_tool == ELLIPSE) {
            finalize_shape(canvas_x, canvas_y);
        }
        m_is_drawing = false;
        update();
    } else if (event->button() == Qt::RightButton) {
        setCursor(Qt::ArrowCursor);
    }
}

void PaintCanvas::wheelEvent(QWheelEvent* event) {
    float zoom_factor = event->angleDelta().y() > 0 ? 1.1f : 0.9f;
    set_zoom(m_zoom * zoom_factor);
}

void PaintCanvas::pan(int dx, int dy) {
    m_pan_x += dx;
    m_pan_y += dy;
    update();
}

bool PaintCanvas::save_as_bmp(const std::string& path) {
    return ig::write_bmp(m_canvas, path);
}

bool PaintCanvas::save_as_png(const std::string& path) {
    return ig::write_png(m_canvas, path);
}

bool PaintCanvas::load_from_file(const std::string& path) {
    // Try loading as PNG first, then BMP
    ig::Image loaded;
    bool ok = ig::read_png(loaded, path);
    if (!ok) {
        ok = ig::read_bmp(loaded, path);
    }
    if (ok && loaded.width > 0 && loaded.height > 0) {
        push_history();
        m_canvas = loaded;
        update();
    }
    return ok;
}

void PaintCanvas::undo() {
    if (!m_history.empty()) {
        m_canvas = m_history.back();
        m_history.pop_back();
        update();
    }
}

void PaintCanvas::redo() {
    if (!m_redoStack.empty()) {
        m_history.push_back(m_canvas);
        m_canvas = m_redoStack.back();
        m_redoStack.pop_back();
        update();
    }
}

// ======================== PaintApp Implementation ========================

PaintApp::PaintApp(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("RawrXD Paint - AI Enhanced Canvas");
    resize(1200, 800);

    // Create central widget
    auto central_widget = new QWidget(this);
    setCentralWidget(central_widget);

    // Create main layout
    auto main_layout = new QHBoxLayout(central_widget);

    // Create paint canvas
    m_canvas = new PaintCanvas(1024, 768, this);
    main_layout->addWidget(m_canvas, 1);

    // Create tool panel
    create_tool_panel();

    // Create menu bar
    create_menu_bar();
}

void PaintApp::create_menu_bar() {
    auto file_menu = menuBar()->addMenu(tr("&File"));

    auto new_action = file_menu->addAction(tr("&New"));
    connect(new_action, &QAction::triggered, this, &PaintApp::on_new_file);

    auto open_action = file_menu->addAction(tr("&Open"));
    connect(open_action, &QAction::triggered, this, &PaintApp::on_open_file);

    file_menu->addSeparator();

    auto save_bmp_action = file_menu->addAction(tr("Save as &BMP"));
    connect(save_bmp_action, &QAction::triggered, this, &PaintApp::on_save_as_bmp);

    auto save_png_action = file_menu->addAction(tr("Save as &PNG"));
    connect(save_png_action, &QAction::triggered, this, &PaintApp::on_save_as_png);

    file_menu->addSeparator();

    auto exit_action = file_menu->addAction(tr("E&xit"));
    connect(exit_action, &QAction::triggered, this, &QWidget::close);

    auto edit_menu = menuBar()->addMenu(tr("&Edit"));

    auto undo_action = edit_menu->addAction(tr("&Undo"));
    connect(undo_action, &QAction::triggered, this, &PaintApp::on_undo);

    auto redo_action = edit_menu->addAction(tr("&Redo"));
    connect(redo_action, &QAction::triggered, this, &PaintApp::on_redo);

    edit_menu->addSeparator();

    auto clear_action = edit_menu->addAction(tr("&Clear Canvas"));
    connect(clear_action, &QAction::triggered, this, &PaintApp::on_clear_canvas);

    auto view_menu = menuBar()->addMenu(tr("&View"));

    auto zoom_in_action = view_menu->addAction(tr("Zoom &In"));
    connect(zoom_in_action, &QAction::triggered, this, &PaintApp::on_zoom_in);

    auto zoom_out_action = view_menu->addAction(tr("Zoom &Out"));
    connect(zoom_out_action, &QAction::triggered, this, &PaintApp::on_zoom_out);

    auto zoom_reset_action = view_menu->addAction(tr("&Reset Zoom"));
    connect(zoom_reset_action, &QAction::triggered, this, &PaintApp::on_zoom_reset);
}

QLayout* PaintApp::create_tool_panel() {
    auto panel_layout = new QVBoxLayout();

    // Tool selection
    panel_layout->addWidget(new QLabel("Tools:"));
    m_tool_combo = new QComboBox();
    m_tool_combo->addItem("Pencil");
    m_tool_combo->addItem("Brush");
    m_tool_combo->addItem("Eraser");
    m_tool_combo->addItem("Line");
    m_tool_combo->addItem("Rectangle");
    m_tool_combo->addItem("Circle");
    m_tool_combo->addItem("Ellipse");
    m_tool_combo->addItem("Fill");
    m_tool_combo->addItem("Color Picker");
    m_tool_combo->addItem("Text");
    connect(m_tool_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PaintApp::on_tool_changed);
    panel_layout->addWidget(m_tool_combo);

    panel_layout->addSpacing(20);

    // Color selection
    panel_layout->addWidget(new QLabel("Colors:"));
    m_foreground_button = new QPushButton();
    m_foreground_button->setFixedSize(60, 60);
    m_foreground_button->setStyleSheet("background-color: black;");
    connect(m_foreground_button, &QPushButton::clicked, this, &PaintApp::on_foreground_color_clicked);
    panel_layout->addWidget(new QLabel("Foreground:"));
    panel_layout->addWidget(m_foreground_button);

    m_background_button = new QPushButton();
    m_background_button->setFixedSize(60, 60);
    m_background_button->setStyleSheet("background-color: white;");
    connect(m_background_button, &QPushButton::clicked, this, &PaintApp::on_background_color_clicked);
    panel_layout->addWidget(new QLabel("Background:"));
    panel_layout->addWidget(m_background_button);

    panel_layout->addSpacing(20);

    // Brush settings
    panel_layout->addWidget(new QLabel("Brush Size:"));
    m_brush_size_spin = new QSpinBox();
    m_brush_size_spin->setRange(1, 100);
    m_brush_size_spin->setValue(3);
    connect(m_brush_size_spin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PaintApp::on_brush_size_changed);
    panel_layout->addWidget(m_brush_size_spin);

    m_brush_size_slider = new QSlider(Qt::Horizontal);
    m_brush_size_slider->setRange(1, 100);
    m_brush_size_slider->setValue(3);
    connect(m_brush_size_slider, &QSlider::valueChanged,
            this, &PaintApp::on_brush_size_changed);
    panel_layout->addWidget(m_brush_size_slider);

    panel_layout->addSpacing(20);

    // Opacity
    panel_layout->addWidget(new QLabel("Opacity:"));
    m_opacity_slider = new QSlider(Qt::Horizontal);
    m_opacity_slider->setRange(0, 100);
    m_opacity_slider->setValue(100);
    connect(m_opacity_slider, &QSlider::valueChanged,
            this, &PaintApp::on_opacity_changed);
    panel_layout->addWidget(m_opacity_slider);

    panel_layout->addSpacing(20);

    // Zoom info
    m_zoom_label = new QLabel("Zoom: 100%");
    panel_layout->addWidget(m_zoom_label);

    panel_layout->addStretch();

    // Create tool panel widget and add to main layout
    auto tool_panel_widget = new QWidget();
    tool_panel_widget->setLayout(panel_layout);
    tool_panel_widget->setMaximumWidth(250);
    
    auto main_layout = qobject_cast<QHBoxLayout*>(centralWidget()->layout());
    if (main_layout) {
        main_layout->addWidget(tool_panel_widget, 0);
    }
}

void PaintApp::update_color_buttons() {
    QColor fg(static_cast<int>(m_canvas->get_foreground_color().r * 255),
              static_cast<int>(m_canvas->get_foreground_color().g * 255),
              static_cast<int>(m_canvas->get_foreground_color().b * 255));
    m_foreground_button->setStyleSheet(
        QString("background-color: rgb(%1, %2, %3);").arg(fg.red()).arg(fg.green()).arg(fg.blue()));

    QColor bg(static_cast<int>(m_canvas->get_background_color().r * 255),
              static_cast<int>(m_canvas->get_background_color().g * 255),
              static_cast<int>(m_canvas->get_background_color().b * 255));
    m_background_button->setStyleSheet(
        QString("background-color: rgb(%1, %2, %3);").arg(bg.red()).arg(bg.green()).arg(bg.blue()));
}

void PaintApp::on_new_file() {
    m_canvas->clear_canvas(ig::Color::white());
}

void PaintApp::on_open_file() {
    QString file_path = QFileDialog::getOpenFileName(this, tr("Open Image"), "", tr("Image Files (*.bmp *.png)"));
    if (!file_path.isEmpty()) {
        m_canvas->load_from_file(file_path.toStdString());
    }
}

void PaintApp::on_save_as_bmp() {
    QString file_path = QFileDialog::getSaveFileName(this, tr("Save as BMP"), "", tr("BMP Files (*.bmp)"));
    if (!file_path.isEmpty()) {
        if (m_canvas->save_as_bmp(file_path.toStdString())) {
            QMessageBox::information(this, tr("Success"), tr("Image saved successfully!"));
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Failed to save image."));
        }
    }
}

void PaintApp::on_save_as_png() {
    QString file_path = QFileDialog::getSaveFileName(this, tr("Save as PNG"), "", tr("PNG Files (*.png)"));
    if (!file_path.isEmpty()) {
        if (m_canvas->save_as_png(file_path.toStdString())) {
            QMessageBox::information(this, tr("Success"), tr("Image saved successfully!"));
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Failed to save image."));
        }
    }
}

void PaintApp::on_foreground_color_clicked() {
    auto current = m_canvas->get_foreground_color();
    QColor qcolor(static_cast<int>(current.r * 255), static_cast<int>(current.g * 255),
                  static_cast<int>(current.b * 255), static_cast<int>(current.a * 255));
    QColor selected = QColorDialog::getColor(qcolor, this, tr("Select Foreground Color"));
    if (selected.isValid()) {
        m_canvas->set_foreground_color(
            ig::Color::rgba(selected.red() / 255.0f, selected.green() / 255.0f,
                           selected.blue() / 255.0f, selected.alpha() / 255.0f));
        update_color_buttons();
    }
}

void PaintApp::on_background_color_clicked() {
    auto current = m_canvas->get_background_color();
    QColor qcolor(static_cast<int>(current.r * 255), static_cast<int>(current.g * 255),
                  static_cast<int>(current.b * 255), static_cast<int>(current.a * 255));
    QColor selected = QColorDialog::getColor(qcolor, this, tr("Select Background Color"));
    if (selected.isValid()) {
        m_canvas->set_background_color(
            ig::Color::rgba(selected.red() / 255.0f, selected.green() / 255.0f,
                           selected.blue() / 255.0f, selected.alpha() / 255.0f));
        update_color_buttons();
    }
}

void PaintApp::on_tool_changed(int index) {
    m_canvas->set_tool(static_cast<PaintCanvas::Tool>(index));
}

void PaintApp::on_brush_size_changed(int value) {
    m_canvas->set_brush_size(static_cast<float>(value));
    m_brush_size_slider->blockSignals(true);
    m_brush_size_slider->setValue(value);
    m_brush_size_slider->blockSignals(false);
    m_brush_size_spin->blockSignals(true);
    m_brush_size_spin->setValue(value);
    m_brush_size_spin->blockSignals(false);
}

void PaintApp::on_opacity_changed(int value) {
    m_canvas->set_opacity(value / 100.0f);
}

void PaintApp::on_undo() {
    m_canvas->undo();
}

void PaintApp::on_redo() {
    m_canvas->redo();
}

void PaintApp::on_clear_canvas() {
    auto reply = QMessageBox::question(this, tr("Clear Canvas"), tr("Are you sure you want to clear the canvas?"));
    if (reply == QMessageBox::Yes) {
        on_new_file();
    }
}

void PaintApp::on_zoom_in() {
    m_canvas->set_zoom(m_canvas->get_zoom() * 1.2f);
    m_zoom_label->setText(QString("Zoom: %1%").arg(static_cast<int>(m_canvas->get_zoom() * 100)));
}

void PaintApp::on_zoom_out() {
    m_canvas->set_zoom(m_canvas->get_zoom() / 1.2f);
    m_zoom_label->setText(QString("Zoom: %1%").arg(static_cast<int>(m_canvas->get_zoom() * 100)));
}

void PaintApp::on_zoom_reset() {
    m_canvas->set_zoom(1.0f);
    m_zoom_label->setText("Zoom: 100%");
}
