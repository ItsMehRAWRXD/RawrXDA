#include "ComponentFactory.h"

// Safe includes - these will only be loaded when factory methods are called
// This prevents static initialization issues

// Agent system headers - included only here for factory implementation
// Forward declarations are used in the header to prevent static initialization
// #include "../agent/auto_bootstrap.hpp"
// Note: Other agent headers (MetaPlanner, ActionExecutor, ExecutionContext) will be included
// when they are properly implemented and tested to avoid static initialization stack overflow

InferenceEngine* ComponentFactory::createInferenceEngine(const std::string& ggufPath, void* parent)
{
    // STUBBED FOR DEBUGGING
    return nullptr;
    /*
    try {
        auto engine = new InferenceEngine(parent);
        return engine;
    } catch (const std::exception& e) {
        return nullptr;
    }
    */
}

GGUFServer* ComponentFactory::createGGUFServer(InferenceEngine* engine, void* parent)
{
    // STUBBED FOR DEBUGGING
    return nullptr;
    /*
    try {
        auto server = new GGUFServer(engine, parent);
        return server;
    } catch (const std::exception& e) {
        return nullptr;
    }
    */
}

StreamingInference* ComponentFactory::createStreamingInference(QPlainTextEdit* outputEdit, void* parent)
{
    // STUBBED FOR DEBUGGING
    return nullptr;
    /*
    try {
        auto streaming = new StreamingInference(outputEdit, parent);
        return streaming;
    } catch (const std::exception& e) {
        return nullptr;
    }
    */
}

CommandPalette* ComponentFactory::createCommandPalette(void* parent)
{
    // STUBBED FOR DEBUGGING
    return nullptr;
    /*
    try {
        auto palette = new CommandPalette(parent);
        return palette;
    } catch (const std::exception& e) {
        return nullptr;
    }
    */
}

AIChatPanel* ComponentFactory::createAIChatPanel(void* parent)
{
    // STUBBED FOR DEBUGGING
    return nullptr;
    /*
    try {
        auto panel = new AIChatPanel(parent);
        return panel;
    } catch (const std::exception& e) {
        return nullptr;
    }
    */
}

LayerQuantWidget* ComponentFactory::createLayerQuantWidget(void* parent)
{
    // STUBBED FOR DEBUGGING
    return nullptr;
    /*
    try {
        auto widget = new LayerQuantWidget(parent);
        return widget;
    } catch (const std::exception& e) {
        return nullptr;
    }
    */
}

ModelMonitor* ComponentFactory::createModelMonitor(InferenceEngine* engine, void* parent)
{
    // STUBBED FOR DEBUGGING
    return nullptr;
    /*
    try {
        auto monitor = new ModelMonitor(engine, parent);
        return monitor;
    } catch (const std::exception& e) {
        return nullptr;
    }
    */
}

// Helper functions to convert components to void for signal connections
void* ComponentFactory::asQObject(InferenceEngine* engine) {
    return qobject_cast<void*>(engine);
}

void* ComponentFactory::asQObject(AIChatPanel* panel) {
    return qobject_cast<void*>(panel);
}

void* ComponentFactory::asQObject(CommandPalette* palette) {
    return qobject_cast<void*>(palette);
}

void* ComponentFactory::asQObject(LayerQuantWidget* widget) {
    return qobject_cast<void*>(widget);
}

void* ComponentFactory::asQObject(MetaPlanner* planner) {
    // For now, return the object directly cast as void using reinterpret_cast
    // This is safe since we're creating mock QObjects in createMetaPlanner
    return reinterpret_cast<void*>(planner);
}

void* ComponentFactory::asQObject(ActionExecutor* executor) {
    // For now, return the object directly cast as void using reinterpret_cast
    // This is safe since we're creating mock QObjects in createActionExecutor
    return reinterpret_cast<void*>(executor);
}

void* ComponentFactory::asQObject(ExecutionContext* context) {
    // For now, return the object directly cast as void using reinterpret_cast
    // This is safe since we're creating mock QObjects in createExecutionContext
    return reinterpret_cast<void*>(context);
}

// Agent system factory methods - safe runtime creation
MetaPlanner* ComponentFactory::createMetaPlanner(void* parent)
{
    try {
        // For now, create a simple mock object instead of real MetaPlanner to avoid static initialization
        // TODO: Replace with real MetaPlanner when headers are properly implemented
        auto planner = new void(parent);
        planner->setObjectName("MockMetaPlanner");
        return reinterpret_cast<MetaPlanner*>(planner);
    } catch (const std::exception& e) {
        return nullptr;
    }
}

ActionExecutor* ComponentFactory::createActionExecutor(void* parent)
{
    try {
        // For now, create a simple mock object instead of real ActionExecutor to avoid static initialization
        // TODO: Replace with real ActionExecutor when headers are properly implemented
        auto executor = new void(parent);
        executor->setObjectName("MockActionExecutor");
        return reinterpret_cast<ActionExecutor*>(executor);
    } catch (const std::exception& e) {
        return nullptr;
    }
}

ExecutionContext* ComponentFactory::createExecutionContext(void* parent)
{
    try {
        // For now, create a simple mock object instead of real ExecutionContext to avoid static initialization
        // TODO: Replace with real ExecutionContext when headers are properly implemented
        auto context = new void(parent);
        context->setObjectName("MockExecutionContext");
        return reinterpret_cast<ExecutionContext*>(context);
    } catch (const std::exception& e) {
        return nullptr;
    }
}

AutoBootstrap* ComponentFactory::createAutoBootstrap(void* parent)
{
    try {
        // AutoBootstrap uses singleton pattern, so use instance() method instead of constructor
        // auto bootstrap = AutoBootstrap::instance();
        // if (bootstrap && parent) {
        //     bootstrap->setParent(parent);
        // }
        return nullptr; // bootstrap;
    } catch (const std::exception& e) {
        return nullptr;
    }
}

HotReload* ComponentFactory::createHotReload(void* parent)
{
    try {
        // For now, create a simple mock object instead of real HotReload to avoid static initialization
        // TODO: Replace with real HotReload when headers are properly implemented
        auto hotreload = new void(parent);
        hotreload->setObjectName("MockHotReload");
        return reinterpret_cast<HotReload*>(hotreload);
    } catch (const std::exception& e) {
        return nullptr;
    }
}

ModelInvoker* ComponentFactory::createModelInvoker(void* parent)
{
    try {
        // For now, create a simple mock object instead of real ModelInvoker to avoid static initialization
        // TODO: Replace with real ModelInvoker when headers are properly implemented
        auto invoker = new void(parent);
        invoker->setObjectName("MockModelInvoker");
        return reinterpret_cast<ModelInvoker*>(invoker);
    } catch (const std::exception& e) {
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
