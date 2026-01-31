/*  ContextVisualizer.cpp  -  Implementation
    
    Renders breadcrumb context and integrates with the drawing engine
    for comprehensive context visualization.
*/

#include "../../include/visualization/ContextVisualizer.h"
#include <cmath>

namespace RawrXD {
namespace Visualization {

// ============================================================================
// BREADCRUMB RENDERER IMPLEMENTATION
// ============================================================================

BreadcrumbRenderer::BreadcrumbRenderer()
    : m_itemHeight(32.0f), m_itemPadding(8.0f),
      m_backgroundColor(240, 240, 240, 255),
      m_textColor(0, 0, 0, 255),
      m_activeColor(52, 152, 219, 255) {}

BreadcrumbRenderer::~BreadcrumbRenderer() {}

void BreadcrumbRenderer::renderBreadcrumbs(DrawingContext& ctx, const BreadcrumbChain& chain, const Rect& bounds) {
    auto trail = chain.getChain();
    renderBreadcrumbTrail(ctx, trail, bounds);
}

void BreadcrumbRenderer::renderBreadcrumbTrail(DrawingContext& ctx, const std::vector<Breadcrumb>& trail, const Rect& bounds) {
    if (trail.empty()) return;
    
    // Render background
    FillStyle bgFill;
    bgFill.solidColor = m_backgroundColor;
    ctx.drawRect(bounds, bgFill);
    
    // Render border
    StrokeStyle border;
    border.width = 1.0f;
    border.color = Color(200, 200, 200, 255);
    ctx.drawRect(bounds, FillStyle(), border);
    
    float currentX = bounds.x + m_itemPadding;
    float itemWidth = 120.0f;
    
    for (int i = 0; i < trail.size(); ++i) {
        Rect itemBounds(currentX, bounds.y, itemWidth, bounds.height);
        bool isActive = (i == trail.size() - 1);
        
        renderBreadcrumbItem(ctx, trail[i], itemBounds, isActive);
        
        currentX += itemWidth + 4.0f;
        
        // Render separator if not last item
        if (i < trail.size() - 1) {
            renderSeparator(ctx, Point(currentX, bounds.y + bounds.height / 2));
            currentX += 12.0f;
        }
    }
}

void BreadcrumbRenderer::renderSeparator(DrawingContext& ctx, const Point& pos) {
    StrokeStyle style;
    style.width = 1.0f;
    style.color = Color(180, 180, 180, 255);
    ctx.drawLine(pos, Point(pos.x + 6.0f, pos.y), style);
}

void BreadcrumbRenderer::renderBreadcrumbItem(DrawingContext& ctx, const Breadcrumb& crumb, const Rect& itemBounds, bool isActive) {
    // Background
    FillStyle bgFill;
    bgFill.solidColor = isActive ? m_activeColor : Color(220, 220, 220, 255);
    ctx.drawRoundedRect(itemBounds.inset(2.0f), 3.0f, bgFill);
    
    // Border
    StrokeStyle border;
    border.width = 1.0f;
    border.color = isActive ? m_activeColor : Color(150, 150, 150, 255);
    ctx.drawRoundedRect(itemBounds.inset(2.0f), 3.0f, FillStyle(), border);
    
    // Text
    Color textColor = isActive ? Color(255, 255, 255, 255) : m_textColor;
    ctx.drawText(
        crumb.label,
        Point(itemBounds.x + m_itemPadding, itemBounds.y + itemBounds.height / 2 - 6.0f),
        "Arial",
        12.0f,
        textColor
    );
}

int BreadcrumbRenderer::hitTest(const Point& p, const Rect& bounds) const {
    if (!bounds.contains(p)) return -1;
    
    float currentX = bounds.x + m_itemPadding;
    float itemWidth = 120.0f;
    int itemIndex = 0;
    
    while (currentX + itemWidth < p.x && currentX < bounds.x + bounds.width) {
        currentX += itemWidth + 4.0f + 12.0f;
        itemIndex++;
    }
    
    return itemIndex;
}

std::string BreadcrumbRenderer::getIconPath(ContextType type) {
    switch (type) {
        case ContextType::Tool:              return ":/icons/tool.png";
        case ContextType::Symbol:            return ":/icons/symbol.png";
        case ContextType::File:              return ":/icons/file.png";
        case ContextType::SourceControl:     return ":/icons/git.png";
        case ContextType::Screenshot:        return ":/icons/screenshot.png";
        case ContextType::Instruction:       return ":/icons/instruction.png";
        case ContextType::Relationship:      return ":/icons/link.png";
        case ContextType::OpenEditor:        return ":/icons/editor.png";
        default:                             return ":/icons/unknown.png";
    }
}

// ============================================================================
// CONTEXT PANEL RENDERER IMPLEMENTATION
// ============================================================================

ContextPanelRenderer::ContextPanelRenderer(BreadcrumbContextManager& contextManager)
    : m_contextManager(contextManager) {}

ContextPanelRenderer::~ContextPanelRenderer() {}

void ContextPanelRenderer::renderContextPanel(DrawingContext& ctx, const Rect& bounds) {
    FillStyle bgFill;
    bgFill.solidColor = Color(250, 250, 250, 255);
    ctx.drawRect(bounds, bgFill);
    
    // Render tabs or content based on context
}

void ContextPanelRenderer::renderSymbolPanel(DrawingContext& ctx, const std::string& filePath, const Rect& bounds) {
    renderHeader(ctx, "Symbols", bounds);
    
    auto symbols = m_contextManager.getSymbolsInFile(filePath);
    float y = bounds.y + 40.0f;
    
    for (const auto& symbol : symbols) {
        Rect itemBounds(bounds.x, y, bounds.width, 24.0f);
        std::string itemText = std::string("%1 (%2)"));
        renderListItem(ctx, itemText, itemBounds);
        y += 28.0f;
    }
}

void ContextPanelRenderer::renderFilePanel(DrawingContext& ctx, const std::string& filePath, const Rect& bounds) {
    renderHeader(ctx, "File Information", bounds);
    
    auto fileCtx = m_contextManager.getFileContext(filePath);
    float y = bounds.y + 40.0f;
    
    // Display file info
    ctx.drawText(
        std::string("Path: %1"),
        Point(bounds.x + 10, y),
        "Arial", 11.0f, Color(0, 0, 0, 255)
    );
    y += 20.0f;
    
    ctx.drawText(
        std::string("Size: %1 bytes"),
        Point(bounds.x + 10, y),
        "Arial", 11.0f, Color(0, 0, 0, 255)
    );
    y += 20.0f;
    
    ctx.drawText(
        std::string("Modified: %1")),
        Point(bounds.x + 10, y),
        "Arial", 11.0f, Color(0, 0, 0, 255)
    );
}

void ContextPanelRenderer::renderToolsPanel(DrawingContext& ctx, const Rect& bounds) {
    renderHeader(ctx, "Available Tools", bounds);
    
    auto tools = m_contextManager.getAllTools();
    float y = bounds.y + 40.0f;
    
    for (const auto& tool : tools) {
        Rect itemBounds(bounds.x, y, bounds.width, 24.0f);
        std::string status = tool.isAvailable ? "✓" : "✗";
        std::string itemText = std::string("[%1] %2");
        renderListItem(ctx, itemText, itemBounds);
        y += 28.0f;
    }
}

void ContextPanelRenderer::renderSourceControlPanel(DrawingContext& ctx, const Rect& bounds) {
    renderHeader(ctx, "Source Control", bounds);
    
    auto scCtx = m_contextManager.getSourceControlContext();
    float y = bounds.y + 40.0f;
    
    ctx.drawText(
        std::string("Repository: %1"),
        Point(bounds.x + 10, y),
        "Arial", 11.0f, Color(0, 0, 0, 255)
    );
    y += 20.0f;
    
    ctx.drawText(
        std::string("Branch: %1"),
        Point(bounds.x + 10, y),
        "Arial", 11.0f, Color(0, 0, 0, 255)
    );
    y += 20.0f;
    
    ctx.drawText(
        std::string("Commit: %1...")),
        Point(bounds.x + 10, y),
        "Arial", 11.0f, Color(0, 0, 0, 255)
    );
}

void ContextPanelRenderer::renderInstructionsPanel(DrawingContext& ctx, const std::string& filePath, const Rect& bounds) {
    renderHeader(ctx, "Instructions", bounds);
    
    auto instructions = m_contextManager.getInstructionsForFile(filePath);
    float y = bounds.y + 40.0f;
    
    for (const auto& instr : instructions) {
        if (!instr.isVisible) continue;
        
        Rect itemBounds(bounds.x, y, bounds.width, 24.0f);
        renderListItem(ctx, instr.title, itemBounds);
        y += 28.0f;
    }
}

void ContextPanelRenderer::renderRelationshipsPanel(DrawingContext& ctx, const std::string& entityId, const Rect& bounds) {
    renderHeader(ctx, "Relationships", bounds);
    
    auto relationships = m_contextManager.getRelationshipsFor(entityId);
    float y = bounds.y + 40.0f;
    
    for (const auto& rel : relationships) {
        Rect itemBounds(bounds.x, y, bounds.width, 24.0f);
        std::string relText = std::string("%1 %2 %3")
            ;
        renderListItem(ctx, relText, itemBounds);
        y += 28.0f;
    }
}

void ContextPanelRenderer::renderOpenEditorsPanel(DrawingContext& ctx, const Rect& bounds) {
    renderHeader(ctx, "Open Editors", bounds);
    
    auto editors = m_contextManager.getOpenEditors();
    float y = bounds.y + 40.0f;
    
    for (const auto& editor : editors) {
        Rect itemBounds(bounds.x, y, bounds.width, 24.0f);
        std::string editorText = std::string("%1:%2");
        renderListItem(ctx, editorText, itemBounds);
        y += 28.0f;
    }
}

void ContextPanelRenderer::renderHeader(DrawingContext& ctx, const std::string& title, const Rect& bounds) {
    FillStyle headerFill;
    headerFill.solidColor = Color(230, 230, 230, 255);
    Rect headerBounds(bounds.x, bounds.y, bounds.width, 36.0f);
    ctx.drawRect(headerBounds, headerFill);
    
    StrokeStyle border;
    border.width = 1.0f;
    border.color = Color(200, 200, 200, 255);
    ctx.drawRect(headerBounds, FillStyle(), border);
    
    ctx.drawText(
        title,
        Point(bounds.x + 10, bounds.y + 10),
        "Arial", 14.0f, Color(0, 0, 0, 255)
    );
}

void ContextPanelRenderer::renderListItem(DrawingContext& ctx, const std::string& text, const Rect& bounds, bool selected) {
    if (selected) {
        FillStyle fill;
        fill.solidColor = Color(200, 220, 240, 255);
        ctx.drawRect(bounds, fill);
    }
    
    ctx.drawText(
        text,
        Point(bounds.x + 5, bounds.y + 4),
        "Arial", 11.0f, Color(0, 0, 0, 255)
    );
}

void ContextPanelRenderer::renderStatusBadge(DrawingContext& ctx, const std::string& status, const Point& pos, const Color& color) {
    Rect badgeBounds(pos.x, pos.y, 60.0f, 20.0f);
    FillStyle fill;
    fill.solidColor = color;
    ctx.drawRoundedRect(badgeBounds, 3.0f, fill);
    
    ctx.drawText(
        status,
        Point(pos.x + 5, pos.y + 3),
        "Arial", 9.0f, Color(255, 255, 255, 255)
    );
}

// ============================================================================
// CONTEXT GRAPH RENDERER IMPLEMENTATION
// ============================================================================

ContextGraphRenderer::ContextGraphRenderer(BreadcrumbContextManager& contextManager)
    : m_contextManager(contextManager) {}

ContextGraphRenderer::~ContextGraphRenderer() {}

void ContextGraphRenderer::renderDependencyGraph(DrawingContext& ctx, const std::string& centerEntity, const Rect& bounds) {
    
    auto dependencies = m_contextManager.getDependencies(centerEntity);
    auto dependents = m_contextManager.getDependents(centerEntity);
    
    // Calculate layout
    std::vector<Node> nodes;
    Node centerNode;
    centerNode.id = centerEntity;
    centerNode.label = centerEntity;
    centerNode.position = bounds.center();
    nodes.push_back(centerNode);
    
    // Layout dependency nodes in a circle
    float radius = 100.0f;
    int depCount = dependencies.size();
    for (int i = 0; i < depCount; ++i) {
        float angle = (2.0f * M_PI * i) / depCount;
        Node depNode;
        depNode.id = dependencies[i];
        depNode.label = dependencies[i];
        depNode.position = Point(
            bounds.center().x + radius * std::cos(angle),
            bounds.center().y + radius * std::sin(angle)
        );
        nodes.push_back(depNode);
    }
    
    // Render edges
    for (const auto& dep : dependencies) {
        auto rels = m_contextManager.getRelationshipsFor(centerEntity);
        for (const auto& rel : rels) {
            if (rel.targetId == dep) {
                renderEdge(ctx, centerNode.position, 
                          nodes[dependencies.indexOf(dep) + 1].position, 
                          rel.description);
            }
        }
    }
    
    // Render nodes
    for (const auto& node : nodes) {
        renderNode(ctx, node);
    }
}

void ContextGraphRenderer::renderFileRelationships(DrawingContext& ctx, const std::string& filePath, const Rect& bounds) {
    
    auto relatedFiles = m_contextManager.getRelatedFiles(filePath);
    
    // Similar layout logic
    std::vector<Node> nodes;
    Node centerNode;
    centerNode.id = filePath;
    centerNode.label = std::string(filePath).split("/").last();
    centerNode.position = bounds.center();
    nodes.push_back(centerNode);
    
    // Render file relationships
    for (const auto& relFile : relatedFiles) {
        Node relNode;
        relNode.id = relFile.absolutePath;
        relNode.label = relFile.fileName;
        nodes.push_back(relNode);
    }
}

void ContextGraphRenderer::renderSymbolCallGraph(DrawingContext& ctx, const std::string& symbolName, const Rect& bounds) {
}

void ContextGraphRenderer::layoutNodes(std::vector<Node>& nodes, const Rect& bounds) {
    if (nodes.empty()) return;
    
    // Simple force-directed layout
    for (auto& node : nodes) {
        node.bounds = Rect(node.position.x - 20, node.position.y - 20, 40, 40);
    }
}

void ContextGraphRenderer::renderNode(DrawingContext& ctx, const Node& node) {
    FillStyle fill;
    fill.solidColor = Color(52, 152, 219, 255);
    ctx.drawCircle(node.position, 20.0f, fill);
    
    StrokeStyle stroke;
    stroke.width = 2.0f;
    stroke.color = Color(0, 0, 0, 255);
    ctx.drawCircle(node.position, 20.0f, FillStyle(), stroke);
    
    auto textRect = ctx.measureText(node.label, "Arial", 10.0f);
    ctx.drawText(
        node.label,
        Point(node.position.x - textRect.width / 2, node.position.y - 5),
        "Arial", 10.0f, Color(255, 255, 255, 255)
    );
}

void ContextGraphRenderer::renderEdge(DrawingContext& ctx, const Point& from, const Point& to, const std::string& label) {
    StrokeStyle style;
    style.width = 2.0f;
    style.color = Color(100, 100, 100, 255);
    ctx.drawLine(from, to, style);
    
    if (!label.empty()) {
        Point mid((from.x + to.x) / 2, (from.y + to.y) / 2);
        ctx.drawText(label, Point(mid.x - 20, mid.y - 10), "Arial", 9.0f, Color(0, 0, 0, 255));
    }
}

// ============================================================================
// CONTEXT WINDOW IMPLEMENTATION
// ============================================================================

ContextWindow::ContextWindow(BreadcrumbContextManager& contextManager, const Rect& bounds)
    : Component(bounds),
      m_contextManager(contextManager),
      m_breadcrumbRenderer(),
      m_panelRenderer(contextManager),
      m_graphRenderer(contextManager),
      m_currentEntity(""),
      m_activeTab("breadcrumbs"),
      m_showBreadcrumbs(true),
      m_showContextPanel(true),
      m_showGraphView(false) {
    
    layoutPanels();
}

ContextWindow::~ContextWindow() {}

void ContextWindow::render(DrawingContext& ctx) {
    if (!m_visible) return;
    
    // Render background
    FillStyle bgFill;
    bgFill.solidColor = Color(240, 240, 240, 255);
    ctx.drawRect(m_bounds, bgFill);
    
    // Render tabs
    renderTabs(ctx);
    
    // Render breadcrumbs
    if (m_showBreadcrumbs) {
        m_breadcrumbRenderer.renderBreadcrumbs(ctx, m_contextManager.getBreadcrumbChain(), m_breadcrumbArea);
    }
    
    // Render content based on active tab
    if (m_activeTab == "breadcrumbs" && m_showBreadcrumbs) {
        m_breadcrumbRenderer.renderBreadcrumbs(ctx, m_contextManager.getBreadcrumbChain(), m_panelArea);
    }
    else if (m_activeTab == "context" && m_showContextPanel) {
        m_panelRenderer.renderContextPanel(ctx, m_panelArea);
    }
    else if (m_activeTab == "graph" && m_showGraphView) {
        if (!m_currentEntity.empty()) {
            m_graphRenderer.renderDependencyGraph(ctx, m_currentEntity, m_graphArea);
        }
    }
}

void ContextWindow::navigateToBreadcrumb(int index) {
    m_contextManager.navigateToBreadcrumb(index);
}

void ContextWindow::navigateToEntity(const std::string& entityId) {
    m_currentEntity = entityId;
    m_contextManager.pushContextBreadcrumb(ContextType::Tool, entityId);
}

void ContextWindow::addTab(const std::string& tabName) {
    m_tabVisibility[tabName] = true;
}

void ContextWindow::selectTab(const std::string& tabName) {
    if (m_tabVisibility.contains(tabName)) {
        m_activeTab = tabName;
    }
}

void ContextWindow::onMouseDown(const Point& pos) {
    // Handle tab clicks
}

void ContextWindow::onMouseUp(const Point& pos) {
    // Handle tab selection
}

void ContextWindow::onMouseMove(const Point& pos) {
    // Handle hover effects
}

void ContextWindow::layoutPanels() {
    float tabAreaHeight = 30.0f;
    float breadcrumbAreaHeight = 40.0f;
    
    m_tabArea = Rect(m_bounds.x, m_bounds.y, m_bounds.width, tabAreaHeight);
    m_breadcrumbArea = Rect(m_bounds.x, m_bounds.y + tabAreaHeight, m_bounds.width, breadcrumbAreaHeight);
    m_panelArea = Rect(m_bounds.x, m_bounds.y + tabAreaHeight + breadcrumbAreaHeight,
                      m_bounds.width * 0.5f, m_bounds.height - tabAreaHeight - breadcrumbAreaHeight);
    m_graphArea = Rect(m_bounds.x + m_bounds.width * 0.5f, m_bounds.y + tabAreaHeight + breadcrumbAreaHeight,
                      m_bounds.width * 0.5f, m_bounds.height - tabAreaHeight - breadcrumbAreaHeight);
}

void ContextWindow::renderTabs(DrawingContext& ctx) {
    float tabWidth = 100.0f;
    float x = m_tabArea.x + 5;
    
    std::stringList tabs = {"Breadcrumbs", "Context", "Graph"};
    for (const auto& tab : tabs) {
        Rect tabRect(x, m_tabArea.y, tabWidth, m_tabArea.height);
        
        FillStyle fill;
        fill.solidColor = (m_activeTab == tab) ? Color(52, 152, 219, 255) : Color(200, 200, 200, 255);
        ctx.drawRect(tabRect, fill);
        
        Color textColor = (m_activeTab == tab) ? Color(255, 255, 255, 255) : Color(0, 0, 0, 255);
        ctx.drawText(tab, Point(x + 5, m_tabArea.y + 8), "Arial", 11.0f, textColor);
        
        x += tabWidth + 5;
    }
}

// ============================================================================
// SCREENSHOT ANNOTATOR IMPLEMENTATION
// ============================================================================

ScreenshotAnnotator::ScreenshotAnnotator() {}

ScreenshotAnnotator::~ScreenshotAnnotator() {}

void ScreenshotAnnotator::addAnnotation(const ScreenshotAnnotation& annotation) {
    m_annotations.append(annotation);
}

void ScreenshotAnnotator::removeAnnotation(const std::string& annotationId) {
    for (int i = 0; i < m_annotations.size(); ++i) {
        if (m_annotations[i].id == annotationId) {
            m_annotations.removeAt(i);
            break;
        }
    }
}

void ScreenshotAnnotator::renderAnnotations(DrawingContext& ctx, const Surface& screenshot, const Rect& bounds) {
    for (const auto& annotation : m_annotations) {
        renderAnnotation(ctx, annotation);
    }
}

void ScreenshotAnnotator::renderAnnotation(DrawingContext& ctx, const ScreenshotAnnotation& annotation) {
    FillStyle fill;
    fill.solidColor = Color(255, 200, 0, 100);
    Rect annotRect(annotation.x, annotation.y, annotation.width, annotation.height);
    ctx.drawRect(annotRect, fill);
    
    StrokeStyle stroke;
    stroke.width = 2.0f;
    stroke.color = Color(255, 200, 0, 255);
    ctx.drawRect(annotRect, FillStyle(), stroke);
    
    ctx.drawText(annotation.text, Point(annotation.x + 5, annotation.y + 5),
                "Arial", 10.0f, Color(0, 0, 0, 255));
}

// ============================================================================
// INSTRUCTION PANEL IMPLEMENTATION
// ============================================================================

InstructionPanel::InstructionPanel(const Rect& bounds)
    : Component(bounds), m_isEmpty(true) {}

InstructionPanel::~InstructionPanel() {}

void InstructionPanel::render(DrawingContext& ctx) {
    if (!m_visible || m_isEmpty) return;
    
    FillStyle bgFill;
    bgFill.solidColor = Color(m_instruction.backgroundColor.empty() ? void(240, 240, 240) : void(m_instruction.backgroundColor)).rgba();
    ctx.drawRect(m_bounds, bgFill);
    
    StrokeStyle border;
    border.width = 2.0f;
    border.color = Color(m_instruction.borderColor.empty() ? void(100, 150, 200) : void(m_instruction.borderColor)).rgba();
    ctx.drawRect(m_bounds, FillStyle(), border);
    
    ctx.drawText(m_instruction.title, Point(m_bounds.x + 10, m_bounds.y + 10),
                "Arial", 14.0f, Color(0, 0, 0, 255));
    
    ctx.drawText(m_instruction.content, Point(m_bounds.x + 10, m_bounds.y + 35),
                "Arial", 11.0f, Color(50, 50, 50, 255));
}

void InstructionPanel::setInstruction(const InstructionBlock& instruction) {
    m_instruction = instruction;
    m_isEmpty = false;
}

void InstructionPanel::clear() {
    m_isEmpty = true;
}

// ============================================================================
// FILE BROWSER IMPLEMENTATION
// ============================================================================

FileBrowser::FileBrowser(BreadcrumbContextManager& contextManager, const Rect& bounds)
    : Component(bounds), m_contextManager(contextManager), m_scrollOffset(0) {}

FileBrowser::~FileBrowser() {}

void FileBrowser::render(DrawingContext& ctx) {
    if (!m_visible) return;
    
    FillStyle bgFill;
    bgFill.solidColor = Color(250, 250, 250, 255);
    ctx.drawRect(m_bounds, bgFill);
    
    StrokeStyle border;
    border.width = 1.0f;
    border.color = Color(200, 200, 200, 255);
    ctx.drawRect(m_bounds, FillStyle(), border);
}

void FileBrowser::navigateToFile(const std::string& filePath) {
    m_selectedFile = filePath;
    if (onFileSelected) {
        onFileSelected(filePath);
    }
}

void FileBrowser::expandFolder(const std::string& folderPath) {
    m_expandedFolders.insert(folderPath);
}

void FileBrowser::collapseFolder(const std::string& folderPath) {
    m_expandedFolders.remove(folderPath);
}

} // namespace Visualization
} // namespace RawrXD

