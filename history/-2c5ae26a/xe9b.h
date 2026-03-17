/**
 * @file spreadsheet_widget.h
 * @brief Header for SpreadsheetWidget - Embedded spreadsheet functionality
 */

#ifndef SPREADSHEET_WIDGET_H
#define SPREADSHEET_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QSpinBox>
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
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QList>
#include <QVector>
#include <QPair>
#include <QMap>
#include <QSet>
#include <QStack>
#include <QQueue>
#include <cmath>
#include <algorithm>
#include <functional>

// ============================================================================
// Forward Declarations and Type Definitions
// ============================================================================

class SpreadsheetCell;
class SpreadsheetFormula;
class SpreadsheetRange;
class SpreadsheetWidget;

// ============================================================================
// SpreadsheetCell Class
// ============================================================================

class SpreadsheetCell : public QTableWidgetItem
{
public:
    enum CellType {
        Text = 0,
        Number = 1,
        Formula = 2,
        Date = 3,
        Boolean = 4
    };

    SpreadsheetCell();
    SpreadsheetCell(const QString& text);
    ~SpreadsheetCell();

    void setType(CellType type);
    CellType getType() const { return type_; }

    void setFormula(const QString& formula);
    QString getFormula() const { return formula_; }

    void setValue(const QVariant& value);
    QVariant getValue() const { return value_; }

    void setDisplayText(const QString& text);
    QString getDisplayText() const;

    bool isEmpty() const;
    void clear();

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);

    // Formula evaluation
    QVariant evaluateFormula(const QMap<QString, SpreadsheetCell*>& cells);
    
    // Make evaluateFunction public for SpreadsheetFormula access
    QVariant evaluateFunction(const QString& func, const QList<QVariant>& args);

private:
    CellType type_;
    QString formula_;
    QVariant value_;

    QVariant evaluateExpression(const QString& expr, const QMap<QString, SpreadsheetCell*>& cells);
    QList<QVariant> parseArguments(const QString& argsStr, const QMap<QString, SpreadsheetCell*>& cells);
    SpreadsheetRange parseRange(const QString& rangeStr);
};

// ============================================================================
// SpreadsheetRange Class
// ============================================================================

class SpreadsheetRange
{
public:
    SpreadsheetRange();
    SpreadsheetRange(int startRow, int startCol, int endRow, int endCol);
    SpreadsheetRange(const QString& rangeStr);

    bool isValid() const;
    bool contains(int row, int col) const;
    QList<QPair<int, int>> getCells() const;

    int startRow() const { return startRow_; }
    int startCol() const { return startCol_; }
    int endRow() const { return endRow_; }
    int endCol() const { return endCol_; }

    QString toString() const;

private:
    int startRow_;
    int startCol_;
    int endRow_;
    int endCol_;
};

// ============================================================================
// SpreadsheetFormula Class
// ============================================================================

class SpreadsheetFormula
{
public:
    static QVariant evaluate(const QString& formula, const QMap<QString, SpreadsheetCell*>& cells);
    static bool isValidFormula(const QString& formula);
    static QStringList extractDependencies(const QString& formula);

private:
    static QVariant parseExpression(const QString& expr, const QMap<QString, SpreadsheetCell*>& cells);
    static QVariant applyOperator(QVariant left, QVariant right, const QString& op);
    static QVariant callFunction(const QString& name, const QList<QVariant>& args);
};

// ============================================================================
// SpreadsheetWidget Class
// ============================================================================

class SpreadsheetWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SpreadsheetWidget(QWidget* parent = nullptr);
    ~SpreadsheetWidget();

    // File operations
    bool loadSpreadsheet(const QString& filePath);
    bool saveSpreadsheet(const QString& filePath = QString());
    bool createNewSpreadsheet(int rows = 100, int cols = 26);

    // Cell operations
    void setCellValue(int row, int col, const QVariant& value);
    QVariant getCellValue(int row, int col) const;
    void setCellFormula(int row, int col, const QString& formula);
    QString getCellFormula(int row, int col) const;

    // Range operations
    void setRangeValues(const SpreadsheetRange& range, const QList<QVariant>& values);
    QList<QVariant> getRangeValues(const SpreadsheetRange& range) const;
    void clearRange(const SpreadsheetRange& range);

    // Sheet operations
    void insertRow(int row);
    void deleteRow(int row);
    void insertColumn(int col);
    void deleteColumn(int col);

    // Formula operations
    void recalculateAll();
    void recalculateRange(const SpreadsheetRange& range);

    // UI operations
    void refresh();
    void setReadOnly(bool readOnly);
    bool isReadOnly() const { return readOnly_; }

    QString getCurrentFilePath() const { return currentFilePath_; }
    QString getTitle() const;

    // Utility functions
    QString columnToString(int col) const;
    int stringToColumn(const QString& str) const;
    QString cellReference(int row, int col) const;
    QPair<int, int> parseCellReference(const QString& ref) const;

signals:
    void spreadsheetLoaded(const QString& filePath);
    void spreadsheetSaved(const QString& filePath);
    void cellChanged(int row, int col, const QVariant& value);
    void selectionChanged(const SpreadsheetRange& range);
    void formulaError(int row, int col, const QString& error);
    void titleChanged(const QString& title);

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onNewSpreadsheet();
    void onOpenSpreadsheet();
    void onSaveSpreadsheet();
    void onSaveAsSpreadsheet();
    void onExportSpreadsheet();

    void onCellChanged(int row, int col);
    void onCellDoubleClicked(int row, int col);
    void onSelectionChanged();
    void onContextMenuRequested(const QPoint& pos);

    void onCopy();
    void onCut();
    void onPaste();
    void onClear();
    void onInsertRow();
    void onDeleteRow();
    void onInsertColumn();
    void onDeleteColumn();

    void onFormatCell();
    void onSortRange();
    void onFilterRange();

    void updateToolbar();
    void updateFormulaBar();
    void updateStatusBar();

private:
    void setupUI();
    void setupToolbar();
    void setupFormulaBar();
    void setupTable();
    void setupStatusBar();
    void setupConnections();

    void loadSpreadsheetFromJson(const QJsonObject& spreadsheet);
    QJsonObject saveSpreadsheetToJson() const;

    void initializeTable(int rows, int cols);
    void updateTableHeaders();
    void updateCellDisplay(int row, int col);

    SpreadsheetCell* getCell(int row, int col) const;
    void setCell(int row, int col, SpreadsheetCell* cell);

    void copyToClipboard(const SpreadsheetRange& range);
    void pasteFromClipboard(int startRow, int startCol);
    bool hasCircularReference(int row, int col, const QString& formula);

    void sortRange(const SpreadsheetRange& range, int column, bool ascending);
    void filterRange(const SpreadsheetRange& range, int column, const QString& criteria);

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

    // Edit operations
    QPushButton* copyBtn_;
    QPushButton* cutBtn_;
    QPushButton* pasteBtn_;
    QPushButton* clearBtn_;

    // Structure operations
    QPushButton* insertRowBtn_;
    QPushButton* deleteRowBtn_;
    QPushButton* insertColBtn_;
    QPushButton* deleteColBtn_;

    // Data operations
    QPushButton* sortBtn_;
    QPushButton* filterBtn_;
    QPushButton* formatBtn_;

    // Formula bar
    QWidget* formulaBarWidget_;
    QLabel* cellRefLabel_;
    QLineEdit* formulaEdit_;

    // Main table
    QTableWidget* table_;

    // Status bar
    QWidget* statusWidget_;
    QLabel* statusLabel_;
    QLabel* selectionLabel_;
    QLabel* zoomLabel_;

    // State
    QString currentFilePath_;
    QString spreadsheetTitle_;
    bool readOnly_;
    bool modified_;
    QMap<QString, SpreadsheetCell*> cells_; // Key: "A1", "B2", etc.
    SpreadsheetRange currentSelection_;

    // Settings
    QSettings* settings_;
};

#endif // SPREADSHEET_WIDGET_H