#pragma once

// Component factory to safely load heavy components at runtime
// Prevents static initialization stack overflow

#include <memory>
#include <QObject>
#include <QWidget>
#include <QPlainTextEdit>

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
    static InferenceEngine* createInferenceEngine(const QString& ggufPath = QString(), QObject* parent = nullptr);
    static GGUFServer* createGGUFServer(InferenceEngine* engine, QObject* parent = nullptr);
    static StreamingInference* createStreamingInference(QPlainTextEdit* outputEdit, QObject* parent = nullptr);
    static CommandPalette* createCommandPalette(QWidget* parent = nullptr);
    static AIChatPanel* createAIChatPanel(QWidget* parent = nullptr);
    static LayerQuantWidget* createLayerQuantWidget(QWidget* parent = nullptr);
    static ModelMonitor* createModelMonitor(InferenceEngine* engine, QWidget* parent = nullptr);
    
    // Agent system factory methods
    static MetaPlanner* createMetaPlanner(QObject* parent = nullptr);
    static ActionExecutor* createActionExecutor(QObject* parent = nullptr);
    static ExecutionContext* createExecutionContext(QObject* parent = nullptr);
    static AutoBootstrap* createAutoBootstrap(QObject* parent = nullptr);
    static HotReload* createHotReload(QObject* parent = nullptr);
    static ModelInvoker* createModelInvoker(QObject* parent = nullptr);
    
    // Helper functions to convert components to QObject for signal connections
    static QObject* asQObject(InferenceEngine* engine);
    static QObject* asQObject(AIChatPanel* panel);
    static QObject* asQObject(CommandPalette* palette);
    static QObject* asQObject(LayerQuantWidget* widget);
    static QObject* asQObject(MetaPlanner* planner);
    static QObject* asQObject(ActionExecutor* executor);
    static QObject* asQObject(ExecutionContext* context);
    
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