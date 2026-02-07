#pragma once
/*  DrawingEngine.h  -  Custom Drawing Engine from Scratch
    
    A complete, high-performance drawing engine for rendering:
    - Geometric primitives (lines, rectangles, circles, polygons)
    - Text rendering with font support
    - Gradient fills and patterns
    - Transformations (translate, rotate, scale)
    - Clipping regions
    - Layering and composition
    - Custom shapes and paths
    - GUI component rendering
    
    Built on top of a low-level rasterizer with optional GPU acceleration.
*/

#include <QString>
#include <QPoint>
#include <QColor>
#include <QList>
#include <QMap>
#include <QMatrix4x4>
#include <memory>
#include <cstdint>
#include <vector>

namespace RawrXD {
namespace Drawing {

// ============================================================================
// FUNDAMENTAL TYPES
// ============================================================================

struct Point {
    float x, y;
    Point(float x = 0.0f, float y = 0.0f) : x(x), y(y) {}
    Point operator+(const Point& other) const { return Point(x + other.x, y + other.y); }
    Point operator-(const Point& other) const { return Point(x - other.x, y - other.y); }
    Point operator*(float scalar) const { return Point(x * scalar, y * scalar); }
    float dot(const Point& other) const { return x * other.x + y * other.y; }
    float distance() const { return std::sqrt(x * x + y * y); }
};

struct Rect {
    float x, y, width, height;
    Rect(float x = 0, float y = 0, float w = 0, float h = 0) 
        : x(x), y(y), width(w), height(h) {}
    bool contains(const Point& p) const {
        return p.x >= x && p.x < x + width && p.y >= y && p.y < y + height;
    }
    Point center() const { return Point(x + width / 2, y + height / 2); }
    Rect offset(float dx, float dy) const { return Rect(x + dx, y + dy, width, height); }
    Rect inset(float margin) const { return Rect(x + margin, y + margin, width - 2*margin, height - 2*margin); }
};

struct Color {
    uint8_t r, g, b, a;
    Color(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t a = 255)
        : r(r), g(g), b(b), a(a) {}
    Color(const QColor& qc) : r(qc.red()), g(qc.green()), b(qc.blue()), a(qc.alpha()) {}
    
    uint32_t toRGBA() const { return (uint32_t(r) << 24) | (uint32_t(g) << 16) | (uint32_t(b) << 8) | a; }
    Color lerp(const Color& other, float t) const {
        return Color(
            uint8_t(r + (other.r - r) * t),
            uint8_t(g + (other.g - g) * t),
            uint8_t(b + (other.b - b) * t),
            uint8_t(a + (other.a - a) * t)
        );
    }
};

enum class LineCap { Butt, Round, Square };
enum class LineJoin { Miter, Round, Bevel };
enum class TextAlignment { Left, Center, Right };
enum class VerticalAlignment { Top, Middle, Bottom };
enum class FillRule { EvenOdd, NonZero };

struct StrokeStyle {
    float width;
    Color color;
    LineCap cap;
    LineJoin join;
    float miterLimit;
    std::vector<float> dashPattern;
    float dashOffset;
    
    StrokeStyle() : width(1.0f), color(0, 0, 0, 255), cap(LineCap::Butt), 
                    join(LineJoin::Miter), miterLimit(10.0f), dashOffset(0.0f) {}
};

struct FillStyle {
    enum class Type { Solid, LinearGradient, RadialGradient, Pattern };
    Type type;
    Color solidColor;
    std::vector<Color> gradientColors;
    std::vector<float> gradientStops;
    Point gradientStart, gradientEnd;
    FillRule fillRule;
    float opacity;
    
    FillStyle() : type(Type::Solid), solidColor(0, 0, 0, 255), fillRule(FillRule::EvenOdd), opacity(1.0f) {}
};

struct FontMetrics {
    float ascent;
    float descent;
    float lineHeight;
    float averageCharWidth;
};

// ============================================================================
// PATH CONSTRUCTION
// ============================================================================

class Path {
public:
    Path();
    ~Path();
    
    // Path construction
    void moveTo(const Point& p);
    void lineTo(const Point& p);
    void curveTo(const Point& control1, const Point& control2, const Point& end);
    void quadraticCurveTo(const Point& control, const Point& end);
    void arcTo(const Point& center, float radius, float startAngle, float endAngle, bool clockwise = true);
    void rectangle(const Rect& r);
    void circle(const Point& center, float radius);
    void ellipse(const Point& center, float radiusX, float radiusY);
    void polygon(const std::vector<Point>& vertices);
    void closePath();
    
    // Path queries
    const std::vector<Point>& getPoints() const { return m_points; }
    const std::vector<uint8_t>& getCommands() const { return m_commands; }
    Rect getBounds() const;
    bool isPointInside(const Point& p, FillRule rule = FillRule::EvenOdd) const;
    
    // Path operations
    Path offset(const Point& delta) const;
    Path scaled(float scaleX, float scaleY) const;
    Path rotated(float angle, const Point& center = Point(0, 0)) const;
    Path stroked(const StrokeStyle& style) const;
    
    void reset();
    
private:
    std::vector<Point> m_points;
    std::vector<uint8_t> m_commands;  // Cubic, Line, MoveTo, etc.
};

// ============================================================================
// SURFACE AND RASTERIZER
// ============================================================================

class Surface {
public:
    Surface(int width, int height);
    ~Surface();
    
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    
    // Pixel access
    Color getPixel(int x, int y) const;
    void setPixel(int x, int y, const Color& color);
    void blend(int x, int y, const Color& color);
    
    // Direct buffer access
    const uint32_t* getBuffer() const { return m_buffer; }
    uint32_t* getBufferMutable() { return m_buffer; }
    
    // Utilities
    void clear(const Color& color = Color(255, 255, 255, 255));
    void fill(const Color& color);
    
    // Composition
    void compositeFrom(const Surface& source, int offsetX, int offsetY, float opacity = 1.0f);
    
private:
    int m_width, m_height;
    uint32_t* m_buffer;
};

// ============================================================================
// DRAWING CONTEXT
// ============================================================================

class DrawingContext {
public:
    DrawingContext(int width, int height);
    ~DrawingContext();
    
    // Canvas management
    void clear(const Color& color = Color(255, 255, 255, 255));
    void resize(int width, int height);
    int getWidth() const { return m_surface->getWidth(); }
    int getHeight() const { return m_surface->getHeight(); }
    
    // Transform stack
    void save();
    void restore();
    void translate(float x, float y);
    void rotate(float angle);
    void scale(float x, float y);
    void transform(const QMatrix4x4& matrix);
    
    // Clipping
    void beginClip(const Path& path);
    void endClip();
    Rect getClipBounds() const;
    
    // Drawing primitives
    void stroke(const Path& path, const StrokeStyle& style);
    void fill(const Path& path, const FillStyle& style);
    void fillAndStroke(const Path& path, const FillStyle& fillStyle, const StrokeStyle& strokeStyle);
    
    // Shapes (convenience methods)
    void drawLine(const Point& p1, const Point& p2, const StrokeStyle& style);
    void drawRect(const Rect& r, const FillStyle& fillStyle = FillStyle(), const StrokeStyle& strokeStyle = StrokeStyle());
    void drawCircle(const Point& center, float radius, const FillStyle& fillStyle = FillStyle(), const StrokeStyle& strokeStyle = StrokeStyle());
    void drawEllipse(const Point& center, float radiusX, float radiusY, const FillStyle& fillStyle = FillStyle(), const StrokeStyle& strokeStyle = StrokeStyle());
    void drawPolygon(const std::vector<Point>& vertices, const FillStyle& fillStyle = FillStyle(), const StrokeStyle& strokeStyle = StrokeStyle());
    void drawText(const QString& text, const Point& position, const QString& fontFamily, float fontSize, 
                  const Color& color, TextAlignment align = TextAlignment::Left, VerticalAlignment vAlign = VerticalAlignment::Top);
    void drawRoundedRect(const Rect& r, float cornerRadius, const FillStyle& fillStyle = FillStyle(), const StrokeStyle& strokeStyle = StrokeStyle());
    void drawTriangle(const Point& p1, const Point& p2, const Point& p3, const FillStyle& fillStyle = FillStyle(), const StrokeStyle& strokeStyle = StrokeStyle());
    void drawArc(const Point& center, float radius, float startAngle, float endAngle, const StrokeStyle& style);
    
    // Gradients
    void createLinearGradient(const Point& start, const Point& end, const std::vector<Color>& colors, const std::vector<float>& stops, FillStyle& outStyle);
    void createRadialGradient(const Point& center, float radius, const std::vector<Color>& colors, const std::vector<float>& stops, FillStyle& outStyle);
    
    // Image operations
    void drawSurface(const Surface& surface, int x, int y, float opacity = 1.0f);
    Surface capture(const Rect& region);
    
    // Font metrics
    FontMetrics measureFont(const QString& fontFamily, float fontSize);
    Rect measureText(const QString& text, const QString& fontFamily, float fontSize);
    
    // Buffer access
    const uint32_t* getBuffer() const { return m_surface->getBuffer(); }
    uint32_t* getBufferMutable() { return m_surface->getBufferMutable(); }
    
private:
    std::unique_ptr<Surface> m_surface;
    std::vector<QMatrix4x4> m_transformStack;
    std::vector<Rect> m_clipStack;
    QMatrix4x4 m_currentTransform;
    
    // Rasterization helpers
    void rasterizeLine(const Point& p1, const Point& p2, const StrokeStyle& style);
    void rasterizeCircle(const Point& center, float radius, const FillStyle& style);
    void rasterizePolygon(const std::vector<Point>& vertices, const FillStyle& style);
    void rasterizeCurve(const std::vector<Point>& curve, const StrokeStyle& style);
    
    Point transformPoint(const Point& p) const;
    bool isPointInClipRegion(const Point& p) const;
};

// ============================================================================
// GUI COMPONENTS FRAMEWORK
// ============================================================================

class Component {
public:
    Component(const Rect& bounds);
    virtual ~Component();
    
    // Layout
    const Rect& getBounds() const { return m_bounds; }
    void setBounds(const Rect& bounds) { m_bounds = bounds; }
    void setPosition(float x, float y);
    void setSize(float width, float height);
    
    // Rendering
    virtual void render(DrawingContext& ctx);
    
    // Events
    virtual void onMouseDown(const Point& pos);
    virtual void onMouseUp(const Point& pos);
    virtual void onMouseMove(const Point& pos);
    
    // Visibility
    bool isVisible() const { return m_visible; }
    void setVisible(bool visible) { m_visible = visible; }
    
protected:
    Rect m_bounds;
    bool m_visible;
};

class Button : public Component {
public:
    Button(const Rect& bounds, const QString& label);
    void render(DrawingContext& ctx) override;
    void onMouseDown(const Point& pos) override;
    void setLabel(const QString& label) { m_label = label; }
    
    std::function<void()> onClick;
    
private:
    QString m_label;
    bool m_pressed;
    bool m_hovered;
};

class Panel : public Component {
public:
    Panel(const Rect& bounds);
    void render(DrawingContext& ctx) override;
    void addChild(std::shared_ptr<Component> child);
    void removeChild(std::shared_ptr<Component> child);
    
private:
    std::vector<std::shared_ptr<Component>> m_children;
};

class TextBox : public Component {
public:
    TextBox(const Rect& bounds);
    void render(DrawingContext& ctx) override;
    void setText(const QString& text) { m_text = text; }
    QString getText() const { return m_text; }
    
private:
    QString m_text;
    int m_cursorPosition;
};

class Label : public Component {
public:
    Label(const Rect& bounds, const QString& text);
    void render(DrawingContext& ctx) override;
    void setText(const QString& text) { m_text = text; }
    
private:
    QString m_text;
};

class Canvas : public Component {
public:
    Canvas(const Rect& bounds);
    void render(DrawingContext& ctx) override;
    DrawingContext& getDrawingContext() { return *m_drawingContext; }
    
    std::function<void(DrawingContext&)> onCustomRender;
    
private:
    std::unique_ptr<DrawingContext> m_drawingContext;
};

} // namespace Drawing
} // namespace RawrXD
