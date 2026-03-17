/**
 * @file uml_view_widget.h
 * @brief UML diagram viewer and generator from source code
 * 
 * Production-ready implementation providing:
 * - Class diagram generation from C++/Python/Java code
 * - Sequence diagram creation
 * - Interactive diagram editing
 * - Multiple layout algorithms
 * - Export to PNG, SVG, PlantUML
 * - Real-time code synchronization
 */

#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QGraphicsPolygonItem>
#include <QToolBar>
#include <QToolButton>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QLabel>
#include <QSplitter>
#include <QTreeWidget>
#include <QListWidget>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QFileSystemWatcher>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QSet>
#include <QVector>
#include <memory>
#include <vector>
#include <functional>

/**
 * @brief UML diagram types
 */
enum class UMLDiagramType {
    ClassDiagram,
    SequenceDiagram,
    ActivityDiagram,
    UseCaseDiagram,
    StateDiagram,
    ComponentDiagram
};

/**
 * @brief UML relationship types
 */
enum class UMLRelationType {
    Association,
    Aggregation,
    Composition,
    Inheritance,
    Implementation,
    Dependency,
    Realization
};

/**
 * @brief Visibility modifiers
 */
enum class UMLVisibility {
    Public,     // +
    Private,    // -
    Protected,  // #
    Package     // ~
};

/**
 * @brief Represents a class member (field or method)
 */
struct UMLMember {
    QString name;
    QString type;
    QString returnType;  // For methods
    QStringList parameters;  // For methods
    UMLVisibility visibility;
    bool isStatic;
    bool isAbstract;
    bool isVirtual;
    bool isConst;
};

/**
 * @brief Represents a UML class
 */
struct UMLClass {
    QString id;
    QString name;
    QString stereotype;  // e.g., "interface", "abstract", "enum"
    QString package;
    QString filePath;
    int lineNumber;
    
    QList<UMLMember> attributes;
    QList<UMLMember> methods;
    
    QPointF position;
    QSizeF size;
    bool isSelected;
};

/**
 * @brief Represents a UML relationship
 */
struct UMLRelation {
    QString id;
    QString sourceClassId;
    QString targetClassId;
    UMLRelationType type;
    QString label;
    QString sourceMultiplicity;
    QString targetMultiplicity;
    QList<QPointF> waypoints;
};

/**
 * @brief Layout algorithm types
 */
enum class LayoutAlgorithm {
    Hierarchical,
    ForceDirected,
    Orthogonal,
    Circular,
    Tree,
    Manual
};

/**
 * @brief Graphics item for UML class boxes
 */
class UMLClassItem : public QGraphicsRectItem {
public:
    explicit UMLClassItem(const UMLClass& umlClass, QGraphicsItem* parent = nullptr);
    
    void updateFromClass(const UMLClass& umlClass);
    QString classId() const { return m_classId; }
    UMLClass getClass() const;
    
    void setHighlighted(bool highlighted);
    bool isHighlighted() const { return m_highlighted; }
    
    // QGraphicsItem interface
    enum { Type = UserType + 1 };
    int type() const override { return Type; }

protected:
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    void calculateSize();
    QString visibilitySymbol(UMLVisibility vis) const;
    
    QString m_classId;
    QString m_name;
    QString m_stereotype;
    QList<UMLMember> m_attributes;
    QList<UMLMember> m_methods;
    bool m_highlighted;
    
    static constexpr int HEADER_HEIGHT = 30;
    static constexpr int ROW_HEIGHT = 18;
    static constexpr int MIN_WIDTH = 150;
    static constexpr int PADDING = 8;
};

/**
 * @brief Graphics item for UML relationships
 */
class UMLRelationItem : public QGraphicsPathItem {
public:
    explicit UMLRelationItem(const UMLRelation& relation, 
                            UMLClassItem* source, 
                            UMLClassItem* target,
                            QGraphicsItem* parent = nullptr);
    
    void updatePath();
    QString relationId() const { return m_relationId; }
    UMLRelation getRelation() const;
    
    UMLClassItem* sourceItem() const { return m_sourceItem; }
    UMLClassItem* targetItem() const { return m_targetItem; }
    
    enum { Type = UserType + 2 };
    int type() const override { return Type; }

protected:
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    void createPath();
    void drawArrowHead(QPainter* painter, const QPointF& tip, const QPointF& from);
    void drawDiamond(QPainter* painter, const QPointF& tip, const QPointF& from, bool filled);
    QPointF calculateEdgePoint(const QPointF& from, const QPointF& to, const QRectF& rect);
    
    QString m_relationId;
    UMLRelationType m_type;
    QString m_label;
    QString m_sourceMultiplicity;
    QString m_targetMultiplicity;
    
    UMLClassItem* m_sourceItem;
    UMLClassItem* m_targetItem;
};

/**
 * @brief Custom graphics scene for UML diagrams
 */
class UMLDiagramScene : public QGraphicsScene {
    Q_OBJECT
    
public:
    explicit UMLDiagramScene(QObject* parent = nullptr);
    
    void addClass(const UMLClass& umlClass);
    void removeClass(const QString& classId);
    void updateClass(const UMLClass& umlClass);
    UMLClassItem* findClassItem(const QString& classId);
    
    void addRelation(const UMLRelation& relation);
    void removeRelation(const QString& relationId);
    void updateRelations();
    
    void clear();
    void applyLayout(LayoutAlgorithm algorithm);
    
    QList<UMLClass> getAllClasses() const;
    QList<UMLRelation> getAllRelations() const;

signals:
    void classSelected(const QString& classId);
    void classDoubleClicked(const QString& classId);
    void classPositionChanged(const QString& classId, const QPointF& pos);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

private:
    void layoutHierarchical();
    void layoutForceDirected();
    void layoutOrthogonal();
    void layoutCircular();
    
    QMap<QString, UMLClassItem*> m_classItems;
    QMap<QString, UMLRelationItem*> m_relationItems;
};

/**
 * @class UMLViewWidget
 * @brief Full-featured UML diagram viewer and generator
 * 
 * Features:
 * - Parse source code to generate class diagrams
 * - Interactive diagram editing
 * - Multiple layout algorithms
 * - Export to various formats
 * - Real-time synchronization with code
 */
class UMLViewWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit UMLViewWidget(QWidget* parent = nullptr);
    ~UMLViewWidget() override;
    
    // Diagram management
    void newDiagram(UMLDiagramType type);
    void loadDiagram(const QString& filePath);
    void saveDiagram(const QString& filePath);
    void exportDiagram(const QString& filePath, const QString& format);
    
    // Code parsing
    void parseSourceFile(const QString& filePath);
    void parseSourceDirectory(const QString& dirPath, bool recursive = true);
    void clearParsedData();
    
    // Class management
    void addClass(const UMLClass& umlClass);
    void removeClass(const QString& classId);
    void updateClass(const UMLClass& umlClass);
    void selectClass(const QString& classId);
    QList<UMLClass> getClasses() const;
    
    // Relation management
    void addRelation(const UMLRelation& relation);
    void removeRelation(const QString& relationId);
    QList<UMLRelation> getRelations() const;
    
    // Layout
    void applyLayout(LayoutAlgorithm algorithm);
    void setLayoutAlgorithm(LayoutAlgorithm algorithm);
    LayoutAlgorithm getLayoutAlgorithm() const;
    
    // View control
    void zoomIn();
    void zoomOut();
    void zoomToFit();
    void resetZoom();
    void setZoom(qreal factor);
    qreal getZoom() const;
    
    // Settings
    void setShowAttributes(bool show);
    void setShowMethods(bool show);
    void setShowVisibility(bool show);
    void setShowStereotypes(bool show);
    
    // State
    void saveState(QSettings& settings);
    void restoreState(QSettings& settings);

public slots:
    void refresh();
    void autoLayout();

signals:
    void diagramChanged();
    void classSelected(const QString& classId, const QString& filePath, int line);
    void relationSelected(const QString& relationId);
    void exportCompleted(const QString& filePath, bool success);
    void parseCompleted(int classCount, int relationCount);
    void parseError(const QString& error);

private slots:
    void onClassSelected(const QString& classId);
    void onClassDoubleClicked(const QString& classId);
    void onClassTreeItemClicked(QTreeWidgetItem* item, int column);
    void onLayoutChanged(int index);
    void onZoomSliderChanged(int value);
    void onFileChanged(const QString& path);
    void showContextMenu(const QPoint& pos);

private:
    void setupUI();
    void setupToolbar();
    void setupSidebar();
    void setupDiagramView();
    void setupConnections();
    
    void updateClassTree();
    void updateInfoPanel(const QString& classId);
    
    // Code parsing
    void parseCppFile(const QString& filePath);
    void parsePythonFile(const QString& filePath);
    void parseJavaFile(const QString& filePath);
    void detectRelations();
    
    QString generatePlantUML() const;
    QByteArray renderToImage(const QString& format) const;
    
    // UI Components
    QVBoxLayout* m_mainLayout;
    QToolBar* m_toolbar;
    QSplitter* m_splitter;
    
    // Toolbar actions
    QAction* m_newAction;
    QAction* m_openAction;
    QAction* m_saveAction;
    QAction* m_exportAction;
    QAction* m_parseAction;
    QAction* m_refreshAction;
    QAction* m_zoomInAction;
    QAction* m_zoomOutAction;
    QAction* m_zoomFitAction;
    QAction* m_autoLayoutAction;
    
    // Toolbar widgets
    QComboBox* m_layoutCombo;
    QComboBox* m_diagramTypeCombo;
    QSlider* m_zoomSlider;
    QLabel* m_zoomLabel;
    
    // Sidebar
    QTabWidget* m_sidebarTabs;
    QTreeWidget* m_classTree;
    QListWidget* m_relationsList;
    QWidget* m_infoPanel;
    QLabel* m_classNameLabel;
    QLabel* m_classFileLabel;
    QListWidget* m_membersList;
    
    // Diagram view
    QGraphicsView* m_graphicsView;
    UMLDiagramScene* m_scene;
    
    // Data
    QMap<QString, UMLClass> m_classes;
    QMap<QString, UMLRelation> m_relations;
    UMLDiagramType m_diagramType;
    LayoutAlgorithm m_layoutAlgorithm;
    
    // File watching
    QFileSystemWatcher* m_fileWatcher;
    QStringList m_watchedFiles;
    QTimer* m_refreshTimer;
    
    // View state
    qreal m_zoomFactor;
    bool m_showAttributes;
    bool m_showMethods;
    bool m_showVisibility;
    bool m_showStereotypes;
    QString m_currentDiagramPath;
    QString m_selectedClassId;
    
    // Constants
    static constexpr qreal MIN_ZOOM = 0.1;
    static constexpr qreal MAX_ZOOM = 4.0;
    static constexpr qreal ZOOM_STEP = 0.1;
};



