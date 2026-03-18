// ============================================================================
// Win32IDE_VisionEncoder.cpp — Vision Model Integration for Win32 IDE
// ============================================================================
// Provides image loading, vision encoding, and multi-modal AI integration.
// Features:
//   - Load images from file/clipboard
//   - Vision model encoding (CLIP, LLaVA, etc.)
//   - Image description generation
//   - Code extraction from screenshots
//   - Diagram structure analysis
//   - Multi-modal prompt creation for LLM
//
// Integration: Menu → Load Image → Encode → Display Results
// ============================================================================

#include "Win32IDE.h"
#include "IDELogger.h"
#include <commdlg.h>
#include <shlobj.h>
#include <gdiplus.h>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>

// Link GDI+ for image loading
#pragma comment(lib, "gdiplus.lib")

// Vision encoder includes
#include "../core/vision_encoder.hpp"
#include "../core/vision_embedding_cache.hpp"

using namespace RawrXD::Vision;

// ============================================================================
// Vision Encoder Window Class
// ============================================================================
class VisionEncoderWindow {
public:
    static const char* CLASS_NAME;

    VisionEncoderWindow(HINSTANCE hInstance, HWND hwndParent)
        : hInstance_(hInstance), hwndParent_(hwndParent), hwnd_(nullptr),
          currentImage_(nullptr), imageWidth_(0), imageHeight_(0) {
        registerClass();
    }

    ~VisionEncoderWindow() {
        cleanup();
    }

    bool create() {
        hwnd_ = CreateWindowExA(
            WS_EX_CLIENTEDGE,
            CLASS_NAME,
            "Vision Encoder",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
            hwndParent_,
            nullptr,
            hInstance_,
            this
        );

        if (!hwnd_) {
            LOG_ERROR("Failed to create vision encoder window");
            return false;
        }

        // Create child controls
        createControls();

        // Initialize GDI+
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        GdiplusStartup(&gdiplusToken_, &gdiplusStartupInput, nullptr);

        return true;
    }

    void show() {
        if (hwnd_) {
            ShowWindow(hwnd_, SW_SHOW);
            UpdateWindow(hwnd_);
        }
    }

    void hide() {
        if (hwnd_) {
            ShowWindow(hwnd_, SW_HIDE);
        }
    }

    // Load image from file
    bool loadImageFromFile() {
        OPENFILENAMEA ofn = {};
        char szFile[MAX_PATH] = {};

        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd_;
        ofn.lpstrFilter = "Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tiff\0All Files\0*.*\0";
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        ofn.lpstrTitle = "Select Image for Vision Analysis";

        if (GetOpenFileNameA(&ofn)) {
            return loadImage(szFile);
        }

        return false;
    }

    // Load image from clipboard
    bool loadImageFromClipboard() {
        if (!OpenClipboard(hwnd_)) {
            LOG_ERROR("Failed to open clipboard");
            return false;
        }

        HANDLE hData = GetClipboardData(CF_DIB);
        if (!hData) {
            CloseClipboard();
            LOG_ERROR("No bitmap data in clipboard");
            return false;
        }

        // Convert DIB to GDI+ bitmap
        BITMAPINFO* bmi = (BITMAPINFO*)GlobalLock(hData);
        if (!bmi) {
            CloseClipboard();
            return false;
        }

        void* bits = (BYTE*)bmi + bmi->bmiHeader.biSize + bmi->bmiHeader.biClrUsed * sizeof(RGBQUAD);

        HBITMAP hBitmap = CreateDIBitmap(GetDC(nullptr), &bmi->bmiHeader, CBM_INIT, bits, bmi, DIB_RGB_COLORS);
        GlobalUnlock(hData);
        CloseClipboard();

        if (hBitmap) {
            currentImage_ = new Gdiplus::Bitmap(hBitmap, nullptr);
            if (currentImage_->GetLastStatus() == Gdiplus::Ok) {
                imageWidth_ = currentImage_->GetWidth();
                imageHeight_ = currentImage_->GetHeight();
                updateImageDisplay();
                return true;
            }
        }

        return false;
    }

private:
    HINSTANCE hInstance_;
    HWND hwndParent_;
    HWND hwnd_;
    HWND hwndImage_;
    HWND hwndResults_;
    HWND hwndLoadFile_;
    HWND hwndLoadClipboard_;
    HWND hwndAnalyze_;

    Gdiplus::GdiplusStartupInput gdiplusStartupInput_;
    ULONG_PTR gdiplusToken_;

    Gdiplus::Bitmap* currentImage_;
    int imageWidth_;
    int imageHeight_;

    void registerClass() {
        WNDCLASSA wc = {};
        wc.lpfnWndProc = windowProc;
        wc.hInstance = hInstance_;
        wc.lpszClassName = CLASS_NAME;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

        RegisterClassA(&wc);
    }

    void createControls() {
        // Load File button
        hwndLoadFile_ = CreateWindowA(
            "BUTTON", "Load Image File",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            10, 10, 120, 30,
            hwnd_, (HMENU)IDC_LOAD_FILE, hInstance_, nullptr
        );

        // Load Clipboard button
        hwndLoadClipboard_ = CreateWindowA(
            "BUTTON", "Load from Clipboard",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            140, 10, 120, 30,
            hwnd_, (HMENU)IDC_LOAD_CLIPBOARD, hInstance_, nullptr
        );

        // Analyze button
        hwndAnalyze_ = CreateWindowA(
            "BUTTON", "Analyze Image",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            270, 10, 120, 30,
            hwnd_, (HMENU)IDC_ANALYZE, hInstance_, nullptr
        );

        // Image display area (static control)
        hwndImage_ = CreateWindowA(
            "STATIC", "",
            WS_VISIBLE | WS_CHILD | SS_OWNERDRAW,
            10, 50, 380, 300,
            hwnd_, (HMENU)IDC_IMAGE_DISPLAY, hInstance_, nullptr
        );

        // Results text area
        hwndResults_ = CreateWindowA(
            "EDIT", "",
            WS_VISIBLE | WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            400, 50, 380, 480,
            hwnd_, (HMENU)IDC_RESULTS, hInstance_, nullptr
        );

        // Set fonts
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        SendMessage(hwndLoadFile_, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hwndLoadClipboard_, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hwndAnalyze_, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hwndResults_, WM_SETFONT, (WPARAM)hFont, TRUE);
    }

    bool loadImage(const std::string& path) {
        cleanup();

        currentImage_ = new Gdiplus::Bitmap(std::wstring(path.begin(), path.end()).c_str());
        if (currentImage_->GetLastStatus() != Gdiplus::Ok) {
            delete currentImage_;
            currentImage_ = nullptr;
            LOG_ERROR("Failed to load image: " + path);
            return false;
        }

        imageWidth_ = currentImage_->GetWidth();
        imageHeight_ = currentImage_->GetHeight();
        updateImageDisplay();

        // Update results
        std::string info = "Loaded image: " + path + "\n";
        info += "Dimensions: " + std::to_string(imageWidth_) + "x" + std::to_string(imageHeight_) + "\n";
        SetWindowTextA(hwndResults_, info.c_str());

        return true;
    }

    void updateImageDisplay() {
        if (hwndImage_) {
            InvalidateRect(hwndImage_, nullptr, TRUE);
        }
    }

    void analyzeImage() {
        if (!currentImage_) {
            MessageBoxA(hwnd_, "No image loaded", "Error", MB_OK | MB_ICONERROR);
            return;
        }

        // Convert GDI+ bitmap to ImageBuffer
        ImageBuffer imgBuf;
        if (!convertBitmapToImageBuffer(*currentImage_, imgBuf)) {
            MessageBoxA(hwnd_, "Failed to convert image", "Error", MB_OK | MB_ICONERROR);
            return;
        }

        // Get vision encoder instance
        VisionEncoder& encoder = VisionEncoder::instance();

        if (!encoder.isReady()) {
            // Try to load default model
            VisionModelConfig config;
            config.modelPath = "models/vision/clip-vit-l14.gguf"; // Default path
            config.useGPU = true;

            VisionResult result = encoder.loadModel(config);
            if (!result.success) {
                std::string msg = "Failed to load vision model: " + std::string(result.detail);
                MessageBoxA(hwnd_, msg.c_str(), "Error", MB_OK | MB_ICONERROR);
                ImagePreprocessor::freeBuffer(imgBuf);
                return;
            }
        }

        // Analyze image
        std::string results = "Analyzing image...\n\n";

        // Generate description
        std::string description;
        VisionResult descResult = encoder.describeImage(imgBuf, description);
        if (descResult.success) {
            results += "Description: " + description + "\n\n";
        } else {
            results += "Description failed: " + std::string(descResult.detail) + "\n\n";
        }

        // Extract code if it looks like a screenshot
        std::string code;
        VisionResult codeResult = encoder.extractCodeFromScreenshot(imgBuf, code);
        if (codeResult.success && !code.empty()) {
            results += "Extracted Code:\n" + code + "\n\n";
        }

        // Extract diagram structure
        std::string diagramJson;
        VisionResult diagramResult = encoder.extractDiagramStructure(imgBuf, diagramJson);
        if (diagramResult.success && !diagramJson.empty()) {
            results += "Diagram Structure (JSON):\n" + diagramJson + "\n\n";
        }

        // Create multimodal prompt
        VisionTextPair multimodal;
        VisionResult mmResult = encoder.createMultiModalPrompt(imgBuf, "Analyze this image", multimodal);
        if (mmResult.success) {
            results += "Multimodal Prompt:\n" + multimodal.textPrompt + "\n\n";
            results += "Relevance Score: " + std::to_string(multimodal.relevanceScore) + "\n";
        }

        SetWindowTextA(hwndResults_, results.c_str());
        ImagePreprocessor::freeBuffer(imgBuf);
    }

    bool convertBitmapToImageBuffer(Gdiplus::Bitmap& bitmap, ImageBuffer& output) {
        // Get bitmap dimensions
        int width = bitmap.GetWidth();
        int height = bitmap.GetHeight();

        // Create buffer for RGB data
        output.data = new uint8_t[width * height * 3];
        output.width = width;
        output.height = height;
        output.channels = 3;
        output.stride = width * 3;
        output.format = ImageFormat::RGB8;
        output.dataSize = width * height * 3;

        // Lock bitmap bits
        Gdiplus::Rect rect(0, 0, width, height);
        Gdiplus::BitmapData bitmapData;
        if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat24bppRGB, &bitmapData) != Gdiplus::Ok) {
            delete[] output.data;
            return false;
        }

        // Copy data (GDI+ gives BGR, we want RGB)
        uint8_t* src = (uint8_t*)bitmapData.Scan0;
        uint8_t* dst = output.data;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // BGR to RGB conversion
                dst[0] = src[2]; // R
                dst[1] = src[1]; // G
                dst[2] = src[0]; // B
                src += 3;
                dst += 3;
            }
        }

        bitmap.UnlockBits(&bitmapData);
        return true;
    }

    void cleanup() {
        if (currentImage_) {
            delete currentImage_;
            currentImage_ = nullptr;
        }
        imageWidth_ = imageHeight_ = 0;
    }

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        VisionEncoderWindow* pThis = nullptr;

        if (uMsg == WM_CREATE) {
            CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
            pThis = (VisionEncoderWindow*)pCreate->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        } else {
            pThis = (VisionEncoderWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        }

        if (pThis) {
            return pThis->handleMessage(hwnd, uMsg, wParam, lParam);
        }

        return DefWindowProcA(hwnd, uMsg, wParam, lParam);
    }

    LRESULT handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_COMMAND:
                switch (LOWORD(wParam)) {
                    case IDC_LOAD_FILE:
                        loadImageFromFile();
                        break;
                    case IDC_LOAD_CLIPBOARD:
                        loadImageFromClipboard();
                        break;
                    case IDC_ANALYZE:
                        analyzeImage();
                        break;
                }
                break;

            case WM_DRAWITEM:
                if (wParam == IDC_IMAGE_DISPLAY) {
                    drawImage((DRAWITEMSTRUCT*)lParam);
                    return TRUE;
                }
                break;

            case WM_SIZE:
                // Resize controls
                {
                    RECT rc;
                    GetClientRect(hwnd, &rc);
                    int width = rc.right - rc.left;
                    int height = rc.bottom - rc.top;

                    MoveWindow(hwndImage_, 10, 50, (width - 30) / 2, height - 60, TRUE);
                    MoveWindow(hwndResults_, (width / 2) + 5, 50, (width - 30) / 2, height - 60, TRUE);
                }
                break;

            case WM_DESTROY:
                cleanup();
                Gdiplus::GdiplusShutdown(gdiplusToken_);
                break;
        }

        return DefWindowProcA(hwnd, uMsg, wParam, lParam);
    }

    void drawImage(DRAWITEMSTRUCT* pDraw) {
        if (!currentImage_) return;

        HDC hdc = pDraw->hDC;
        RECT rc = pDraw->rcItem;

        // Create compatible DC
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hBitmap = nullptr;

        // Convert Gdiplus::Bitmap to HBITMAP
        currentImage_->GetHBITMAP(Gdiplus::Color::White, &hBitmap);

        if (hBitmap) {
            HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hBitmap);

            // Calculate scaling to fit the control
            int ctrlWidth = rc.right - rc.left;
            int ctrlHeight = rc.bottom - rc.top;

            double scaleX = (double)ctrlWidth / imageWidth_;
            double scaleY = (double)ctrlHeight / imageHeight_;
            double scale = std::min(scaleX, scaleY);

            int drawWidth = (int)(imageWidth_ * scale);
            int drawHeight = (int)(imageHeight_ * scale);
            int x = (ctrlWidth - drawWidth) / 2;
            int y = (ctrlHeight - drawHeight) / 2;

            // Draw the bitmap
            StretchBlt(hdc, rc.left + x, rc.top + y, drawWidth, drawHeight,
                      hdcMem, 0, 0, imageWidth_, imageHeight_, SRCCOPY);

            SelectObject(hdcMem, hOld);
            DeleteObject(hBitmap);
        }

        DeleteDC(hdcMem);
    }

    enum {
        IDC_LOAD_FILE = 1001,
        IDC_LOAD_CLIPBOARD = 1002,
        IDC_ANALYZE = 1003,
        IDC_IMAGE_DISPLAY = 1004,
        IDC_RESULTS = 1005
    };
};

const char* VisionEncoderWindow::CLASS_NAME = "RawrXD_VisionEncoder";

// ============================================================================
// Win32IDE Vision Encoder Integration
// ============================================================================

static std::unique_ptr<VisionEncoderWindow> g_visionWindow;

void Win32IDE::showVisionEncoder() {
    if (!g_visionWindow) {
        g_visionWindow = std::make_unique<VisionEncoderWindow>(m_hInstance, m_hwndMain);
        if (!g_visionWindow->create()) {
            LOG_ERROR("Failed to create vision encoder window");
            g_visionWindow.reset();
            return;
        }
    }

    g_visionWindow->show();
}

void Win32IDE::hideVisionEncoder() {
    if (g_visionWindow) {
        g_visionWindow->hide();
    }
}

void Win32IDE::initVisionEncoder() {
    // Initialize vision encoder singleton
    VisionEncoder& encoder = VisionEncoder::instance();

    // Check if model is available
    if (!encoder.isReady()) {
        LOG_INFO("Vision encoder initialized but no model loaded yet");
    } else {
        LOG_INFO("Vision encoder ready with model");
    }
}

void Win32IDE::shutdownVisionEncoder() {
    if (g_visionWindow) {
        g_visionWindow.reset();
    }

    VisionEncoder::instance().shutdown();
}
