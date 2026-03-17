#include "SchematicStudio.h"
#include "IDELogger.h"
#include <nlohmann/json.hpp>
#include <windowsx.h>
#include <commdlg.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>

using nlohmann::json;

namespace RawrXD::Schematic {

namespace {
    std::string approvalStateToString(ApprovalState state) {
        switch (state) {
            case ApprovalState::Pending: return "pending";
            case ApprovalState::Approved: return "approved";
            case ApprovalState::Rejected: return "rejected";
            case ApprovalState::Modified: return "modified";
            default: return "pending";
        }
    }

    ApprovalState approvalStateFromString(const std::string& value) {
        if (value == "approved") return ApprovalState::Approved;
        if (value == "rejected") return ApprovalState::Rejected;
        if (value == "modified") return ApprovalState::Modified;
        return ApprovalState::Pending;
    }

    std::string markTypeToString(MarkType type) {
        switch (type) {
            case MarkType::Check: return "check";
            case MarkType::Cross: return "cross";
            case MarkType::Arrow: return "arrow";
            case MarkType::Question: return "question";
            default: return "check";
        }
    }

    MarkType markTypeFromString(const std::string& value) {
        if (value == "cross") return MarkType::Cross;
        if (value == "arrow") return MarkType::Arrow;
        if (value == "question") return MarkType::Question;
        return MarkType::Check;
    }

    std::string nowIso8601() {
        auto now = std::chrono::system_clock::now();
        std::time_t time = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf{};
        localtime_s(&tm_buf, &time);
        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
        return oss.str();
    }

    struct ScopedTimer {
        std::chrono::high_resolution_clock::time_point start;
        const char* label;
        ScopedTimer(const char* name) : start(std::chrono::high_resolution_clock::now()), label(name) {}
        ~ScopedTimer() {
            auto end = std::chrono::high_resolution_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            LOG_DEBUG(std::string(label) + " took " + std::to_string(ms) + "ms");
        }
    };
}

bool Model::loadFromFile(const std::string& path)
{
    ScopedTimer timer("Model::loadFromFile");
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_WARNING("Failed to open schematic file: " + path);
        return false;
    }

    json data;
    try {
        file >> data;
    } catch (const std::exception& ex) {
        LOG_ERROR(std::string("JSON parse error: ") + ex.what());
        return false;
    }

    clear();

    if (data.contains("nodes") && data["nodes"].is_array()) {
        for (const auto& nodeJson : data["nodes"]) {
            Node node;
            node.id = nodeJson.value("id", "");
            node.label = nodeJson.value("label", "");
            node.caps = nodeJson.value("caps", 0ull);
            node.state = approvalStateFromString(nodeJson.value("state", "pending"));

            node.bounds.left = nodeJson.value("x", 0);
            node.bounds.top = nodeJson.value("y", 0);
            node.bounds.right = node.bounds.left + nodeJson.value("w", 0);
            node.bounds.bottom = node.bounds.top + nodeJson.value("h", 0);

            if (nodeJson.contains("marks")) {
                for (const auto& markJson : nodeJson["marks"]) {
                    Mark mark;
                    mark.x = markJson.value("x", 0);
                    mark.y = markJson.value("y", 0);
                    mark.type = markTypeFromString(markJson.value("type", "check"));
                    node.marks.push_back(mark);
                }
            }

            m_nodes.push_back(node);
        }
    }

    if (data.contains("wires") && data["wires"].is_array()) {
        for (const auto& wireJson : data["wires"]) {
            Wire wire;
            wire.fromId = wireJson.value("from", "");
            wire.toId = wireJson.value("to", "");
            wire.caps = wireJson.value("caps", 0ull);
            m_wires.push_back(wire);
        }
    }

    LOG_INFO("Loaded schematic with nodes=" + std::to_string(m_nodes.size()) +
             " wires=" + std::to_string(m_wires.size()));
    return true;
}

bool Model::saveToFile(const std::string& path) const
{
    ScopedTimer timer("Model::saveToFile");
    json data;
    data["version"] = 1;
    data["generatedAt"] = nowIso8601();
    data["nodes"] = json::array();
    data["wires"] = json::array();

    for (const auto& node : m_nodes) {
        json nodeJson;
        nodeJson["id"] = node.id;
        nodeJson["label"] = node.label;
        nodeJson["caps"] = node.caps;
        nodeJson["state"] = approvalStateToString(node.state);
        nodeJson["x"] = node.bounds.left;
        nodeJson["y"] = node.bounds.top;
        nodeJson["w"] = node.bounds.right - node.bounds.left;
        nodeJson["h"] = node.bounds.bottom - node.bounds.top;

        nodeJson["marks"] = json::array();
        for (const auto& mark : node.marks) {
            json markJson;
            markJson["x"] = mark.x;
            markJson["y"] = mark.y;
            markJson["type"] = markTypeToString(mark.type);
            nodeJson["marks"].push_back(markJson);
        }
        data["nodes"].push_back(nodeJson);
    }

    for (const auto& wire : m_wires) {
        json wireJson;
        wireJson["from"] = wire.fromId;
        wireJson["to"] = wire.toId;
        wireJson["caps"] = wire.caps;
        data["wires"].push_back(wireJson);
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to write schematic file: " + path);
        return false;
    }

    file << std::setw(2) << data;
    file.flush();
    LOG_INFO("Saved schematic to " + path);
    return true;
}

void Model::clear()
{
    m_nodes.clear();
    m_wires.clear();
}

void Model::addNode(const Node& node)
{
    m_nodes.push_back(node);
}

void Model::addWire(const Wire& wire)
{
    m_wires.push_back(wire);
}

std::vector<Node>& Model::nodes()
{
    return m_nodes;
}

const std::vector<Node>& Model::nodes() const
{
    return m_nodes;
}

std::vector<Wire>& Model::wires()
{
    return m_wires;
}

const std::vector<Wire>& Model::wires() const
{
    return m_wires;
}

int Model::findNodeAt(int x, int y) const
{
    for (int i = static_cast<int>(m_nodes.size()) - 1; i >= 0; --i) {
        const auto& node = m_nodes[static_cast<size_t>(i)];
        if (x >= node.bounds.left && x <= node.bounds.right &&
            y >= node.bounds.top && y <= node.bounds.bottom) {
            return i;
        }
    }
    return -1;
}

int Model::findNodeById(const std::string& id) const
{
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        if (m_nodes[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void Model::updateNodePosition(int index, int x, int y)
{
    if (index < 0 || index >= static_cast<int>(m_nodes.size())) return;
    auto& node = m_nodes[static_cast<size_t>(index)];
    int w = node.bounds.right - node.bounds.left;
    int h = node.bounds.bottom - node.bounds.top;
    node.bounds.left = x;
    node.bounds.top = y;
    node.bounds.right = x + w;
    node.bounds.bottom = y + h;
}

void Model::setNodeState(int index, ApprovalState state)
{
    if (index < 0 || index >= static_cast<int>(m_nodes.size())) return;
    m_nodes[static_cast<size_t>(index)].state = state;
}

void Model::addNodeMark(int index, const Mark& mark)
{
    if (index < 0 || index >= static_cast<int>(m_nodes.size())) return;
    m_nodes[static_cast<size_t>(index)].marks.push_back(mark);
}

void Model::clearNodeMarks(int index)
{
    if (index < 0 || index >= static_cast<int>(m_nodes.size())) return;
    m_nodes[static_cast<size_t>(index)].marks.clear();
}

StudioWindow::StudioWindow()
{
    LOG_INFO("Schematic Studio initialized");
}

StudioWindow::~StudioWindow()
{
    if (m_memBitmap) {
        DeleteObject(m_memBitmap);
        m_memBitmap = nullptr;
    }
    if (m_memDC) {
        DeleteDC(m_memDC);
        m_memDC = nullptr;
    }
}

bool StudioWindow::initialize(HINSTANCE instance, const StudioConfig& config)
{
    m_instance = instance;
    m_config = config;

    LOG_INFO("Schematic Studio loading input: " + m_config.inputPath);
    loadInput();

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = StudioWindow::WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = "RawrXD_SchematicStudio";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassExA(&wc);

    m_hwnd = CreateWindowExA(
        0,
        wc.lpszClassName,
        "RawrXD Schematic Studio",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, m_width, m_height,
        nullptr, nullptr, instance, this);

    if (!m_hwnd) {
        LOG_ERROR("Failed to create Schematic Studio window");
        return false;
    }

    SetTimer(m_hwnd, 1, static_cast<UINT>(m_config.autoSaveMs), nullptr);
    createBackBuffer(m_width, m_height);
    return true;
}

int StudioWindow::run()
{
    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK StudioWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    StudioWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* createStruct = reinterpret_cast<CREATESTRUCTA*>(lParam);
        self = reinterpret_cast<StudioWindow*>(createStruct->lpCreateParams);
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<StudioWindow*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->handleMessage(msg, wParam, lParam);
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

LRESULT StudioWindow::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_SIZE: {
            m_width = LOWORD(lParam);
            m_height = HIWORD(lParam);
            createBackBuffer(m_width, m_height);
            m_dirty = true;
            return 0;
        }
        case WM_TIMER: {
            checkForExternalUpdates();
            maybeAutoSave();
            if (m_dirty) InvalidateRect(m_hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hwnd, &ps);
            render();
            BitBlt(hdc, 0, 0, m_width, m_height, m_memDC, 0, 0, SRCCOPY);
            EndPaint(m_hwnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE:
            onMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_LBUTTONDOWN:
            onLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_LBUTTONUP:
            onLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_RBUTTONDOWN:
            onRButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_MBUTTONDOWN:
            onMButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_KEYDOWN:
            onKeyDown(wParam);
            return 0;
        case WM_DESTROY:
            KillTimer(m_hwnd, 1);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcA(m_hwnd, msg, wParam, lParam);
}

void StudioWindow::createBackBuffer(int width, int height)
{
    if (m_memBitmap) {
        DeleteObject(m_memBitmap);
        m_memBitmap = nullptr;
    }
    if (!m_memDC) {
        HDC screen = GetDC(nullptr);
        m_memDC = CreateCompatibleDC(screen);
        ReleaseDC(nullptr, screen);
    }
    HDC screen = GetDC(nullptr);
    m_memBitmap = CreateCompatibleBitmap(screen, width, height);
    ReleaseDC(nullptr, screen);
    SelectObject(m_memDC, m_memBitmap);
}

void StudioWindow::render()
{
    ScopedTimer timer("StudioWindow::render");
    RECT bounds = {0, 0, m_width, m_height};
    FillRect(m_memDC, &bounds, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));

    drawGrid();
    drawWires();
    drawNodes();

    m_dirty = false;
}

void StudioWindow::drawGrid()
{
    HPEN pen = CreatePen(PS_DOT, 1, RGB(230, 230, 230));
    SelectObject(m_memDC, pen);
    for (int x = 0; x < m_width; x += 20) {
        MoveToEx(m_memDC, x, 0, nullptr);
        LineTo(m_memDC, x, m_height);
    }
    for (int y = 0; y < m_height; y += 20) {
        MoveToEx(m_memDC, 0, y, nullptr);
        LineTo(m_memDC, m_width, y);
    }
    DeleteObject(pen);
}

void StudioWindow::drawWires()
{
    for (const auto& wire : m_model.wires()) {
        int fromIndex = m_model.findNodeById(wire.fromId);
        int toIndex = m_model.findNodeById(wire.toId);
        if (fromIndex < 0 || toIndex < 0) continue;

        const auto& fromNode = m_model.nodes()[static_cast<size_t>(fromIndex)];
        const auto& toNode = m_model.nodes()[static_cast<size_t>(toIndex)];

        POINT from = {
            (fromNode.bounds.left + fromNode.bounds.right) / 2,
            (fromNode.bounds.top + fromNode.bounds.bottom) / 2
        };
        POINT to = {
            (toNode.bounds.left + toNode.bounds.right) / 2,
            (toNode.bounds.top + toNode.bounds.bottom) / 2
        };

        HPEN pen = CreatePen(PS_SOLID, 2, colorForCaps(wire.caps));
        SelectObject(m_memDC, pen);
        MoveToEx(m_memDC, from.x, from.y, nullptr);
        LineTo(m_memDC, to.x, to.y);
        DeleteObject(pen);
    }
}

void StudioWindow::drawNodes()
{
    for (size_t i = 0; i < m_model.nodes().size(); ++i) {
        const auto& node = m_model.nodes()[i];
        COLORREF fill = colorForCaps(node.caps);

        if (node.state == ApprovalState::Rejected) {
            fill = RGB(200, 200, 200);
        } else if (node.state == ApprovalState::Approved) {
            fill = RGB(
                std::min(255, GetRValue(fill) + 40),
                std::min(255, GetGValue(fill) + 40),
                std::min(255, GetBValue(fill) + 40));
        }

        HBRUSH brush = CreateSolidBrush(fill);
        FillRect(m_memDC, &node.bounds, brush);
        DeleteObject(brush);

        HPEN pen = CreatePen(PS_SOLID, (m_selectedIndex == static_cast<int>(i) ? 3 : 1),
            node.state == ApprovalState::Rejected ? RGB(180, 0, 0) : RGB(0, 0, 0));
        SelectObject(m_memDC, pen);
        SelectObject(m_memDC, GetStockObject(NULL_BRUSH));
        Rectangle(m_memDC, node.bounds.left, node.bounds.top, node.bounds.right, node.bounds.bottom);
        DeleteObject(pen);

        if (m_config.showLabels) {
            RECT textRect = node.bounds;
            SetBkMode(m_memDC, TRANSPARENT);
            SetTextColor(m_memDC, RGB(0, 0, 0));
            std::string label = labelForNode(node);
            DrawTextA(m_memDC, label.c_str(), -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        drawNodeMarks(node);

        if (node.state == ApprovalState::Approved) {
            drawCheck(node.bounds.right - 18, node.bounds.top + 4);
        } else if (node.state == ApprovalState::Rejected) {
            drawCross(node.bounds.right - 18, node.bounds.top + 4);
        }
    }
}

void StudioWindow::drawNodeMarks(const Node& node)
{
    for (const auto& mark : node.marks) {
        switch (mark.type) {
            case MarkType::Check:
                drawCheck(mark.x, mark.y);
                break;
            case MarkType::Cross:
                drawCross(mark.x, mark.y);
                break;
            case MarkType::Arrow:
                drawArrow(mark.x, mark.y);
                break;
            case MarkType::Question:
                drawQuestion(mark.x, mark.y);
                break;
        }
    }
}

void StudioWindow::drawCheck(int x, int y)
{
    HPEN pen = CreatePen(PS_SOLID, 2, RGB(0, 140, 0));
    SelectObject(m_memDC, pen);
    MoveToEx(m_memDC, x, y + 6, nullptr);
    LineTo(m_memDC, x + 5, y + 12);
    LineTo(m_memDC, x + 14, y - 2);
    DeleteObject(pen);
}

void StudioWindow::drawCross(int x, int y)
{
    HPEN pen = CreatePen(PS_SOLID, 2, RGB(180, 0, 0));
    SelectObject(m_memDC, pen);
    MoveToEx(m_memDC, x, y, nullptr);
    LineTo(m_memDC, x + 12, y + 12);
    MoveToEx(m_memDC, x + 12, y, nullptr);
    LineTo(m_memDC, x, y + 12);
    DeleteObject(pen);
}

void StudioWindow::drawArrow(int x, int y)
{
    HPEN pen = CreatePen(PS_SOLID, 2, RGB(0, 0, 160));
    SelectObject(m_memDC, pen);
    MoveToEx(m_memDC, x, y, nullptr);
    LineTo(m_memDC, x + 12, y);
    LineTo(m_memDC, x + 8, y - 4);
    MoveToEx(m_memDC, x + 12, y, nullptr);
    LineTo(m_memDC, x + 8, y + 4);
    DeleteObject(pen);
}

void StudioWindow::drawQuestion(int x, int y)
{
    SetTextColor(m_memDC, RGB(180, 120, 0));
    SetBkMode(m_memDC, TRANSPARENT);
    TextOutA(m_memDC, x, y, "?", 1);
}

void StudioWindow::onMouseMove(int x, int y)
{
    if (m_dragging && m_selectedIndex >= 0) {
        int newX = x - m_dragOffset.x;
        int newY = y - m_dragOffset.y;
        m_model.updateNodePosition(m_selectedIndex, newX, newY);
        m_model.setNodeState(m_selectedIndex, ApprovalState::Modified);
        m_dirty = true;
    }
}

void StudioWindow::onLButtonDown(int x, int y)
{
    m_selectedIndex = m_model.findNodeAt(x, y);
    if (m_selectedIndex >= 0) {
        const auto& node = m_model.nodes()[static_cast<size_t>(m_selectedIndex)];
        m_dragging = true;
        m_dragOffset.x = x - node.bounds.left;
        m_dragOffset.y = y - node.bounds.top;
        SetCapture(m_hwnd);
        m_dirty = true;
    }
}

void StudioWindow::onLButtonUp(int x, int y)
{
    if (m_dragging) {
        m_dragging = false;
        ReleaseCapture();
        m_dirty = true;
    }
}

void StudioWindow::onRButtonDown(int x, int y)
{
    int index = m_model.findNodeAt(x, y);
    if (index < 0) return;

    HMENU menu = CreatePopupMenu();
    AppendMenuA(menu, MF_STRING, 1, "Approve");
    AppendMenuA(menu, MF_STRING, 2, "Reject");
    AppendMenuA(menu, MF_STRING, 3, "Reset to Pending");
    AppendMenuA(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(menu, MF_STRING, 4, "Clear Marks");

    POINT pt = {x, y};
    ClientToScreen(m_hwnd, &pt);
    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hwnd, nullptr);
    DestroyMenu(menu);

    if (cmd == 1) {
        m_model.setNodeState(index, ApprovalState::Approved);
    } else if (cmd == 2) {
        m_model.setNodeState(index, ApprovalState::Rejected);
    } else if (cmd == 3) {
        m_model.setNodeState(index, ApprovalState::Pending);
    } else if (cmd == 4) {
        m_model.clearNodeMarks(index);
    }

    m_selectedIndex = index;
    m_dirty = true;
}

void StudioWindow::onMButtonDown(int x, int y)
{
    int index = m_model.findNodeAt(x, y);
    if (index < 0) return;

    HMENU menu = CreatePopupMenu();
    AppendMenuA(menu, MF_STRING, 1, "Place Check");
    AppendMenuA(menu, MF_STRING, 2, "Place X");
    AppendMenuA(menu, MF_STRING, 3, "Place Arrow");
    AppendMenuA(menu, MF_STRING, 4, "Place Question");

    POINT pt = {x, y};
    ClientToScreen(m_hwnd, &pt);
    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hwnd, nullptr);
    DestroyMenu(menu);

    MarkType type = MarkType::Check;
    if (cmd == 2) type = MarkType::Cross;
    else if (cmd == 3) type = MarkType::Arrow;
    else if (cmd == 4) type = MarkType::Question;

    if (cmd >= 1 && cmd <= 4) {
        Mark mark{ x, y, type };
        m_model.addNodeMark(index, mark);
        m_model.setNodeState(index, ApprovalState::Modified);
        m_dirty = true;
    }
}

void StudioWindow::onKeyDown(WPARAM key)
{
    if (m_selectedIndex < 0) return;

    if (key == 'A') {
        m_model.setNodeState(m_selectedIndex, ApprovalState::Approved);
    } else if (key == 'R') {
        m_model.setNodeState(m_selectedIndex, ApprovalState::Rejected);
    } else if (key == 'P') {
        m_model.setNodeState(m_selectedIndex, ApprovalState::Pending);
    } else if (key == 'X') {
        Mark mark{ m_model.nodes()[static_cast<size_t>(m_selectedIndex)].bounds.left + 6,
            m_model.nodes()[static_cast<size_t>(m_selectedIndex)].bounds.top + 6, MarkType::Check };
        m_model.addNodeMark(m_selectedIndex, mark);
    } else if (key == 'Z') {
        Mark mark{ m_model.nodes()[static_cast<size_t>(m_selectedIndex)].bounds.left + 6,
            m_model.nodes()[static_cast<size_t>(m_selectedIndex)].bounds.top + 6, MarkType::Cross };
        m_model.addNodeMark(m_selectedIndex, mark);
    } else if (key == VK_RETURN) {
        saveOutput();
    }

    m_dirty = true;
}

void StudioWindow::loadInput()
{
    if (m_config.inputPath.empty()) return;

    if (m_model.loadFromFile(m_config.inputPath)) {
        getFileWriteTime(m_config.inputPath, m_lastInputWrite);
        m_dirty = true;
    }
}

void StudioWindow::saveOutput()
{
    if (m_config.outputPath.empty()) return;
    m_model.saveToFile(m_config.outputPath);
}

void StudioWindow::maybeAutoSave()
{
    if (!m_config.autoSave || !m_dirty) return;
    saveOutput();
}

void StudioWindow::checkForExternalUpdates()
{
    if (m_config.inputPath.empty()) return;
    FILETIME current{};
    if (!getFileWriteTime(m_config.inputPath, current)) return;

    if (CompareFileTime(&current, &m_lastInputWrite) > 0) {
        m_lastInputWrite = current;
        m_model.loadFromFile(m_config.inputPath);
        m_dirty = true;
    }
}

COLORREF StudioWindow::colorForCaps(uint64_t caps) const
{
    if (caps & 0x1) return RGB(100, 149, 237);
    if (caps & 0x2) return RGB(144, 238, 144);
    if (caps & 0x4) return RGB(255, 182, 193);
    if (caps & 0x8) return RGB(255, 255, 224);
    if (caps & 0x10) return RGB(221, 160, 221);
    if (caps & 0x20) return RGB(255, 218, 185);
    return RGB(220, 220, 220);
}

std::string StudioWindow::labelForNode(const Node& node) const
{
    if (!node.label.empty()) return node.label;
    return node.id;
}

bool StudioWindow::getFileWriteTime(const std::string& path, FILETIME& outTime) const
{
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data)) {
        return false;
    }
    outTime = data.ftLastWriteTime;
    return true;
}

int RunSchematicStudio(const StudioConfig& config, HINSTANCE instance)
{
    IDELogger::getInstance().initialize("SchematicStudio.log");
    try {
        StudioWindow window;
        if (!window.initialize(instance, config)) {
            LOG_ERROR("Failed to start Schematic Studio window");
            return 1;
        }
        return window.run();
    } catch (const std::exception& ex) {
        LOG_CRITICAL(std::string("Unhandled exception: ") + ex.what());
        return 2;
    } catch (...) {
        LOG_CRITICAL("Unhandled unknown exception");
        return 3;
    }
}

} // namespace RawrXD::Schematic
