#pragma once
#include <QWidget>
#include <QDockWidget>
#include <QLabel>
#include <QVBoxLayout>

// Macro to quickly define a stub widget
#define DEFINE_STUB_WIDGET(ClassName) \
class ClassName : public QWidget { \
public: \
    explicit ClassName(QWidget* parent = nullptr) : QWidget(parent) { \
        QVBoxLayout* layout = new QVBoxLayout(this); \
        layout->addWidget(new QLabel(#ClassName " - Not Implemented Yet", this)); \
    } \
};

// Subsystems
// ProjectExplorerWidget has real implementation in widgets/project_explorer.h
// BuildSystemWidget has REAL implementation in widgets/build_system_widget.h
// VersionControlWidget has REAL implementation in widgets/version_control_widget.h
#include "widgets/build_system_widget.h"
#include "widgets/version_control_widget.h"
DEFINE_STUB_WIDGET(RunDebugWidget)
DEFINE_STUB_WIDGET(ProfilerWidget)
DEFINE_STUB_WIDGET(TestExplorerWidget)
DEFINE_STUB_WIDGET(DatabaseToolWidget)
DEFINE_STUB_WIDGET(DockerToolWidget)
DEFINE_STUB_WIDGET(CloudExplorerWidget)
DEFINE_STUB_WIDGET(PackageManagerWidget)
DEFINE_STUB_WIDGET(DocumentationWidget)
DEFINE_STUB_WIDGET(UMLViewWidget)
DEFINE_STUB_WIDGET(ImageToolWidget)
DEFINE_STUB_WIDGET(TranslationWidget)
DEFINE_STUB_WIDGET(DesignToCodeWidget)
DEFINE_STUB_WIDGET(AIChatWidget)
DEFINE_STUB_WIDGET(NotebookWidget)
DEFINE_STUB_WIDGET(MarkdownViewer)
DEFINE_STUB_WIDGET(SpreadsheetWidget)
DEFINE_STUB_WIDGET(TerminalClusterWidget)
DEFINE_STUB_WIDGET(SnippetManagerWidget)
DEFINE_STUB_WIDGET(RegexTesterWidget)
// DiffViewerWidget has potential real implementation - checking
DEFINE_STUB_WIDGET(DiffViewerWidget)
DEFINE_STUB_WIDGET(ColorPickerWidget)
DEFINE_STUB_WIDGET(IconFontWidget)
DEFINE_STUB_WIDGET(PluginManagerWidget)
// SettingsWidget has REAL implementation in widgets/settings_dialog.h
// Use RawrXD::SettingsDialog instead of stub
DEFINE_STUB_WIDGET(NotificationCenter)
DEFINE_STUB_WIDGET(ShortcutsConfigurator)
DEFINE_STUB_WIDGET(TelemetryWidget)
DEFINE_STUB_WIDGET(UpdateCheckerWidget)
DEFINE_STUB_WIDGET(WelcomeScreenWidget)
// CommandPalette has real implementation in command_palette.hpp
DEFINE_STUB_WIDGET(ProgressManager)
DEFINE_STUB_WIDGET(AIQuickFixWidget)
DEFINE_STUB_WIDGET(CodeMinimap)
DEFINE_STUB_WIDGET(BreadcrumbBar)
DEFINE_STUB_WIDGET(StatusBarManager)
DEFINE_STUB_WIDGET(TerminalEmulator)
DEFINE_STUB_WIDGET(SearchResultWidget)
DEFINE_STUB_WIDGET(BookmarkWidget)
DEFINE_STUB_WIDGET(TodoWidget)
DEFINE_STUB_WIDGET(MacroRecorderWidget)
DEFINE_STUB_WIDGET(AICompletionCache)
DEFINE_STUB_WIDGET(LanguageClientHost)

// Providers and other classes
class CodeLensProvider : public QObject { Q_OBJECT public: explicit CodeLensProvider(QObject* p=nullptr):QObject(p){} };
class InlayHintProvider : public QObject { Q_OBJECT public: explicit InlayHintProvider(QObject* p=nullptr):QObject(p){} };
class SemanticHighlighter : public QObject { Q_OBJECT public: explicit SemanticHighlighter(QObject* p=nullptr):QObject(p){} };
DEFINE_STUB_WIDGET(InlineChatWidget)
DEFINE_STUB_WIDGET(AIReviewWidget)
DEFINE_STUB_WIDGET(CodeStreamWidget)
DEFINE_STUB_WIDGET(AudioCallWidget)
DEFINE_STUB_WIDGET(ScreenShareWidget)
DEFINE_STUB_WIDGET(WhiteboardWidget)
DEFINE_STUB_WIDGET(TimeTrackerWidget)
DEFINE_STUB_WIDGET(TaskManagerWidget)
DEFINE_STUB_WIDGET(PomodoroWidget)
DEFINE_STUB_WIDGET(WallpaperWidget)
DEFINE_STUB_WIDGET(AccessibilityWidget)
DEFINE_STUB_WIDGET(UMLLViewWidget) // Typo in header? UMLViewWidget vs UMLLViewWidget

// Forward decls for existing classes if needed
class StreamerClient : public QObject { Q_OBJECT public: explicit StreamerClient(QObject* p=nullptr):QObject(p){} };
class AgentOrchestrator : public QObject { Q_OBJECT public: explicit AgentOrchestrator(QObject* p=nullptr):QObject(p){} };
class AISuggestionOverlay : public QWidget { Q_OBJECT public: explicit AISuggestionOverlay(QWidget* p=nullptr):QWidget(p){} };
class TaskProposalWidget : public QWidget { Q_OBJECT public: explicit TaskProposalWidget(QWidget* p=nullptr):QWidget(p){} };
