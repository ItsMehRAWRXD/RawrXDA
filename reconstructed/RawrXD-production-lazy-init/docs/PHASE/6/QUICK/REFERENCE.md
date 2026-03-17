# Phase 6 Quick Reference Guide

## Terminal Panel API

### Creating a Terminal Panel
```cpp
#include "src/qtapp/widgets/TerminalPanel.h"

// In MainWindow
TerminalPanel* terminalPanel = new TerminalPanel(this);
addDockWidget(Qt::BottomDockWidgetArea, terminalPanel);
```

### Adding a Terminal Tab
```cpp
// Add new terminal with default shell
TerminalTab* tab = terminalPanel->addNewTerminal();

// Add terminal with specific shell
TerminalTab* tab = terminalPanel->addNewTerminal("powershell");

// Add terminal with full configuration
TerminalConfig config;
config.shell = "/bin/bash";
config.workingDirectory = "/home/user/project";
config.initialArgs = QStringList() << "-l" << "-i";
config.environment["MY_VAR"] = "value";
TerminalTab* tab = terminalPanel->addTerminal(config, "My Terminal");
```

### Executing Commands
```cpp
// Execute command in current terminal
terminalPanel->execute("ls -la");

// Get current terminal and execute
if (TerminalTab* current = terminalPanel->getCurrentTerminal())
    current->sendCommand("make build");

// Execute in specific terminal by index
if (TerminalTab* tab = terminalPanel->getTerminal(0))
    tab->sendCommand("git status");
```

### Process Control
```cpp
// Stop current terminal gracefully
terminalPanel->stopCurrent();

// Kill current terminal forcefully
terminalPanel->killCurrent();

// Clear current terminal output
terminalPanel->clearCurrent();

// Check if terminal is running
if (TerminalTab* tab = terminalPanel->getCurrentTerminal())
    if (tab->isRunning())
        qDebug() << "Terminal is active";
```

### Terminal Tab Operations
```cpp
// Get terminal by index
TerminalTab* tab = terminalPanel->getTerminal(0);

// Get current active terminal
TerminalTab* current = terminalPanel->getCurrentTerminal();

// Get terminal count
int count = terminalPanel->getTerminalCount();

// Remove terminal by index
terminalPanel->removeTerminal(0);

// Remove all terminals
terminalPanel->removeAllTerminals();
```

### Shell Management
```cpp
// Get available shells
QStringList shells = terminalPanel->getAvailableShells();
// Returns: ["cmd.exe", "powershell", "bash", etc.]

// Set default shell
terminalPanel->setDefaultShell("powershell");

// Get current shell
QString shell = terminalPanel->getCurrentShell();
```

### Configuration
```cpp
// Set font for all terminals
terminalPanel->setFontFamily("Consolas");
terminalPanel->setFontSize(12);

// Set buffer size
terminalPanel->setBufferSize(20000); // 20K lines

// Set output update interval
terminalPanel->setOutputUpdateInterval(100); // 100ms

// Add environment variable
terminalPanel->setEnvironmentVariable("PATH", "/usr/local/bin:$PATH");
```

### Session Management
```cpp
// Save all terminal tabs to JSON
terminalPanel->saveSessionTabs("session.json");

// Load terminals from JSON
terminalPanel->loadSessionTabs("session.json");

// Session format:
// {
//   "tabs": [
//     {
//       "name": "Main",
//       "shell": "cmd.exe",
//       "workdir": "C:\\project"
//     },
//     {
//       "name": "Build",
//       "shell": "powershell",
//       "workdir": "C:\\project\\build"
//     }
//   ]
// }
```

---

## Terminal Tab API

### Starting/Stopping
```cpp
TerminalTab* tab = terminalPanel->getCurrentTerminal();

// Start terminal
tab->start();

// Stop gracefully (SIGTERM)
tab->stop();

// Kill forcefully (SIGKILL)
tab->kill();

// Check status
bool running = tab->isRunning();
```

### Sending Input
```cpp
// Send command (with Enter key)
tab->sendCommand("git status");

// Send raw text (without Enter)
tab->sendInput("hello");

// Send special key
tab->sendKey(Qt::Key_Up);      // Navigate history up
tab->sendKey(Qt::Key_Down);    // Navigate history down
tab->sendKey(Qt::Key_Tab);     // Tab completion
tab->sendKey(Qt::Key_Return);  // Press Enter
```

### Configuration
```cpp
// Set working directory
tab->setWorkingDirectory("/home/user/project");

// Set shell
tab->setShell("/bin/bash");

// Set initial arguments
tab->setShellArguments(QStringList() << "-l" << "-i");

// Set environment variable
tab->setEnvironmentVariable("MY_VAR", "my_value");

// Set output buffer size
tab->setBufferSize(5000); // 5K lines

// Set output update interval
tab->setOutputUpdateInterval(50); // 50ms
```

### Display & Editing
```cpp
// Set terminal font
tab->setFontFamily("Consolas");
tab->setFontSize(12);

// Clipboard operations
tab->copy();         // Copy selected text
tab->paste();        // Paste from clipboard
tab->selectAll();    // Select all output

// Clear operations
tab->clearOutput();  // Clear displayed output
tab->clearHistory(); // Clear command history

// Set command history
QStringList history = {"ls", "git status", "make"};
tab->setCommandHistory(history);
```

### State Queries
```cpp
// Get current state
QString shell = tab->getShell();
QString workdir = tab->getWorkingDirectory();
bool running = tab->isRunning();
int exitCode = tab->getExitCode();

// Get environment
QString myVar = tab->getEnvironmentVariable("MY_VAR");

// Get display properties
QString fontFamily = tab->getFontFamily();
int fontSize = tab->getFontSize();
```

### Event Handling
```cpp
// Connect to terminal events
connect(tab, &TerminalTab::outputReceived,
        this, [](const QString& output) {
    qDebug() << "Output:" << output;
});

connect(tab, &TerminalTab::processStarted,
        this, [](){ qDebug() << "Started"; });

connect(tab, &TerminalTab::processFinished,
        this, [](int code){ 
            qDebug() << "Finished with code:" << code; 
        });

connect(tab, &TerminalTab::processError,
        this, [](const QString& error){ 
            qDebug() << "Error:" << error; 
        });

connect(tab, &TerminalTab::commandExecuted,
        this, [](const QString& cmd){ 
            qDebug() << "Executed:" << cmd; 
        });

connect(tab, &TerminalTab::workingDirectoryChanged,
        this, [](const QString& dir){ 
            qDebug() << "Dir changed to:" << dir; 
        });
```

---

## ANSI Color Parser

### Available Colors (16 Standard + 16 Bright)

#### Foreground Colors (Codes 30-37, 90-97)
```cpp
Black       (30/90)     Red         (31/91)
Green       (32/92)     Yellow      (33/93)
Blue        (34/94)     Magenta     (35/95)
Cyan        (36/96)     White       (37/97)
```

#### Background Colors (Codes 40-47, 100-107)
```cpp
Black BG    (40/100)    Red BG      (41/101)
Green BG    (42/102)    Yellow BG   (43/103)
Blue BG     (44/104)    Magenta BG  (45/105)
Cyan BG     (46/106)    White BG    (47/107)
```

#### Text Attributes
```cpp
Bold        (1)         Dim         (2)
Italic      (3)         Underline   (4)
Blink       (5)         Reverse     (7)
Hidden      (8)         Strikethrough (9)
Reset       (0)
```

### Example ANSI Codes

```bash
# Colored output examples
echo "\x1b[31mRed Text\x1b[0m"           # Red text
echo "\x1b[1;32mBold Green\x1b[0m"       # Bold green
echo "\x1b[44mBlue Background\x1b[0m"    # Blue background
echo "\x1b[1;31;44mBold Red on Blue\x1b[0m"  # Combined

# Multi-segment output
echo "\x1b[31mError: \x1b[0m\x1b[33m%s is missing\x1b[0m"
# Displays: "Error: (in red) <filename> is missing (in yellow)"
```

### Parser Usage
```cpp
#include "src/qtapp/widgets/ANSIColorParser.h"

ANSIColorParser parser;

// Parse a line of ANSI-colored text
QList<ANSIColorParser::ANSISegment> segments = 
    parser.parseLine("\x1b[31mRed\x1b[0m \x1b[32mGreen\x1b[0m");

// Apply to QTextEdit
for (const auto& segment : segments) {
    QTextCursor cursor = textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    
    QTextCharFormat format = segment.state.toFormat();
    cursor.insertText(segment.text, format);
}

// Check if text has ANSI codes
if (parser.hasANSICodes(text))
    qDebug() << "Contains color codes";

// Strip ANSI codes for plain text
QString plainText = parser.stripANSICodes(coloredText);
```

---

## Terminal Configuration Examples

### Basic Setup
```cpp
// Create main window
QMainWindow* main = new QMainWindow();

// Create terminal panel
TerminalPanel* terminal = new TerminalPanel();
main->addDockWidget(Qt::BottomDockWidgetArea, terminal);

// Add terminal tab
terminal->addNewTerminal();

// Execute a command
terminal->execute("ls");
```

### Build System Integration
```cpp
// In BuildSystemPanel or BuildPanel
connect(this, &BuildPanel::buildStarted, [this]() {
    TerminalPanel* term = mainWindow()->getTerminalPanel();
    term->addNewTerminal("build-terminal");
    term->execute("cmake --build . --config Release");
});

connect(this, &BuildPanel::buildFinished, [this](bool success) {
    if (success)
        mainWindow()->getTerminalPanel()
            ->execute("echo 'Build completed successfully!'");
});
```

### Git Operations
```cpp
// In version control panel
TerminalTab* tab = terminalPanel->getCurrentTerminal();

// Clone repository
tab->sendCommand("git clone https://github.com/user/repo.git");

// Check status
tab->sendCommand("git status");

// Commit changes
tab->setWorkingDirectory("/path/to/repo");
tab->sendCommand("git add .");
tab->sendCommand("git commit -m 'Update files'");
```

### File Execution
```cpp
// Execute Python script
TerminalTab* tab = terminalPanel->addNewTerminal();
tab->setWorkingDirectory(scriptDirectory);
tab->sendCommand("python script.py");

// Run compiled executable
tab->setWorkingDirectory(buildDirectory);
tab->sendCommand(".\\application.exe");

// Run with arguments
tab->sendCommand("make test ARGS=\"-v --output junit.xml\"");
```

### Multi-Terminal Workflow
```cpp
// Create separate terminals for different tasks
TerminalTab* devTab = terminalPanel->addNewTerminal("Development");
TerminalTab* buildTab = terminalPanel->addNewTerminal("Build");
TerminalTab* testTab = terminalPanel->addNewTerminal("Tests");

// Set working directories
devTab->setWorkingDirectory(".");
buildTab->setWorkingDirectory("build/");
testTab->setWorkingDirectory("test/");

// Execute parallel tasks
devTab->sendCommand("npm start");
buildTab->sendCommand("cmake --build .");
testTab->sendCommand("npm test");
```

---

## Keyboard Shortcuts

### Terminal Navigation
```
Ctrl+Tab      - Next terminal tab
Ctrl+Shift+Tab - Previous terminal tab
Ctrl+N        - New terminal
Ctrl+W        - Close current terminal
```

### Command Line Editing
```
Up/Down       - Command history navigation
Home/End      - Beginning/end of line
Ctrl+A        - Select all
Ctrl+C        - Send Ctrl+C to process
Ctrl+D        - Send EOF
```

### Copy/Paste
```
Ctrl+C        - Copy selected text (or send Ctrl+C)
Ctrl+V        - Paste from clipboard
Ctrl+Shift+C  - Copy (without sending to terminal)
```

---

## Common Tasks

### Running a Build
```cpp
// In editor's Run menu
TerminalPanel* term = mainWindow()->getTerminalPanel();
term->addNewTerminal("build");
term->execute("cmake --build . --config Release");
```

### Monitoring Build Progress
```cpp
// In BuildPanel
TerminalTab* buildTab = terminalPanel->getTerminal(buildIndex);
connect(buildTab, &TerminalTab::outputReceived,
        this, &BuildPanel::onBuildOutput);

void BuildPanel::onBuildOutput(const QString& output)
{
    if (output.contains("error"))
        statusLabel->setText("Build error!");
    else if (output.contains("warning"))
        statusLabel->setText("Build warning");
}
```

### Git Status Display
```cpp
// Get git status in terminal
TerminalTab* vcsTab = terminalPanel->addNewTerminal("vcs");
vcsTab->setWorkingDirectory(projectPath);
vcsTab->sendCommand("git status");

// Parse output for UI update
connect(vcsTab, &TerminalTab::commandExecuted,
        this, &VersionControlPanel::updateStatus);
```

### Script Execution
```cpp
// Run with output capture
TerminalTab* scriptTab = terminalPanel->getCurrentTerminal();
scriptTab->setWorkingDirectory(projectDir);

connect(scriptTab, &TerminalTab::outputReceived,
        this, [this](const QString& output) {
    // Process output
    if (output.contains("PASS"))
        passCount++;
    else if (output.contains("FAIL"))
        failCount++;
});

scriptTab->sendCommand("python test_runner.py");
```

---

## Troubleshooting

### Terminal Not Starting
```cpp
// Check if shell is available
if (terminalPanel->getAvailableShells().isEmpty())
    qDebug() << "No shells detected!";

// Fallback to cmd.exe on Windows
terminalPanel->setDefaultShell("cmd.exe");
```

### No Output Displayed
```cpp
// Check if process is running
if (!tab->isRunning())
    qDebug() << "Process not started";

// Verify working directory
qDebug() << "Working dir:" << tab->getWorkingDirectory();

// Check output buffer size
tab->setBufferSize(5000);
```

### ANSI Colors Not Rendering
```cpp
// Verify parser is working
ANSIColorParser parser;
if (!parser.hasANSICodes(output))
    qDebug() << "Output has no ANSI codes";

// Check terminal output widget
// Ensure QTextEdit supports rich text
textEdit->setAcceptRichText(true);
```

### Slow Performance
```cpp
// Reduce update frequency
terminalPanel->setOutputUpdateInterval(200); // 200ms

// Reduce buffer size
terminalPanel->setBufferSize(1000); // 1K lines

// Check for CPU-intensive commands
// in connected output handlers
```

---

## Version Information

- **Phase**: 6 (Integrated Terminal)
- **Version**: 1.0.0
- **Qt Version**: 6.x
- **Platforms**: Windows, Linux, macOS
- **Status**: ✅ Production Ready

---

## References

- **Complete Feature Documentation**: `PHASE_6_TERMINAL_COMPLETE.md`
- **Completion Summary**: `PHASE_6_COMPLETION_SUMMARY.md`
- **Source Code**: `src/qtapp/widgets/TerminalPanel.*`, `TerminalTab.*`, `ANSIColorParser.*`

