#pragma once
#include <QWidget>
#include <QDockWidget>
#include <QLabel>
#include <QVBoxLayout>

class StubWidgetBase : public QWidget
{
public:
    explicit StubWidgetBase(const char* title, QWidget* parent = nullptr) : QWidget(parent)
    {
        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->addWidget(new QLabel(QString::fromLatin1(title) + " - Not Implemented Yet", this));
    }
};

// Subsystems
// ProjectExplorerWidget has real implementation in widgets/project_explorer.h
// BuildSystemWidget has REAL implementation in widgets/build_system_widget.h
// VersionControlWidget has REAL implementation in widgets/version_control_widget.h
#include "widgets/build_system_widget.h"
#include "widgets/version_control_widget.h"
class RunDebugWidget : public StubWidgetBase { public: explicit RunDebugWidget(QWidget* p = nullptr) : StubWidgetBase("RunDebugWidget", p) {} };
class ProfilerWidget : public StubWidgetBase { public: explicit ProfilerWidget(QWidget* p = nullptr) : StubWidgetBase("ProfilerWidget", p) {} };
class TestExplorerWidget : public StubWidgetBase { public: explicit TestExplorerWidget(QWidget* p = nullptr) : StubWidgetBase("TestExplorerWidget", p) {} };
class DatabaseToolWidget : public StubWidgetBase { public: explicit DatabaseToolWidget(QWidget* p = nullptr) : StubWidgetBase("DatabaseToolWidget", p) {} };
class DockerToolWidget : public StubWidgetBase { public: explicit DockerToolWidget(QWidget* p = nullptr) : StubWidgetBase("DockerToolWidget", p) {} };
class CloudExplorerWidget : public StubWidgetBase { public: explicit CloudExplorerWidget(QWidget* p = nullptr) : StubWidgetBase("CloudExplorerWidget", p) {} };
class PackageManagerWidget : public StubWidgetBase { public: explicit PackageManagerWidget(QWidget* p = nullptr) : StubWidgetBase("PackageManagerWidget", p) {} };
class DocumentationWidget : public StubWidgetBase { public: explicit DocumentationWidget(QWidget* p = nullptr) : StubWidgetBase("DocumentationWidget", p) {} };
class UMLViewWidget : public StubWidgetBase { public: explicit UMLViewWidget(QWidget* p = nullptr) : StubWidgetBase("UMLViewWidget", p) {} };
class ImageToolWidget : public StubWidgetBase { public: explicit ImageToolWidget(QWidget* p = nullptr) : StubWidgetBase("ImageToolWidget", p) {} };
class TranslationWidget : public StubWidgetBase { public: explicit TranslationWidget(QWidget* p = nullptr) : StubWidgetBase("TranslationWidget", p) {} };
class DesignToCodeWidget : public StubWidgetBase { public: explicit DesignToCodeWidget(QWidget* p = nullptr) : StubWidgetBase("DesignToCodeWidget", p) {} };
class AIChatWidget : public StubWidgetBase { public: explicit AIChatWidget(QWidget* p = nullptr) : StubWidgetBase("AIChatWidget", p) {} };
class NotebookWidget : public StubWidgetBase { public: explicit NotebookWidget(QWidget* p = nullptr) : StubWidgetBase("NotebookWidget", p) {} };
class MarkdownViewer : public StubWidgetBase { public: explicit MarkdownViewer(QWidget* p = nullptr) : StubWidgetBase("MarkdownViewer", p) {} };
class SpreadsheetWidget : public StubWidgetBase { public: explicit SpreadsheetWidget(QWidget* p = nullptr) : StubWidgetBase("SpreadsheetWidget", p) {} };
class TerminalClusterWidget : public StubWidgetBase { public: explicit TerminalClusterWidget(QWidget* p = nullptr) : StubWidgetBase("TerminalClusterWidget", p) {} };
class SnippetManagerWidget : public StubWidgetBase { public: explicit SnippetManagerWidget(QWidget* p = nullptr) : StubWidgetBase("SnippetManagerWidget", p) {} };
class RegexTesterWidget : public StubWidgetBase { public: explicit RegexTesterWidget(QWidget* p = nullptr) : StubWidgetBase("RegexTesterWidget", p) {} };
// DiffViewerWidget has potential real implementation - checking
class DiffViewerWidget : public StubWidgetBase { public: explicit DiffViewerWidget(QWidget* p = nullptr) : StubWidgetBase("DiffViewerWidget", p) {} };
class ColorPickerWidget : public StubWidgetBase { public: explicit ColorPickerWidget(QWidget* p = nullptr) : StubWidgetBase("ColorPickerWidget", p) {} };
class IconFontWidget : public StubWidgetBase { public: explicit IconFontWidget(QWidget* p = nullptr) : StubWidgetBase("IconFontWidget", p) {} };
class PluginManagerWidget : public StubWidgetBase { public: explicit PluginManagerWidget(QWidget* p = nullptr) : StubWidgetBase("PluginManagerWidget", p) {} };
// SettingsWidget has REAL implementation in widgets/settings_dialog.h
// Use RawrXD::SettingsDialog instead of stub
class NotificationCenter : public StubWidgetBase { public: explicit NotificationCenter(QWidget* p = nullptr) : StubWidgetBase("NotificationCenter", p) {} };
class ShortcutsConfigurator : public StubWidgetBase { public: explicit ShortcutsConfigurator(QWidget* p = nullptr) : StubWidgetBase("ShortcutsConfigurator", p) {} };
class TelemetryWidget : public StubWidgetBase { public: explicit TelemetryWidget(QWidget* p = nullptr) : StubWidgetBase("TelemetryWidget", p) {} };
class UpdateCheckerWidget : public StubWidgetBase { public: explicit UpdateCheckerWidget(QWidget* p = nullptr) : StubWidgetBase("UpdateCheckerWidget", p) {} };
class WelcomeScreenWidget : public StubWidgetBase { public: explicit WelcomeScreenWidget(QWidget* p = nullptr) : StubWidgetBase("WelcomeScreenWidget", p) {} };
// CommandPalette has real implementation in command_palette.hpp
class ProgressManager : public StubWidgetBase { public: explicit ProgressManager(QWidget* p = nullptr) : StubWidgetBase("ProgressManager", p) {} };
class AIQuickFixWidget : public StubWidgetBase { public: explicit AIQuickFixWidget(QWidget* p = nullptr) : StubWidgetBase("AIQuickFixWidget", p) {} };
class CodeMinimap : public StubWidgetBase { public: explicit CodeMinimap(QWidget* p = nullptr) : StubWidgetBase("CodeMinimap", p) {} };
class BreadcrumbBar : public StubWidgetBase { public: explicit BreadcrumbBar(QWidget* p = nullptr) : StubWidgetBase("BreadcrumbBar", p) {} };
class StatusBarManager : public StubWidgetBase { public: explicit StatusBarManager(QWidget* p = nullptr) : StubWidgetBase("StatusBarManager", p) {} };
class TerminalEmulator : public StubWidgetBase { public: explicit TerminalEmulator(QWidget* p = nullptr) : StubWidgetBase("TerminalEmulator", p) {} };
class SearchResultWidget : public StubWidgetBase { public: explicit SearchResultWidget(QWidget* p = nullptr) : StubWidgetBase("SearchResultWidget", p) {} };
class BookmarkWidget : public StubWidgetBase { public: explicit BookmarkWidget(QWidget* p = nullptr) : StubWidgetBase("BookmarkWidget", p) {} };
class TodoWidget : public StubWidgetBase { public: explicit TodoWidget(QWidget* p = nullptr) : StubWidgetBase("TodoWidget", p) {} };
class MacroRecorderWidget : public StubWidgetBase { public: explicit MacroRecorderWidget(QWidget* p = nullptr) : StubWidgetBase("MacroRecorderWidget", p) {} };
class AICompletionCache : public StubWidgetBase { public: explicit AICompletionCache(QWidget* p = nullptr) : StubWidgetBase("AICompletionCache", p) {} };
class LanguageClientHost : public StubWidgetBase { public: explicit LanguageClientHost(QWidget* p = nullptr) : StubWidgetBase("LanguageClientHost", p) {} };

// Providers and other classes
class CodeLensProvider : public QObject { Q_OBJECT public: explicit CodeLensProvider(QObject* p=nullptr):QObject(p){} };
class InlayHintProvider : public QObject { Q_OBJECT public: explicit InlayHintProvider(QObject* p=nullptr):QObject(p){} };
class SemanticHighlighter : public QObject { Q_OBJECT public: explicit SemanticHighlighter(QObject* p=nullptr):QObject(p){} };
class InlineChatWidget : public StubWidgetBase { public: explicit InlineChatWidget(QWidget* p = nullptr) : StubWidgetBase("InlineChatWidget", p) {} };
class AIReviewWidget : public StubWidgetBase { public: explicit AIReviewWidget(QWidget* p = nullptr) : StubWidgetBase("AIReviewWidget", p) {} };
class CodeStreamWidget : public StubWidgetBase { public: explicit CodeStreamWidget(QWidget* p = nullptr) : StubWidgetBase("CodeStreamWidget", p) {} };
class AudioCallWidget : public StubWidgetBase { public: explicit AudioCallWidget(QWidget* p = nullptr) : StubWidgetBase("AudioCallWidget", p) {} };
class ScreenShareWidget : public StubWidgetBase { public: explicit ScreenShareWidget(QWidget* p = nullptr) : StubWidgetBase("ScreenShareWidget", p) {} };
class WhiteboardWidget : public StubWidgetBase { public: explicit WhiteboardWidget(QWidget* p = nullptr) : StubWidgetBase("WhiteboardWidget", p) {} };
class TimeTrackerWidget : public StubWidgetBase { public: explicit TimeTrackerWidget(QWidget* p = nullptr) : StubWidgetBase("TimeTrackerWidget", p) {} };
class TaskManagerWidget : public StubWidgetBase { public: explicit TaskManagerWidget(QWidget* p = nullptr) : StubWidgetBase("TaskManagerWidget", p) {} };
class PomodoroWidget : public StubWidgetBase { public: explicit PomodoroWidget(QWidget* p = nullptr) : StubWidgetBase("PomodoroWidget", p) {} };
class WallpaperWidget : public StubWidgetBase { public: explicit WallpaperWidget(QWidget* p = nullptr) : StubWidgetBase("WallpaperWidget", p) {} };
class AccessibilityWidget : public StubWidgetBase { public: explicit AccessibilityWidget(QWidget* p = nullptr) : StubWidgetBase("AccessibilityWidget", p) {} };
class UMLLViewWidget : public StubWidgetBase { public: explicit UMLLViewWidget(QWidget* p = nullptr) : StubWidgetBase("UMLLViewWidget", p) {} }; // Typo in header? UMLViewWidget vs UMLLViewWidget

// Forward decls for existing classes if needed
class StreamerClient : public QObject { Q_OBJECT public: explicit StreamerClient(QObject* p=nullptr):QObject(p){} };
class AgentOrchestrator : public QObject { Q_OBJECT public: explicit AgentOrchestrator(QObject* p=nullptr):QObject(p){} };
class AISuggestionOverlay : public QWidget { Q_OBJECT public: explicit AISuggestionOverlay(QWidget* p=nullptr):QWidget(p){} };
class TaskProposalWidget : public QWidget { Q_OBJECT public: explicit TaskProposalWidget(QWidget* p=nullptr):QWidget(p){} };
