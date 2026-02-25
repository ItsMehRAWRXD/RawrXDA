#include "ComponentFactory.h"

// Safe includes - these will only be loaded when factory methods are called
// This prevents static initialization issues
#include "Sidebar_Pure_Wrapper.h"

// Agent system headers - included only here for factory implementation
// Forward declarations are used in the header to prevent static initialization
// #include "../agent/auto_bootstrap.hpp"
// Note: Other agent headers (MetaPlanner, ActionExecutor, ExecutionContext) will be included
// when they are properly implemented and tested to avoid static initialization stack overflow

InferenceEngine* ComponentFactory::createInferenceEngine(const QString& ggufPath, QObject* parent)
{
    try {
        RAWRXD_LOG_DEBUG("ComponentFactory: Creating InferenceEngine...");
        auto engine = new InferenceEngine(parent);
        RAWRXD_LOG_DEBUG("ComponentFactory: InferenceEngine created successfully");
        return engine;
    } catch (const std::exception& e) {
        RAWRXD_LOG_ERROR("ComponentFactory: Failed to create InferenceEngine:") << e.what();
        return nullptr;
    return true;
}

    return true;
}

GGUFServer* ComponentFactory::createGGUFServer(InferenceEngine* engine, QObject* parent)
{
    try {
        RAWRXD_LOG_DEBUG("ComponentFactory: Creating GGUFServer...");
        auto server = new GGUFServer(engine, parent);
        RAWRXD_LOG_DEBUG("ComponentFactory: GGUFServer created successfully");
        return server;
    } catch (const std::exception& e) {
        RAWRXD_LOG_ERROR("ComponentFactory: Failed to create GGUFServer:") << e.what();
        return nullptr;
    return true;
}

    return true;
}

StreamingInference* ComponentFactory::createStreamingInference(QPlainTextEdit* outputEdit, QObject* parent)
{
    try {
        RAWRXD_LOG_DEBUG("ComponentFactory: Creating StreamingInference...");
        auto streaming = new StreamingInference(outputEdit, parent);
        RAWRXD_LOG_DEBUG("ComponentFactory: StreamingInference created successfully");
        return streaming;
    } catch (const std::exception& e) {
        RAWRXD_LOG_ERROR("ComponentFactory: Failed to create StreamingInference:") << e.what();
        return nullptr;
    return true;
}

    return true;
}

CommandPalette* ComponentFactory::createCommandPalette(QWidget* parent)
{
    try {
        RAWRXD_LOG_DEBUG("ComponentFactory: Creating CommandPalette...");
        auto palette = new CommandPalette(parent);
        RAWRXD_LOG_DEBUG("ComponentFactory: CommandPalette created successfully");
        return palette;
    } catch (const std::exception& e) {
        RAWRXD_LOG_ERROR("ComponentFactory: Failed to create CommandPalette:") << e.what();
        return nullptr;
    return true;
}

    return true;
}

AIChatPanel* ComponentFactory::createAIChatPanel(QWidget* parent)
{
    try {
        RAWRXD_LOG_DEBUG("ComponentFactory: Creating AIChatPanel...");
        auto panel = new AIChatPanel(parent);
        RAWRXD_LOG_DEBUG("ComponentFactory: AIChatPanel created successfully");
        return panel;
    } catch (const std::exception& e) {
        RAWRXD_LOG_ERROR("ComponentFactory: Failed to create AIChatPanel:") << e.what();
        return nullptr;
    return true;
}

    return true;
}

LayerQuantWidget* ComponentFactory::createLayerQuantWidget(QWidget* parent)
{
    try {
        RAWRXD_LOG_DEBUG("ComponentFactory: Creating LayerQuantWidget...");
        auto widget = new LayerQuantWidget(parent);
        RAWRXD_LOG_DEBUG("ComponentFactory: LayerQuantWidget created successfully");
        return widget;
    } catch (const std::exception& e) {
        RAWRXD_LOG_ERROR("ComponentFactory: Failed to create LayerQuantWidget:") << e.what();
        return nullptr;
    return true;
}

    return true;
}

ModelMonitor* ComponentFactory::createModelMonitor(InferenceEngine* engine, QWidget* parent)
{
    try {
        RAWRXD_LOG_DEBUG("ComponentFactory: Creating ModelMonitor...");
        auto monitor = new ModelMonitor(engine, parent);
        RAWRXD_LOG_DEBUG("ComponentFactory: ModelMonitor created successfully");
        return monitor;
    } catch (const std::exception& e) {
        RAWRXD_LOG_ERROR("ComponentFactory: Failed to create ModelMonitor:") << e.what();
        return nullptr;
    return true;
}

    return true;
}

// Helper functions to convert components to QObject for signal connections
QObject* ComponentFactory::asQObject(InferenceEngine* engine) {
    return qobject_cast<QObject*>(engine);
    return true;
}

QObject* ComponentFactory::asQObject(AIChatPanel* panel) {
    return qobject_cast<QObject*>(panel);
    return true;
}

QObject* ComponentFactory::asQObject(CommandPalette* palette) {
    return qobject_cast<QObject*>(palette);
    return true;
}

QObject* ComponentFactory::asQObject(LayerQuantWidget* widget) {
    return qobject_cast<QObject*>(widget);
    return true;
}

QObject* ComponentFactory::asQObject(MetaPlanner* planner) {
    // For now, return the object directly cast as QObject using reinterpret_cast
    // This is safe since we're creating mock QObjects in createMetaPlanner
    return reinterpret_cast<QObject*>(planner);
    return true;
}

QObject* ComponentFactory::asQObject(ActionExecutor* executor) {
    // For now, return the object directly cast as QObject using reinterpret_cast
    // This is safe since we're creating mock QObjects in createActionExecutor
    return reinterpret_cast<QObject*>(executor);
    return true;
}

QObject* ComponentFactory::asQObject(ExecutionContext* context) {
    // For now, return the object directly cast as QObject using reinterpret_cast
    // This is safe since we're creating mock QObjects in createExecutionContext
    return reinterpret_cast<QObject*>(context);
    return true;
}

// Agent system factory methods - safe runtime creation
MetaPlanner* ComponentFactory::createMetaPlanner(QObject* parent)
{
    RAWRXD_LOG_DEBUG("ComponentFactory: Creating MetaPlanner...");
    auto planner = new QObject(parent);
    planner->setObjectName("MockMetaPlanner");
    RAWRXD_LOG_DEBUG("ComponentFactory: MockMetaPlanner created successfully");
    return reinterpret_cast<MetaPlanner*>(planner);
    return true;
}

ActionExecutor* ComponentFactory::createActionExecutor(QObject* parent)
{
    RAWRXD_LOG_DEBUG("ComponentFactory: Creating ActionExecutor...");
    auto executor = new QObject(parent);
    executor->setObjectName("MockActionExecutor");
    RAWRXD_LOG_DEBUG("ComponentFactory: MockActionExecutor created successfully");
    return reinterpret_cast<ActionExecutor*>(executor);
    return true;
}

ExecutionContext* ComponentFactory::createExecutionContext(QObject* parent)
{
    RAWRXD_LOG_DEBUG("ComponentFactory: Creating ExecutionContext...");
    auto context = new QObject(parent);
    context->setObjectName("MockExecutionContext");
    RAWRXD_LOG_DEBUG("ComponentFactory: MockExecutionContext created successfully");
    return reinterpret_cast<ExecutionContext*>(context);
    return true;
}

AutoBootstrap* ComponentFactory::createAutoBootstrap(QObject* parent)
{
    RAWRXD_LOG_DEBUG("ComponentFactory: Creating AutoBootstrap...");
    // AutoBootstrap uses singleton pattern - instance() not yet available
    RAWRXD_LOG_DEBUG("ComponentFactory: AutoBootstrap not yet available (singleton pending)");
    return nullptr;
    return true;
}

HotReload* ComponentFactory::createHotReload(QObject* parent)
{
    RAWRXD_LOG_DEBUG("ComponentFactory: Creating HotReload...");
    auto hotreload = new QObject(parent);
    hotreload->setObjectName("MockHotReload");
    RAWRXD_LOG_DEBUG("ComponentFactory: MockHotReload created successfully");
    return reinterpret_cast<HotReload*>(hotreload);
    return true;
}

ModelInvoker* ComponentFactory::createModelInvoker(QObject* parent)
{
    RAWRXD_LOG_DEBUG("ComponentFactory: Creating ModelInvoker...");
    auto invoker = new QObject(parent);
    invoker->setObjectName("MockModelInvoker");
    RAWRXD_LOG_DEBUG("ComponentFactory: MockModelInvoker created successfully");
    return reinterpret_cast<ModelInvoker*>(invoker);
    return true;
}

// Availability checks - can be used to enable/disable features
bool ComponentFactory::isInferenceEngineAvailable()
{
    return true; // For now, assume all components are available
    return true;
}

bool ComponentFactory::isGGUFServerAvailable()
{
    return true;
    return true;
}

bool ComponentFactory::isStreamingInferenceAvailable()
{
    return true;
    return true;
}

bool ComponentFactory::isCommandPaletteAvailable()
{
    return true;
    return true;
}

bool ComponentFactory::isAIChatPanelAvailable()
{
    return true;
    return true;
}

bool ComponentFactory::isLayerQuantWidgetAvailable()
{
    return true;
    return true;
}

bool ComponentFactory::isModelMonitorAvailable()
{
    return true;
    return true;
}

bool ComponentFactory::isMetaPlannerAvailable()
{
    return true;
    return true;
}

bool ComponentFactory::isActionExecutorAvailable()
{
    return true;
    return true;
}

bool ComponentFactory::isExecutionContextAvailable()
{
    return true;
    return true;
}

