#include "paint_app.h"
<<<<<<< HEAD
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
=======
#include <cstring>

// ======================== PaintCanvas Implementation ========================

PaintCanvas::PaintCanvas(int width, int height, void* parent)
    : // Widget(parent), m_canvas(width, height), m_temp_preview(width, height),
      m_undo_stack(std::make_unique<QUndoStack>()) {
    m_canvas.clear(m_background_color);
    m_temp_preview.clear(ig::Color::transparent());
    setFocusPolicy(StrongFocus);
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
>>>>>>> origin/main
        ig::Color erase_color = ig::Color::transparent();
        ig::line_thick(m_canvas, prev_x, prev_y, x, y, m_brush_size, erase_color);
    }
}

<<<<<<< HEAD
void PaintCanvas::draw_line_preview(float x, float y)
{
    m_temp_preview = m_canvas;
    if (m_current_tool == LINE)
    {
=======
void PaintCanvas::draw_line_preview(float x, float y) {
    m_temp_preview = m_canvas;
    if (m_current_tool == LINE) {
>>>>>>> origin/main
        ig::Color line_color = m_foreground_color;
        line_color.a = m_opacity;
        ig::line_aa(m_temp_preview, m_shape_start_x, m_shape_start_y, x, y, line_color);
    }
}

<<<<<<< HEAD
void PaintCanvas::draw_shape_preview(float x, float y)
{
=======
void PaintCanvas::draw_shape_preview(float x, float y) {
>>>>>>> origin/main
    m_temp_preview = m_canvas;
    ig::Color shape_color = m_foreground_color;
    shape_color.a = m_opacity;

<<<<<<< HEAD
    switch (m_current_tool)
    {
        case RECTANGLE:
        {
=======
    switch (m_current_tool) {
        case RECTANGLE: {
>>>>>>> origin/main
            int x1 = static_cast<int>(std::min(m_shape_start_x, x));
            int y1 = static_cast<int>(std::min(m_shape_start_y, y));
            int x2 = static_cast<int>(std::max(m_shape_start_x, x));
            int y2 = static_cast<int>(std::max(m_shape_start_y, y));
            ig::fill_rect(m_temp_preview, x1, y1, x2 - x1, y2 - y1, shape_color);
            break;
        }
<<<<<<< HEAD
        case CIRCLE:
        {
=======
        case CIRCLE: {
>>>>>>> origin/main
            float r = std::sqrt((x - m_shape_start_x) * (x - m_shape_start_x) +
                                (y - m_shape_start_y) * (y - m_shape_start_y));
            ig::fill_circle(m_temp_preview, m_shape_start_x, m_shape_start_y, r, shape_color);
            break;
        }
<<<<<<< HEAD
        case ELLIPSE:
        {
=======
        case ELLIPSE: {
>>>>>>> origin/main
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

<<<<<<< HEAD
void PaintCanvas::finalize_shape(float x, float y)
{
    ig::Color shape_color = m_foreground_color;
    shape_color.a = m_opacity;

    switch (m_current_tool)
    {
        case RECTANGLE:
        {
=======
void PaintCanvas::finalize_shape(float x, float y) {
    ig::Color shape_color = m_foreground_color;
    shape_color.a = m_opacity;

    switch (m_current_tool) {
        case RECTANGLE: {
>>>>>>> origin/main
            int x1 = static_cast<int>(std::min(m_shape_start_x, x));
            int y1 = static_cast<int>(std::min(m_shape_start_y, y));
            int x2 = static_cast<int>(std::max(m_shape_start_x, x));
            int y2 = static_cast<int>(std::max(m_shape_start_y, y));
            ig::fill_rect(m_canvas, x1, y1, x2 - x1, y2 - y1, shape_color);
            break;
        }
<<<<<<< HEAD
        case CIRCLE:
        {
=======
        case CIRCLE: {
>>>>>>> origin/main
            float r = std::sqrt((x - m_shape_start_x) * (x - m_shape_start_x) +
                                (y - m_shape_start_y) * (y - m_shape_start_y));
            ig::fill_circle(m_canvas, m_shape_start_x, m_shape_start_y, r, shape_color);
            break;
        }
<<<<<<< HEAD
        case ELLIPSE:
        {
=======
        case ELLIPSE: {
>>>>>>> origin/main
            float rx = std::abs(x - m_shape_start_x);
            float ry = std::abs(y - m_shape_start_y);
            ig::fill_ellipse(m_canvas, m_shape_start_x, m_shape_start_y, rx, ry, shape_color);
            break;
        }
        case LINE:
            ig::line_aa(m_canvas, m_shape_start_x, m_shape_start_y, x, y, shape_color);
            break;
<<<<<<< HEAD
        case FILL:
        {
            int ix = static_cast<int>(x);
            int iy = static_cast<int>(y);
            if (m_canvas.in_bounds(ix, iy))
            {
=======
        case FILL: {
            int ix = static_cast<int>(x);
            int iy = static_cast<int>(y);
            if (m_canvas.in_bounds(ix, iy)) {
>>>>>>> origin/main
                ig::flood_fill(m_canvas, ix, iy, shape_color);
            }
            break;
        }
<<<<<<< HEAD
        case PICKER:
        {
            int ix = static_cast<int>(x);
            int iy = static_cast<int>(y);
            if (m_canvas.in_bounds(ix, iy))
            {
=======
        case PICKER: {
            int ix = static_cast<int>(x);
            int iy = static_cast<int>(y);
            if (m_canvas.in_bounds(ix, iy)) {
>>>>>>> origin/main
                m_foreground_color = m_canvas.get(ix, iy);
            }
            break;
        }
        default:
            break;
    }
}

<<<<<<< HEAD
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
=======
void PaintCanvas::paintEvent(void* event) {
    void painter(this);
    
    // Create void from canvas data
    void void(m_canvas.width, m_canvas.height, void::Format_RGBA8888);
    std::memcpy(void.bits(), m_canvas.data.data(), m_canvas.data.size());

    // Draw on window
    struct { int x; int y; int w; int h; } target_rect(m_pan_x, m_pan_y, 
                      static_cast<int>(m_canvas.width * m_zoom), 
                      static_cast<int>(m_canvas.height * m_zoom));
    painter.drawImage(target_rect, void);

    // Draw grid if zoomed in
    if (m_zoom > 2.0f) {
        painter.setPen(void(void(200, 200, 200, 100), 1, DashLine));
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

void PaintCanvas::mousePressEvent(void* event) {
    float canvas_x = (event->x() - m_pan_x) / m_zoom;
    float canvas_y = (event->y() - m_pan_y) / m_zoom;

>>>>>>> origin/main
    m_last_x = canvas_x;
    m_last_y = canvas_y;
    m_shape_start_x = canvas_x;
    m_shape_start_y = canvas_y;
    m_is_drawing = true;
<<<<<<< HEAD
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
=======

    if (event->button() == LeftButton) {
        if (m_current_tool == FILL || m_current_tool == PICKER) {
            finalize_shape(canvas_x, canvas_y);
            m_is_drawing = false;
        } else {
            save_state_for_undo();
        }
    } else if (event->button() == RightButton) {
        // Pan mode
        setCursor(ClosedHandCursor);
    }

    update();
}

void PaintCanvas::mouseMoveEvent(void* event) {
    float canvas_x = (event->x() - m_pan_x) / m_zoom;
    float canvas_y = (event->y() - m_pan_y) / m_zoom;

    if (event->buttons() & LeftButton && m_is_drawing) {
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
    } else if (event->buttons() & RightButton) {
        // Pan
        pan(event->x() - m_last_x, event->y() - m_last_y);
        m_last_x = event->x();
        m_last_y = event->y();
    }
}

void PaintCanvas::mouseReleaseEvent(void* event) {
    if (event->button() == LeftButton && m_is_drawing) {
        float canvas_x = (event->x() - m_pan_x) / m_zoom;
        float canvas_y = (event->y() - m_pan_y) / m_zoom;

        if (m_current_tool == LINE || m_current_tool == RECTANGLE || 
            m_current_tool == CIRCLE || m_current_tool == ELLIPSE) {
            finalize_shape(canvas_x, canvas_y);
        }
        m_is_drawing = false;
        update();
    } else if (event->button() == RightButton) {
        setCursor(ArrowCursor);
    }
}

void PaintCanvas::wheelEvent(void* event) {
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
    // Implement image loading
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Image"), "", tr("Images (*.png *.xpm *.jpg)"));
    if (!fileName.isEmpty()) {
        QImage loadedImage;
        if (loadedImage.load(fileName)) {
            image = loadedImage;
            update();
        }
    }
    return false;
}

void PaintCanvas::undo() {
    if (!m_history.empty()) {
        m_canvas = m_history.back();
        m_history.pop_back();
        update();
    }
}

void PaintCanvas::redo() {
    // Implement redo functionality
    if (undoStack->canRedo()) {
        undoStack->redo();
        image = undoStack->currentImage();
        update();
>>>>>>> origin/main
    }
}

// ======================== PaintApp Implementation ========================

PaintApp::PaintApp(void* parent)
<<<<<<< HEAD
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
=======
    : void(parent) {
    setWindowTitle("RawrXD Paint - AI Enhanced Canvas");
    resize(1200, 800);

    // Create central widget
    auto central_widget = new // Widget(this);
    setCentralWidget(central_widget);

    // Create main layout
    auto main_layout = new void(central_widget);

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

    auto new_action = file_menu->addAction(tr("&New"));  // Signal connection removed\nauto open_action = file_menu->addAction(tr("&Open"));  // Signal connection removed\nfile_menu->addSeparator();

    auto save_bmp_action = file_menu->addAction(tr("Save as &BMP"));  // Signal connection removed\nauto save_png_action = file_menu->addAction(tr("Save as &PNG"));  // Signal connection removed\nfile_menu->addSeparator();

    auto exit_action = file_menu->addAction(tr("E&xit"));  // Signal connection removed\nauto edit_menu = menuBar()->addMenu(tr("&Edit"));

    auto undo_action = edit_menu->addAction(tr("&Undo"));  // Signal connection removed\nauto redo_action = edit_menu->addAction(tr("&Redo"));  // Signal connection removed\nedit_menu->addSeparator();

    auto clear_action = edit_menu->addAction(tr("&Clear Canvas"));  // Signal connection removed\nauto view_menu = menuBar()->addMenu(tr("&View"));

    auto zoom_in_action = view_menu->addAction(tr("Zoom &In"));  // Signal connection removed\nauto zoom_out_action = view_menu->addAction(tr("Zoom &Out"));  // Signal connection removed\nauto zoom_reset_action = view_menu->addAction(tr("&Reset Zoom"));  // Signal connection removed\n}

void* PaintApp::create_tool_panel() {
    auto panel_layout = new void();

    // Tool selection
    panel_layout->addWidget(new void("Tools:"));
    m_tool_combo = new void();
    m_tool_combo->addItem("Pencil");
    m_tool_combo->addItem("Brush");
    m_tool_combo->addItem("Eraser");
    m_tool_combo->addItem("Line");
    m_tool_combo->addItem("Rectangle");
    m_tool_combo->addItem("Circle");
    m_tool_combo->addItem("Ellipse");
    m_tool_combo->addItem("Fill");
    m_tool_combo->addItem("Color Picker");
    m_tool_combo->addItem("Text");  // Signal connection removed\npanel_layout->addWidget(m_tool_combo);

    panel_layout->addSpacing(20);

    // Color selection
    panel_layout->addWidget(new void("Colors:"));
    m_foreground_button = new void();
    m_foreground_button->setFixedSize(60, 60);
    m_foreground_button->setStyleSheet("background-color: black;");  // Signal connection removed\npanel_layout->addWidget(new void("Foreground:"));
    panel_layout->addWidget(m_foreground_button);

    m_background_button = new void();
    m_background_button->setFixedSize(60, 60);
    m_background_button->setStyleSheet("background-color: white;");  // Signal connection removed\npanel_layout->addWidget(new void("Background:"));
    panel_layout->addWidget(m_background_button);

    panel_layout->addSpacing(20);

    // Brush settings
    panel_layout->addWidget(new void("Brush Size:"));
    m_brush_size_spin = new void();
    m_brush_size_spin->setRange(1, 100);
    m_brush_size_spin->setValue(3);  // Signal connection removed\npanel_layout->addWidget(m_brush_size_spin);

    m_brush_size_slider = new void(Horizontal);
    m_brush_size_slider->setRange(1, 100);
    m_brush_size_slider->setValue(3);  // Signal connection removed\npanel_layout->addWidget(m_brush_size_slider);

    panel_layout->addSpacing(20);

    // Opacity
    panel_layout->addWidget(new void("Opacity:"));
    m_opacity_slider = new void(Horizontal);
    m_opacity_slider->setRange(0, 100);
    m_opacity_slider->setValue(100);  // Signal connection removed\npanel_layout->addWidget(m_opacity_slider);

    panel_layout->addSpacing(20);

    // Zoom info
    m_zoom_label = new void("Zoom: 100%");
    panel_layout->addWidget(m_zoom_label);

    panel_layout->addStretch();

    // Create tool panel widget and add to main layout
    auto tool_panel_widget = new // Widget();
    tool_panel_widget->setLayout(panel_layout);
    tool_panel_widget->setMaximumWidth(250);
    
// REMOVED_QT:     auto main_layout = qobject_cast<void*>(centralWidget()->layout());
    if (main_layout) {
        main_layout->addWidget(tool_panel_widget, 0);
    }

    return panel_layout;
}

void PaintApp::update_color_buttons() {
    void fg(static_cast<int>(m_canvas->get_foreground_color().r * 255),
              static_cast<int>(m_canvas->get_foreground_color().g * 255),
              static_cast<int>(m_canvas->get_foreground_color().b * 255));
    m_foreground_button->setStyleSheet(
        std::string("background-color: rgb(%1, %2, %3);")))));

    void bg(static_cast<int>(m_canvas->get_background_color().r * 255),
              static_cast<int>(m_canvas->get_background_color().g * 255),
              static_cast<int>(m_canvas->get_background_color().b * 255));
    m_background_button->setStyleSheet(
        std::string("background-color: rgb(%1, %2, %3);")))));
}

void PaintApp::on_new_file() {
    m_canvas->clear_canvas(ig::Color::white());
}

void PaintApp::on_open_file() {
    std::string file_path = // Dialog::getOpenFileName(this, tr("Open Image"), "", tr("Image Files (*.bmp *.png)"));
    if (!file_path.empty()) {
        m_canvas->load_from_file(file_path);
    }
}

void PaintApp::on_save_as_bmp() {
    std::string file_path = // Dialog::getSaveFileName(this, tr("Save as BMP"), "", tr("BMP Files (*.bmp)"));
    if (!file_path.empty()) {
        if (m_canvas->save_as_bmp(file_path)) {
            void::information(this, tr("Success"), tr("Image saved successfully!"));
        } else {
            void::warning(this, tr("Error"), tr("Failed to save image."));
        }
    }
}

void PaintApp::on_save_as_png() {
    std::string file_path = // Dialog::getSaveFileName(this, tr("Save as PNG"), "", tr("PNG Files (*.png)"));
    if (!file_path.empty()) {
        if (m_canvas->save_as_png(file_path)) {
            void::information(this, tr("Success"), tr("Image saved successfully!"));
        } else {
            void::warning(this, tr("Error"), tr("Failed to save image."));
        }
    }
}

void PaintApp::on_foreground_color_clicked() {
    auto current = m_canvas->get_foreground_color();
    void void(static_cast<int>(current.r * 255), static_cast<int>(current.g * 255),
                  static_cast<int>(current.b * 255), static_cast<int>(current.a * 255));
    void selected = void::getColor(void, this, tr("Select Foreground Color"));
    if (selected.isValid()) {
        m_canvas->set_foreground_color(
            ig::Color::rgba(selected.red() / 255.0f, selected.green() / 255.0f,
                           selected.blue() / 255.0f, selected.alpha() / 255.0f));
        update_color_buttons();
    }
}

void PaintApp::on_background_color_clicked() {
    auto current = m_canvas->get_background_color();
    void void(static_cast<int>(current.r * 255), static_cast<int>(current.g * 255),
                  static_cast<int>(current.b * 255), static_cast<int>(current.a * 255));
    void selected = void::getColor(void, this, tr("Select Background Color"));
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
    auto reply = void::question(this, tr("Clear Canvas"), tr("Are you sure you want to clear the canvas?"));
    if (reply == void::Yes) {
        on_new_file();
    }
}

void PaintApp::on_zoom_in() {
    m_canvas->set_zoom(m_canvas->get_zoom() * 1.2f);
    m_zoom_label->setText(std::string("Zoom: %1%") * 100)));
}

void PaintApp::on_zoom_out() {
    m_canvas->set_zoom(m_canvas->get_zoom() / 1.2f);
    m_zoom_label->setText(std::string("Zoom: %1%") * 100)));
}

void PaintApp::on_zoom_reset() {
    m_canvas->set_zoom(1.0f);
    m_zoom_label->setText("Zoom: 100%");
}

>>>>>>> origin/main
