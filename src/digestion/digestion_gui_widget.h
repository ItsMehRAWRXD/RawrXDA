// digestion_gui_widget.h
#pragma once
#include "digestion_reverse_engineering.h"

class DigestionGuiWidget {public:
    explicit DigestionGuiWidget(void* parent = nullptr);
    void setRootDirectory(const std::string &path);

\nprivate:\n    void startDigestion();
    void stopDigestion();
    void onProgress(int done, int total, int stubs, int percent);
    void onFileScanned(const std::string &path, const std::string &lang, int stubs);
    void onFinished(const void* &report, int64_t elapsed);
    void browseDirectory();

private:
    DigestionReverseEngineeringSystem *m_digester;
    voidEdit *m_pathEdit;
    void *m_progressBar;
    void *m_resultsTable;  // HWND list-view control (was QTableWidget*)
    void *m_startBtn;
    void *m_stopBtn;
    void *m_applyFixesCheck;
    void *m_gitModeCheck;
    void *m_incrementalCheck;
};

