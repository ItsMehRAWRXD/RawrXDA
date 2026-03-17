#include "ComponentFactory.h"

// Safe includes - these will only be loaded when factory methods are called
// This prevents static initialization issues

// Agent system headers - included only here for factory implementation
// Forward declarations are used in the header to prevent static initialization
#include "../agent/auto_bootstrap.hpp"
#include "../agent/meta_planner.hpp"
#include "../agent/action_executor.hpp"
#include "../agent/execution_context.hpp"
#include "../agent/model_invoker.hpp"
#include "../agent/hot_reload.hpp"
#include "streaming_inference.hpp"
#include "command_palette.hpp"
#include "ai_chat_panel.hpp"
#include "widgets/layer_quant_widget.hpp"
#include "model_monitor.hpp"

InferenceEngine* ComponentFactory::createInferenceEngine(const std::string& ggufPath, void* parent)
{
    try {
        auto engine = new InferenceEngine(parent);
        if (!ggufPath.empty()) {
             engine->loadModel(ggufPath);
        }
        return engine;
    } catch (const std::exception& e) {
        return nullptr;
    }
}

GGUFServer* ComponentFactory::createGGUFServer(InferenceEngine* engine, void* parent)
{
    try {
        auto server = new GGUFServer(engine, parent);
        return server;
    } catch (const std::exception& e) {
        return nullptr;
    }
}
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
    // REAL IMPLEMENTATION
    try {
        auto streaming = new StreamingInference(outputEdit, parent);
        return streaming;
    } catch (const std::exception& e) {
        return nullptr;
    }
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
// REMOVED_QT: void* ComponentFactory::asQObject(InferenceEngine* engine) {
// REMOVED_QT:     return qobject_cast<void*>(engine);
}

// REMOVED_QT: void* ComponentFactory::asQObject(AIChatPanel* panel) {
// REMOVED_QT:     return qobject_cast<void*>(panel);
}

// REMOVED_QT: void* ComponentFactory::asQObject(CommandPalette* palette) {
// REMOVED_QT:     return qobject_cast<void*>(palette);
}

// REMOVED_QT: void* ComponentFactory::asQObject(LayerQuantWidget* widget) {
// REMOVED_QT:     return qobject_cast<void*>(widget);
}

// REMOVED_QT: void* ComponentFactory::asQObject(MetaPlanner* planner) {
    // For now, return the object directly cast as void using reinterpret_cast
// REMOVED_QT:     // This is safe since we're creating mock QObjects in createMetaPlanner
    return reinterpret_cast<void*>(planner);
}

// REMOVED_QT: void* ComponentFactory::asQObject(ActionExecutor* executor) {
    // For now, return the object directly cast as void using reinterpret_cast
// REMOVED_QT:     // This is safe since we're creating mock QObjects in createActionExecutor
    return reinterpret_cast<void*>(executor);
}

// REMOVED_QT: void* ComponentFactory::asQObject(ExecutionContext* context) {
    // For now, return the object directly cast as void using reinterpret_cast
// REMOVED_QT:     // This is safe since we're creating mock QObjects in createExecutionContext
    return reinterpret_cast<void*>(context);
}

// Agent system factory methods - safe runtime creation
MetaPlanner* ComponentFactory::createMetaPlanner(void* parent)
{
    try {
        // Real MetaPlanner instantiation
        auto planner = new MetaPlanner();
        return planner;
    } catch (const std::exception& e) {
        return nullptr;
    }
}

ActionExecutor* ComponentFactory::createActionExecutor(void* parent)
{
    try {
        // Real ActionExecutor instantiation
        auto executor = new ActionExecutor();
        return executor;
    } catch (const std::exception& e) {
        return nullptr;
    }
}

ExecutionContext* ComponentFactory::createExecutionContext(void* parent)
{
    try {
        // Real ExecutionContext instantiation
        auto context = new ExecutionContext();
        return context;
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
