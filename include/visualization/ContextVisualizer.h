#pragma once
/*  ContextVisualizer.h  -  Integration of Context System with Drawing Engine
    
    Renders breadcrumb-style context navigation and complete context visualization
    using the custom drawing engine. Supports:
    - Breadcrumb trails with icons
    - File hierarchies and dependencies
    - Symbol references and relationships
    - Source control status indicators
    - Interactive GUI elements for context navigation
*/

#include "../context/BreadcrumbContextManager.h"
#include "../drawing/DrawingEngine.h"
#include <memory>

namespace RawrXD {
namespace Visualization {

using namespace Context;
using namespace Drawing;

// ============================================================================
// BREADCRUMB RENDERER
// ============================================================================

class BreadcrumbRenderer {
public:
    BreadcrumbRenderer();
    ~BreadcrumbRenderer();
    
    // Rendering
    void renderBreadcrumbs(DrawingContext& ctx, const BreadcrumbChain& chain, const Rect& bounds);
    void renderBreadcrumbTrail(DrawingContext& ctx, const QList<Breadcrumb>& trail, const Rect& bounds);
    
    // Styling
    void setItemHeight(float height) { m_itemHeight = height; }
    void setItemPadding(float padding) { m_itemPadding = padding; }
    void setColors(const Color& bg, const Color& text, const Color& active);
    
    // Hit testing
    int hitTest(const Point& p, const Rect& bounds) const;
    
private:
    float m_itemHeight;
    float m_itemPadding;
    Color m_backgroundColor;
    Color m_textColor;
    Color m_activeColor;
    
    void renderSeparator(DrawingContext& ctx, const Point& pos);
    void renderBreadcrumbItem(DrawingContext& ctx, const Breadcrumb& crumb, const Rect& itemBounds, bool isActive);
    QString getIconPath(ContextType type);
};

// ============================================================================
// CONTEXT PANEL RENDERER
// ============================================================================

class ContextPanelRenderer {
public:
    ContextPanelRenderer(BreadcrumbContextManager& contextManager);
    ~ContextPanelRenderer();
    
    // Rendering
    void renderContextPanel(DrawingContext& ctx, const Rect& bounds);
    void renderSymbolPanel(DrawingContext& ctx, const QString& filePath, const Rect& bounds);
    void renderFilePanel(DrawingContext& ctx, const QString& filePath, const Rect& bounds);
    void renderToolsPanel(DrawingContext& ctx, const Rect& bounds);
    void renderSourceControlPanel(DrawingContext& ctx, const Rect& bounds);
    void renderInstructionsPanel(DrawingContext& ctx, const QString& filePath, const Rect& bounds);
    void renderRelationshipsPanel(DrawingContext& ctx, const QString& entityId, const Rect& bounds);
    void renderOpenEditorsPanel(DrawingContext& ctx, const Rect& bounds);
    
private:
    BreadcrumbContextManager& m_contextManager;
    
    void renderHeader(DrawingContext& ctx, const QString& title, const Rect& bounds);
    void renderListItem(DrawingContext& ctx, const QString& text, const Rect& bounds, bool selected = false);
    void renderStatusBadge(DrawingContext& ctx, const QString& status, const Point& pos, const Color& color);
};

// ============================================================================
// CONTEXT GRAPH RENDERER
// ============================================================================

class ContextGraphRenderer {
public:
    ContextGraphRenderer(BreadcrumbContextManager& contextManager);
    ~ContextGraphRenderer();
    
    // Rendering
    void renderDependencyGraph(DrawingContext& ctx, const QString& centerEntity, const Rect& bounds);
    void renderFileRelationships(DrawingContext& ctx, const QString& filePath, const Rect& bounds);
    void renderSymbolCallGraph(DrawingContext& ctx, const QString& symbolName, const Rect& bounds);
    
private:
    BreadcrumbContextManager& m_contextManager;
    
    struct Node {
        QString id;
        QString label;
        Point position;
        Rect bounds;
    };
    
    struct Edge {
        QString sourceId;
        QString targetId;
        QString label;
    };
    
    void layoutNodes(std::vector<Node>& nodes, const Rect& bounds);
    void renderNode(DrawingContext& ctx, const Node& node);
    void renderEdge(DrawingContext& ctx, const Point& from, const Point& to, const QString& label);
};

// ============================================================================
// CONTEXT WINDOW
// ============================================================================

class ContextWindow : public Component {
public:
    ContextWindow(BreadcrumbContextManager& contextManager, const Rect& bounds);
    ~ContextWindow();
    
    // Rendering
    void render(DrawingContext& ctx) override;
    
    // Navigation
    void navigateToBreadcrumb(int index);
    void navigateToEntity(const QString& entityId);
    
    // Tab management
    void addTab(const QString& tabName);
    void selectTab(const QString& tabName);
    
    // Event handling
    void onMouseDown(const Point& pos) override;
    void onMouseUp(const Point& pos) override;
    void onMouseMove(const Point& pos) override;
    
    // Configuration
    void setShowBreadcrumbs(bool show) { m_showBreadcrumbs = show; }
    void setShowContextPanel(bool show) { m_showContextPanel = show; }
    void setShowGraphView(bool show) { m_showGraphView = show; }
    
private:
    BreadcrumbContextManager& m_contextManager;
    BreadcrumbRenderer m_breadcrumbRenderer;
    ContextPanelRenderer m_panelRenderer;
    ContextGraphRenderer m_graphRenderer;
    
    QString m_currentEntity;
    QString m_activeTab;
    QMap<QString, bool> m_tabVisibility;
    
    bool m_showBreadcrumbs;
    bool m_showContextPanel;
    bool m_showGraphView;
    
    Rect m_breadcrumbArea;
    Rect m_panelArea;
    Rect m_graphArea;
    Rect m_tabArea;
    
    void layoutPanels();
    void renderTabs(DrawingContext& ctx);
};

// ============================================================================
// SCREENSHOT ANNOTATOR
// ============================================================================

class ScreenshotAnnotator {
public:
    ScreenshotAnnotator();
    ~ScreenshotAnnotator();
    
    // Annotation
    void addAnnotation(const ScreenshotAnnotation& annotation);
    void removeAnnotation(const QString& annotationId);
    
    // Rendering
    void renderAnnotations(DrawingContext& ctx, const Surface& screenshot, const Rect& bounds);
    
private:
    QList<ScreenshotAnnotation> m_annotations;
    
    void renderAnnotation(DrawingContext& ctx, const ScreenshotAnnotation& annotation);
};

// ============================================================================
// INSTRUCTION PANEL
// ============================================================================

class InstructionPanel : public Component {
public:
    InstructionPanel(const Rect& bounds);
    ~InstructionPanel();
    
    // Rendering
    void render(DrawingContext& ctx) override;
    
    // Content
    void setInstruction(const InstructionBlock& instruction);
    void clear();
    
private:
    InstructionBlock m_instruction;
    bool m_isEmpty;
};

// ============================================================================
// FILE BROWSER
// ============================================================================

class FileBrowser : public Component {
public:
    FileBrowser(BreadcrumbContextManager& contextManager, const Rect& bounds);
    ~FileBrowser();
    
    // Rendering
    void render(DrawingContext& ctx) override;
    
    // Navigation
    void navigateToFile(const QString& filePath);
    void expandFolder(const QString& folderPath);
    void collapseFolder(const QString& folderPath);
    
    // Events
    std::function<void(const QString&)> onFileSelected;
    
private:
    BreadcrumbContextManager& m_contextManager;
    QString m_rootPath;
    QString m_selectedFile;
    QSet<QString> m_expandedFolders;
    int m_scrollOffset;
};

} // namespace Visualization
} // namespace RawrXD
