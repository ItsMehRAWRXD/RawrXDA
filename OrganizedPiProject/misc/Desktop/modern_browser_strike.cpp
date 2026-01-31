#define UNICODE
#define _UNICODE
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <fstream>
#include <sstream>

// Simple math structures
struct Vec3 {
    float x, y, z;
    Vec3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
    Vec3 operator+(const Vec3& other) const { return Vec3(x + other.x, y + other.y, z + other.z); }
    Vec3 operator-(const Vec3& other) const { return Vec3(x - other.x, y - other.y, z - other.z); }
    Vec3 operator*(float scale) const { return Vec3(x * scale, y * scale, z * scale); }
    float length() const { return sqrt(x*x + y*y + z*z); }
    Vec3 normalize() const { float len = length(); return len > 0 ? *this * (1.0f/len) : Vec3(); }
};

// Simple matrix operations
struct Mat4 {
    float m[16];
    Mat4() {
        for(int i = 0; i < 16; i++) m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }
    Mat4 operator*(const Mat4& other) const {
        Mat4 result;
        for(int i = 0; i < 4; i++) {
            for(int j = 0; j < 4; j++) {
                result.m[i*4+j] = 0;
                for(int k = 0; k < 4; k++) {
                    result.m[i*4+j] += m[i*4+k] * other.m[k*4+j];
                }
            }
        }
        return result;
    }
    void translate(const Vec3& pos) {
        m[12] += pos.x; m[13] += pos.y; m[14] += pos.z;
    }
    void scale(const Vec3& scale) {
        m[0] *= scale.x; m[5] *= scale.y; m[10] *= scale.z;
    }
    void rotateY(float angle) {
        float c = cos(angle), s = sin(angle);
        Mat4 rot;
        rot.m[0] = c; rot.m[2] = s;
        rot.m[8] = -s; rot.m[10] = c;
        *this = *this * rot;
    }
};

// Camera system
class Camera {
public:
    enum Mode { FIRST_PERSON, THIRD_PERSON };
    Mode mode = FIRST_PERSON;
    
    Vec3 eye{0, 2, 5};
    Vec3 target{0, 0, 0};
    float yaw = 0.0f, pitch = 0.0f;
    float thirdPersonDist = 4.0f;
    
    void updateFirstPerson(const Vec3& pos, float yawRad, float pitchRad) {
        mode = FIRST_PERSON;
        yaw = yawRad; pitch = pitchRad;
        eye = pos;
        target = pos + Vec3(cos(yaw) * cos(pitch), sin(pitch), sin(yaw) * cos(pitch));
    }
    
    void updateThirdPerson(const Vec3& pos, float yawRad) {
        mode = THIRD_PERSON;
        yaw = yawRad;
        Vec3 behind = pos - Vec3(cos(yaw), 0, sin(yaw)) * thirdPersonDist;
        eye = behind + Vec3(0, 2, 0);
        target = pos + Vec3(0, 1, 0);
    }
    
    void toggleMode() {
        mode = (mode == FIRST_PERSON) ? THIRD_PERSON : FIRST_PERSON;
    }
    
    void apply() {
        gluLookAt(eye.x, eye.y, eye.z, target.x, target.y, target.z, 0, 1, 0);
    }
};

// Wall structure
struct Wall {
    Vec3 pos, size;
    int material = 0; // 0=wood, 1=metal, 2=concrete, 3=stone
};

// Map structure
struct Map {
    std::string name = "Random Rooms";
    std::vector<Wall> walls;
};

// Random Room Generator
class RandomRoomGenerator {
public:
    Map generate(int seed, int width, int height) {
        Map map;
        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> dist(1, 4);
        
        // Generate random walls
        for(int i = 0; i < 20; i++) {
            Wall w;
            w.pos = Vec3(rng() % width, 0, rng() % height);
            w.size = Vec3(dist(rng), 3, dist(rng));
            w.material = rng() % 4;
            map.walls.push_back(w);
        }
        
        return map;
    }
};

// Global variables
HDC hDC;
HGLRC hRC;
HWND hWnd;
Camera camera;
Map currentMap;
Vec3 playerPos{0, 1, 0};
float playerYaw = 0.0f, playerPitch = 0.0f;
bool wireframeMode = true;
bool vKeyPressed = false;

// Function declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void setupPixelFormat(HDC hdc);
void drawCube(const Vec3& pos, const Vec3& size, const Vec3& color, bool wireframe = false);
void drawSpinningCube(float time);
void processInput();

int main() {
    // Create window
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"BrowserStrike";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    if (!RegisterClass(&wc)) return 1;
    
    hWnd = CreateWindowEx(0, L"BrowserStrike", L"Browser Strike - Modern OpenGL",
        WS_OVERLAPPEDWINDOW, 100, 100, 800, 600, NULL, NULL, GetModuleHandle(NULL), NULL);
    
    if (!hWnd) return 1;
    
    hDC = GetDC(hWnd);
    setupPixelFormat(hDC);
    hRC = wglCreateContext(hDC);
    wglMakeCurrent(hDC, hRC);
    
    // Initialize OpenGL
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    
    // Generate map
    RandomRoomGenerator generator;
    currentMap = generator.generate(42, 20, 20);
    
    ShowWindow(hWnd, SW_SHOW);
    
    std::cout << "Browser Strike - Modern OpenGL Engine" << std::endl;
    std::cout << "Generated " << currentMap.walls.size() << " walls" << std::endl;
    std::cout << "Controls: WASD-Move, Mouse-Look, V-Camera, W-Wireframe, ESC-Exit" << std::endl;
    
    // Main loop
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        
        // Render frame
        processInput();
        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Set up projection and view
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(60.0f, 800.0f/600.0f, 0.1f, 100.0f);
        
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        
        // Update camera
        camera.updateFirstPerson(playerPos, playerYaw, playerPitch);
        camera.apply();
        
        // Draw walls in wireframe
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        for (const auto& wall : currentMap.walls) {
            Vec3 color(0.0f, 1.0f, 0.0f); // Green wireframe
            drawCube(wall.pos, wall.size, color, true);
        }
        
        // Draw spinning cube at origin
        drawSpinningCube(GetTickCount() / 1000.0f);
        
        // Draw player
        glPolygonMode(GL_FRONT_AND_BACK, wireframeMode ? GL_LINE : GL_FILL);
        Vec3 playerColor(1.0f, 1.0f, 1.0f);
        drawCube(playerPos, Vec3(1.0f), playerColor);
        
        SwapBuffers(hDC);
    }
    
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hRC);
    ReleaseDC(hWnd, hDC);
    DestroyWindow(hWnd);
    
    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) PostQuitMessage(0);
            if (wParam == 'V' && !vKeyPressed) {
                camera.toggleMode();
                vKeyPressed = true;
            }
            if (wParam == 'W') {
                wireframeMode = !wireframeMode;
            }
            return 0;
        case WM_KEYUP:
            if (wParam == 'V') vKeyPressed = false;
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void setupPixelFormat(HDC hdc) {
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR), 1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        16, 0, 0, PFD_MAIN_PLANE, 0, 0, 0, 0
    };
    
    int pixelFormat = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, pixelFormat, &pfd);
}

void drawCube(const Vec3& pos, const Vec3& size, const Vec3& color, bool wireframe) {
    glPushMatrix();
    glTranslatef(pos.x, pos.y, pos.z);
    glScalef(size.x, size.y, size.z);
    glColor3f(color.x, color.y, color.z);
    
    if (wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    
    glBegin(GL_QUADS);
    // Front face
    glVertex3f(-1, -1, 1); glVertex3f(1, -1, 1); glVertex3f(1, 1, 1); glVertex3f(-1, 1, 1);
    // Back face
    glVertex3f(-1, -1, -1); glVertex3f(-1, 1, -1); glVertex3f(1, 1, -1); glVertex3f(1, -1, -1);
    // Top face
    glVertex3f(-1, 1, -1); glVertex3f(-1, 1, 1); glVertex3f(1, 1, 1); glVertex3f(1, 1, -1);
    // Bottom face
    glVertex3f(-1, -1, -1); glVertex3f(1, -1, -1); glVertex3f(1, -1, 1); glVertex3f(-1, -1, 1);
    // Right face
    glVertex3f(1, -1, -1); glVertex3f(1, 1, -1); glVertex3f(1, 1, 1); glVertex3f(1, -1, 1);
    // Left face
    glVertex3f(-1, -1, -1); glVertex3f(-1, -1, 1); glVertex3f(-1, 1, 1); glVertex3f(-1, 1, -1);
    glEnd();
    
    glPopMatrix();
}

void drawSpinningCube(float time) {
    glPushMatrix();
    glRotatef(time * 50.0f, 0, 1, 0); // Spin around Y axis
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glColor3f(1.0f, 0.0f, 0.0f); // Red color
    drawCube(Vec3(), Vec3(1.0f), Vec3(1.0f, 0.0f, 0.0f), false);
    glPopMatrix();
}

void processInput() {
    // Simple movement (you can expand this)
    if (GetAsyncKeyState('W') & 0x8000) {
        playerPos = playerPos + Vec3(cos(playerYaw), 0, sin(playerYaw)) * 0.1f;
    }
    if (GetAsyncKeyState('S') & 0x8000) {
        playerPos = playerPos - Vec3(cos(playerYaw), 0, sin(playerYaw)) * 0.1f;
    }
    if (GetAsyncKeyState('A') & 0x8000) {
        playerPos = playerPos - Vec3(sin(playerYaw), 0, -cos(playerYaw)) * 0.1f;
    }
    if (GetAsyncKeyState('D') & 0x8000) {
        playerPos = playerPos + Vec3(sin(playerYaw), 0, -cos(playerYaw)) * 0.1f;
    }
    if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
        playerPos.y += 0.1f;
    }
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
        playerPos.y -= 0.1f;
    }
}
