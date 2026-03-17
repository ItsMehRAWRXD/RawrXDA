import os

def patch_profiler():
    path = r"d:\RawrXD-production-lazy-init\src\qtapp\widgets\profiler_widget.h"
    if not os.path.exists(path):
        print(f"File not found: {path}")
        return
        
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
        
    if "QT_CHARTS_USE_NAMESPACE;" not in content:
        print("Patching profiler_widget.h...")
        content = content.replace("QT_CHARTS_USE_NAMESPACE", "QT_CHARTS_USE_NAMESPACE;")
        with open(path, 'w', encoding='utf-8') as f:
            f.write(content)
        print("Patched.")
    else:
        print("profiler_widget.h already patched.")

def patch_mainwindow():
    path = r"d:\RawrXD-production-lazy-init\src\qtapp\MainWindow.h"
    if not os.path.exists(path):
        print(f"File not found: {path}")
        return

    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    modified = False
    
    # Forward declarations
    if "class RunDebugWidget;" not in content:
        print("Patching MainWindow.h forward decls...")
        content = content.replace("class LatencyStatusPanel;", 
                                "class LatencyStatusPanel;\n    class ProfilerWidget;\n    class RunDebugWidget;\n    class DatabaseToolWidget;\n    class SnippetManagerWidget;")
        modified = True
    
    # Members
    replacements = [
        ("QPointer<RunDebugWidget>", "QPointer<RawrXD::RunDebugWidget>"),
        ("QPointer<ProfilerWidget>", "QPointer<RawrXD::ProfilerWidget>"),
        ("QPointer<DatabaseToolWidget>", "QPointer<RawrXD::DatabaseToolWidget>"),
        ("QPointer<SnippetManagerWidget>", "QPointer<RawrXD::SnippetManagerWidget>")
    ]
    
    for old, new in replacements:
        if old in content and new not in content:
            print(f"Patching {old}...")
            content = content.replace(old, new)
            modified = True
            
    if modified:
        with open(path, 'w', encoding='utf-8') as f:
            f.write(content)
        print("MainWindow.h patched.")
    else:
        print("MainWindow.h already patched.")

if __name__ == "__main__":
    patch_profiler()
    patch_mainwindow()
