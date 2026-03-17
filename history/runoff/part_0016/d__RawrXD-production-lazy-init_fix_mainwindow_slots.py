import os

def patch_profiler():
    path = r"d:\RawrXD-production-lazy-init\src\qtapp\widgets\profiler_widget.h"
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Comment out using namespace if it causes error
    if "using namespace QtCharts;" in content:
        content = content.replace("using namespace QtCharts;", "// using namespace QtCharts;")
    elif "QT_CHARTS_USE_NAMESPACE;" in content:
        content = content.replace("QT_CHARTS_USE_NAMESPACE;", "// QT_CHARTS_USE_NAMESPACE;")
    elif "QT_CHARTS_USE_NAMESPACE" in content:
        content = content.replace("QT_CHARTS_USE_NAMESPACE", "// QT_CHARTS_USE_NAMESPACE")

    with open(path + '.new', 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"Created {path}.new")

def patch_mainwindow():
    path = r"d:\RawrXD-production-lazy-init\src\qtapp\MainWindow.h"
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
        
    # Add missing slots
    if "void toggleCodeStream" not in content:
        # Insert after toggleWhiteboard (which I added hopefully)
        # If toggleWhiteboard not there (failed patch?), use toggleSettings
        
        anchor = "void toggleSettings(bool visible);"
        # Clean up valid insertion point
        insertion_list = [
            "void toggleWhiteboard(bool visible);",
            "void toggleAudioCall(bool visible);",
            "void toggleScreenShare(bool visible);",
            "void toggleCodeStream(bool visible);",
            "void toggleAIReview(bool visible);",
            "void toggleInlineChat(bool visible);",
            "void toggleTimeTracker(bool visible);",
            "void toggleTaskManager(bool visible);",
            "void togglePomodoro(bool visible);"
        ]
        
        insert_str = "\n    " + "\n    ".join(insertion_list)
        
        # Check if I already added some
        if "void toggleWhiteboard" in content:
             # Just add rest
             rest = [
                 "void toggleCodeStream(bool visible);",
                 "void toggleAIReview(bool visible);",
                 "void toggleInlineChat(bool visible);",
                 "void toggleTimeTracker(bool visible);",
                 "void toggleTaskManager(bool visible);",
                 "void togglePomodoro(bool visible);"
             ]
             anchor = "void toggleScreenShare(bool visible);"
             if anchor in content:
                 content = content.replace(anchor, anchor + "\n    " + "\n    ".join(rest))
        else:
             if anchor in content:
                 content = content.replace(anchor, anchor + insert_str)
    
    with open(path + '.new', 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"Created {path}.new")

if __name__ == "__main__":
    patch_profiler()
    patch_mainwindow()
