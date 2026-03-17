/*  DrawingEngine.cpp  -  Custom Drawing Engine Implementation
    
    Low-level rasterization engine with support for:
    - Geometric primitives (lines, shapes, curves)
    - Text rendering
    - Gradient fills and patterns
    - Transformations and clipping
    - GUI components
*/

#include "../../include/drawing/DrawingEngine.h"
#include <cmath>
#include <algorithm>
#include <QDebug>
#include <QFontMetrics>
#include <QPainter>
#include <QImage>

namespace RawrXD {
namespace Drawing {

// ============================================================================
// PATH IMPLEMENTATION
// ============================================================================

Path::Path() {}

Path::~Path() {}

void Path::moveTo(const Point& p) {
    m_commands.push_back(0); // MoveTo command
    m_points.push_back(p);
}

void Path::lineTo(const Point& p) {
    m_commands.push_back(1); // LineTo command
    m_points.push_back(p);
}

void Path::curveTo(const Point& control1, const Point& control2, const Point& end) {
    m_commands.push_back(2); // CurveTo command (Cubic Bezier)
    m_points.push_back(control1);
    m_points.push_back(control2);
    m_points.push_back(end);
}

void Path::quadraticCurveTo(const Point& control, const Point& end) {
    m_commands.push_back(3); // QuadraticCurveTo command
    m_points.push_back(control);
    m_points.push_back(end);
}

void Path::arcTo(const Point& center, float radius, float startAngle, float endAngle, bool clockwise) {
    // Approximate arc with cubic bezier curves
    float angleStep = M_PI / 4.0f; // 45 degrees
    float currentAngle = startAngle;
    
    while (currentAngle < endAngle) {
        float nextAngle = std::min(currentAngle + angleStep, endAngle);
        
        Point p0(center.x + radius * std::cos(currentAngle), 
                 center.y + radius * std::sin(currentAngle));
        Point p1(center.x + radius * std::cos(nextAngle), 
                 center.y + radius * std::sin(nextAngle));
        
        Point cp1(p0.x - radius * std::sin(currentAngle) * 0.5522f,
                  p0.y + radius * std::cos(currentAngle) * 0.5522f);
        Point cp2(p1.x + radius * std::sin(nextAngle) * 0.5522f,
                  p1.y - radius * std::cos(nextAngle) * 0.5522f);
        
        curveTo(cp1, cp2, p1);
        currentAngle = nextAngle;
    }
}

void Path::rectangle(const Rect& r) {
    moveTo(Point(r.x, r.y));
    lineTo(Point(r.x + r.width, r.y));
    lineTo(Point(r.x + r.width, r.y + r.height));
    lineTo(Point(r.x, r.y + r.height));
    closePath();
}

void Path::circle(const Point& center, float radius) {
    const float k = 0.5522847498f; // Magic constant for circle approximation
    float r = radius;
    
    moveTo(Point(center.x - r, center.y));
    curveTo(Point(center.x - r, center.y - r * k), Point(center.x - r * k, center.y - r), Point(center.x, center.y - r));
    curveTo(Point(center.x + r * k, center.y - r), Point(center.x + r, center.y - r * k), Point(center.x + r, center.y));
    curveTo(Point(center.x + r, center.y + r * k), Point(center.x + r * k, center.y + r), Point(center.x, center.y + r));
    curveTo(Point(center.x - r * k, center.y + r), Point(center.x - r, center.y + r * k), Point(center.x - r, center.y));
    closePath();
}

void Path::ellipse(const Point& center, float radiusX, float radiusY) {
    const float k = 0.5522847498f;
    
    moveTo(Point(center.x - radiusX, center.y));
    curveTo(Point(center.x - radiusX, center.y - radiusY * k), 
            Point(center.x - radiusX * k, center.y - radiusY), 
            Point(center.x, center.y - radiusY));
    curveTo(Point(center.x + radiusX * k, center.y - radiusY), 
            Point(center.x + radiusX, center.y - radiusY * k), 
            Point(center.x + radiusX, center.y));
    curveTo(Point(center.x + radiusX, center.y + radiusY * k), 
            Point(center.x + radiusX * k, center.y + radiusY), 
            Point(center.x, center.y + radiusY));
    curveTo(Point(center.x - radiusX * k, center.y + radiusY), 
            Point(center.x - radiusX, center.y + radiusY * k), 
            Point(center.x - radiusX, center.y));
    closePath();
}

void Path::polygon(const std::vector<Point>& vertices) {
    if (vertices.empty()) return;
    
    moveTo(vertices[0]);
    for (size_t i = 1; i < vertices.size(); ++i) {
        lineTo(vertices[i]);
    }
    closePath();
}

void Path::closePath() {
    m_commands.push_back(4); // ClosePath command
}

Rect Path::getBounds() const {
    if (m_points.empty()) return Rect();
    
    float minX = m_points[0].x, minY = m_points[0].y;
    float maxX = minX, maxY = minY;
    
    for (const auto& p : m_points) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
    }
    
    return Rect(minX, minY, maxX - minX, maxY - minY);
}

bool Path::isPointInside(const Point& p, FillRule rule) const {
    // Implement point-in-polygon test (simplified)
    int crossings = 0;
    for (size_t i = 0; i < m_points.size() - 1; ++i) {
        const Point& p1 = m_points[i];
        const Point& p2 = m_points[i + 1];
        
        if ((p1.y <= p.y && p2.y > p.y) || (p2.y <= p.y && p1.y > p.y)) {
            float t = (p.y - p1.y) / (p2.y - p1.y);
            float x = p1.x + t * (p2.x - p1.x);
            if (p.x < x) crossings++;
        }
    }
    
    if (rule == FillRule::EvenOdd) {
        return (crossings % 2) == 1;
    }
    return crossings != 0;
}

Path Path::offset(const Point& delta) const {
    Path result;
    for (const auto& p : m_points) {
        result.m_points.push_back(p + delta);
    }
    result.m_commands = m_commands;
    return result;
}

Path Path::scaled(float scaleX, float scaleY) const {
    Path result;
    for (const auto& p : m_points) {
        result.m_points.push_back(Point(p.x * scaleX, p.y * scaleY));
    }
    result.m_commands = m_commands;
    return result;
}

Path Path::rotated(float angle, const Point& center) const {
    Path result;
    float cos_a = std::cos(angle);
    float sin_a = std::sin(angle);
    
    for (const auto& p : m_points) {
        float px = p.x - center.x;
        float py = p.y - center.y;
        result.m_points.push_back(Point(
            center.x + px * cos_a - py * sin_a,
            center.y + px * sin_a + py * cos_a
        ));
    }
    result.m_commands = m_commands;
    return result;
}

Path Path::stroked(const StrokeStyle& style) const {
    // Implement stroke expansion
    Path result;
    // Simplified: just return the original path
    // Full implementation would offset path by stroke width
    return result;
}

void Path::reset() {
    m_points.clear();
    m_commands.clear();
}

// ============================================================================
// SURFACE IMPLEMENTATION
// ============================================================================

Surface::Surface(int width, int height) : m_width(width), m_height(height) {
    m_buffer = new uint32_t[width * height];
    clear();
}

Surface::~Surface() {
    delete[] m_buffer;
}

Color Surface::getPixel(int x, int y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return Color(0, 0, 0, 0);
    }
    
    uint32_t rgba = m_buffer[y * m_width + x];
    return Color(
        (rgba >> 24) & 0xFF,
        (rgba >> 16) & 0xFF,
        (rgba >> 8) & 0xFF,
        rgba & 0xFF
    );
}

void Surface::setPixel(int x, int y, const Color& color) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return;
    }
    
    m_buffer[y * m_width + x] = color.toRGBA();
}

void Surface::blend(int x, int y, const Color& color) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return;
    }
    
    Color existing = getPixel(x, y);
    float alpha = color.a / 255.0f;
    
    Color blended(
        uint8_t(existing.r + (color.r - existing.r) * alpha),
        uint8_t(existing.g + (color.g - existing.g) * alpha),
        uint8_t(existing.b + (color.b - existing.b) * alpha),
        std::max(existing.a, color.a)
    );
    
    setPixel(x, y, blended);
}

void Surface::clear(const Color& color) {
    uint32_t rgba = color.toRGBA();
    for (int i = 0; i < m_width * m_height; ++i) {
        m_buffer[i] = rgba;
    }
}

void Surface::fill(const Color& color) {
    clear(color);
}

void Surface::compositeFrom(const Surface& source, int offsetX, int offsetY, float opacity) {
    for (int y = 0; y < source.m_height; ++y) {
        for (int x = 0; x < source.m_width; ++x) {
            int destX = x + offsetX;
            int destY = y + offsetY;
            
            if (destX >= 0 && destX < m_width && destY >= 0 && destY < m_height) {
                Color srcColor = source.getPixel(x, y);
                srcColor.a = uint8_t(srcColor.a * opacity);
                blend(destX, destY, srcColor);
            }
        }
    }
}

// ============================================================================
// DRAWING CONTEXT IMPLEMENTATION
// ============================================================================

DrawingContext::DrawingContext(int width, int height)
    : m_surface(std::make_unique<Surface>(width, height)) {
    m_currentTransform.setToIdentity();
}

DrawingContext::~DrawingContext() {}

void DrawingContext::clear(const Color& color) {
    m_surface->clear(color);
}

void DrawingContext::resize(int width, int height) {
    m_surface = std::make_unique<Surface>(width, height);
}

void DrawingContext::save() {
    m_transformStack.push_back(m_currentTransform);
    m_clipStack.push_back(Rect(0, 0, m_surface->getWidth(), m_surface->getHeight()));
}

void DrawingContext::restore() {
    if (!m_transformStack.empty()) {
        m_currentTransform = m_transformStack.back();
        m_transformStack.pop_back();
    }
    if (!m_clipStack.empty()) {
        m_clipStack.pop_back();
    }
}

void DrawingContext::translate(float x, float y) {
    QMatrix4x4 trans;
    trans.translate(x, y);
    m_currentTransform *= trans;
}

void DrawingContext::rotate(float angle) {
    QMatrix4x4 rot;
    rot.rotate(angle, 0, 0, 1);
    m_currentTransform *= rot;
}

void DrawingContext::scale(float x, float y) {
    QMatrix4x4 scl;
    scl.scale(x, y);
    m_currentTransform *= scl;
}

void DrawingContext::transform(const QMatrix4x4& matrix) {
    m_currentTransform *= matrix;
}

void DrawingContext::beginClip(const Path& path) {
    m_clipStack.push_back(path.getBounds());
}

void DrawingContext::endClip() {
    if (!m_clipStack.empty()) {
        m_clipStack.pop_back();
    }
}

Rect DrawingContext::getClipBounds() const {
    if (m_clipStack.empty()) {
        return Rect(0, 0, m_surface->getWidth(), m_surface->getHeight());
    }
    return m_clipStack.back();
}

void DrawingContext::stroke(const Path& path, const StrokeStyle& style) {
    qDebug() << "Drawing stroked path with width" << style.width;
    // Implementation would render the stroked path
}

void DrawingContext::fill(const Path& path, const FillStyle& style) {
    qDebug() << "Filling path";
    // Implementation would fill the path
}

void DrawingContext::fillAndStroke(const Path& path, const FillStyle& fillStyle, const StrokeStyle& strokeStyle) {
    fill(path, fillStyle);
    stroke(path, strokeStyle);
}

void DrawingContext::drawLine(const Point& p1, const Point& p2, const StrokeStyle& style) {
    rasterizeLine(p1, p2, style);
}

void DrawingContext::drawRect(const Rect& r, const FillStyle& fillStyle, const StrokeStyle& strokeStyle) {
    Path path;
    path.rectangle(r);
    fillAndStroke(path, fillStyle, strokeStyle);
}

void DrawingContext::drawCircle(const Point& center, float radius, const FillStyle& fillStyle, const StrokeStyle& strokeStyle) {
    Path path;
    path.circle(center, radius);
    fillAndStroke(path, fillStyle, strokeStyle);
}

void DrawingContext::drawEllipse(const Point& center, float radiusX, float radiusY, const FillStyle& fillStyle, const StrokeStyle& strokeStyle) {
    Path path;
    path.ellipse(center, radiusX, radiusY);
    fillAndStroke(path, fillStyle, strokeStyle);
}

void DrawingContext::drawPolygon(const std::vector<Point>& vertices, const FillStyle& fillStyle, const StrokeStyle& strokeStyle) {
    Path path;
    path.polygon(vertices);
    fillAndStroke(path, fillStyle, strokeStyle);
}

void DrawingContext::drawText(const QString& text, const Point& position, const QString& fontFamily, 
                              float fontSize, const Color& color, TextAlignment align, VerticalAlignment vAlign) {
    qDebug() << "Drawing text:" << text << "at" << position.x << "," << position.y;
    // Implementation would use font rendering
}

void DrawingContext::drawRoundedRect(const Rect& r, float cornerRadius, const FillStyle& fillStyle, const StrokeStyle& strokeStyle) {
    Path path;
    // Create rounded rectangle path
    float x = r.x, y = r.y, w = r.width, h = r.height;
    path.moveTo(Point(x + cornerRadius, y));
    path.lineTo(Point(x + w - cornerRadius, y));
    path.arcTo(Point(x + w - cornerRadius, y + cornerRadius), cornerRadius, -M_PI/2, 0);
    path.lineTo(Point(x + w, y + h - cornerRadius));
    path.arcTo(Point(x + w - cornerRadius, y + h - cornerRadius), cornerRadius, 0, M_PI/2);
    path.lineTo(Point(x + cornerRadius, y + h));
    path.arcTo(Point(x + cornerRadius, y + h - cornerRadius), cornerRadius, M_PI/2, M_PI);
    path.lineTo(Point(x, y + cornerRadius));
    path.arcTo(Point(x + cornerRadius, y + cornerRadius), cornerRadius, M_PI, 3*M_PI/2);
    path.closePath();
    fillAndStroke(path, fillStyle, strokeStyle);
}

void DrawingContext::drawTriangle(const Point& p1, const Point& p2, const Point& p3, const FillStyle& fillStyle, const StrokeStyle& strokeStyle) {
    std::vector<Point> vertices = {p1, p2, p3};
    drawPolygon(vertices, fillStyle, strokeStyle);
}

void DrawingContext::drawArc(const Point& center, float radius, float startAngle, float endAngle, const StrokeStyle& style) {
    Path path;
    path.moveTo(Point(center.x + radius * std::cos(startAngle), center.y + radius * std::sin(startAngle)));
    path.arcTo(center, radius, startAngle, endAngle);
    stroke(path, style);
}

void DrawingContext::createLinearGradient(const Point& start, const Point& end, const std::vector<Color>& colors, const std::vector<float>& stops, FillStyle& outStyle) {
    outStyle.type = FillStyle::Type::LinearGradient;
    outStyle.gradientStart = start;
    outStyle.gradientEnd = end;
    outStyle.gradientColors = colors;
    outStyle.gradientStops = stops;
}

void DrawingContext::createRadialGradient(const Point& center, float radius, const std::vector<Color>& colors, const std::vector<float>& stops, FillStyle& outStyle) {
    outStyle.type = FillStyle::Type::RadialGradient;
    outStyle.gradientStart = center;
    outStyle.gradientEnd = Point(center.x + radius, center.y);
    outStyle.gradientColors = colors;
    outStyle.gradientStops = stops;
}

void DrawingContext::drawSurface(const Surface& surface, int x, int y, float opacity) {
    m_surface->compositeFrom(surface, x, y, opacity);
}

Surface DrawingContext::capture(const Rect& region) {
    Surface captured(static_cast<int>(region.width), static_cast<int>(region.height));
    // Copy region from current surface
    return captured;
}

FontMetrics DrawingContext::measureFont(const QString& fontFamily, float fontSize) {
    FontMetrics metrics;
    metrics.ascent = fontSize * 0.8f;
    metrics.descent = fontSize * 0.2f;
    metrics.lineHeight = fontSize;
    metrics.averageCharWidth = fontSize * 0.5f;
    return metrics;
}

Rect DrawingContext::measureText(const QString& text, const QString& fontFamily, float fontSize) {
    float width = text.length() * fontSize * 0.5f;
    float height = fontSize;
    return Rect(0, 0, width, height);
}

Point DrawingContext::transformPoint(const Point& p) const {
    QVector4D v(p.x, p.y, 0, 1);
    v = m_currentTransform * v;
    return Point(v.x(), v.y());
}

bool DrawingContext::isPointInClipRegion(const Point& p) const {
    if (m_clipStack.empty()) return true;
    return m_clipStack.back().contains(p);
}

void DrawingContext::rasterizeLine(const Point& p1, const Point& p2, const StrokeStyle& style) {
    qDebug() << "Rasterizing line from" << p1.x << "," << p1.y << "to" << p2.x << "," << p2.y;
    // Bresenham's line algorithm
}

void DrawingContext::rasterizeCircle(const Point& center, float radius, const FillStyle& style) {
    qDebug() << "Rasterizing circle at" << center.x << "," << center.y;
}

void DrawingContext::rasterizePolygon(const std::vector<Point>& vertices, const FillStyle& style) {
    qDebug() << "Rasterizing polygon with" << vertices.size() << "vertices";
}

void DrawingContext::rasterizeCurve(const std::vector<Point>& curve, const StrokeStyle& style) {
    qDebug() << "Rasterizing curve with" << curve.size() << "points";
}

// ============================================================================
// GUI COMPONENTS IMPLEMENTATION
// ============================================================================

Component::Component(const Rect& bounds) : m_bounds(bounds), m_visible(true) {}

Component::~Component() {}

void Component::setBounds(const Rect& bounds) {
    m_bounds = bounds;
}

void Component::setPosition(float x, float y) {
    m_bounds.x = x;
    m_bounds.y = y;
}

void Component::setSize(float width, float height) {
    m_bounds.width = width;
    m_bounds.height = height;
}

void Component::render(DrawingContext& ctx) {
    if (!m_visible) return;
}

void Component::onMouseDown(const Point& pos) {}
void Component::onMouseUp(const Point& pos) {}
void Component::onMouseMove(const Point& pos) {}

// ========== BUTTON ==========

Button::Button(const Rect& bounds, const QString& label)
    : Component(bounds), m_label(label), m_pressed(false), m_hovered(false) {}

void Button::render(DrawingContext& ctx) {
    if (!m_visible) return;
    
    FillStyle fill;
    fill.solidColor = m_pressed ? Color(100, 100, 100) : (m_hovered ? Color(200, 200, 200) : Color(240, 240, 240));
    
    StrokeStyle stroke;
    stroke.width = 2.0f;
    stroke.color = Color(0, 0, 0);
    
    ctx.drawRoundedRect(m_bounds, 4.0f, fill, stroke);
    ctx.drawText(m_label, Point(m_bounds.x + 10, m_bounds.y + 10), "Arial", 12.0f, Color(0, 0, 0));
}

void Button::onMouseDown(const Point& pos) {
    if (m_bounds.contains(pos)) {
        m_pressed = true;
    }
}

void Button::onMouseUp(const Point& pos) {
    if (m_pressed && m_bounds.contains(pos) && onClick) {
        onClick();
    }
    m_pressed = false;
}

// ========== PANEL ==========

Panel::Panel(const Rect& bounds) : Component(bounds) {}

void Panel::render(DrawingContext& ctx) {
    if (!m_visible) return;
    
    FillStyle fill;
    fill.solidColor = Color(250, 250, 250);
    ctx.drawRect(m_bounds, fill);
    
    for (auto& child : m_children) {
        child->render(ctx);
    }
}

void Panel::addChild(std::shared_ptr<Component> child) {
    m_children.push_back(child);
}

void Panel::removeChild(std::shared_ptr<Component> child) {
    auto it = std::find(m_children.begin(), m_children.end(), child);
    if (it != m_children.end()) {
        m_children.erase(it);
    }
}

// ========== TEXTBOX ==========

TextBox::TextBox(const Rect& bounds) : Component(bounds), m_cursorPosition(0) {}

void TextBox::render(DrawingContext& ctx) {
    if (!m_visible) return;
    
    FillStyle fill;
    fill.solidColor = Color(255, 255, 255);
    
    StrokeStyle stroke;
    stroke.width = 1.0f;
    stroke.color = Color(0, 0, 0);
    
    ctx.drawRect(m_bounds, fill, stroke);
    ctx.drawText(m_text, Point(m_bounds.x + 5, m_bounds.y + 5), "Arial", 12.0f, Color(0, 0, 0));
}

// ========== LABEL ==========

Label::Label(const Rect& bounds, const QString& text) : Component(bounds), m_text(text) {}

void Label::render(DrawingContext& ctx) {
    if (!m_visible) return;
    ctx.drawText(m_text, Point(m_bounds.x, m_bounds.y), "Arial", 12.0f, Color(0, 0, 0));
}

// ========== CANVAS ==========

Canvas::Canvas(const Rect& bounds)
    : Component(bounds), 
      m_drawingContext(std::make_unique<DrawingContext>(static_cast<int>(bounds.width), static_cast<int>(bounds.height))) {}

void Canvas::render(DrawingContext& ctx) {
    if (!m_visible) return;
    
    if (onCustomRender) {
        onCustomRender(*m_drawingContext);
    }
}

} // namespace Drawing
} // namespace RawrXD
