#include "ComponentFactory.h"

// Safe includes - these will only be loaded when factory methods are called
// This prevents static initialization issues
#include <QDebug>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>

// Agent system headers - properly included for real implementations
#include "../agent/meta_planner.hpp"
#include "../agent/action_executor.hpp"
#include "../agent/model_invoker.hpp"
#include "../agent/hot_reload.hpp"
// Note: AutoBootstrap uses singleton pattern, included conditionally
// #include "../agent/auto_bootstrap.hpp"

InferenceEngine* ComponentFactory::createInferenceEngine(const QString& ggufPath, QObject* parent)
{
    // STUBBED FOR DEBUGGING
    // return nullptr;
    /*
    try {
        qDebug() << "ComponentFactory: Creating InferenceEngine...";
        auto engine = new InferenceEngine(parent);
        qDebug() << "ComponentFactory: InferenceEngine created successfully";
        return engine;
    } catch (const std::exception& e) {
        qCritical() << "ComponentFactory: Failed to create InferenceEngine:" << e.what();
        return nullptr;
    }
    */
    // Use actual implementation instead of stub
    try {
        qDebug() << "ComponentFactory: Creating InferenceEngine...";
        auto engine = new InferenceEngine(parent);
        if (!ggufPath.isEmpty()) {
            engine->loadModel(ggufPath);
        }
        qDebug() << "ComponentFactory: InferenceEngine created successfully";
        return engine;
    } catch (const std::exception& e) {
        qCritical() << "ComponentFactory: Failed to create InferenceEngine:" << e.what();
        return nullptr;
    }
}

GGUFServer* ComponentFactory::createGGUFServer(InferenceEngine* engine, QObject* parent)
{
    // STUBBED FOR DEBUGGING
    // return nullptr;
    /*
    try {
        qDebug() << "ComponentFactory: Creating GGUFServer...";
        auto server = new GGUFServer(engine, parent);
        qDebug() << "ComponentFactory: GGUFServer created successfully";
        return server;
    } catch (const std::exception& e) {
        qCritical() << "ComponentFactory: Failed to create GGUFServer:" << e.what();
        return nullptr;
    }
    */
    // Use actual implementation instead of stub
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
    // Production implementation - create real StreamingInference
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
    // Production implementation - create real CommandPalette
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
    // Production implementation - create real AIChatPanel
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
    // Production implementation - create real LayerQuantWidget
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
    // Production implementation - create real ModelMonitor
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
    // MetaPlanner is not QObject-derived, return nullptr
    // Callers should use MetaPlanner directly
    Q_UNUSED(planner);
    return nullptr;
}

QObject* ComponentFactory::asQObject(ActionExecutor* executor) {
    // ActionExecutor inherits QObject - safe to cast
    return qobject_cast<QObject*>(executor);
}

QObject* ComponentFactory::asQObject(ExecutionContext* context) {
    // ExecutionContext is a struct, not QObject-derived
    Q_UNUSED(context);
    return nullptr;
}

// Agent system factory methods - real production implementations
MetaPlanner* ComponentFactory::createMetaPlanner(QObject* parent)
{
    try {
        qDebug() << "ComponentFactory: Creating MetaPlanner...";
        // Create real MetaPlanner instance - it's a standalone class, not QObject-derived
        auto planner = new MetaPlanner();
        Q_UNUSED(parent); // MetaPlanner doesn't inherit QObject
        qDebug() << "ComponentFactory: MetaPlanner created successfully";
        return planner;
    } catch (const std::exception& e) {
        qCritical() << "ComponentFactory: Failed to create MetaPlanner:" << e.what();
        return nullptr;
    }
}

ActionExecutor* ComponentFactory::createActionExecutor(QObject* parent)
{
    try {
        qDebug() << "ComponentFactory: Creating ActionExecutor...";
        // Create real ActionExecutor - this is QObject-derived
        auto executor = new ActionExecutor(parent);
        qDebug() << "ComponentFactory: ActionExecutor created successfully";
        return executor;
    } catch (const std::exception& e) {
        qCritical() << "ComponentFactory: Failed to create ActionExecutor:" << e.what();
        return nullptr;
    }
}

ExecutionContext* ComponentFactory::createExecutionContext(QObject* parent)
{
    try {
        qDebug() << "ComponentFactory: Creating ExecutionContext...";
        // ExecutionContext is a struct - create it on the heap
        auto context = new ExecutionContext();
        Q_UNUSED(parent); // ExecutionContext is a struct, not QObject-derived
        context->projectRoot = QDir::currentPath();
        context->timeoutMs = 30000;
        context->dryRun = false;
        qDebug() << "ComponentFactory: ExecutionContext created successfully";
        return context;
    } catch (const std::exception& e) {
        qCritical() << "ComponentFactory: Failed to create ExecutionContext:" << e.what();
        return nullptr;
    }
}

AutoBootstrap* ComponentFactory::createAutoBootstrap(QObject* parent)
{
    try {
        qDebug() << "ComponentFactory: Creating AutoBootstrap...";
        // AutoBootstrap uses singleton pattern, so use instance() method instead of constructor
        // auto bootstrap = AutoBootstrap::instance();
        // if (bootstrap && parent) {
        //     bootstrap->setParent(parent);
        // }
        qDebug() << "ComponentFactory: AutoBootstrap created successfully (MOCKED)";
        return nullptr; // bootstrap;
    } catch (const std::exception& e) {
        qCritical() << "ComponentFactory: Failed to create AutoBootstrap:" << e.what();
        return nullptr;
    }
}

HotReload* ComponentFactory::createHotReload(QObject* parent)
{
    try {
        qDebug() << "ComponentFactory: Creating HotReload...";
        // Create real HotReload - this is QObject-derived
        auto hotreload = new HotReload(parent);
        qDebug() << "ComponentFactory: HotReload created successfully";
        return hotreload;
    } catch (const std::exception& e) {
        qCritical() << "ComponentFactory: Failed to create HotReload:" << e.what();
        return nullptr;
    }
}

ModelInvoker* ComponentFactory::createModelInvoker(QObject* parent)
{
    try {
        qDebug() << "ComponentFactory: Creating ModelInvoker...";
        // Create real ModelInvoker - this is QObject-derived
        auto invoker = new ModelInvoker(parent);
        qDebug() << "ComponentFactory: ModelInvoker created successfully";
        return invoker;
    } catch (const std::exception& e) {
        qCritical() << "ComponentFactory: Failed to create ModelInvoker:" << e.what();
        return nullptr;
    }
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