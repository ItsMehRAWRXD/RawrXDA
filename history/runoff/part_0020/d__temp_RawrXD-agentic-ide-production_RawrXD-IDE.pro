QT += core gui concurrent
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++20
CONFIG += hide_symbols

TARGET = RawrXD-IDE
TEMPLATE = app

SOURCES += \
    src/ModelLoaderBridge.cpp \
    src/ModelCacheManager.cpp \
    src/InferenceSession.cpp \
    src/TokenStreamRouter.cpp \
    src/ModelSelectionDialog.cpp \
    src/IDEIntegration.cpp \
    src/PerformanceMonitor.cpp \
    src/ModelMetadataParser.cpp \
    src/StreamingInferenceEngine.cpp

HEADERS += \
    src/ModelLoaderBridge.h \
    src/ModelCacheManager.h \
    src/InferenceSession.h \
    src/TokenStreamRouter.h \
    src/ModelSelectionDialog.h \
    src/IDEIntegration.h \
    src/PerformanceMonitor.h \
    src/ModelMetadataParser.h \
    src/StreamingInferenceEngine.h

# Sovereign Loader integration paths
SOVEREIGN_BUILD = $$PWD/build-sovereign-static/bin

INCLUDEPATH += $$PWD/RawrXD-ModelLoader/include
INCLUDEPATH += $$PWD/RawrXD-ModelLoader/src

# Linker configuration for static Sovereign Loader
win32 {
    # Use .lib file (static linking - preferred)
    LIBS += -L$$SOVEREIGN_BUILD -lRawrXD-SovereignLoader
    
    # Post-link: Copy DLL to output directory for runtime
    QMAKE_POST_LINK += $$quote(cmd /c copy /Y $$SOVEREIGN_BUILD\RawrXD-SovereignLoader.dll $$OUTDIR)
}

unix {
    LIBS += -L$$SOVEREIGN_BUILD -lRawrXD-SovereignLoader
}

# Performance optimization flags
win32:CONFIG(release, debug|release) {
    # MSVC Release: Vectorization, whole program optimization, fast math
    QMAKE_CXXFLAGS += /arch:AVX512 /O2 /W4 /Oi /Ot /GL /fp:fast
    QMAKE_CFLAGS += /arch:AVX512 /O2 /GL /fp:fast
    QMAKE_LFLAGS += /LTCG
} else:win32:CONFIG(debug, debug|release) {
    # MSVC Debug: Full symbols, no optimization
    QMAKE_CXXFLAGS += /ZI /Od /W4
    QMAKE_CFLAGS += /ZI /Od
}

# Version information
VERSION = 1.0.0.0
DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_NO_WARNING_OUTPUT

# Debug vs Release configuration
CONFIG(debug, debug|release) {
    TARGET = RawrXD-IDE-debug
    DEFINES += DEBUG_BUILD
    OUTDIR = $$PWD/build/debug
} else {
    DEFINES += RELEASE_BUILD
    OUTDIR = $$PWD/build/release
}

# Output directories
MOC_DIR = $$OUTDIR/.moc
OBJECTS_DIR = $$OUTDIR/.obj
RCC_DIR = $$OUTDIR/.rcc
UI_DIR = $$OUTDIR/.ui

# Installation settings
target.path = $$OUTDIR/bin
INSTALLS += target

# Visual Studio project files
win32 {
    QMAKE_EXTRA_COMPILERS += masm
    masm.input = MASM_FILES
    masm.variable_out = OBJECTS
    masm.commands = ml64 /c /Fo $$OUTDIR/.obj/asm_$${QMAKE_FILE_BASE}.obj $$QMAKE_FILE_IN
    masm.name = Assembling $$QMAKE_FILE_IN
    masm.silent = true
}

message("========================================")
message("Building RawrXD-IDE v1.0 (Production Release)")
message("Configuration: $$CONFIG")
message("Qt Version: $$QT_VERSION")
message("Sovereign Loader: $$SOVEREIGN_BUILD")
message("Output: $$OUTDIR")
message("========================================")

