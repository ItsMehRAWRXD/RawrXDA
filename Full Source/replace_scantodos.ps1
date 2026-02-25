$filePath = "src\qtapp\MainWindow_v5.cpp"
$content = Get-Content $filePath -Raw

$oldCode = @"
void MainWindow::scanCodeForTodos()
{
    if (!m_todoManager || !m_fileBrowser) return;
    
    // TODO: Implement recursive scan of project files for // TODO: comments
    QMessageBox::information(this, "Scan for TODOs",
        "This will scan all project files for TODO comments.\n\n"
        "Feature coming soon!");
}
"@

$newCode = @"
void MainWindow::scanCodeForTodos()
{
    if (!m_todoManager) return;
    
    // Get current project directory (use current working directory)
    QString projectDir = QDir::currentPath();
    
    // Allow user to select directory
    QString selectedDir = QFileDialog::getExistingDirectory(
        this,
        "Select Project Directory to Scan",
        projectDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (selectedDir.isEmpty()) return;
    projectDir = selectedDir;
    
    // Confirm scan
    auto reply = QMessageBox::question(this, "Scan for TODOs",
        QString("Scan all source files in:\n%1\n\nfor TODO/FIXME/XXX comments?").arg(projectDir),
        QMessageBox::Yes | QMessageBox::Cancel);
    
    if (reply != QMessageBox::Yes) return;
    
    // Scan recursively with production-grade implementation
    int foundCount = 0;
    QStringList filters;
    filters << "*.cpp" << "*.h" << "*.hpp" << "*.c" << "*.cc" << "*.cxx"
            << "*.py" << "*.js" << "*.ts" << "*.java" << "*.cs" << "*.rs"
            << "*.go" << "*.rb" << "*.php" << "*.swift" << "*.kt" << "*.scala"
            << "*.md" << "*.txt" << "*.cmake" << "CMakeLists.txt";
    
    QDirIterator it(projectDir, filters, QDir::Files | QDir::NoSymLinks, 
                    QDirIterator::Subdirectories);
    
    // Production regex pattern for TODO comment detection
    QRegularExpression todoRegex(
        R"((//|#|;|<!--|/\*)\s*(TODO|FIXME|XXX|HACK|NOTE|BUG)(:|\s+)(.*))",
        QRegularExpression::CaseInsensitiveOption
    );
    
    qDebug() << "[scanCodeForTodos] Starting scan in:" << projectDir;
    
    while (it.hasNext()) {
        QString filePath = it.next();
        
        // Skip build directories and version control
        if (filePath.contains("/build/") || filePath.contains("\\build\\") ||
            filePath.contains("/build_") || filePath.contains("\\build_") ||
            filePath.contains("/.git/") || filePath.contains("\\.git\\") ||
            filePath.contains("/.vs/") || filePath.contains("\\.vs\\")) {
            continue;
        }
        
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "[scanCodeForTodos] Failed to open:" << filePath;
            continue;
        }
        
        QTextStream in(&file);
        int lineNum = 0;
        while (!in.atEnd()) {
            lineNum++;
            QString line = in.readLine();
            
            QRegularExpressionMatch match = todoRegex.match(line);
            if (match.hasMatch()) {
                QString todoType = match.captured(2).toUpper();
                QString todoText = match.captured(4).trimmed();
                
                if (todoText.isEmpty()) {
                    todoText = QString("[%1]").arg(todoType);
                } else {
                    todoText = QString("[%1] %2").arg(todoType, todoText);
                }
                
                m_todoManager->addTodo(todoText, filePath, lineNum);
                foundCount++;
                
                qDebug() << QString("[scanCodeForTodos] Found at %1:%2 - %3")
                            .arg(filePath).arg(lineNum).arg(todoText);
            }
        }
        file.close();
    }
    
    qDebug() << QString("[scanCodeForTodos] Scan complete: %1 items found").arg(foundCount);
    statusBar()->showMessage(QString("Scan complete: %1 TODO items found").arg(foundCount), 5000);
    
    QMessageBox::information(this, "Scan Complete",
        QString("Found %1 TODO/FIXME/XXX comments in project files.\n\n"
                "Items have been added to the TODO panel.").arg(foundCount));
}
"@

$content = $content -replace [regex]::Escape($oldCode), $newCode
Set-Content -Path $filePath -Value $content -NoNewline
Write-Host "File updated successfully"
