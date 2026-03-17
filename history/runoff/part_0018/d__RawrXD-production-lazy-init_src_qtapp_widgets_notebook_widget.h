/**
 * @file notebook_widget.h
 * @brief Header for NotebookWidget - Interactive notebook interface
 */

#ifndef NOTEBOOK_WIDGET_H
#define NOTEBOOK_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QListWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QScrollArea>
#include <QGroupBox>
#include <QProgressBar>
#include <QTimer>
#include <QMenu>
#include <QAction>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QSettings>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>
#include <QTextCharFormat>
#include <QFont>
#include <QColor>
#include <QBrush>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDateTime>
#include <QUuid>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QTemporaryFile>
#include <QDebug>

// ============================================================================
// Forward Declarations and Type Definitions
// ============================================================================

class NotebookCell;
class NotebookKernel;
class NotebookOutput;
class CodeHighlighter;
class NotebookWidget;

// ============================================================================
// NotebookCell Class
// ============================================================================

class NotebookCell : public QWidget
{
    Q_OBJECT

public:
    enum CellType {
        Code = 0,
        Markdown = 1,
        Raw = 2
    };

    explicit NotebookCell(QWidget* parent = nullptr);
    ~NotebookCell();

    void setType(CellType type);
    CellType getType() const { return type_; }

    void setContent(const QString& content);
    QString getContent() const;

    void setOutput(const QString& output);
    QString getOutput() const;

    void setExecutionCount(int count);
    int getExecutionCount() const { return executionCount_; }

    void setRunning(bool running);
    bool isRunning() const { return isRunning_; }

    void setError(const QString& error);
    QString getError() const { return error_; }

    void focusEditor();
    void clearOutput();

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);

signals:
    void contentChanged();
    void executeRequested();
    void deleteRequested();
    void moveUpRequested();
    void moveDownRequested();
    void typeChanged(NotebookCell::CellType newType);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void onContentChanged();
    void onExecuteClicked();
    void onTypeChanged(int index);
    void showContextMenu(const QPoint& pos);

private:
    void setupUI();
    void setupConnections();
    void updateUI();
    void updateTypeIndicator();

    CellType type_;
    int executionCount_;
    bool isRunning_;
    QString error_;

    // UI components
    QVBoxLayout* mainLayout_;
    QHBoxLayout* headerLayout_;
    QLabel* typeIndicator_;
    QComboBox* typeCombo_;
    QPushButton* runBtn_;
    QPushButton* stopBtn_;
    QLabel* executionLabel_;
    QPushButton* menuBtn_;

    QTextEdit* editor_;
    CodeHighlighter* highlighter_;

    QWidget* outputWidget_;
    QVBoxLayout* outputLayout_;
    QLabel* outputLabel_;
    QLabel* errorLabel_;

    QMenu* contextMenu_;
};

// ============================================================================
// CodeHighlighter Class
// ============================================================================

class CodeHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit CodeHighlighter(QTextDocument* parent = nullptr);

    void setLanguage(const QString& language);
    QString getLanguage() const { return language_; }

protected:
    void highlightBlock(const QString& text) override;

private:
    void setupPythonRules();
    void setupJavaScriptRules();
    void setupCppRules();
    void setupJavaRules();
    void setupMarkdownRules();

    QString language_;
    QVector<QTextCharFormat> formats_;
    QVector<QRegularExpression> rules_;
};

// ============================================================================
// NotebookOutput Class
// ============================================================================

class NotebookOutput : public QWidget
{
    Q_OBJECT

public:
    explicit NotebookOutput(QWidget* parent = nullptr);

    void setOutput(const QString& output, const QString& outputType = "text");
    void appendOutput(const QString& output);
    void setError(const QString& error);
    void clear();

    void setMimeType(const QString& mimeType);
    QString getMimeType() const { return mimeType_; }

    QJsonObject toJson() const;

signals:
    void outputChanged();

private:
    void setupUI();
    void updateDisplay();

    QString output_;
    QString error_;
    QString mimeType_;

    QVBoxLayout* layout_;
    QLabel* outputLabel_;
    QLabel* errorLabel_;
    QScrollArea* scrollArea_;
};

// ============================================================================
// NotebookKernel Class
// ============================================================================

class NotebookKernel : public QObject
{
    Q_OBJECT

public:
    explicit NotebookKernel(QObject* parent = nullptr);
    ~NotebookKernel();

    void setLanguage(const QString& language);
    QString getLanguage() const { return language_; }

    void executeCode(const QString& code, const QString& cellId);
    void interrupt();
    bool isRunning() const { return isRunning_; }

signals:
    void executionStarted(const QString& cellId);
    void executionFinished(const QString& cellId, const QString& output, const QString& error);
    void executionError(const QString& cellId, const QString& error);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();

private:
    void startKernel();
    void stopKernel();

    QString language_;
    bool isRunning_;
    QProcess* process_;
    QString currentCellId_;
    QString outputBuffer_;
    QString errorBuffer_;
};

// ============================================================================
// NotebookWidget Class
// ============================================================================

class NotebookWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NotebookWidget(QWidget* parent = nullptr);
    ~NotebookWidget();

    // File operations
    bool loadNotebook(const QString& filePath);
    bool saveNotebook(const QString& filePath = QString());
    bool saveNotebookAs();
    bool createNewNotebook();

    // Cell operations
    void addCell(NotebookCell::CellType type = NotebookCell::Code, int index = -1);
    void deleteCell(int index);
    void moveCell(int fromIndex, int toIndex);
    void executeCell(int index);
    void executeAllCells();
    void clearAllOutputs();

    // Kernel operations
    void setKernelLanguage(const QString& language);
    void restartKernel();
    void interruptKernel();

    // UI operations
    void refresh();
    void setReadOnly(bool readOnly);
    bool isReadOnly() const { return readOnly_; }

    QString getCurrentFilePath() const { return currentFilePath_; }
    QString getTitle() const;

signals:
    void notebookLoaded(const QString& filePath);
    void notebookSaved(const QString& filePath);
    void cellExecuted(int index, const QString& output);
    void kernelRestarted();
    void titleChanged(const QString& title);
    void cellAdded();
    void cellDeleted();
    void cellMoved();
    void modified();

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onNewNotebook();
    void onOpenNotebook();
    void onSaveNotebook();
    void onSaveAsNotebook();
    void onExportNotebook();

    void onAddCodeCell();
    void onAddMarkdownCell();
    void onAddRawCell();
    void onDeleteCell();
    void onMoveCellUp();
    void onMoveCellDown();
    void onExecuteCell();
    void onExecuteAllCells();
    void onClearOutputs();

    void onKernelLanguageChanged(const QString& language);
    void onRestartKernel();
    void onInterruptKernel();

    void onCellExecuted(const QString& cellId, const QString& output, const QString& error);
    void onCellAdded();
    void onCellDeleted();
    void onCellMoved();

    void updateToolbar();
    void updateWindowTitle();

private:
    void setupUI();
    void setupToolbar();
    void setupCellList();
    void setupKernelControls();
    void setupConnections();

    void loadNotebookFromJson(const QJsonObject& notebook);
    QJsonObject saveNotebookToJson() const;

    NotebookCell* getCellById(const QString& cellId) const;
    int getCellIndex(NotebookCell* cell) const;
    QString generateCellId() const;

    void updateCellNumbers();
    void scrollToCell(int index);

    bool checkUnsavedChanges();
    void markAsModified();

    // UI components
    QVBoxLayout* mainLayout_;
    QWidget* toolbarWidget_;
    QHBoxLayout* toolbarLayout_;

    // File operations
    QPushButton* newBtn_;
    QPushButton* openBtn_;
    QPushButton* saveBtn_;
    QPushButton* saveAsBtn_;
    QPushButton* exportBtn_;

    // Cell operations
    QPushButton* addCodeBtn_;
    QPushButton* addMarkdownBtn_;
    QPushButton* addRawBtn_;
    QPushButton* deleteCellBtn_;
    QPushButton* moveUpBtn_;
    QPushButton* moveDownBtn_;
    QPushButton* executeBtn_;
    QPushButton* executeAllBtn_;
    QPushButton* clearBtn_;

    // Kernel controls
    QLabel* kernelLabel_;
    QComboBox* kernelCombo_;
    QPushButton* restartKernelBtn_;
    QPushButton* interruptBtn_;
    QLabel* statusLabel_;

    // Main content
    QScrollArea* scrollArea_;
    QWidget* cellsContainer_;
    QVBoxLayout* cellsLayout_;

    // Kernel
    NotebookKernel* kernel_;
    QThread* kernelThread_;

    // State
    QString currentFilePath_;
    QString notebookTitle_;
    bool readOnly_;
    bool modified_;
    QList<NotebookCell*> cells_;
    QString currentKernelLanguage_;

    // Settings
    QSettings* settings_;
};

#endif // NOTEBOOK_WIDGET_H