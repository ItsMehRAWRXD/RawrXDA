import os

def patch_profiler():
    path = r"d:\RawrXD-production-lazy-init\src\qtapp\widgets\profiler_widget.h"
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Replace macro with standard using
    if "QT_CHARTS_USE_NAMESPACE;" in content:
        content = content.replace("QT_CHARTS_USE_NAMESPACE;", "using namespace QtCharts;")
    elif "QT_CHARTS_USE_NAMESPACE" in content:
        content = content.replace("QT_CHARTS_USE_NAMESPACE", "using namespace QtCharts;")

    with open(path + '.new', 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"Created {path}.new")

def patch_mainwindow():
    path = r"d:\RawrXD-production-lazy-init\src\qtapp\MainWindow.h"
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
        
    # Add missing slots
    if "void toggleWhiteboard" not in content:
        # Insert after toggleSettings
        anchor = "void toggleSettings(bool visible);"
        insertion = "\n    void toggleWhiteboard(bool visible);\n    void toggleAudioCall(bool visible);\n    void toggleScreenShare(bool visible);"
        if anchor in content:
            content = content.replace(anchor, anchor + insertion)
    
    # Add missing members
    if "bool m_showHiddenFiles_" not in content:
        # Insert at end of original members
        anchor = "bool m_autonomousMode{false}; // New: State for self-healing autonomy"
        insertion = "\n    bool m_showHiddenFiles_{false};\n    bool m_showDrives_{false};"
        if anchor in content:
            content = content.replace(anchor, anchor + insertion)
            
    # Add missing method
    if "void refreshDriveList();" not in content:
        anchor = "void refreshDriveRoots();"
        insertion = "\n    void refreshDriveList();"
        if anchor in content:
            content = content.replace(anchor, anchor + insertion)

    with open(path + '.new', 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"Created {path}.new")

if __name__ == "__main__":
    patch_profiler()
    patch_mainwindow()
