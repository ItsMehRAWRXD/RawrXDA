/**
 * @file uml_view_widget.cpp
 * @brief Implementation of UMLViewWidget - UML diagram viewer and generator
 */

#include "uml_view_widget.h"
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QRegularExpression>
#include <QDirIterator>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsDropShadowEffect>
#include <QDebug>
#include <cmath>
#include <algorithm>

// ============================================================================
// UMLClassItem Implementation
// ============================================================================

UMLClassItem::UMLClassItem(const UMLClass& umlClass, QGraphicsItem* parent)
    : QGraphicsRectItem(parent)
    , m_classId(umlClass.id)
    , m_name(umlClass.name)
    , m_stereotype(umlClass.stereotype)
    , m_attributes(umlClass.attributes)
    , m_methods(umlClass.methods)
    , m_highlighted(false)
{
    setFlag(QGraphicsItem::ItemIsMovable);
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges);
    
    updateFromClass(umlClass);
    
    // Add shadow effect
    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(8);
    shadow->setOffset(2, 2);
    shadow->setColor(Qt::black);
    setGraphicsEffect(shadow);
}

void UMLClassItem::updateFromClass(const UMLClass& umlClass)
{
    m_name = umlClass.name;
    m_stereotype = umlClass.stereotype;
    m_attributes = umlClass.attributes;
    m_methods = umlClass.methods;
    
    calculateSize();
    update();
}

UMLClass UMLClassItem::getClass() const
{
    UMLClass umlClass;
    umlClass.id = m_classId;
    umlClass.name = m_name;
    umlClass.stereotype = m_stereotype;
    umlClass.attributes = m_attributes;
    umlClass.methods = m_methods;
    umlClass.position = pos();
    umlClass.size = rect().size();
    umlClass.isSelected = isSelected();
    
    return umlClass;
}

void UMLClassItem::setHighlighted(bool highlighted)
{
    m_highlighted = highlighted;
    update();
}

void UMLClassItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)
    
    painter->setRenderHint(QPainter::Antialiasing);
    
    // Draw background
    QColor bgColor = m_highlighted ? QColor(173, 216, 230) : QColor(255, 255, 255);
    if (isSelected()) {
        bgColor = bgColor.darker(120);
    }
    
    painter->setBrush(QBrush(bgColor));
    painter->setPen(QPen(Qt::black, 1));
    painter->drawRect(rect());
    
    // Draw stereotype if present
    if (!m_stereotype.isEmpty()) {
        QFont stereotypeFont("Arial", 8);
        stereotypeFont.setItalic(true);
        painter->setFont(stereotypeFont);
        painter->drawText(rect().adjusted(PADDING, 2, -PADDING, 0), 
                         Qt::AlignTop | Qt::AlignHCenter, 
                         QString("«%1»").arg(m_stereotype));
    }
    
    // Draw class name
    painter->setFont(QFont("Arial", 10, QFont::Bold));
    QRectF nameRect = rect().adjusted(PADDING, HEADER_HEIGHT/2, -PADDING, 0);
    painter->drawText(nameRect, Qt::AlignTop | Qt::AlignHCenter, m_name);
    
    // Draw separator line
    painter->setPen(QPen(Qt::black, 1));
    qreal separatorY = HEADER_HEIGHT;
    painter->drawLine(rect().left(), separatorY, rect().right(), separatorY);
    
    // Draw attributes
    painter->setFont(QFont("Arial", 9));
    qreal y = HEADER_HEIGHT + PADDING;
    for (const UMLMember& attr : m_attributes) {
        QString attrText = QString("%1 %2: %3")
                          .arg(visibilitySymbol(attr.visibility))
                          .arg(attr.name)
                          .arg(attr.type);
        
        if (attr.isStatic) {
            painter->setFont(QFont("Arial", 9, QFont::Bold));
        }
        
        painter->drawText(rect().left() + PADDING, y, attrText);
        y += ROW_HEIGHT;
        
        if (attr.isStatic) {
            painter->setFont(QFont("Arial", 9));
        }
    }
    
    // Draw separator line between attributes and methods
    if (!m_attributes.isEmpty() && !m_methods.isEmpty()) {
        painter->setPen(QPen(Qt::black, 1));
        painter->drawLine(rect().left(), y - PADDING/2, rect().right(), y - PADDING/2);
    }
    
    // Draw methods
    for (const UMLMember& method : m_methods) {
        QString methodText = QString("%1 %2(%3)")
                            .arg(visibilitySymbol(method.visibility))
                            .arg(method.name)
                            .arg(method.parameters.join(", "));
        
        if (!method.returnType.isEmpty() && method.returnType != "void") {
            methodText += ": " + method.returnType;
        }
        
        if (method.isStatic) {
            painter->setFont(QFont("Arial", 9, QFont::Bold));
        }
        if (method.isAbstract) {
            QFont abstractFont("Arial", 9);
            abstractFont.setItalic(true);
            painter->setFont(abstractFont);
        }
        
        painter->drawText(rect().left() + PADDING, y, methodText);
        y += ROW_HEIGHT;
        
        painter->setFont(QFont("Arial", 9));
    }
}

void UMLClassItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsRectItem::mouseMoveEvent(event);
}

void UMLClassItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsRectItem::mouseReleaseEvent(event);
}

QVariant UMLClassItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemPositionChange) {
        // Emit position change signal through scene
        if (scene()) {
            UMLDiagramScene* umlScene = qobject_cast<UMLDiagramScene*>(scene());
            if (umlScene) {
                // Signal will be emitted after the change
                QMetaObject::invokeMethod(umlScene, "classPositionChanged", 
                                        Qt::QueuedConnection,
                                        Q_ARG(QString, m_classId),
                                        Q_ARG(QPointF, value.toPointF()));
            }
        }
    }
    
    return QGraphicsRectItem::itemChange(change, value);
}

void UMLClassItem::calculateSize()
{
    QFontMetrics nameMetrics(QFont("Arial", 10, QFont::Bold));
    QFontMetrics memberMetrics(QFont("Arial", 9));
    
    qreal maxWidth = MIN_WIDTH;
    
    // Calculate width based on name
    maxWidth = qMax(maxWidth, (qreal)nameMetrics.horizontalAdvance(m_name) + 2 * PADDING);
    
    // Calculate width based on attributes
    for (const UMLMember& attr : m_attributes) {
        QString attrText = QString("%1 %2: %3")
                          .arg(visibilitySymbol(attr.visibility))
                          .arg(attr.name)
                          .arg(attr.type);
        maxWidth = qMax(maxWidth, (qreal)memberMetrics.horizontalAdvance(attrText) + 2 * PADDING);
    }
    
    // Calculate width based on methods
    for (const UMLMember& method : m_methods) {
        QString methodText = QString("%1 %2(%3)")
                            .arg(visibilitySymbol(method.visibility))
                            .arg(method.name)
                            .arg(method.parameters.join(", "));
        
        if (!method.returnType.isEmpty() && method.returnType != "void") {
            methodText += ": " + method.returnType;
        }
        
        maxWidth = qMax(maxWidth, (qreal)memberMetrics.horizontalAdvance(methodText) + 2 * PADDING);
    }
    
    // Calculate height
    qreal height = HEADER_HEIGHT;
    height += m_attributes.size() * ROW_HEIGHT;
    height += m_methods.size() * ROW_HEIGHT;
    height += 2 * PADDING;
    
    setRect(0, 0, maxWidth, height);
}

QString UMLClassItem::visibilitySymbol(UMLVisibility vis) const
{
    switch (vis) {
        case UMLVisibility::Public: return "+";
        case UMLVisibility::Private: return "-";
        case UMLVisibility::Protected: return "#";
        case UMLVisibility::Package: return "~";
        default: return "";
    }
}

// ============================================================================
// UMLRelationItem Implementation
// ============================================================================

UMLRelationItem::UMLRelationItem(const UMLRelation& relation, 
                               UMLClassItem* source, 
                               UMLClassItem* target,
                               QGraphicsItem* parent)
    : QGraphicsPathItem(parent)
    , m_relationId(relation.id)
    , m_type(relation.type)
    , m_label(relation.label)
    , m_sourceMultiplicity(relation.sourceMultiplicity)
    , m_targetMultiplicity(relation.targetMultiplicity)
    , m_sourceItem(source)
    , m_targetItem(target)
{
    setFlag(QGraphicsItem::ItemIsSelectable);
    setZValue(-1); // Draw behind class items
    
    updatePath();
}

void UMLRelationItem::updatePath()
{
    if (!m_sourceItem || !m_targetItem) {
        return;
    }
    
    createPath();
}

UMLRelation UMLRelationItem::getRelation() const
{
    UMLRelation relation;
    relation.id = m_relationId;
    relation.sourceClassId = m_sourceItem ? m_sourceItem->classId() : "";
    relation.targetClassId = m_targetItem ? m_targetItem->classId() : "";
    relation.type = m_type;
    relation.label = m_label;
    relation.sourceMultiplicity = m_sourceMultiplicity;
    relation.targetMultiplicity = m_targetMultiplicity;
    
    return relation;
}

void UMLRelationItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)
    
    painter->setRenderHint(QPainter::Antialiasing);
    
    // Draw the path
    QPen pen(Qt::black, 1);
    if (isSelected()) {
        pen.setWidth(2);
        pen.setColor(Qt::blue);
    }
    painter->setPen(pen);
    painter->drawPath(path());
    
    // Draw label if present
    if (!m_label.isEmpty()) {
        painter->setFont(QFont("Arial", 8));
        painter->setPen(QPen(Qt::black));
        
        // Find midpoint of the path
        QPainterPath::Element element = path().elementAt(path().elementCount() / 2);
        QPointF labelPos(element.x, element.y - 10);
        
        painter->drawText(labelPos.toPoint(), m_label);
    }
}

void UMLRelationItem::createPath()
{
    if (!m_sourceItem || !m_targetItem) {
        return;
    }
    
    QPainterPath path;
    
    // Get center points of class items
    QPointF sourceCenter = m_sourceItem->sceneBoundingRect().center();
    QPointF targetCenter = m_targetItem->sceneBoundingRect().center();
    
    // Calculate connection points on the edges
    QPointF sourcePoint = calculateEdgePoint(sourceCenter, targetCenter, m_sourceItem->sceneBoundingRect());
    QPointF targetPoint = calculateEdgePoint(targetCenter, sourceCenter, m_targetItem->sceneBoundingRect());
    
    // Create the path
    path.moveTo(sourcePoint);
    
    // Add waypoints if any
    // For now, just draw a straight line or simple curve
    
    // Calculate control points for a smooth curve
    QPointF midPoint = (sourcePoint + targetPoint) / 2;
    QPointF controlOffset = QPointF(50, 0); // Simple horizontal offset
    
    if (sourcePoint.x() > targetPoint.x()) {
        controlOffset.setX(-50);
    }
    
    QPointF control1 = sourcePoint + controlOffset;
    QPointF control2 = targetPoint - controlOffset;
    
    path.cubicTo(control1, control2, targetPoint);
    
    setPath(path);
}

QPointF UMLRelationItem::calculateEdgePoint(const QPointF& from, const QPointF& to, const QRectF& rect)
{
    // Calculate the intersection point of the line from 'from' to 'to' with the rectangle 'rect'
    QPointF center = rect.center();
    QPointF direction = to - from;
    direction /= qMax(qAbs(direction.x()), qAbs(direction.y())); // Normalize
    
    QPointF point = center;
    
    // Move in the direction until we hit the edge
    while (rect.contains(point)) {
        point += direction;
    }
    
    // Find the actual intersection
    QPointF topLeft = rect.topLeft();
    QPointF bottomRight = rect.bottomRight();
    
    if (direction.x() > 0) {
        // Moving right
        qreal t = (bottomRight.x() - center.x()) / direction.x();
        point = center + t * direction;
    } else if (direction.x() < 0) {
        // Moving left
        qreal t = (topLeft.x() - center.x()) / direction.x();
        point = center + t * direction;
    }
    
    if (direction.y() > 0) {
        // Moving down
        qreal t = (bottomRight.y() - center.y()) / direction.y();
        QPointF candidate = center + t * direction;
        if (rect.contains(candidate)) {
            point = candidate;
        }
    } else if (direction.y() < 0) {
        // Moving up
        qreal t = (topLeft.y() - center.y()) / direction.y();
        QPointF candidate = center + t * direction;
        if (rect.contains(candidate)) {
            point = candidate;
        }
    }
    
    return point;
}

// ============================================================================
// UMLDiagramScene Implementation
// ============================================================================

UMLDiagramScene::UMLDiagramScene(QObject* parent)
    : QGraphicsScene(parent)
{
    setBackgroundBrush(QBrush(QColor(240, 240, 240)));
}

void UMLDiagramScene::addClass(const UMLClass& umlClass)
{
    UMLClassItem* item = new UMLClassItem(umlClass);
    item->setPos(umlClass.position);
    addItem(item);
    
    m_classItems[umlClass.id] = item;
    
    // Note: Position changes are handled via itemChange override in UMLClassItem
    // which calls classPositionChanged via QMetaObject::invokeMethod
}

void UMLDiagramScene::removeClass(const QString& classId)
{
    if (m_classItems.contains(classId)) {
        UMLClassItem* item = m_classItems[classId];
        removeItem(item);
        delete item;
        m_classItems.remove(classId);
    }
    
    // Remove related relations
    QList<QString> relationsToRemove;
    for (auto it = m_relationItems.begin(); it != m_relationItems.end(); ++it) {
        if (it.value()->sourceItem()->classId() == classId || 
            it.value()->targetItem()->classId() == classId) {
            relationsToRemove.append(it.key());
        }
    }
    
    for (const QString& relationId : relationsToRemove) {
        removeRelation(relationId);
    }
}

void UMLDiagramScene::updateClass(const UMLClass& umlClass)
{
    if (m_classItems.contains(umlClass.id)) {
        m_classItems[umlClass.id]->updateFromClass(umlClass);
        m_classItems[umlClass.id]->setPos(umlClass.position);
    }
}

UMLClassItem* UMLDiagramScene::findClassItem(const QString& classId)
{
    return m_classItems.value(classId, nullptr);
}

void UMLDiagramScene::addRelation(const UMLRelation& relation)
{
    UMLClassItem* sourceItem = findClassItem(relation.sourceClassId);
    UMLClassItem* targetItem = findClassItem(relation.targetClassId);
    
    if (sourceItem && targetItem) {
        UMLRelationItem* item = new UMLRelationItem(relation, sourceItem, targetItem);
        addItem(item);
        m_relationItems[relation.id] = item;
    }
}

void UMLDiagramScene::removeRelation(const QString& relationId)
{
    if (m_relationItems.contains(relationId)) {
        UMLRelationItem* item = m_relationItems[relationId];
        removeItem(item);
        delete item;
        m_relationItems.remove(relationId);
    }
}

void UMLDiagramScene::updateRelations()
{
    for (UMLRelationItem* item : m_relationItems) {
        item->updatePath();
    }
}

void UMLDiagramScene::clear()
{
    clearSelection();
    
    // Delete all items
    QList<QGraphicsItem*> items = this->items();
    for (QGraphicsItem* item : items) {
        removeItem(item);
        delete item;
    }
    
    m_classItems.clear();
    m_relationItems.clear();
}

void UMLDiagramScene::applyLayout(LayoutAlgorithm algorithm)
{
    switch (algorithm) {
        case LayoutAlgorithm::Hierarchical:
            layoutHierarchical();
            break;
        case LayoutAlgorithm::ForceDirected:
            layoutForceDirected();
            break;
        case LayoutAlgorithm::Orthogonal:
            layoutOrthogonal();
            break;
        case LayoutAlgorithm::Circular:
            layoutCircular();
            break;
        case LayoutAlgorithm::Tree:
            // For now, use hierarchical
            layoutHierarchical();
            break;
        case LayoutAlgorithm::Manual:
            // Do nothing
            break;
    }
    
    updateRelations();
}

QList<UMLClass> UMLDiagramScene::getAllClasses() const
{
    QList<UMLClass> classes;
    for (UMLClassItem* item : m_classItems) {
        classes.append(item->getClass());
    }
    return classes;
}

QList<UMLRelation> UMLDiagramScene::getAllRelations() const
{
    QList<UMLRelation> relations;
    for (UMLRelationItem* item : m_relationItems) {
        relations.append(item->getRelation());
    }
    return relations;
}

void UMLDiagramScene::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsScene::mousePressEvent(event);
    
    // Clear selection if clicking on empty space
    if (event->button() == Qt::LeftButton && selectedItems().isEmpty()) {
        emit classSelected("");
    }
}

void UMLDiagramScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsScene::mouseDoubleClickEvent(event);
    
    UMLClassItem* classItem = qgraphicsitem_cast<UMLClassItem*>(itemAt(event->scenePos(), QTransform()));
    if (classItem) {
        emit classDoubleClicked(classItem->classId());
    }
}

void UMLDiagramScene::layoutHierarchical()
{
    if (m_classItems.isEmpty()) return;
    
    // Simple hierarchical layout
    QList<UMLClassItem*> items = m_classItems.values();
    int count = items.size();
    
    // Calculate grid dimensions
    int cols = qCeil(qSqrt(count));
    int rows = qCeil((qreal)count / cols);
    
    qreal spacingX = 200;
    qreal spacingY = 150;
    
    for (int i = 0; i < count; ++i) {
        int row = i / cols;
        int col = i % cols;
        
        qreal x = col * spacingX;
        qreal y = row * spacingY;
        
        items[i]->setPos(x, y);
    }
}

void UMLDiagramScene::layoutForceDirected()
{
    // Simple force-directed layout implementation
    const int iterations = 50;
    const qreal attraction = 0.01;
    const qreal repulsion = 1000.0;
    
    QList<UMLClassItem*> items = m_classItems.values();
    int count = items.size();
    
    for (int iter = 0; iter < iterations; ++iter) {
        QVector<QPointF> forces(count, QPointF(0, 0));
        
        // Calculate repulsive forces
        for (int i = 0; i < count; ++i) {
            for (int j = 0; j < count; ++j) {
                if (i == j) continue;
                
                QPointF delta = items[j]->pos() - items[i]->pos();
                qreal distance = qMax(1.0, delta.manhattanLength());
                
                QPointF force = delta / distance * repulsion / (distance * distance);
                forces[i] -= force;
            }
        }
        
        // Calculate attractive forces (from relations)
        for (UMLRelationItem* relation : m_relationItems) {
            UMLClassItem* source = relation->sourceItem();
            UMLClassItem* target = relation->targetItem();
            
            int sourceIndex = items.indexOf(source);
            int targetIndex = items.indexOf(target);
            
            if (sourceIndex >= 0 && targetIndex >= 0) {
                QPointF delta = target->pos() - source->pos();
                qreal distance = qMax(1.0, delta.manhattanLength());
                
                QPointF force = delta / distance * attraction * distance;
                forces[sourceIndex] += force;
                forces[targetIndex] -= force;
            }
        }
        
        // Apply forces
        for (int i = 0; i < count; ++i) {
            QPointF newPos = items[i]->pos() + forces[i] * 0.1;
            items[i]->setPos(newPos);
        }
    }
}

void UMLDiagramScene::layoutOrthogonal()
{
    // Simple orthogonal layout
    layoutHierarchical(); // For now, use hierarchical
}

void UMLDiagramScene::layoutCircular()
{
    QList<UMLClassItem*> items = m_classItems.values();
    int count = items.size();
    
    if (count == 0) return;
    
    qreal radius = 200;
    qreal angleStep = 2 * M_PI / count;
    
    for (int i = 0; i < count; ++i) {
        qreal angle = i * angleStep;
        qreal x = radius * qCos(angle);
        qreal y = radius * qSin(angle);
        
        items[i]->setPos(x, y);
    }
}

// ============================================================================
// UMLViewWidget Implementation
// ============================================================================

UMLViewWidget::UMLViewWidget(QWidget* parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_toolbar(nullptr)
    , m_splitter(nullptr)
    , m_newAction(nullptr)
    , m_openAction(nullptr)
    , m_saveAction(nullptr)
    , m_exportAction(nullptr)
    , m_parseAction(nullptr)
    , m_refreshAction(nullptr)
    , m_zoomInAction(nullptr)
    , m_zoomOutAction(nullptr)
    , m_zoomFitAction(nullptr)
    , m_autoLayoutAction(nullptr)
    , m_layoutCombo(nullptr)
    , m_diagramTypeCombo(nullptr)
    , m_zoomSlider(nullptr)
    , m_zoomLabel(nullptr)
    , m_sidebarTabs(nullptr)
    , m_classTree(nullptr)
    , m_relationsList(nullptr)
    , m_infoPanel(nullptr)
    , m_classNameLabel(nullptr)
    , m_classFileLabel(nullptr)
    , m_membersList(nullptr)
    , m_graphicsView(nullptr)
    , m_scene(nullptr)
    , m_diagramType(UMLDiagramType::ClassDiagram)
    , m_layoutAlgorithm(LayoutAlgorithm::Hierarchical)
    , m_fileWatcher(nullptr)
    , m_refreshTimer(nullptr)
    , m_zoomFactor(1.0)
    , m_showAttributes(true)
    , m_showMethods(true)
    , m_showVisibility(true)
    , m_showStereotypes(true)
{
    setupUI();
    setupConnections();
    
    newDiagram(UMLDiagramType::ClassDiagram);
    
    qDebug() << "UMLViewWidget initialized";
}

UMLViewWidget::~UMLViewWidget()
{
    if (m_fileWatcher) {
        delete m_fileWatcher;
    }
}

void UMLViewWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    
    setupToolbar();
    
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_mainLayout->addWidget(m_splitter);
    
    setupSidebar();
    setupDiagramView();
    
    // Set initial splitter sizes
    m_splitter->setSizes({200, 600});
}

void UMLViewWidget::setupToolbar()
{
    m_toolbar = new QToolBar(tr("UML"), this);
    m_toolbar->setIconSize(QSize(18, 18));
    m_mainLayout->addWidget(m_toolbar);
    
    // File actions
    m_newAction = m_toolbar->addAction(QIcon::fromTheme("document-new", QIcon(":/icons/new.png")), tr("New"));
    m_openAction = m_toolbar->addAction(QIcon::fromTheme("document-open", QIcon(":/icons/open.png")), tr("Open"));
    m_saveAction = m_toolbar->addAction(QIcon::fromTheme("document-save", QIcon(":/icons/save.png")), tr("Save"));
    m_exportAction = m_toolbar->addAction(QIcon::fromTheme("document-export", QIcon(":/icons/export.png")), tr("Export"));
    
    m_toolbar->addSeparator();
    
    // Parse action
    m_parseAction = m_toolbar->addAction(QIcon::fromTheme("system-search", QIcon(":/icons/parse.png")), tr("Parse Code"));
    m_refreshAction = m_toolbar->addAction(QIcon::fromTheme("view-refresh", QIcon(":/icons/refresh.png")), tr("Refresh"));
    
    m_toolbar->addSeparator();
    
    // Diagram type
    m_diagramTypeCombo = new QComboBox(this);
    m_diagramTypeCombo->addItem(tr("Class Diagram"), static_cast<int>(UMLDiagramType::ClassDiagram));
    m_diagramTypeCombo->addItem(tr("Sequence Diagram"), static_cast<int>(UMLDiagramType::SequenceDiagram));
    m_diagramTypeCombo->addItem(tr("Activity Diagram"), static_cast<int>(UMLDiagramType::ActivityDiagram));
    m_diagramTypeCombo->addItem(tr("Use Case Diagram"), static_cast<int>(UMLDiagramType::UseCaseDiagram));
    m_diagramTypeCombo->addItem(tr("State Diagram"), static_cast<int>(UMLDiagramType::StateDiagram));
    m_diagramTypeCombo->addItem(tr("Component Diagram"), static_cast<int>(UMLDiagramType::ComponentDiagram));
    m_toolbar->addWidget(m_diagramTypeCombo);
    
    // Layout combo
    m_layoutCombo = new QComboBox(this);
    m_layoutCombo->addItem(tr("Hierarchical"), static_cast<int>(LayoutAlgorithm::Hierarchical));
    m_layoutCombo->addItem(tr("Force Directed"), static_cast<int>(LayoutAlgorithm::ForceDirected));
    m_layoutCombo->addItem(tr("Orthogonal"), static_cast<int>(LayoutAlgorithm::Orthogonal));
    m_layoutCombo->addItem(tr("Circular"), static_cast<int>(LayoutAlgorithm::Circular));
    m_layoutCombo->addItem(tr("Tree"), static_cast<int>(LayoutAlgorithm::Tree));
    m_layoutCombo->addItem(tr("Manual"), static_cast<int>(LayoutAlgorithm::Manual));
    m_toolbar->addWidget(m_layoutCombo);
    
    m_autoLayoutAction = m_toolbar->addAction(QIcon::fromTheme("view-sort-ascending", QIcon(":/icons/layout.png")), tr("Auto Layout"));
    
    m_toolbar->addSeparator();
    
    // Zoom controls
    m_zoomOutAction = m_toolbar->addAction(QIcon::fromTheme("zoom-out", QIcon(":/icons/zoom-out.png")), tr("Zoom Out"));
    m_zoomSlider = new QSlider(Qt::Horizontal, this);
    m_zoomSlider->setRange(10, 400);
    m_zoomSlider->setValue(100);
    m_zoomSlider->setMaximumWidth(100);
    m_toolbar->addWidget(m_zoomSlider);
    
    m_zoomInAction = m_toolbar->addAction(QIcon::fromTheme("zoom-in", QIcon(":/icons/zoom-in.png")), tr("Zoom In"));
    m_zoomFitAction = m_toolbar->addAction(QIcon::fromTheme("zoom-fit-best", QIcon(":/icons/zoom-fit.png")), tr("Zoom to Fit"));
    
    m_zoomLabel = new QLabel("100%", this);
    m_zoomLabel->setMinimumWidth(40);
    m_toolbar->addWidget(m_zoomLabel);
}

void UMLViewWidget::setupSidebar()
{
    m_sidebarTabs = new QTabWidget(this);
    m_splitter->addWidget(m_sidebarTabs);
    
    // Classes tree
    m_classTree = new QTreeWidget(this);
    m_classTree->setHeaderLabel(tr("Classes"));
    m_classTree->setRootIsDecorated(true);
    m_classTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_sidebarTabs->addTab(m_classTree, tr("Classes"));
    
    // Relations list
    m_relationsList = new QListWidget(this);
    m_relationsList->setAlternatingRowColors(true);
    m_sidebarTabs->addTab(m_relationsList, tr("Relations"));
    
    // Info panel
    m_infoPanel = new QWidget(this);
    QVBoxLayout* infoLayout = new QVBoxLayout(m_infoPanel);
    
    m_classNameLabel = new QLabel(tr("No class selected"), m_infoPanel);
    m_classNameLabel->setWordWrap(true);
    m_classNameLabel->setStyleSheet("font-weight: bold;");
    infoLayout->addWidget(m_classNameLabel);
    
    m_classFileLabel = new QLabel("", m_infoPanel);
    m_classFileLabel->setWordWrap(true);
    m_classFileLabel->setStyleSheet("color: gray; font-size: 10px;");
    infoLayout->addWidget(m_classFileLabel);
    
    infoLayout->addWidget(new QLabel(tr("Members:"), m_infoPanel));
    
    m_membersList = new QListWidget(m_infoPanel);
    m_membersList->setAlternatingRowColors(true);
    infoLayout->addWidget(m_membersList);
    
    m_sidebarTabs->addTab(m_infoPanel, tr("Info"));
}

void UMLViewWidget::setupDiagramView()
{
    m_scene = new UMLDiagramScene(this);
    m_graphicsView = new QGraphicsView(m_scene, this);
    m_graphicsView->setRenderHint(QPainter::Antialiasing);
    m_graphicsView->setDragMode(QGraphicsView::RubberBandDrag);
    m_graphicsView->setContextMenuPolicy(Qt::CustomContextMenu);
    
    m_splitter->addWidget(m_graphicsView);
}

void UMLViewWidget::setupConnections()
{
    // Toolbar actions
    connect(m_newAction, &QAction::triggered, this, [this]() {
        newDiagram(m_diagramType);
    });
    
    connect(m_openAction, &QAction::triggered, this, [this]() {
        QString fileName = QFileDialog::getOpenFileName(this, tr("Open UML Diagram"), 
                                                      QString(), tr("JSON files (*.json);;All files (*)"));
        if (!fileName.isEmpty()) {
            loadDiagram(fileName);
        }
    });
    
    connect(m_saveAction, &QAction::triggered, this, [this]() {
        QString fileName = QFileDialog::getSaveFileName(this, tr("Save UML Diagram"), 
                                                      QString(), tr("JSON files (*.json);;All files (*)"));
        if (!fileName.isEmpty()) {
            saveDiagram(fileName);
        }
    });
    
    connect(m_exportAction, &QAction::triggered, this, [this]() {
        QString fileName = QFileDialog::getSaveFileName(this, tr("Export Diagram"), 
                                                      QString(), tr("PNG files (*.png);;SVG files (*.svg);;PlantUML files (*.puml);;All files (*)"));
        if (!fileName.isEmpty()) {
            exportDiagram(fileName, QFileInfo(fileName).suffix());
        }
    });
    
    connect(m_parseAction, &QAction::triggered, this, [this]() {
        QString dirPath = QFileDialog::getExistingDirectory(this, tr("Select Source Directory"));
        if (!dirPath.isEmpty()) {
            parseSourceDirectory(dirPath);
        }
    });
    
    connect(m_refreshAction, &QAction::triggered, this, &UMLViewWidget::refresh);
    
    // Diagram type and layout
    connect(m_diagramTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        m_diagramType = static_cast<UMLDiagramType>(m_diagramTypeCombo->itemData(index).toInt());
    });
    
    connect(m_layoutCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &UMLViewWidget::onLayoutChanged);
    connect(m_autoLayoutAction, &QAction::triggered, this, &UMLViewWidget::autoLayout);
    
    // Zoom controls
    connect(m_zoomInAction, &QAction::triggered, this, &UMLViewWidget::zoomIn);
    connect(m_zoomOutAction, &QAction::triggered, this, &UMLViewWidget::zoomOut);
    connect(m_zoomFitAction, &QAction::triggered, this, &UMLViewWidget::zoomToFit);
    connect(m_zoomSlider, &QSlider::valueChanged, this, &UMLViewWidget::onZoomSliderChanged);
    
    // Scene connections
    connect(m_scene, &UMLDiagramScene::classSelected, this, &UMLViewWidget::onClassSelected);
    connect(m_scene, &UMLDiagramScene::classDoubleClicked, this, &UMLViewWidget::onClassDoubleClicked);
    connect(m_scene, &UMLDiagramScene::classPositionChanged, this, [this](const QString& classId, const QPointF& pos) {
        if (m_classes.contains(classId)) {
            m_classes[classId].position = pos;
        }
    });
    
    // Tree connections
    connect(m_classTree, &QTreeWidget::itemClicked, this, &UMLViewWidget::onClassTreeItemClicked);
    connect(m_classTree, &QTreeWidget::customContextMenuRequested, this, &UMLViewWidget::showContextMenu);
    
    // File watcher
    m_fileWatcher = new QFileSystemWatcher(this);
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &UMLViewWidget::onFileChanged);
    
    // Refresh timer
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(1000);
    connect(m_refreshTimer, &QTimer::timeout, this, &UMLViewWidget::refresh);
}

void UMLViewWidget::newDiagram(UMLDiagramType type)
{
    m_diagramType = type;
    m_classes.clear();
    m_relations.clear();
    m_scene->clear();
    updateClassTree();
    m_relationsList->clear();
    m_currentDiagramPath.clear();
    
    emit diagramChanged();
}

void UMLViewWidget::loadDiagram(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Error"), tr("Cannot open file: %1").arg(filePath));
        return;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        QMessageBox::warning(this, tr("Error"), tr("Invalid diagram file"));
        return;
    }
    
    QJsonObject root = doc.object();
    
    // Load classes
    QJsonArray classesArray = root["classes"].toArray();
    for (const QJsonValue& value : classesArray) {
        QJsonObject obj = value.toObject();
        
        UMLClass umlClass;
        umlClass.id = obj["id"].toString();
        umlClass.name = obj["name"].toString();
        umlClass.stereotype = obj["stereotype"].toString();
        umlClass.package = obj["package"].toString();
        umlClass.filePath = obj["filePath"].toString();
        umlClass.lineNumber = obj["lineNumber"].toInt();
        
        QJsonArray positionArray = obj["position"].toArray();
        if (positionArray.size() >= 2) {
            umlClass.position = QPointF(positionArray[0].toDouble(), positionArray[1].toDouble());
        }
        
        // Load attributes
        QJsonArray attrsArray = obj["attributes"].toArray();
        for (const QJsonValue& attrValue : attrsArray) {
            QJsonObject attrObj = attrValue.toObject();
            UMLMember member;
            member.name = attrObj["name"].toString();
            member.type = attrObj["type"].toString();
            member.visibility = static_cast<UMLVisibility>(attrObj["visibility"].toInt());
            member.isStatic = attrObj["isStatic"].toBool();
            umlClass.attributes.append(member);
        }
        
        // Load methods
        QJsonArray methodsArray = obj["methods"].toArray();
        for (const QJsonValue& methodValue : methodsArray) {
            QJsonObject methodObj = methodValue.toObject();
            UMLMember member;
            member.name = methodObj["name"].toString();
            member.returnType = methodObj["returnType"].toString();
            member.visibility = static_cast<UMLVisibility>(methodObj["visibility"].toInt());
            member.isStatic = methodObj["isStatic"].toBool();
            member.isAbstract = methodObj["isAbstract"].toBool();
            
            QJsonArray paramsArray = methodObj["parameters"].toArray();
            for (const QJsonValue& param : paramsArray) {
                member.parameters.append(param.toString());
            }
            
            umlClass.methods.append(member);
        }
        
        addClass(umlClass);
    }
    
    // Load relations
    QJsonArray relationsArray = root["relations"].toArray();
    for (const QJsonValue& value : relationsArray) {
        QJsonObject obj = value.toObject();
        
        UMLRelation relation;
        relation.id = obj["id"].toString();
        relation.sourceClassId = obj["sourceClassId"].toString();
        relation.targetClassId = obj["targetClassId"].toString();
        relation.type = static_cast<UMLRelationType>(obj["type"].toInt());
        relation.label = obj["label"].toString();
        relation.sourceMultiplicity = obj["sourceMultiplicity"].toString();
        relation.targetMultiplicity = obj["targetMultiplicity"].toString();
        
        addRelation(relation);
    }
    
    m_currentDiagramPath = filePath;
    applyLayout(m_layoutAlgorithm);
    
    emit diagramChanged();
}

void UMLViewWidget::saveDiagram(const QString& filePath)
{
    QJsonObject root;
    
    // Save classes
    QJsonArray classesArray;
    for (const UMLClass& umlClass : m_classes) {
        QJsonObject obj;
        obj["id"] = umlClass.id;
        obj["name"] = umlClass.name;
        obj["stereotype"] = umlClass.stereotype;
        obj["package"] = umlClass.package;
        obj["filePath"] = umlClass.filePath;
        obj["lineNumber"] = umlClass.lineNumber;
        
        QJsonArray positionArray;
        positionArray.append(umlClass.position.x());
        positionArray.append(umlClass.position.y());
        obj["position"] = positionArray;
        
        // Save attributes
        QJsonArray attrsArray;
        for (const UMLMember& attr : umlClass.attributes) {
            QJsonObject attrObj;
            attrObj["name"] = attr.name;
            attrObj["type"] = attr.type;
            attrObj["visibility"] = static_cast<int>(attr.visibility);
            attrObj["isStatic"] = attr.isStatic;
            attrsArray.append(attrObj);
        }
        obj["attributes"] = attrsArray;
        
        // Save methods
        QJsonArray methodsArray;
        for (const UMLMember& method : umlClass.methods) {
            QJsonObject methodObj;
            methodObj["name"] = method.name;
            methodObj["returnType"] = method.returnType;
            methodObj["visibility"] = static_cast<int>(method.visibility);
            methodObj["isStatic"] = method.isStatic;
            methodObj["isAbstract"] = method.isAbstract;
            
            QJsonArray paramsArray;
            for (const QString& param : method.parameters) {
                paramsArray.append(param);
            }
            methodObj["parameters"] = paramsArray;
            
            methodsArray.append(methodObj);
        }
        obj["methods"] = methodsArray;
        
        classesArray.append(obj);
    }
    root["classes"] = classesArray;
    
    // Save relations
    QJsonArray relationsArray;
    for (const UMLRelation& relation : m_relations) {
        QJsonObject obj;
        obj["id"] = relation.id;
        obj["sourceClassId"] = relation.sourceClassId;
        obj["targetClassId"] = relation.targetClassId;
        obj["type"] = static_cast<int>(relation.type);
        obj["label"] = relation.label;
        obj["sourceMultiplicity"] = relation.sourceMultiplicity;
        obj["targetMultiplicity"] = relation.targetMultiplicity;
        
        relationsArray.append(obj);
    }
    root["relations"] = relationsArray;
    
    QJsonDocument doc(root);
    
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        m_currentDiagramPath = filePath;
    } else {
        QMessageBox::warning(this, tr("Error"), tr("Cannot save file: %1").arg(filePath));
    }
}

void UMLViewWidget::exportDiagram(const QString& filePath, const QString& format)
{
    if (format.toLower() == "png") {
        QPixmap pixmap = m_graphicsView->grab();
        if (!pixmap.save(filePath, "PNG")) {
            QMessageBox::warning(this, tr("Error"), tr("Cannot export to PNG"));
            return;
        }
    } else if (format.toLower() == "svg") {
        // SVG export would require additional implementation
        QMessageBox::information(this, tr("Info"), tr("SVG export not yet implemented"));
        return;
    } else if (format.toLower() == "puml") {
        QString plantUML = generatePlantUML();
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << plantUML;
            file.close();
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Cannot export to PlantUML"));
            return;
        }
    }
    
    emit exportCompleted(filePath, true);
}

void UMLViewWidget::parseSourceFile(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    
    if (extension == "cpp" || extension == "hpp" || extension == "h" || extension == "cc" || extension == "cxx") {
        parseCppFile(filePath);
    } else if (extension == "py") {
        parsePythonFile(filePath);
    } else if (extension == "java") {
        parseJavaFile(filePath);
    }
    
    detectRelations();
    updateClassTree();
    applyLayout(m_layoutAlgorithm);
    
    emit parseCompleted(m_classes.size(), m_relations.size());
}

void UMLViewWidget::parseSourceDirectory(const QString& dirPath, bool recursive)
{
    QDirIterator::IteratorFlags flags = recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    
    QDirIterator it(dirPath, {"*.cpp", "*.hpp", "*.h", "*.cc", "*.cxx", "*.py", "*.java"}, 
                   QDir::Files, flags);
    
    while (it.hasNext()) {
        parseSourceFile(it.next());
    }
    
    // Watch files for changes
    m_watchedFiles.clear();
    QDirIterator watchIt(dirPath, {"*.cpp", "*.hpp", "*.h", "*.cc", "*.cxx", "*.py", "*.java"}, 
                        QDir::Files, flags);
    while (watchIt.hasNext()) {
        m_watchedFiles.append(watchIt.next());
    }
    m_fileWatcher->addPaths(m_watchedFiles);
    
    emit parseCompleted(m_classes.size(), m_relations.size());
}

void UMLViewWidget::clearParsedData()
{
    m_classes.clear();
    m_relations.clear();
    m_scene->clear();
    updateClassTree();
    m_relationsList->clear();
    
    if (m_fileWatcher) {
        m_fileWatcher->removePaths(m_watchedFiles);
        m_watchedFiles.clear();
    }
}

void UMLViewWidget::addClass(const UMLClass& umlClass)
{
    m_classes[umlClass.id] = umlClass;
    m_scene->addClass(umlClass);
}

void UMLViewWidget::removeClass(const QString& classId)
{
    m_classes.remove(classId);
    m_scene->removeClass(classId);
    updateClassTree();
}

void UMLViewWidget::updateClass(const UMLClass& umlClass)
{
    m_classes[umlClass.id] = umlClass;
    m_scene->updateClass(umlClass);
}

void UMLViewWidget::selectClass(const QString& classId)
{
    m_selectedClassId = classId;
    updateInfoPanel(classId);
    
    // Highlight in scene
    for (QGraphicsItem* item : m_scene->items()) {
        if (UMLClassItem* classItem = dynamic_cast<UMLClassItem*>(item)) {
            classItem->setHighlighted(classItem->classId() == classId);
        }
    }
    
    emit classSelected(classId, m_classes.value(classId).filePath, m_classes.value(classId).lineNumber);
}

void UMLViewWidget::addRelation(const UMLRelation& relation)
{
    m_relations[relation.id] = relation;
    m_scene->addRelation(relation);
}

void UMLViewWidget::removeRelation(const QString& relationId)
{
    m_relations.remove(relationId);
    m_scene->removeRelation(relationId);
}

void UMLViewWidget::applyLayout(LayoutAlgorithm algorithm)
{
    m_layoutAlgorithm = algorithm;
    m_scene->applyLayout(algorithm);
}

void UMLViewWidget::setLayoutAlgorithm(LayoutAlgorithm algorithm)
{
    m_layoutAlgorithm = algorithm;
    m_layoutCombo->setCurrentIndex(static_cast<int>(algorithm));
}

LayoutAlgorithm UMLViewWidget::getLayoutAlgorithm() const
{
    return m_layoutAlgorithm;
}

void UMLViewWidget::zoomIn()
{
    setZoom(m_zoomFactor * (1.0 + ZOOM_STEP));
}

void UMLViewWidget::zoomOut()
{
    setZoom(m_zoomFactor * (1.0 - ZOOM_STEP));
}

void UMLViewWidget::zoomToFit()
{
    if (m_scene->items().isEmpty()) {
        setZoom(1.0);
        return;
    }
    
    QRectF sceneRect = m_scene->itemsBoundingRect();
    if (sceneRect.isEmpty()) {
        setZoom(1.0);
        return;
    }
    
    QRectF viewRect = m_graphicsView->viewport()->rect();
    qreal scaleX = viewRect.width() / sceneRect.width();
    qreal scaleY = viewRect.height() / sceneRect.height();
    qreal scale = qMin(scaleX, scaleY) * 0.9; // Leave some margin
    
    setZoom(scale);
    
    // Center the view
    m_graphicsView->centerOn(sceneRect.center());
}

void UMLViewWidget::resetZoom()
{
    setZoom(1.0);
}

void UMLViewWidget::setZoom(qreal factor)
{
    m_zoomFactor = qBound(MIN_ZOOM, factor, MAX_ZOOM);
    QTransform transform;
    transform.scale(m_zoomFactor, m_zoomFactor);
    m_graphicsView->setTransform(transform);
    
    m_zoomSlider->blockSignals(true);
    m_zoomSlider->setValue(qRound(m_zoomFactor * 100));
    m_zoomSlider->blockSignals(false);
    
    m_zoomLabel->setText(QString("%1%").arg(qRound(m_zoomFactor * 100)));
}

qreal UMLViewWidget::getZoom() const
{
    return m_zoomFactor;
}

void UMLViewWidget::setShowAttributes(bool show)
{
    m_showAttributes = show;
    // Update all class items
    for (QGraphicsItem* item : m_scene->items()) {
        if (UMLClassItem* classItem = dynamic_cast<UMLClassItem*>(item)) {
            classItem->update();
        }
    }
}

void UMLViewWidget::setShowMethods(bool show)
{
    m_showMethods = show;
    // Update all class items
    for (QGraphicsItem* item : m_scene->items()) {
        if (UMLClassItem* classItem = dynamic_cast<UMLClassItem*>(item)) {
            classItem->update();
        }
    }
}

void UMLViewWidget::setShowVisibility(bool show)
{
    m_showVisibility = show;
    // Update all class items
    for (QGraphicsItem* item : m_scene->items()) {
        if (UMLClassItem* classItem = dynamic_cast<UMLClassItem*>(item)) {
            classItem->update();
        }
    }
}

void UMLViewWidget::setShowStereotypes(bool show)
{
    m_showStereotypes = show;
    // Update all class items
    for (QGraphicsItem* item : m_scene->items()) {
        if (UMLClassItem* classItem = dynamic_cast<UMLClassItem*>(item)) {
            classItem->update();
        }
    }
}

void UMLViewWidget::saveState(QSettings& settings)
{
    settings.beginGroup("UMLViewWidget");
    settings.setValue("splitterSizes", m_splitter->saveState());
    settings.setValue("zoomFactor", m_zoomFactor);
    settings.setValue("layoutAlgorithm", static_cast<int>(m_layoutAlgorithm));
    settings.setValue("diagramType", static_cast<int>(m_diagramType));
    settings.setValue("showAttributes", m_showAttributes);
    settings.setValue("showMethods", m_showMethods);
    settings.setValue("showVisibility", m_showVisibility);
    settings.setValue("showStereotypes", m_showStereotypes);
    settings.setValue("sidebarTab", m_sidebarTabs->currentIndex());
    settings.endGroup();
}

void UMLViewWidget::restoreState(QSettings& settings)
{
    settings.beginGroup("UMLViewWidget");
    
    if (settings.contains("splitterSizes")) {
        m_splitter->restoreState(settings.value("splitterSizes").toByteArray());
    }
    
    m_zoomFactor = settings.value("zoomFactor", 1.0).toDouble();
    setZoom(m_zoomFactor);
    
    m_layoutAlgorithm = static_cast<LayoutAlgorithm>(settings.value("layoutAlgorithm", static_cast<int>(LayoutAlgorithm::Hierarchical)).toInt());
    m_layoutCombo->setCurrentIndex(static_cast<int>(m_layoutAlgorithm));
    
    m_diagramType = static_cast<UMLDiagramType>(settings.value("diagramType", static_cast<int>(UMLDiagramType::ClassDiagram)).toInt());
    m_diagramTypeCombo->setCurrentIndex(static_cast<int>(m_diagramType));
    
    m_showAttributes = settings.value("showAttributes", true).toBool();
    m_showMethods = settings.value("showMethods", true).toBool();
    m_showVisibility = settings.value("showVisibility", true).toBool();
    m_showStereotypes = settings.value("showStereotypes", true).toBool();
    
    m_sidebarTabs->setCurrentIndex(settings.value("sidebarTab", 0).toInt());
    
    settings.endGroup();
}

void UMLViewWidget::refresh()
{
    // Re-parse watched files if any
    if (!m_watchedFiles.isEmpty()) {
        clearParsedData();
        for (const QString& filePath : m_watchedFiles) {
            if (QFile::exists(filePath)) {
                parseSourceFile(filePath);
            }
        }
    }
}

void UMLViewWidget::autoLayout()
{
    applyLayout(m_layoutAlgorithm);
}

void UMLViewWidget::onClassSelected(const QString& classId)
{
    selectClass(classId);
}

void UMLViewWidget::onClassDoubleClicked(const QString& classId)
{
    if (m_classes.contains(classId)) {
        const UMLClass& umlClass = m_classes[classId];
        emit classSelected(classId, umlClass.filePath, umlClass.lineNumber);
    }
}

void UMLViewWidget::onClassTreeItemClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column)
    QString classId = item->data(0, Qt::UserRole).toString();
    if (!classId.isEmpty()) {
        selectClass(classId);
    }
}

void UMLViewWidget::onLayoutChanged(int index)
{
    LayoutAlgorithm algorithm = static_cast<LayoutAlgorithm>(m_layoutCombo->itemData(index).toInt());
    applyLayout(algorithm);
}

void UMLViewWidget::onZoomSliderChanged(int value)
{
    setZoom(value / 100.0);
}

void UMLViewWidget::onFileChanged(const QString& path)
{
    // Delay refresh to avoid multiple refreshes for the same change
    m_refreshTimer->start();
}

void UMLViewWidget::showContextMenu(const QPoint& pos)
{
    QWidget* sender = qobject_cast<QWidget*>(this->sender());
    if (!sender) return;
    
    QMenu menu(this);
    
    if (sender == m_classTree) {
        QTreeWidgetItem* item = m_classTree->itemAt(pos);
        if (item) {
            QString classId = item->data(0, Qt::UserRole).toString();
            menu.addAction(tr("Go to Definition"), this, [this, classId]() {
                if (m_classes.contains(classId)) {
                    const UMLClass& umlClass = m_classes[classId];
                    emit classSelected(classId, umlClass.filePath, umlClass.lineNumber);
                }
            });
            menu.addAction(tr("Remove Class"), this, [this, classId]() {
                removeClass(classId);
            });
        }
    } else if (sender == m_graphicsView) {
        menu.addAction(tr("Auto Layout"), this, &UMLViewWidget::autoLayout);
        menu.addAction(tr("Zoom to Fit"), this, &UMLViewWidget::zoomToFit);
        menu.addAction(tr("Reset Zoom"), this, &UMLViewWidget::resetZoom);
    }
    
    if (!menu.isEmpty()) {
        menu.exec(sender->mapToGlobal(pos));
    }
}

void UMLViewWidget::updateClassTree()
{
    m_classTree->clear();
    
    // Group classes by package
    QMap<QString, QList<UMLClass>> packages;
    for (const UMLClass& umlClass : m_classes) {
        packages[umlClass.package.isEmpty() ? tr("(default)") : umlClass.package].append(umlClass);
    }
    
    for (auto it = packages.begin(); it != packages.end(); ++it) {
        QTreeWidgetItem* packageItem = nullptr;
        if (it.key() != tr("(default)") || packages.size() > 1) {
            packageItem = new QTreeWidgetItem(m_classTree);
            packageItem->setText(0, it.key());
            packageItem->setIcon(0, QIcon::fromTheme("folder"));
        }
        
        for (const UMLClass& umlClass : it.value()) {
            QTreeWidgetItem* classItem = packageItem ? 
                new QTreeWidgetItem(packageItem) : 
                new QTreeWidgetItem(m_classTree);
            
            QString displayName = umlClass.name;
            if (!umlClass.stereotype.isEmpty()) {
                displayName += QString(" «%1»").arg(umlClass.stereotype);
            }
            
            classItem->setText(0, displayName);
            classItem->setIcon(0, QIcon::fromTheme("text-x-generic"));
            classItem->setData(0, Qt::UserRole, umlClass.id);
            
            if (packageItem) {
                packageItem->addChild(classItem);
            }
        }
        
        if (packageItem) {
            packageItem->setExpanded(true);
        }
    }
}

void UMLViewWidget::updateInfoPanel(const QString& classId)
{
    if (!m_classes.contains(classId)) {
        m_classNameLabel->setText(tr("No class selected"));
        m_classFileLabel->setText("");
        m_membersList->clear();
        return;
    }
    
    const UMLClass& umlClass = m_classes[classId];
    
    QString className = umlClass.name;
    if (!umlClass.stereotype.isEmpty()) {
        className += QString(" «%1»").arg(umlClass.stereotype);
    }
    m_classNameLabel->setText(className);
    
    m_classFileLabel->setText(umlClass.filePath.isEmpty() ? "" : 
                             QString("%1:%2").arg(umlClass.filePath).arg(umlClass.lineNumber));
    
    m_membersList->clear();
    
    // Add attributes
    for (const UMLMember& attr : umlClass.attributes) {
        QListWidgetItem* item = new QListWidgetItem(m_membersList);
        QString text = QString("%1 %2: %3")
                      .arg(m_showVisibility ? (attr.visibility == UMLVisibility::Public ? "+" :
                                              attr.visibility == UMLVisibility::Private ? "-" :
                                              attr.visibility == UMLVisibility::Protected ? "#" : "~") : "")
                      .arg(attr.name)
                      .arg(attr.type);
        if (attr.isStatic) {
            text = tr("[static] ") + text;
        }
        item->setText(text);
        item->setIcon(QIcon::fromTheme("field"));
    }
    
    // Add methods
    for (const UMLMember& method : umlClass.methods) {
        QListWidgetItem* item = new QListWidgetItem(m_membersList);
        QString text = QString("%1 %2(%3)")
                      .arg(m_showVisibility ? (method.visibility == UMLVisibility::Public ? "+" :
                                              method.visibility == UMLVisibility::Private ? "-" :
                                              method.visibility == UMLVisibility::Protected ? "#" : "~") : "")
                      .arg(method.name)
                      .arg(method.parameters.join(", "));
        if (!method.returnType.isEmpty() && method.returnType != "void") {
            text += ": " + method.returnType;
        }
        if (method.isStatic) {
            text = tr("[static] ") + text;
        }
        if (method.isAbstract) {
            text = tr("[abstract] ") + text;
        }
        item->setText(text);
        item->setIcon(QIcon::fromTheme("method"));
    }
}

QList<UMLClass> UMLViewWidget::getClasses() const
{
    return m_classes.values();
}

QList<UMLRelation> UMLViewWidget::getRelations() const
{
    return m_relations.values();
}

void UMLViewWidget::parseCppFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    
    QString content = QString::fromUtf8(file.readAll());
    file.close();
    
    QStringList lines = content.split('\n');
    int lineNumber = 0;
    
    QRegularExpression classRegex(R"(class\s+(\w+)(?:\s*:\s*(?:public|private|protected)\s+(\w+))?\s*\{)");
    QRegularExpression structRegex(R"(struct\s+(\w+)\s*\{)");
    QRegularExpression memberRegex(R"(^\s*(public|private|protected)\s*:)");
    QRegularExpression attrRegex(R"(^\s*(?:static\s+)?(?:const\s+)?(\w+(?:<[^>]+>)?(?:\s*\*)?)\s+(\w+)(?:\s*=\s*[^;]+)?\s*;)");
    QRegularExpression methodRegex(R"(^\s*(?:virtual\s+)?(?:static\s+)?(?:(\w+(?:<[^>]+>)?(?:\s*\*)?)\s+)?(\w+)\s*\(([^)]*)\)\s*(?:const)?\s*(?:=\s*0)?\s*;)");
    
    UMLVisibility currentVisibility = UMLVisibility::Private;
    UMLClass* currentClass = nullptr;
    
    for (const QString& line : lines) {
        lineNumber++;
        
        // Check for class/struct definition
        QRegularExpressionMatch classMatch = classRegex.match(line);
        if (classMatch.hasMatch()) {
            if (currentClass) {
                addClass(*currentClass);
                delete currentClass;
            }
            
            currentClass = new UMLClass();
            currentClass->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            currentClass->name = classMatch.captured(1);
            currentClass->filePath = filePath;
            currentClass->lineNumber = lineNumber;
            currentClass->stereotype = "class";
            
            if (classMatch.captured(2).isEmpty()) {
                // Abstract class if has pure virtual methods
                currentClass->stereotype = "class";
            }
            
            currentVisibility = UMLVisibility::Private;
            continue;
        }
        
        QRegularExpressionMatch structMatch = structRegex.match(line);
        if (structMatch.hasMatch()) {
            if (currentClass) {
                addClass(*currentClass);
                delete currentClass;
            }
            
            currentClass = new UMLClass();
            currentClass->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            currentClass->name = structMatch.captured(1);
            currentClass->filePath = filePath;
            currentClass->lineNumber = lineNumber;
            currentClass->stereotype = "struct";
            
            currentVisibility = UMLVisibility::Public;
            continue;
        }
        
        if (!currentClass) continue;
        
        // Check for visibility changes
        QRegularExpressionMatch visibilityMatch = memberRegex.match(line);
        if (visibilityMatch.hasMatch()) {
            QString vis = visibilityMatch.captured(1);
            if (vis == "public") currentVisibility = UMLVisibility::Public;
            else if (vis == "private") currentVisibility = UMLVisibility::Private;
            else if (vis == "protected") currentVisibility = UMLVisibility::Protected;
            continue;
        }
        
        // Check for attributes
        QRegularExpressionMatch attrMatch = attrRegex.match(line);
        if (attrMatch.hasMatch()) {
            UMLMember member;
            member.name = attrMatch.captured(2);
            member.type = attrMatch.captured(1);
            member.visibility = currentVisibility;
            member.isStatic = line.contains("static");
            currentClass->attributes.append(member);
            continue;
        }
        
        // Check for methods
        QRegularExpressionMatch methodMatch = methodRegex.match(line);
        if (methodMatch.hasMatch()) {
            UMLMember member;
            member.name = methodMatch.captured(2);
            member.returnType = methodMatch.captured(1);
            member.visibility = currentVisibility;
            member.isStatic = line.contains("static");
            member.isAbstract = line.contains("= 0");
            member.isVirtual = line.contains("virtual");
            
            QString params = methodMatch.captured(3);
            if (!params.trimmed().isEmpty()) {
                member.parameters = params.split(',', Qt::SkipEmptyParts);
                for (QString& param : member.parameters) {
                    param = param.trimmed();
                }
            }
            
            currentClass->methods.append(member);
            continue;
        }
    }
    
    if (currentClass) {
        addClass(*currentClass);
        delete currentClass;
    }
}

void UMLViewWidget::parsePythonFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    
    QString content = QString::fromUtf8(file.readAll());
    file.close();
    
    QStringList lines = content.split('\n');
    int lineNumber = 0;
    
    QRegularExpression classRegex(R"(^class\s+(\w+)(?:\(([^)]+)\))?\s*:)");
    QRegularExpression methodRegex(R"(^\s*def\s+(\w+)\s*\(([^)]*)\)\s*:)");
    QRegularExpression attrRegex(R"(^\s*self\.(\w+)\s*=\s*[^#]+)");
    
    UMLClass* currentClass = nullptr;
    
    for (const QString& line : lines) {
        lineNumber++;
        
        // Check for class definition
        QRegularExpressionMatch classMatch = classRegex.match(line);
        if (classMatch.hasMatch()) {
            if (currentClass) {
                addClass(*currentClass);
                delete currentClass;
            }
            
            currentClass = new UMLClass();
            currentClass->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            currentClass->name = classMatch.captured(1);
            currentClass->filePath = filePath;
            currentClass->lineNumber = lineNumber;
            currentClass->stereotype = "class";
            
            QString baseClasses = classMatch.captured(2);
            if (!baseClasses.isEmpty()) {
                // Could add inheritance relations here
            }
            
            continue;
        }
        
        if (!currentClass) continue;
        
        // Check for methods
        QRegularExpressionMatch methodMatch = methodRegex.match(line);
        if (methodMatch.hasMatch()) {
            UMLMember member;
            member.name = methodMatch.captured(1);
            member.returnType = ""; // Python doesn't have explicit return types
            member.visibility = methodMatch.captured(1).startsWith('_') ? 
                               UMLVisibility::Private : UMLVisibility::Public;
            
            QString params = methodMatch.captured(2);
            if (!params.trimmed().isEmpty()) {
                member.parameters = params.split(',', Qt::SkipEmptyParts);
                for (QString& param : member.parameters) {
                    param = param.trimmed();
                }
            }
            
            currentClass->methods.append(member);
            continue;
        }
        
        // Check for attributes (simple detection)
        QRegularExpressionMatch attrMatch = attrRegex.match(line);
        if (attrMatch.hasMatch()) {
            UMLMember member;
            member.name = attrMatch.captured(1);
            member.type = ""; // Python is dynamically typed
            member.visibility = member.name.startsWith('_') ? 
                               UMLVisibility::Private : UMLVisibility::Public;
            currentClass->attributes.append(member);
        }
    }
    
    if (currentClass) {
        addClass(*currentClass);
        delete currentClass;
    }
}

void UMLViewWidget::parseJavaFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    
    QString content = QString::fromUtf8(file.readAll());
    file.close();
    
    QStringList lines = content.split('\n');
    int lineNumber = 0;
    
    QRegularExpression classRegex(R"(^(?:public\s+|private\s+|protected\s+)?(?:abstract\s+|final\s+)?class\s+(\w+)(?:\s+extends\s+(\w+))?(?:\s+implements\s+([^}]+))?\s*\{)");
    QRegularExpression interfaceRegex(R"(^(?:public\s+|private\s+|protected\s+)?interface\s+(\w+)(?:\s+extends\s+([^}]+))?\s*\{)");
    QRegularExpression memberRegex(R"(^\s*(public|private|protected)\s+.*)");
    QRegularExpression attrRegex(R"(^\s*(?:public|private|protected)?\s*(?:static\s+)?(?:final\s+)?(\w+(?:<[^>]+>)?(?:\[\])?)\s+(\w+)(?:\s*=\s*[^;]+)?\s*;)");
    QRegularExpression methodRegex(R"(^\s*(?:public|private|protected)?\s*(?:static\s+)?(?:abstract\s+)?(?:(\w+(?:<[^>]+>)?(?:\[\])?)\s+)?(\w+)\s*\(([^)]*)\)\s*(?:throws\s+[^;]+)?\s*;)");
    
    UMLVisibility currentVisibility = UMLVisibility::Package;
    UMLClass* currentClass = nullptr;
    
    for (const QString& line : lines) {
        lineNumber++;
        
        // Check for class definition
        QRegularExpressionMatch classMatch = classRegex.match(line);
        if (classMatch.hasMatch()) {
            if (currentClass) {
                addClass(*currentClass);
                delete currentClass;
            }
            
            currentClass = new UMLClass();
            currentClass->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            currentClass->name = classMatch.captured(2);
            currentClass->filePath = filePath;
            currentClass->lineNumber = lineNumber;
            currentClass->stereotype = line.contains("abstract") ? "abstract" : "class";
            
            currentVisibility = UMLVisibility::Package;
            continue;
        }
        
        // Check for interface definition
        QRegularExpressionMatch interfaceMatch = interfaceRegex.match(line);
        if (interfaceMatch.hasMatch()) {
            if (currentClass) {
                addClass(*currentClass);
                delete currentClass;
            }
            
            currentClass = new UMLClass();
            currentClass->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            currentClass->name = interfaceMatch.captured(2);
            currentClass->filePath = filePath;
            currentClass->lineNumber = lineNumber;
            currentClass->stereotype = "interface";
            
            currentVisibility = UMLVisibility::Public;
            continue;
        }
        
        if (!currentClass) continue;
        
        // Check for visibility changes
        QRegularExpressionMatch visibilityMatch = memberRegex.match(line);
        if (visibilityMatch.hasMatch()) {
            QString vis = visibilityMatch.captured(1);
            if (vis == "public") currentVisibility = UMLVisibility::Public;
            else if (vis == "private") currentVisibility = UMLVisibility::Private;
            else if (vis == "protected") currentVisibility = UMLVisibility::Protected;
            continue;
        }
        
        // Check for attributes
        QRegularExpressionMatch attrMatch = attrRegex.match(line);
        if (attrMatch.hasMatch()) {
            UMLMember member;
            member.name = attrMatch.captured(2);
            member.type = attrMatch.captured(1);
            member.visibility = currentVisibility;
            member.isStatic = line.contains("static");
            member.isConst = line.contains("final");
            currentClass->attributes.append(member);
            continue;
        }
        
        // Check for methods
        QRegularExpressionMatch methodMatch = methodRegex.match(line);
        if (methodMatch.hasMatch()) {
            UMLMember member;
            member.name = methodMatch.captured(2);
            member.returnType = methodMatch.captured(1);
            member.visibility = currentVisibility;
            member.isStatic = line.contains("static");
            member.isAbstract = line.contains("abstract");
            
            QString params = methodMatch.captured(3);
            if (!params.trimmed().isEmpty()) {
                member.parameters = params.split(',', Qt::SkipEmptyParts);
                for (QString& param : member.parameters) {
                    param = param.trimmed();
                }
            }
            
            currentClass->methods.append(member);
            continue;
        }
    }
    
    if (currentClass) {
        addClass(*currentClass);
        delete currentClass;
    }
}

void UMLViewWidget::detectRelations()
{
    // Simple relation detection based on inheritance keywords
    QMap<QString, QString> inheritanceMap;
    
    for (const UMLClass& umlClass : m_classes) {
        // This would need more sophisticated parsing to detect actual inheritance
        // For now, we'll create some basic relations based on naming conventions
    }
    
    // Update relations list
    m_relationsList->clear();
    for (const UMLRelation& relation : m_relations) {
        QListWidgetItem* item = new QListWidgetItem(m_relationsList);
        QString sourceName = m_classes.value(relation.sourceClassId).name;
        QString targetName = m_classes.value(relation.targetClassId).name;
        QString typeStr;
        
        switch (relation.type) {
            case UMLRelationType::Association: typeStr = "→"; break;
            case UMLRelationType::Aggregation: typeStr = "◇→"; break;
            case UMLRelationType::Composition: typeStr = "◆→"; break;
            case UMLRelationType::Inheritance: typeStr = "△"; break;
            case UMLRelationType::Implementation: typeStr = "△"; break;
            default: typeStr = "→"; break;
        }
        
        item->setText(QString("%1 %2 %3").arg(sourceName).arg(typeStr).arg(targetName));
        item->setData(Qt::UserRole, relation.id);
    }
}

QString UMLViewWidget::generatePlantUML() const
{
    QString plantUML = "@startuml\n";
    
    // Add classes
    for (const UMLClass& umlClass : m_classes) {
        if (!umlClass.stereotype.isEmpty()) {
            plantUML += QString("%1 %2 {\n").arg(umlClass.stereotype).arg(umlClass.name);
        } else {
            plantUML += QString("class %1 {\n").arg(umlClass.name);
        }
        
        // Add attributes
        for (const UMLMember& attr : umlClass.attributes) {
            QString vis;
            switch (attr.visibility) {
                case UMLVisibility::Public: vis = "+"; break;
                case UMLVisibility::Private: vis = "-"; break;
                case UMLVisibility::Protected: vis = "#"; break;
                case UMLVisibility::Package: vis = "~"; break;
            }
            
            QString attrStr = QString("%1 %2: %3").arg(vis).arg(attr.name).arg(attr.type);
            if (attr.isStatic) attrStr = "{static} " + attrStr;
            plantUML += QString("  %1\n").arg(attrStr);
        }
        
        // Add methods
        for (const UMLMember& method : umlClass.methods) {
            QString vis;
            switch (method.visibility) {
                case UMLVisibility::Public: vis = "+"; break;
                case UMLVisibility::Private: vis = "-"; break;
                case UMLVisibility::Protected: vis = "#"; break;
                case UMLVisibility::Package: vis = "~"; break;
            }
            
            QString methodStr = QString("%1 %2(%3)").arg(vis).arg(method.name).arg(method.parameters.join(", "));
            if (!method.returnType.isEmpty()) {
                methodStr += ": " + method.returnType;
            }
            if (method.isStatic) methodStr = "{static} " + methodStr;
            if (method.isAbstract) methodStr = "{abstract} " + methodStr;
            
            plantUML += QString("  %1\n").arg(methodStr);
        }
        
        plantUML += "}\n\n";
    }
    
    // Add relations
    for (const UMLRelation& relation : m_relations) {
        QString sourceName = m_classes.value(relation.sourceClassId).name;
        QString targetName = m_classes.value(relation.targetClassId).name;
        
        QString relationStr;
        switch (relation.type) {
            case UMLRelationType::Association:
                relationStr = QString("%1 --> %2").arg(sourceName).arg(targetName);
                break;
            case UMLRelationType::Aggregation:
                relationStr = QString("%1 o-- %2").arg(sourceName).arg(targetName);
                break;
            case UMLRelationType::Composition:
                relationStr = QString("%1 *-- %2").arg(sourceName).arg(targetName);
                break;
            case UMLRelationType::Inheritance:
                relationStr = QString("%1 <|-- %2").arg(sourceName).arg(targetName);
                break;
            case UMLRelationType::Implementation:
                relationStr = QString("%1 <|.. %2").arg(sourceName).arg(targetName);
                break;
            default:
                relationStr = QString("%1 --> %2").arg(sourceName).arg(targetName);
                break;
        }
        
        if (!relation.label.isEmpty()) {
            relationStr += QString(" : %1").arg(relation.label);
        }
        
        plantUML += relationStr + "\n";
    }
    
    plantUML += "@enduml\n";
    return plantUML;
}
