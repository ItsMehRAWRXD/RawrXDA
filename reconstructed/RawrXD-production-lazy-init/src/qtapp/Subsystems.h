#pragma once
#include <QWidget>
#include <QDockWidget>
#include <QLabel>
#include <QVBoxLayout>
#include "integration/ProdIntegration.h"

// Include real implementations
#include "widgets/build_system_widget.h"
#include "widgets/version_control_widget.h"
#include "widgets/terminal_emulator.h"
#include "widgets/test_explorer_widget.h"
#include "widgets/run_debug_widget.h"
#include "widgets/profiler_widget.h"
#include "widgets/snippet_manager_widget.h"
#include "widgets/database_tool_widget.h"
#include "widgets/accessibility_widget.h"
#include "widgets/ai_review_widget.h"
#include "widgets/audio_call_widget.h"
#include "widgets/time_tracker_widget.h"
#include "widgets/task_manager_widget.h"
#include "widgets/pomodoro_widget.h"
#include "widgets/wallpaper_widget.h"
#include "widgets/telemetry_widget.h"
#include "widgets/update_checker_widget.h"
#include "widgets/welcome_screen_widget.h"
#include "widgets/project_explorer.h"
#include "widgets/docker_tool_widget.h"
#include "widgets/cloud_explorer_widget.h"
#include "widgets/package_manager_widget.h"
#include "widgets/documentation_widget.h"
#include "widgets/uml_view_widget.h"
#include "widgets/image_tool_widget.h"
#include "widgets/translation_widget.h"
#include "widgets/design_to_code_widget.h"
#include "widgets/ai_chat_widget.h"
#include "widgets/notebook_widget.h"
#include "widgets/markdown_viewer.h"
#include "widgets/spreadsheet_widget.h"
#include "widgets/regex_tester_widget.h"
#include "widgets/diff_viewer_widget.h"
#include "widgets/color_picker_widget.h"
#include "widgets/icon_font_widget.h"
#include "widgets/plugin_manager_widget.h"
#include "widgets/notification_center.h"
#include "widgets/shortcuts_configurator.h"
#include "widgets/progress_manager.h"
#include "widgets/ai_quick_fix_widget.h"
#include "widgets/code_minimap.h"
#include "widgets/status_bar_manager.h"
#include "widgets/search_result_widget.h"
#include "widgets/breadcrumb_navigation.hpp"
#include "widgets/bookmark_widget.h"
#include "widgets/todo_widget.h"
#include "widgets/AICompletionCache.h"
#include "widgets/language_client_host.h"
#include "widgets/inline_chat_widget.h"
#include "widgets/code_stream_widget.h"
#include "widgets/screen_share_widget.h"
#include "widgets/whiteboard_widget.h"
#include "widgets/macro_recorder_widget.h"

// Macro to quickly define a stub widget (for widgets not yet fully implemented)
#define DEFINE_STUB_WIDGET(ClassName) \
class ClassName : public QWidget { \
public: \
    explicit ClassName(QWidget* parent = nullptr) : QWidget(parent) { \
        RawrXD::Integration::ScopedTimer _stubTimer("Widget", #ClassName, "construct"); \
        QVBoxLayout* layout = new QVBoxLayout(this); \
        layout->addWidget(new QLabel(#ClassName " - Not Implemented Yet", this)); \
        if (RawrXD::Integration::Config::stubLoggingEnabled()) { \
            RawrXD::Integration::logInfo("Widget", "stub_constructed", QString::fromUtf8(#ClassName)); \
            RawrXD::Integration::recordMetric("widget_stub_constructed"); \
            RawrXD::Integration::traceEvent(#ClassName, "constructed"); \
        } \
    } \
};

// Subsystems
// ProjectExplorerWidget has real implementation in widgets/project_explorer.h
// BuildSystemWidget has real implementation in widgets/build_system_widget.h
// VersionControlWidget has real implementation in widgets/version_control_widget.h
// TerminalEmulator has real implementation in widgets/terminal_emulator.h
// TestExplorerWidget has real implementation in widgets/test_explorer_widget.h
// RunDebugWidget has real implementation in widgets/run_debug_widget.h
// ProfilerWidget has real implementation in widgets/profiler_widget.h
// DEFINE_STUB_WIDGET(RunDebugWidget) // Real implementation included
// DEFINE_STUB_WIDGET(ProfilerWidget) // Real implementation included
// DEFINE_STUB_WIDGET(TestExplorerWidget) // Real implementation included
// DEFINE_STUB_WIDGET(DatabaseToolWidget) // Real implementation included
// DEFINE_STUB_WIDGET(DockerToolWidget) // Real implementation included
// DEFINE_STUB_WIDGET(CloudExplorerWidget) // Real implementation included
// DEFINE_STUB_WIDGET(PackageManagerWidget) // Real implementation included
// DEFINE_STUB_WIDGET(DocumentationWidget) // Real implementation included
// DEFINE_STUB_WIDGET(UMLViewWidget) // Real implementation included
// DEFINE_STUB_WIDGET(ImageToolWidget) // Real implementation included
// DEFINE_STUB_WIDGET(TranslationWidget) // Real implementation included
// DEFINE_STUB_WIDGET(DesignToCodeWidget) // Real implementation included
// DEFINE_STUB_WIDGET(AIChatWidget) // Real implementation included
// DEFINE_STUB_WIDGET(NotebookWidget) // Real implementation included
// DEFINE_STUB_WIDGET(MarkdownViewer) // Real implementation included
// DEFINE_STUB_WIDGET(SpreadsheetWidget) // Real implementation included
// TerminalClusterWidget has real implementation in widgets/TerminalClusterWidget.h
class TerminalClusterWidget;
// DEFINE_STUB_WIDGET(SnippetManagerWidget) // Real implementation included
// DEFINE_STUB_WIDGET(RegexTesterWidget) // Real implementation included
// DEFINE_STUB_WIDGET(DiffViewerWidget) // Real implementation included
// DEFINE_STUB_WIDGET(ColorPickerWidget) // Real implementation included
// DEFINE_STUB_WIDGET(IconFontWidget) // Real implementation included
// DEFINE_STUB_WIDGET(PluginManagerWidget) // Real implementation included
// SettingsWidget has REAL implementation in widgets/settings_dialog.h
// Use RawrXD::SettingsDialog instead of stub
// DEFINE_STUB_WIDGET(NotificationCenter) // Real implementation included
// DEFINE_STUB_WIDGET(ShortcutsConfigurator) // Real implementation included
// TelemetryWidget, UpdateCheckerWidget, WelcomeScreenWidget have real implementations in widgets/
// CommandPalette has real implementation in command_palette.hpp
// DEFINE_STUB_WIDGET(ProgressManager) // Real implementation included
// DEFINE_STUB_WIDGET(AIQuickFixWidget) // Real implementation included
// DEFINE_STUB_WIDGET(CodeMinimap) // Real implementation included
// DEFINE_STUB_WIDGET(StatusBarManager) // Real implementation included
// DEFINE_STUB_WIDGET(SearchResultWidget) // Real implementation included
// BookmarkWidget, TodoWidget, MacroRecorderWidget have real implementations in widgets/
// DEFINE_STUB_WIDGET(AICompletionCache) // No real implementation found, keeping as stub
// DEFINE_STUB_WIDGET(LanguageClientHost) // Real implementation included
// Providers and other classes
class CodeLensProvider : public QObject { Q_OBJECT public: explicit CodeLensProvider(QObject* p=nullptr):QObject(p){} };
class InlayHintProvider : public QObject { Q_OBJECT public: explicit InlayHintProvider(QObject* p=nullptr):QObject(p){} };
class SemanticHighlighter : public QObject { Q_OBJECT public: explicit SemanticHighlighter(QObject* p=nullptr):QObject(p){} };
// DEFINE_STUB_WIDGET(InlineChatWidget) // Real implementation included
// AIReviewWidget has real implementation in widgets/ai_review_widget.h
// AudioCallWidget has real implementation in widgets/audio_call_widget.h  
// DEFINE_STUB_WIDGET(CodeStreamWidget) // Real implementation included
// DEFINE_STUB_WIDGET(ScreenShareWidget) // Real implementation included
// DEFINE_STUB_WIDGET(WhiteboardWidget) // Real implementation included
// TimeTrackerWidget has real implementation in widgets/time_tracker_widget.h
// TaskManagerWidget has real implementation in widgets/task_manager_widget.h
// PomodoroWidget has real implementation in widgets/pomodoro_widget.h
// WallpaperWidget has real implementation in widgets/wallpaper_widget.h
// AccessibilityWidget has real implementation in widgets/accessibility_widget.h
// DEFINE_STUB_WIDGET(UMLLViewWidget) // Typo - use UMLViewWidget instead (already integrated)

// Forward decls for existing classes if needed
class StreamerClient : public QObject { Q_OBJECT public: explicit StreamerClient(QObject* p=nullptr):QObject(p){} };
class AgentOrchestrator : public QObject { Q_OBJECT public: explicit AgentOrchestrator(QObject* p=nullptr):QObject(p){} };
class AISuggestionOverlay : public QWidget { Q_OBJECT public: explicit AISuggestionOverlay(QWidget* p=nullptr):QWidget(p){} };
class TaskProposalWidget : public QWidget { Q_OBJECT public: explicit TaskProposalWidget(QWidget* p=nullptr):QWidget(p){} };
