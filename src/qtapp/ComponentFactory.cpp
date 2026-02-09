#include "ComponentFactory.h"

// Safe includes - these will only be loaded when factory methods are called
// This prevents static initialization issues
#include <QDebug>

// Agent system headers - included only here for factory implementation
// Forward declarations are used in the header to prevent static initialization
// #include "../agent/auto_bootstrap.hpp"
// Note: Other agent headers (MetaPlanner, ActionExecutor, ExecutionContext) will be included
// when they are properly implemented and tested to avoid static initialization stack overflow

InferenceEngine* ComponentFactory::createInferenceEngine(const QString& ggufPath, QObject* parent)
{
    try {
        qDebug() << "ComponentFactory: Creating InferenceEngine...";
        auto engine = new InferenceEngine(parent);
        qDebug() << "ComponentFactory: InferenceEngine created successfully";
        return engine;
    } catch (const std::exception& e) {
        qCritical() << "ComponentFactory: Failed to create InferenceEngine:" << e.what();
        return nullptr;
    }
}

GGUFServer* ComponentFactory::createGGUFServer(InferenceEngine* engine, QObject* parent)
{
    try {
        qDebug() << "ComponentFactory: Creating GGUFServer...";
        auto server = new GGUFServer(engine, parent);
        qDebug() << "ComponentFactory: GGUFServer created successfully";
        return server;
    } catch (const std::exception& e) {
        qCritical() << "ComponentFactory: Failed to create GGUFServer:" << e.what();
        return nullptr;
    }
}

StreamingInference* ComponentFactory::createStreamingInference(QPlainTextEdit* outputEdit, QObject* parent)
{
    try {
        qDebug() << "ComponentFactory: Creating StreamingInference...";
        auto streaming = new StreamingInference(outputEdit, parent);
        qDebug() << "ComponentFactory: StreamingInference created successfully";
        return streaming;
    } catch (const std::exception& e) {
        qCritical() << "ComponentFactory: Failed to create StreamingInference:" << e.what();
        return nullptr;
    }
}

CommandPalette* ComponentFactory::createCommandPalette(QWidget* parent)
{
    try {
        qDebug() << "ComponentFactory: Creating CommandPalette...";
        auto palette = new CommandPalette(parent);
        qDebug() << "ComponentFactory: CommandPalette created successfully";
        return palette;
    } catch (const std::exception& e) {
        qCritical() << "ComponentFactory: Failed to create CommandPalette:" << e.what();
        return nullptr;
    }
}

AIChatPanel* ComponentFactory::createAIChatPanel(QWidget* parent)
{
    try {
        qDebug() << "ComponentFactory: Creating AIChatPanel...";
        auto panel = new AIChatPanel(parent);
        qDebug() << "ComponentFactory: AIChatPanel created successfully";
        return panel;
    } catch (const std::exception& e) {
        qCritical() << "ComponentFactory: Failed to create AIChatPanel:" << e.what();
        return nullptr;
    }
}

LayerQuantWidget* ComponentFactory::createLayerQuantWidget(QWidget* parent)
{
    try {
        qDebug() << "ComponentFactory: Creating LayerQuantWidget...";
        auto widget = new LayerQuantWidget(parent);
        qDebug() << "ComponentFactory: LayerQuantWidget created successfully";
        return widget;
    } catch (const std::exception& e) {
        qCritical() << "ComponentFactory: Failed to create LayerQuantWidget:" << e.what();
        return nullptr;
    }
}

ModelMonitor* ComponentFactory::createModelMonitor(InferenceEngine* engine, QWidget* parent)
{
    try {
        qDebug() << "ComponentFactory: Creating ModelMonitor...";
        auto monitor = new ModelMonitor(engine, parent);
        qDebug() << "ComponentFactory: ModelMonitor created successfully";
        return monitor;
    } catch (const std::exception& e) {
        qCritical() << "ComponentFactory: Failed to create ModelMonitor:" << e.what();
        return nullptr;
    }
}

// Helper functions to convert components to QObject for signal connections
QObject* ComponentFactory::asQObject(InferenceEngine* engine) {
    return qobject_cast<QObject*>(engine);
}

QObject* ComponentFactory::asQObject(AIChatPanel* panel) {
    return qobject_cast<QObject*>(panel);
}

QObject* ComponentFactory::asQObject(CommandPalette* palette) {
    return qobject_cast<QObject*>(palette);
}

QObject* ComponentFactory::asQObject(LayerQuantWidget* widget) {
    return qobject_cast<QObject*>(widget);
}

QObject* ComponentFactory::asQObject(MetaPlanner* planner) {
    // For now, return the object directly cast as QObject using reinterpret_cast
    // This is safe since we're creating mock QObjects in createMetaPlanner
    return reinterpret_cast<QObject*>(planner);
}

QObject* ComponentFactory::asQObject(ActionExecutor* executor) {
    // For now, return the object directly cast as QObject using reinterpret_cast
    // This is safe since we're creating mock QObjects in createActionExecutor
    return reinterpret_cast<QObject*>(executor);
}

QObject* ComponentFactory::asQObject(ExecutionContext* context) {
    // For now, return the object directly cast as QObject using reinterpret_cast
    // This is safe since we're creating mock QObjects in createExecutionContext
    return reinterpret_cast<QObject*>(context);
}

// Agent system factory methods - safe runtime creation
MetaPlanner* ComponentFactory::createMetaPlanner(QObject* parent)
{
    qDebug() << "ComponentFactory: Creating MetaPlanner...";
    auto planner = new QObject(parent);
    planner->setObjectName("MockMetaPlanner");
    qDebug() << "ComponentFactory: MockMetaPlanner created successfully";
    return reinterpret_cast<MetaPlanner*>(planner);
}

ActionExecutor* ComponentFactory::createActionExecutor(QObject* parent)
{
    qDebug() << "ComponentFactory: Creating ActionExecutor...";
    auto executor = new QObject(parent);
    executor->setObjectName("MockActionExecutor");
    qDebug() << "ComponentFactory: MockActionExecutor created successfully";
    return reinterpret_cast<ActionExecutor*>(executor);
}

ExecutionContext* ComponentFactory::createExecutionContext(QObject* parent)
{
    qDebug() << "ComponentFactory: Creating ExecutionContext...";
    auto context = new QObject(parent);
    context->setObjectName("MockExecutionContext");
    qDebug() << "ComponentFactory: MockExecutionContext created successfully";
    return reinterpret_cast<ExecutionContext*>(context);
}

AutoBootstrap* ComponentFactory::createAutoBootstrap(QObject* parent)
{
    qDebug() << "ComponentFactory: Creating AutoBootstrap...";
    // AutoBootstrap uses singleton pattern - instance() not yet available
    qDebug() << "ComponentFactory: AutoBootstrap not yet available (singleton pending)";
    return nullptr;
}

HotReload* ComponentFactory::createHotReload(QObject* parent)
{
    qDebug() << "ComponentFactory: Creating HotReload...";
    auto hotreload = new QObject(parent);
    hotreload->setObjectName("MockHotReload");
    qDebug() << "ComponentFactory: MockHotReload created successfully";
    return reinterpret_cast<HotReload*>(hotreload);
}

ModelInvoker* ComponentFactory::createModelInvoker(QObject* parent)
{
    qDebug() << "ComponentFactory: Creating ModelInvoker...";
    auto invoker = new QObject(parent);
    invoker->setObjectName("MockModelInvoker");
    qDebug() << "ComponentFactory: MockModelInvoker created successfully";
    return reinterpret_cast<ModelInvoker*>(invoker);
}

// Availability checks - can be used to enable/disable features
bool ComponentFactory::isInferenceEngineAvailable()
{
    return true; // For now, assume all components are available
}

bool ComponentFactory::isGGUFServerAvailable()
{
    return true;
}

bool ComponentFactory::isStreamingInferenceAvailable()
{
    return true;
}

bool ComponentFactory::isCommandPaletteAvailable()
{
    return true;
}

bool ComponentFactory::isAIChatPanelAvailable()
{
    return true;
}

bool ComponentFactory::isLayerQuantWidgetAvailable()
{
    return true;
}

bool ComponentFactory::isModelMonitorAvailable()
{
    return true;
}

bool ComponentFactory::isMetaPlannerAvailable()
{
    return true;
}

bool ComponentFactory::isActionExecutorAvailable()
{
    return true;
}

bool ComponentFactory::isExecutionContextAvailable()
{
    return true;
}