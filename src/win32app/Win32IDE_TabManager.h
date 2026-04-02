#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "Win32IDE_Types.h"

// GPU Sovereign Control Extern Declarations
extern "C" {
    // KFD Interface
    uint64_t KFD_Get_Driver_Version();
    void KFD_Ring_Hardware_Doorbell();

    // VM Management
    void RDNA3_Trap_Handler();
    void RDNA3_Shadow_Pager_Init();
    void RDNA3_Sovereign_Page_Fault();

    // Compression/Decompression
    void BitPlane_Inflate_4bit_to_FP16(uint8_t* input, float* bias, uint16_t* output);
    void RDNA3_3x_Expand(uint8_t* compressed, uint8_t* output);
    void RDNA3_Custom_Inflate(uint8_t* input, uint8_t* output);
    void RDNA3_Sovereign_Deflate(uint8_t* input, uint8_t* output);

    // Power and RT Cores
    void RDNA3_Power_Pulse();
    void RDNA3_Speculative_Preload();
    void Neural_Entropy_Generate();

    // Security
    uint64_t Silicon_PUF_Generate();
    bool RDNA3_Silicon_Authenticate(uint64_t expected);

    // Hardware Access
    uint64_t RDNA3_MMIO_Read(uint32_t offset);
    void RDNA3_MMIO_Write(uint32_t offset, uint64_t value);
    void RDNA3_MicroSequencer_Load(uint8_t* microcode);
    void RDNA3_GDS_Neural_Write(uint64_t data, uint32_t offset);
    uint64_t RDNA3_GDS_Neural_Read(uint32_t offset);

    // Telemetry and Performance
    uint64_t RDNA3_Telemetry_Read();
    void DDR5_Saturate_Bandwidth();

    // Memory Management
    void RDNA3_BusMaster_DMA(uint64_t source, uint64_t dest, uint32_t size);
    uint64_t RDNA3_HugePage_Allocate();
    uint64_t RDNA3_3X_Virtualize(uint64_t va);
    uint64_t RDNA3_Elastic_Scale(uint64_t resources);
}

// Forward declarations
class Win32IDE;

class Win32IDE_TabManager
{
public:
    explicit Win32IDE_TabManager(Win32IDE* ide);
    ~Win32IDE_TabManager();

    // Core lifecycle
    bool initialize(HWND hwndParent);
    void cleanup();

    // Tab operations
    int addTab(const std::string& filePath, const std::string& displayName = "");
    void removeTab(int index);
    void setActiveTab(int index);
    int findTabByPath(const std::string& filePath) const;

    // Tab state management
    void setTabModified(int index, bool modified);
    const EditorTab* getActiveTab() const;
    const std::vector<EditorTab>& getAllTabs() const { return m_editorTabs; }

    // UI interaction
    void handleTabClick(POINT pt);
    void drawTabItem(DRAWITEMSTRUCT* dis);
    void updateTabDisplay(int index);

    // Persistence
    void persistTabState();
    void loadPersistedTabs();

    // Accessors
    HWND getTabBarHandle() const { return m_hwndTabBar; }
    int getActiveTabIndex() const { return m_activeTabIndex; }
    size_t getTabCount() const { return m_editorTabs.size(); }

    // GPU Sovereign Control Interface
    void initializeGPUSovereignControl();
    void performGPUHealthCheck();
    void enableNeuralEntropyShield();
    void optimizeVirtualMemory();
    void compressModelData(uint8_t* data, size_t size);
    void authenticateSiliconIntegrity();

private:
    // Theme and styling
    void applySovereignTheme();

    // UI updates
    void rebuildTabControl();

    // Tab close handling
    void handleTabClose(int index);

    // Persistence helpers
    std::string getTabStatePath() const;

    // Utility functions
    std::string extractFileName(const std::string& filePath) const;
    std::wstring utf8ToWide(const std::string& str) const;

    // Member variables
    Win32IDE* m_ide;
    HWND m_hwndTabBar;
    std::vector<EditorTab> m_editorTabs;
    int m_activeTabIndex;
};