#pragma once

#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>

namespace RawrXD::Schematic {

enum class ApprovalState {
    Pending = 0,
    Approved = 1,
    Rejected = 2,
    Modified = 3
};

enum class MarkType {
    Check = 0,
    Cross = 1,
    Arrow = 2,
    Question = 3
};

struct Mark {
    int x = 0;
    int y = 0;
    MarkType type = MarkType::Check;
};

struct Node {
    std::string id;
    std::string label;
    uint64_t caps = 0;
    RECT bounds = {0, 0, 0, 0};
    ApprovalState state = ApprovalState::Pending;
    std::vector<Mark> marks;
};

struct Wire {
    std::string fromId;
    std::string toId;
    uint64_t caps = 0;
};

struct StudioConfig {
    std::string inputPath;
    std::string outputPath;
    bool autoSave = true;
    int autoSaveMs = 500;
    bool showLabels = true;
};

class Model {
public:
    bool loadFromFile(const std::string& path);
    bool saveToFile(const std::string& path) const;

    void clear();
    void addNode(const Node& node);
    void addWire(const Wire& wire);

    std::vector<Node>& nodes();
    const std::vector<Node>& nodes() const;
    std::vector<Wire>& wires();
    const std::vector<Wire>& wires() const;

    int findNodeAt(int x, int y) const;
    int findNodeById(const std::string& id) const;

    void updateNodePosition(int index, int x, int y);
    void setNodeState(int index, ApprovalState state);
    void addNodeMark(int index, const Mark& mark);
    void clearNodeMarks(int index);

private:
    std::vector<Node> m_nodes;
    std::vector<Wire> m_wires;
};

class StudioWindow {
public:
    StudioWindow();
    ~StudioWindow();

    bool initialize(HINSTANCE instance, const StudioConfig& config);
    int run();

private:
    HWND m_hwnd = nullptr;
    HINSTANCE m_instance = nullptr;
    StudioConfig m_config{};
    Model m_model;
    bool m_dirty = true;
    int m_selectedIndex = -1;
    bool m_dragging = false;
    POINT m_dragOffset = {0, 0};

    HDC m_memDC = nullptr;
    HBITMAP m_memBitmap = nullptr;
    int m_width = 1200;
    int m_height = 800;

    FILETIME m_lastInputWrite = {0, 0};

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void createBackBuffer(int width, int height);
    void render();
    void drawGrid();
    void drawWires();
    void drawNodes();
    void drawNodeMarks(const Node& node);

    void drawCheck(int x, int y);
    void drawCross(int x, int y);
    void drawArrow(int x, int y);
    void drawQuestion(int x, int y);

    void onMouseMove(int x, int y);
    void onLButtonDown(int x, int y);
    void onLButtonUp(int x, int y);
    void onRButtonDown(int x, int y);
    void onMButtonDown(int x, int y);
    void onKeyDown(WPARAM key);

    void loadInput();
    void saveOutput();
    void maybeAutoSave();
    void checkForExternalUpdates();

    COLORREF colorForCaps(uint64_t caps) const;
    std::string labelForNode(const Node& node) const;
    bool getFileWriteTime(const std::string& path, FILETIME& outTime) const;
};

int RunSchematicStudio(const StudioConfig& config, HINSTANCE instance);

} // namespace RawrXD::Schematic
