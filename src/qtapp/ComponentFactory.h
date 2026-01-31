#pragma once

// Component factory to safely load heavy components at runtime
// Prevents static initialization stack overflow

#include <memory>


// Include the actual headers - the factory pattern should work WITH the headers, not replace them
#include "inference_engine.hpp"
#include "gguf_server.hpp"
#include "streaming_inference.hpp"
#include "command_palette.hpp"
#include "ai_chat_panel.hpp"
#include "layer_quant_widget.hpp"
#include "model_monitor.hpp"

// Forward declarations for agent system classes to prevent static initialization
// These will be included only in the .cpp file when factory methods are called
class MetaPlanner;
class ActionExecutor;
struct ExecutionContext;  // Use struct to match the expected type
class AutoBootstrap;
class HotReload;
class ModelInvoker;

class ComponentFactory
{
public:
    // Static factory methods to create components safely at runtime
    static InferenceEngine* createInferenceEngine(const std::string& ggufPath = std::string(), void* parent = nullptr);
    static GGUFServer* createGGUFServer(InferenceEngine* engine, void* parent = nullptr);
    static StreamingInference* createStreamingInference(QPlainTextEdit* outputEdit, void* parent = nullptr);
    static CommandPalette* createCommandPalette(void* parent = nullptr);
    static AIChatPanel* createAIChatPanel(void* parent = nullptr);
    static LayerQuantWidget* createLayerQuantWidget(void* parent = nullptr);
    static ModelMonitor* createModelMonitor(InferenceEngine* engine, void* parent = nullptr);
    
    // Agent system factory methods
    static MetaPlanner* createMetaPlanner(void* parent = nullptr);
    static ActionExecutor* createActionExecutor(void* parent = nullptr);
    static ExecutionContext* createExecutionContext(void* parent = nullptr);
    static AutoBootstrap* createAutoBootstrap(void* parent = nullptr);
    static HotReload* createHotReload(void* parent = nullptr);
    static ModelInvoker* createModelInvoker(void* parent = nullptr);
    
    // Helper functions to convert components to void for signal connections
    static void* asQObject(InferenceEngine* engine);
    static void* asQObject(AIChatPanel* panel);
    static void* asQObject(CommandPalette* palette);
    static void* asQObject(LayerQuantWidget* widget);
    static void* asQObject(MetaPlanner* planner);
    static void* asQObject(ActionExecutor* executor);
    static void* asQObject(ExecutionContext* context);
    
    // Check if components are available
    static bool isInferenceEngineAvailable();
    static bool isGGUFServerAvailable();
    static bool isStreamingInferenceAvailable();
    static bool isCommandPaletteAvailable();
    static bool isAIChatPanelAvailable();
    static bool isMetaPlannerAvailable();
    static bool isActionExecutorAvailable();
    static bool isExecutionContextAvailable();
    static bool isLayerQuantWidgetAvailable();
    static bool isModelMonitorAvailable();
};

