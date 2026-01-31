#define UNICODE
#define _UNICODE
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <iostream>
#include <vector>
#include <random>
#include <cmath>

// Simple math structures (replacing GLM)
struct Vec3 {
    float x, y, z;
    Vec3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
    Vec3 operator+(const Vec3& other) const { return Vec3(x + other.x, y + other.y, z + other.z); }
    Vec3 operator-(const Vec3& other) const { return Vec3(x - other.x, y - other.y, z - other.z); }
    Vec3 operator*(float scale) const { return Vec3(x * scale, y * scale, z * scale); }
    float length() const { return sqrt(x*x + y*y + z*z); }
    Vec3 normalize() const { float len = length(); return len > 0 ? *this * (1.0f/len) : Vec3(); }
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
};

// Map system
enum Material { WOOD, METAL, CONCRETE, STONE };

struct Wall {
    Vec3 pos;
    Vec3 size;
    Material material;
    int ownerId = -1;
};

struct Spawn {
    Vec3 pos;
    int teamId = 0;
};

struct LootSpawn {
    Vec3 pos;
    std::string itemType;
    int quantity = 1;
};

struct Map {
    std::vector<Wall> walls;
    std::vector<Spawn> spawns;
    std::vector<LootSpawn> lootSpawns;
    Vec3 size{100, 20, 100};
    std::string name = "Generated Map";
};

// Simple map generator
class SimpleMapGenerator {
public:
    Map generate(int seed, int w, int h) {
        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> sizeDist(4, 12);
        std::uniform_int_distribution<int> posDist(0, 80);
        std::uniform_int_distribution<int> materialDist(0, 3);
        
        Map map;
        map.size = Vec3(w, 20, h);
        map.name = "Simple Arena";
        
        // Generate random walls
        for(int i = 0; i < 15; i++) {
            Wall wall;
            wall.pos = Vec3(posDist(rng), 0, posDist(rng));
            wall.size = Vec3(sizeDist(rng), 3, sizeDist(rng));
            wall.material = static_cast<Material>(materialDist(rng));
            wall.ownerId = -1;
            map.walls.push_back(wall);
        }
        
        // Generate spawn points
        for(int i = 0; i < 8; i++) {
            Spawn spawn;
            spawn.pos = Vec3(posDist(rng), 1, posDist(rng));
            spawn.teamId = i % 2;
            map.spawns.push_back(spawn);
        }
        
        // Generate loot spawns
        std::vector<std::string> lootTypes = {"ammo", "health", "armor", "weapon"};
        for(int i = 0; i < 10; i++) {
            LootSpawn loot;
            loot.pos = Vec3(posDist(rng), 0.5f, posDist(rng));
            loot.itemType = lootTypes[i % lootTypes.size()];
            loot.quantity = 1 + (i % 3);
            map.lootSpawns.push_back(loot);
        }
        
        return map;
    }
};

// OpenGL rendering functions
void drawCube(const Vec3& pos, const Vec3& size, bool wireframe = true) {
    glPushMatrix();
    glTranslatef(pos.x, pos.y, pos.z);
    glScalef(size.x, size.y, size.z);
    
    if(wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glColor3f(0.0f, 1.0f, 0.0f); // Green wireframe
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glColor3f(1.0f, 1.0f, 1.0f); // White solid
    }
    
    // Draw cube faces
    glBegin(GL_QUADS);
    // Front face
    glVertex3f(-0.5f, -0.5f,  0.5f);
    glVertex3f( 0.5f, -0.5f,  0.5f);
    glVertex3f( 0.5f,  0.5f,  0.5f);
    glVertex3f(-0.5f,  0.5f,  0.5f);
    
    // Back face
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f,  0.5f, -0.5f);
    glVertex3f( 0.5f,  0.5f, -0.5f);
    glVertex3f( 0.5f, -0.5f, -0.5f);
    
    // Top face
    glVertex3f(-0.5f,  0.5f, -0.5f);
    glVertex3f(-0.5f,  0.5f,  0.5f);
    glVertex3f( 0.5f,  0.5f,  0.5f);
    glVertex3f( 0.5f,  0.5f, -0.5f);
    
    // Bottom face
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f( 0.5f, -0.5f, -0.5f);
    glVertex3f( 0.5f, -0.5f,  0.5f);
    glVertex3f(-0.5f, -0.5f,  0.5f);
    
    // Right face
    glVertex3f( 0.5f, -0.5f, -0.5f);
    glVertex3f( 0.5f,  0.5f, -0.5f);
    glVertex3f( 0.5f,  0.5f,  0.5f);
    glVertex3f( 0.5f, -0.5f,  0.5f);
    
    // Left face
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, -0.5f,  0.5f);
    glVertex3f(-0.5f,  0.5f,  0.5f);
    glVertex3f(-0.5f,  0.5f, -0.5f);
    glEnd();
    
    glPopMatrix();
}

void setMaterialColor(Material material) {
    switch(material) {
        case WOOD:
            glColor3f(0.6f, 0.4f, 0.2f); // Brown
            break;
        case METAL:
            glColor3f(0.7f, 0.7f, 0.7f); // Gray
            break;
        case CONCRETE:
            glColor3f(0.5f, 0.5f, 0.5f); // Dark gray
            break;
        case STONE:
            glColor3f(0.4f, 0.4f, 0.4f); // Darker gray
            break;
    }
}

void renderMap(const Map& map, bool wireframeMode) {
    // Render walls
    for(const auto& wall : map.walls) {
        setMaterialColor(wall.material);
        drawCube(wall.pos, wall.size, wireframeMode);
    }
    
    // Render spawn points (small cubes)
    glColor3f(1.0f, 0.0f, 0.0f); // Red
    for(const auto& spawn : map.spawns) {
        drawCube(spawn.pos, Vec3(0.5f, 0.5f, 0.5f), false);
    }
    
    // Render loot spawns (small cubes)
    glColor3f(0.0f, 0.0f, 1.0f); // Blue
    for(const auto& loot : map.lootSpawns) {
        drawCube(loot.pos, Vec3(0.3f, 0.3f, 0.3f), false);
    }
}

void renderPlayer(const Vec3& pos, const Camera& camera) {
    // Render player as a cube
    glColor3f(1.0f, 1.0f, 0.0f); // Yellow
    drawCube(pos, Vec3(0.8f, 1.8f, 0.8f), false);
    
    // Render camera target line in third person
    if(camera.mode == Camera::THIRD_PERSON) {
        glColor3f(1.0f, 0.0f, 1.0f); // Magenta
        glBegin(GL_LINES);
        glVertex3f(camera.eye.x, camera.eye.y, camera.eye.z);
        glVertex3f(camera.target.x, camera.target.y, camera.target.z);
        glEnd();
    }
}

// Global variables
HWND hWnd;
HDC hDC;
HGLRC hRC;
Camera camera;
Map map;
Vec3 playerPos{0, 1, 0};
float playerYaw = 0.0f;
float playerPitch = 0.0f;
bool wireframeMode = true;
bool running = true;
SimpleMapGenerator generator;

// Window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch(uMsg) {
        case WM_CLOSE:
            running = false;
            return 0;
            
        case WM_KEYDOWN:
            switch(wParam) {
                case 'W':
                    if(GetKeyState(VK_SHIFT) & 0x8000) {
                        wireframeMode = !wireframeMode;
                    } else {
                        playerPos = playerPos + Vec3(0, 0, -0.1f);
                    }
                    break;
                case 'S':
                    playerPos = playerPos + Vec3(0, 0, 0.1f);
                    break;
                case 'A':
                    playerPos = playerPos + Vec3(-0.1f, 0, 0);
                    break;
                case 'D':
                    playerPos = playerPos + Vec3(0.1f, 0, 0);
                    break;
                case 'V':
                    camera.toggleMode();
                    break;
                case VK_ESCAPE:
                    running = false;
                    break;
            }
            break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Initialize OpenGL
bool initOpenGL() {
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        32,
        0, 0, 0, 0, 0, 0,
        0,
        0,
        0,
        0, 0, 0, 0,
        24,
        8,
        0,
        PFD_MAIN_PLANE,
        0,
        0, 0, 0
    };
    
    int pixelFormat = ChoosePixelFormat(hDC, &pfd);
    if(!pixelFormat) return false;
    
    if(!SetPixelFormat(hDC, pixelFormat, &pfd)) return false;
    
    hRC = wglCreateContext(hDC);
    if(!hRC) return false;
    
    if(!wglMakeCurrent(hDC, hRC)) return false;
    
    // Set up OpenGL
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    
    return true;
}

// Render frame
void render() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Set up projection matrix
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, 800.0f/600.0f, 0.1f, 100.0f);
    
    // Set up view matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Update camera
    if(camera.mode == Camera::FIRST_PERSON) {
        camera.updateFirstPerson(playerPos, playerYaw, playerPitch);
    } else {
        camera.updateThirdPerson(playerPos, playerYaw);
    }
    
    // Set camera view
    gluLookAt(camera.eye.x, camera.eye.y, camera.eye.z,
              camera.target.x, camera.target.y, camera.target.z,
              0.0f, 1.0f, 0.0f);
    
    // Render map
    renderMap(map, wireframeMode);
    
    // Render player
    renderPlayer(playerPos, camera);
    
    SwapBuffers(hDC);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Register window class
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"BrowserStrike";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    if(!RegisterClass(&wc)) return 1;
    
    // Create window
    hWnd = CreateWindow(
        L"BrowserStrike",
        L"Browser Strike - Assault on Boredom",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        NULL, NULL, hInstance, NULL
    );
    
    if(!hWnd) return 1;
    
    hDC = GetDC(hWnd);
    if(!initOpenGL()) return 1;
    
    // Generate map
    map = generator.generate(12345, 100, 100);
    
    ShowWindow(hWnd, nCmdShow);
    
    // Game loop
    MSG msg = {};
    while(running) {
        while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        render();
        Sleep(16); // ~60 FPS
    }
    
    // Cleanup
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hRC);
    ReleaseDC(hWnd, hDC);
    DestroyWindow(hWnd);
    
    return 0;
}
