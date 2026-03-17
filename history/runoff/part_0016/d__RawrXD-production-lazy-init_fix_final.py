import os

def patch_mainwindow_h():
    path = r"d:\RawrXD-production-lazy-init\src\qtapp\MainWindow.h"
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
        
    # Add missing slots
    if "void toggleAccessibility" not in content:
        anchor = "void togglePomodoro(bool visible);"
        insertion = "\n    void toggleAccessibility(bool visible);\n    void toggleWallpaper(bool visible);"
        if anchor in content:
            content = content.replace(anchor, anchor + insertion)
    
    with open(path + '.new', 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"Created {path}.new")

def patch_mainwindow_cpp():
    path = r"d:\RawrXD-production-lazy-init\src\qtapp\MainWindow.cpp"
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
        
    if "using namespace RawrXD;" not in content:
        content = "#include \"MainWindow.h\"\nusing namespace RawrXD;\n" + content.replace("#include \"MainWindow.h\"\n", "")
        
    with open(path + '.new', 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"Created {path}.new")

if __name__ == "__main__":
    patch_mainwindow_h()
    patch_mainwindow_cpp()
