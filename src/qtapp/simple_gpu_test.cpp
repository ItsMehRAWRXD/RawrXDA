#include <iostream>
#include <chrono>
#include <thread>
#include <windows.h>

#ifdef HAVE_VULKAN
#include <vulkan/vulkan.h>
#endif

void testVulkanGPU() {


#ifdef HAVE_VULKAN
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "GPU Test";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Test";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    VkInstance instance;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    
    if (result != VK_SUCCESS) {


        return;
    }

    // Enumerate physical devices
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    
    if (deviceCount == 0) {
        
        vkDestroyInstance(instance, nullptr);
        return;
    }

    VkPhysicalDevice* devices = new VkPhysicalDevice[deviceCount];
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices);

    // Get properties of first device
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceProperties(devices[0], &deviceProperties);
    vkGetPhysicalDeviceMemoryProperties(devices[0], &memProperties);


    // Find total VRAM
    size_t totalVRAM = 0;
    for (uint32_t i = 0; i < memProperties.memoryHeapCount; i++) {
        if (memProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            totalVRAM = memProperties.memoryHeaps[i].size;
            break;
        }
    }


    delete[] devices;
    vkDestroyInstance(instance, nullptr);
    
#else


#endif
}

void testMetrics() {


    auto start1 = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    for (int i = 0; i < 10; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    auto latency1 = std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1).count();
    
    auto start2 = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    for (int i = 0; i < 15; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    auto latency2 = std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2).count();


}

void testSystemInfo() {


    // Get Windows version
    OSVERSIONINFOEX osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);


    // Get CPU info
    SYSTEM_INFO siSysInfo;
    GetSystemInfo(&siSysInfo);


    // Get memory info
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx(&statex);


}

int main() {


    auto testStart = std::chrono::high_resolution_clock::now();
    
    testSystemInfo();
    testVulkanGPU();
    testMetrics();
    
    auto testEnd = std::chrono::high_resolution_clock::now();
    auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(testEnd - testStart).count();


    return 0;
}
