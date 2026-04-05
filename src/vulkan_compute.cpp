#include <vulkan/vulkan.h>
#include "vulkan_compute.h"
#include <iostream>
#include <algorithm>
#include <atomic>
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <cmath>
#include <cstring>
#include <windows.h>

// SCAFFOLD_105: Vulkan compute backend init


VulkanCompute::VulkanCompute()
    : instance_(nullptr), physical_device_(nullptr), device_(nullptr),
      command_pool_(nullptr), compute_queue_(nullptr) {
    std::memset(&device_info_, 0, sizeof(VulkanDeviceInfo));
}

VulkanCompute::~VulkanCompute() {
    Cleanup();
}

bool VulkanCompute::Initialize() {
    if (!CreateInstance()) {
        std::cerr << "Failed to create Vulkan instance" << std::endl;
        return false;
    }
    
    if (!SelectPhysicalDevice()) {
        std::cerr << "Failed to select physical device" << std::endl;
        return false;
    }
    
    if (!CreateLogicalDevice()) {
        std::cerr << "Failed to create logical device" << std::endl;
        return false;
    }
    
    if (!CreateCommandPool()) {
        std::cerr << "Failed to create command pool" << std::endl;
        return false;
    }
    
    // Initialize async command buffer pool
    InitializeCommandBufferPool(4);  // Start with 4 reusable command buffers
    
    std::cout << "Vulkan initialized successfully on device: " << device_info_.device_name << std::endl;
    std::cout << "AMD Device: " << (IsAMDDevice() ? "Yes" : "No") << std::endl;
    std::cout << "Compute Queue Family: " << device_info_.compute_queue_family << std::endl;
    
    return true;
}

void VulkanCompute::InitializeCommandBufferPool(uint32_t pool_size) {
    command_buffer_pool_.resize(pool_size);
    
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool_;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = pool_size;
    
    std::vector<VkCommandBuffer> buffers(pool_size);
    if (vkAllocateCommandBuffers(device_, &alloc_info, buffers.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate command buffers for pool" << std::endl;
        return;
    }
    
    // Create fences and populate pool
    for (uint32_t i = 0; i < pool_size; ++i) {
        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // Start signaled (available)
        
        if (vkCreateFence(device_, &fence_info, nullptr, &command_buffer_pool_[i].fence) != VK_SUCCESS) {
            std::cerr << "Failed to create fence for command buffer pool" << std::endl;
            return;
        }
        
        command_buffer_pool_[i].buffer = buffers[i];
        command_buffer_pool_[i].is_available = true;
        available_buffer_indices_.push(i);
    }
    
    std::cout << "Initialized command buffer pool with " << pool_size << " buffers" << std::endl;
}

void VulkanCompute::CleanupCommandBufferPool() {
    for (auto& cmd_buf : command_buffer_pool_) {
        if (cmd_buf.fence) {
            vkDestroyFence(device_, cmd_buf.fence, nullptr);
        }
    }
    command_buffer_pool_.clear();
    while (!available_buffer_indices_.empty()) {
        available_buffer_indices_.pop();
    }
}

VkCommandBuffer VulkanCompute::AcquireAsyncCommandBuffer() {
    // Try to find available command buffer
    while (!available_buffer_indices_.empty()) {
        size_t idx = available_buffer_indices_.front();
        available_buffer_indices_.pop();
        
        // Check if fence is complete (non-blocking)
        VkResult result = vkGetFenceStatus(device_, command_buffer_pool_[idx].fence);
        if (result == VK_SUCCESS) {
            // Fence is signaled, buffer is available
            command_buffer_pool_[idx].is_available = false;
            
            // Reset fence for next submission
            vkResetFences(device_, 1, &command_buffer_pool_[idx].fence);
            
            // Reset command buffer for reuse
            vkResetCommandBuffer(command_buffer_pool_[idx].buffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
            
            return command_buffer_pool_[idx].buffer;
        } else if (result == VK_NOT_READY) {
            // Buffer still in use, put it back and try next
            available_buffer_indices_.push(idx);
        }
    }
    
    // No buffers available - need to wait (shouldn't happen with adequate pool size)
    std::cerr << "WARNING: All command buffers in use, this may cause stalls. Increase pool size." << std::endl;
    return nullptr;
}

bool VulkanCompute::SubmitAsyncCommandBuffer(VkCommandBuffer cmd_buffer) {
    // Find which pool entry this is
    int pool_idx = -1;
    for (size_t i = 0; i < command_buffer_pool_.size(); ++i) {
        if (command_buffer_pool_[i].buffer == cmd_buffer) {
            pool_idx = i;
            break;
        }
    }
    
    if (pool_idx < 0) {
        std::cerr << "Command buffer not from pool" << std::endl;
        return false;
    }
    
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buffer;
    
    if (vkQueueSubmit(compute_queue_, 1, &submit_info, command_buffer_pool_[pool_idx].fence) != VK_SUCCESS) {
        std::cerr << "Failed to submit command buffer" << std::endl;
        return false;
    }
    
    return true;
}

bool VulkanCompute::FlushAsyncCommands() {
    // Wait for all command buffers to complete
    std::vector<VkFence> all_fences;
    for (auto& cmd_buf : command_buffer_pool_) {
        if (!cmd_buf.is_available) {
            all_fences.push_back(cmd_buf.fence);
        }
    }
    
    if (all_fences.empty()) {
        return true;  // Nothing to wait for
    }
    
    if (vkWaitForFences(device_, (uint32_t)all_fences.size(), all_fences.data(), VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        std::cerr << "Failed to wait for async command buffers" << std::endl;
        return false;
    }
    
    // Reset all fences after successful wait
    vkResetFences(device_, (uint32_t)all_fences.size(), all_fences.data());
    
    // Clear and repopulate available indices (simplified management)
    while (!available_buffer_indices_.empty()) {
        available_buffer_indices_.pop();
    }
    
    // Mark all as available and rebuild queue
    for (size_t i = 0; i < command_buffer_pool_.size(); ++i) {
        command_buffer_pool_[i].is_available = true;
        available_buffer_indices_.push(i);
    }
    
    return true;
}

bool VulkanCompute::CheckAsyncCompletion(VkCommandBuffer cmd_buffer) {
    // Find which pool entry this is
    int pool_idx = -1;
    for (size_t i = 0; i < command_buffer_pool_.size(); ++i) {
        if (command_buffer_pool_[i].buffer == cmd_buffer) {
            pool_idx = i;
            break;
        }
    }
    
    if (pool_idx < 0) {
        return false;
    }
    
    return vkGetFenceStatus(device_, command_buffer_pool_[pool_idx].fence) == VK_SUCCESS;
}

bool VulkanCompute::ExecuteSingleTimeCommands(std::function<void(VkCommandBuffer)> record_func) {
    // Allocate a one-time command buffer
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool_;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    if (vkAllocateCommandBuffers(device_, &alloc_info, &command_buffer) != VK_SUCCESS) {
        std::cerr << "Failed to allocate command buffer" << std::endl;
        return false;
    }

    // Begin recording
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
        std::cerr << "Failed to begin command buffer recording" << std::endl;
        vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
        return false;
    }

    // Execute the recording function
    record_func(command_buffer);

    // End recording
    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
        std::cerr << "Failed to end command buffer recording" << std::endl;
        vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
        return false;
    }

    // Submit and wait for completion
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    VkFence fence;
    if (vkCreateFence(device_, &fence_info, nullptr, &fence) != VK_SUCCESS) {
        std::cerr << "Failed to create fence" << std::endl;
        vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
        return false;
    }

    if (vkQueueSubmit(compute_queue_, 1, &submit_info, fence) != VK_SUCCESS) {
        std::cerr << "Failed to submit command buffer" << std::endl;
        vkDestroyFence(device_, fence, nullptr);
        vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
        return false;
    }

    // Wait for fence
    if (vkWaitForFences(device_, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        std::cerr << "Failed to wait for fence" << std::endl;
        vkDestroyFence(device_, fence, nullptr);
        vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
        return false;
    }

    // Cleanup
    vkDestroyFence(device_, fence, nullptr);
    vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);

    return true;
}

bool VulkanCompute::ExecuteCommandBuffer(VkCommandBuffer cmd_buffer) {
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buffer;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    VkFence fence;
    if (vkCreateFence(device_, &fence_info, nullptr, &fence) != VK_SUCCESS) {
        std::cerr << "Failed to create fence" << std::endl;
        return false;
    }

    if (vkQueueSubmit(compute_queue_, 1, &submit_info, fence) != VK_SUCCESS) {
        std::cerr << "Failed to submit command buffer" << std::endl;
        vkDestroyFence(device_, fence, nullptr);
        return false;
    }

    // Wait for fence with timeout of 5 seconds
    VkResult wait_result = vkWaitForFences(device_, 1, &fence, VK_TRUE, 5000000000ULL);
    if (wait_result != VK_SUCCESS) {
        std::cerr << "Command buffer execution timeout or failed" << std::endl;
        vkDestroyFence(device_, fence, nullptr);
        return false;
    }

    vkDestroyFence(device_, fence, nullptr);
    return true;
}

bool VulkanCompute::CreateInstance() {
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "RawrXD-ModelLoader";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "RawrXD";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    if (vkCreateInstance(&create_info, nullptr, &instance_) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance" << std::endl;
        return false;
    }

    return true;
}

bool VulkanCompute::SelectPhysicalDevice() {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);
    
    if (device_count == 0) {
        std::cerr << "No Vulkan devices found" << std::endl;
        return false;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance_, &device_count, devices.data());

    // Prefer AMD devices, then other discrete GPUs
    int best_device_idx = -1;
    int best_device_score = -1;

    for (uint32_t i = 0; i < device_count; ++i) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[i], &props);

        uint32_t vendor_id = props.vendorID;
        int score = 0;

        // AMD RDNA3 (7800XT is vendor 0x1002)
        if (vendor_id == 0x1002) {
            score = 100;  // Highest priority
        } else if (vendor_id == 0x10DE) {
            score = 80;   // Nvidia
        } else if (vendor_id == 0x8086) {
            score = 60;   // Intel
        } else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score = 50;
        } else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            score = 30;
        }

        if (score > best_device_score) {
            best_device_score = score;
            best_device_idx = i;
        }

        std::cout << "Found device " << i << ": " << props.deviceName 
                  << " (Vendor: 0x" << std::hex << vendor_id << std::dec 
                  << ", Score: " << score << ")" << std::endl;
    }

    if (best_device_idx < 0) {
        std::cerr << "No suitable device found" << std::endl;
        return false;
    }

    physical_device_ = devices[best_device_idx];
    vkGetPhysicalDeviceProperties(physical_device_, &device_info_.properties);
    vkGetPhysicalDeviceMemoryProperties(physical_device_, &device_info_.memory_props);
    
    device_info_.device_name = device_info_.properties.deviceName;
    device_info_.vendor_id = device_info_.properties.vendorID;
    device_info_.device_id = device_info_.properties.deviceID;

    std::cout << "Selected device: " << device_info_.device_name << std::endl;

    return true;
}

bool VulkanCompute::CreateLogicalDevice() {
    // Find compute queue family
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, queue_families.data());

    int compute_queue_family = -1;
    for (uint32_t i = 0; i < queue_family_count; ++i) {
        if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            compute_queue_family = i;
            break;
        }
    }

    if (compute_queue_family < 0) {
        std::cerr << "No compute queue family found" << std::endl;
        return false;
    }

    device_info_.compute_queue_family = compute_queue_family;
    device_info_.supports_compute = true;

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = compute_queue_family;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &queue_create_info;

    if (vkCreateDevice(physical_device_, &device_create_info, nullptr, &device_) != VK_SUCCESS) {
        std::cerr << "Failed to create logical device" << std::endl;
        return false;
    }

    vkGetDeviceQueue(device_, compute_queue_family, 0, &compute_queue_);

    return true;
}

bool VulkanCompute::CreateCommandPool() {
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = device_info_.compute_queue_family;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_) != VK_SUCCESS) {
        std::cerr << "Failed to create command pool" << std::endl;
        return false;
    }

    return true;
}

bool VulkanCompute::LoadShader(const std::string& name, const std::string& spirv_path) {
    std::vector<uint32_t> spirv_code;
    if (!LoadSPIRVCode(spirv_path, spirv_code)) {
        std::cerr << "Failed to load SPIR-V code: " << spirv_path << std::endl;
        return false;
    }

    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = spirv_code.size() * sizeof(uint32_t);
    create_info.pCode = spirv_code.data();

    ComputeShader shader;
    shader.name = name;
    shader.spirv_code = spirv_code;

    if (vkCreateShaderModule(device_, &create_info, nullptr, &shader.module) != VK_SUCCESS) {
        std::cerr << "Failed to create shader module: " << name << std::endl;
        return false;
    }

    shaders_[name] = std::move(shader);
    std::cout << "Loaded shader: " << name << std::endl;

    return true;
}

bool VulkanCompute::CreateComputePipeline(const std::string& shader_name) {
    auto it = shaders_.find(shader_name);
    if (it == shaders_.end()) {
        std::cerr << "Shader not found: " << shader_name << std::endl;
        return false;
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if (vkCreatePipelineLayout(device_, &pipeline_layout_info, nullptr, &it->second.layout) != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline layout: " << shader_name << std::endl;
        return false;
    }

    VkPipelineShaderStageCreateInfo stage_info{};
    stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage_info.module = it->second.module;
    stage_info.pName = "main";

    VkComputePipelineCreateInfo compute_pipeline_info{};
    compute_pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_pipeline_info.layout = it->second.layout;
    compute_pipeline_info.stage = stage_info;

    if (vkCreateComputePipelines(device_, nullptr, 1, &compute_pipeline_info, nullptr, &it->second.pipeline) != VK_SUCCESS) {
        std::cerr << "Failed to create compute pipeline: " << shader_name << std::endl;
        return false;
    }

    std::cout << "Created compute pipeline: " << shader_name << std::endl;
    return true;
}

bool VulkanCompute::AllocateBuffer(size_t size, VkBuffer& buffer, VkDeviceMemory& memory) {
    buffer = nullptr;
    memory = nullptr;

    if (!device_ || size == 0) {
        std::cerr << "AllocateBuffer called with invalid device state or zero size" << std::endl;
        return false;
    }

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (vkCreateBuffer(device_, &buffer_info, nullptr, &buffer) != VK_SUCCESS) {
        std::cerr << "Failed to create buffer" << std::endl;
        return false;
    }

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device_, buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = FindMemoryType(mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (alloc_info.memoryTypeIndex == UINT32_MAX) {
        std::cerr << "Failed to find suitable device-local memory type" << std::endl;
        vkDestroyBuffer(device_, buffer, nullptr);
        return false;
    }

    if (vkAllocateMemory(device_, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate memory" << std::endl;
        vkDestroyBuffer(device_, buffer, nullptr);
        return false;
    }

    if (vkBindBufferMemory(device_, buffer, memory, 0) != VK_SUCCESS) {
        std::cerr << "Failed to bind buffer memory" << std::endl;
        vkFreeMemory(device_, memory, nullptr);
        vkDestroyBuffer(device_, buffer, nullptr);
        buffer = nullptr;
        memory = nullptr;
        return false;
    }

    return true;
}

// ==================== STAGING BUFFER HELPER ====================
bool VulkanCompute::CreateStagingBuffer(size_t size, VkBuffer& buffer, VkDeviceMemory& memory) {
    buffer = nullptr;
    memory = nullptr;

    if (!device_ || size == 0) {
        std::cerr << "CreateStagingBuffer called with invalid device state or zero size" << std::endl;
        return false;
    }

    // Create staging buffer with host-accessible memory
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (vkCreateBuffer(device_, &buffer_info, nullptr, &buffer) != VK_SUCCESS) {
        std::cerr << "Failed to create staging buffer" << std::endl;
        return false;
    }

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device_, buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = FindMemoryType(mem_requirements.memoryTypeBits,
                                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (alloc_info.memoryTypeIndex == UINT32_MAX) {
        std::cerr << "Failed to find suitable host-visible staging memory type" << std::endl;
        vkDestroyBuffer(device_, buffer, nullptr);
        return false;
    }

    if (vkAllocateMemory(device_, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate staging memory" << std::endl;
        vkDestroyBuffer(device_, buffer, nullptr);
        return false;
    }

    if (vkBindBufferMemory(device_, buffer, memory, 0) != VK_SUCCESS) {
        std::cerr << "Failed to bind staging buffer memory" << std::endl;
        vkFreeMemory(device_, memory, nullptr);
        vkDestroyBuffer(device_, buffer, nullptr);
        buffer = nullptr;
        memory = nullptr;
        return false;
    }

    return true;
}
// ==================== END STAGING BUFFER HELPER ====================

bool VulkanCompute::AllocateBuffer(size_t size, uint32_t& buffer_idx, size_t& memory_size) {
    VkBuffer buffer;
    VkDeviceMemory memory;
    
    if (!AllocateBuffer(size, buffer, memory)) {
        return false;
    }
    
    buffer_idx = static_cast<uint32_t>(allocated_buffers_.size());
    allocated_buffers_.push_back(std::make_pair(buffer, memory));
    memory_size = size;
    
    std::cout << "Allocated buffer " << buffer_idx << " with " << size << " bytes" << std::endl;
    return true;
}

bool VulkanCompute::CopyBufferToHost(VkBuffer device_buffer, void* host_data, size_t size) {
    if (!device_buffer || !host_data || size == 0) {
        std::cerr << "CopyBufferToHost called with invalid arguments" << std::endl;
        return false;
    }

    // Create staging buffer using helper
    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    
    if (!CreateStagingBuffer(size, staging_buffer, staging_memory)) {
        return false;
    }

    // Record copy command from device buffer to staging buffer
    if (!ExecuteSingleTimeCommands([&](VkCommandBuffer cmd_buffer) {
        VkBufferCopy copy_region{};
        copy_region.srcOffset = 0;
        copy_region.dstOffset = 0;
        copy_region.size = size;
        vkCmdCopyBuffer(cmd_buffer, device_buffer, staging_buffer, 1, &copy_region);
    })) {
        std::cerr << "Failed to copy buffer from device to host" << std::endl;
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        vkFreeMemory(device_, staging_memory, nullptr);
        return false;
    }

    // Map and read staging buffer
    void* mapped_data;
    if (vkMapMemory(device_, staging_memory, 0, size, 0, &mapped_data) != VK_SUCCESS) {
        std::cerr << "Failed to map staging memory" << std::endl;
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        vkFreeMemory(device_, staging_memory, nullptr);
        return false;
    }

    std::memcpy(host_data, mapped_data, size);
    vkUnmapMemory(device_, staging_memory);

    // Cleanup staging buffer
    vkDestroyBuffer(device_, staging_buffer, nullptr);
    vkFreeMemory(device_, staging_memory, nullptr);

    return true;
}

bool VulkanCompute::CopyBufferToHost(uint32_t buffer_idx, void* host_data, size_t size) {
    if (buffer_idx >= allocated_buffers_.size()) {
        std::cerr << "Invalid buffer index: " << buffer_idx << std::endl;
        return false;
    }
    
    return CopyBufferToHost(allocated_buffers_[buffer_idx].first, host_data, size);
}

bool VulkanCompute::CopyHostToBuffer(void* host_data, VkBuffer device_buffer, size_t size) {
    if (!host_data || !device_buffer || size == 0) {
        std::cerr << "CopyHostToBuffer called with invalid arguments" << std::endl;
        return false;
    }

    // Create staging buffer using helper
    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    
    if (!CreateStagingBuffer(size, staging_buffer, staging_memory)) {
        return false;
    }

    // Map and copy host data to staging buffer
    void* mapped_data;
    if (vkMapMemory(device_, staging_memory, 0, size, 0, &mapped_data) != VK_SUCCESS) {
        std::cerr << "Failed to map staging memory" << std::endl;
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        vkFreeMemory(device_, staging_memory, nullptr);
        return false;
    }

    std::memcpy(mapped_data, host_data, size);
    vkUnmapMemory(device_, staging_memory);

    // Record copy command from staging buffer to device buffer
    if (!ExecuteSingleTimeCommands([&](VkCommandBuffer cmd_buffer) {
        VkBufferCopy copy_region{};
        copy_region.srcOffset = 0;
        copy_region.dstOffset = 0;
        copy_region.size = size;
        vkCmdCopyBuffer(cmd_buffer, staging_buffer, device_buffer, 1, &copy_region);
    })) {
        std::cerr << "Failed to copy buffer from host to device" << std::endl;
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        vkFreeMemory(device_, staging_memory, nullptr);
        return false;
    }

    // Cleanup staging buffer
    vkDestroyBuffer(device_, staging_buffer, nullptr);
    vkFreeMemory(device_, staging_memory, nullptr);

    return true;
}

bool VulkanCompute::CopyHostToBuffer(void* host_data, uint32_t buffer_idx, size_t size) {
    if (buffer_idx >= allocated_buffers_.size()) {
        std::cerr << "Invalid buffer index: " << buffer_idx << std::endl;
        return false;
    }
    
    return CopyHostToBuffer(host_data, allocated_buffers_[buffer_idx].first, size);
}

VulkanTensor VulkanCompute::TransferGGUFTensor(const std::string& tensor_name,
                                               const void* data_ptr,
                                               size_t size_bytes,
                                               uint32_t usage) {
    VulkanTensor tensor;
    tensor.name = tensor_name;
    tensor.size_bytes = size_bytes;
    
    // Copy data to host memory
    tensor.host_data.resize(size_bytes / sizeof(float));
    std::memcpy(tensor.host_data.data(), data_ptr, size_bytes);
    
    // Allocate device buffer
    if (!AllocateBuffer(size_bytes, tensor.device_buffer, tensor.device_memory)) {
        std::cerr << "Failed to allocate device buffer for tensor: " << tensor_name << std::endl;
        return tensor;
    }
    
    // Copy host data to device
    if (!CopyHostToBuffer(tensor.host_data.data(), tensor.device_buffer, size_bytes)) {
        std::cerr << "Failed to transfer tensor to device: " << tensor_name << std::endl;
        return tensor;
    }
    
    uploaded_tensors_.push_back(tensor);
    std::cout << "Transferred tensor '" << tensor_name << "' (" << size_bytes << " bytes) to device" << std::endl;
    
    return tensor;
}

void VulkanCompute::ReleaseTensors() {
    for (auto& tensor : uploaded_tensors_) {
        if (tensor.device_buffer) {
            vkDestroyBuffer(device_, tensor.device_buffer, nullptr);
        }
        if (tensor.device_memory) {
            vkFreeMemory(device_, tensor.device_memory, nullptr);
        }
    }
    uploaded_tensors_.clear();
    std::cout << "Released all tensors" << std::endl;
}

bool VulkanCompute::EnsureMatMulPipeline(const std::string& spirv_path) {
    // Check if matmul pipeline already exists
    auto it = shaders_.find("matmul");
    if (it != shaders_.end() && it->second.pipeline && matmul_descriptor_set_layout_ && matmul_descriptor_pool_) {
        return true;  // Already fully initialized
    }
    
    // 1. Load Shader
    if (!LoadShader("matmul", spirv_path)) {
        std::cerr << "Failed to load matmul shader from: " << spirv_path << std::endl;
        return false;
    }
    
    // Get the shader iterator again after loading
    it = shaders_.find("matmul");
    if (it == shaders_.end()) {
        std::cerr << "Shader not found after loading" << std::endl;
        return false;
    }
    
    // --- 2. Create PERMANENT Descriptor Set Layout (Bindings 0, 1, 2 for A, B, Output) ---
    std::vector<VkDescriptorSetLayoutBinding> bindings(3);
    for (uint32_t i = 0; i < 3; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[i].pImmutableSamplers = nullptr;
    }
    
    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 3;
    layout_info.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device_, &layout_info, nullptr, &matmul_descriptor_set_layout_) != VK_SUCCESS) {
        std::cerr << "Failed to create MatMul descriptor set layout!" << std::endl;
        return false;
    }
    std::cout << "Created permanent MatMul descriptor set layout" << std::endl;

    // --- 3. Create PERMANENT Descriptor Pool ---
    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_size.descriptorCount = 30;  // Max 10 simultaneous MatMul sets (3 buffers each)
    
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    pool_info.maxSets = 10;  // Max 10 descriptor sets
    
    if (vkCreateDescriptorPool(device_, &pool_info, nullptr, &matmul_descriptor_pool_) != VK_SUCCESS) {
        std::cerr << "Failed to create MatMul descriptor pool!" << std::endl;
        vkDestroyDescriptorSetLayout(device_, matmul_descriptor_set_layout_, nullptr);
        matmul_descriptor_set_layout_ = nullptr;
        return false;
    }
    std::cout << "Created permanent MatMul descriptor pool" << std::endl;
    
    // --- 4. Create Pipeline Layout (using the permanent layout with Push Constants) ---
    VkPushConstantRange push_constant{};
    push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push_constant.offset = 0;
    push_constant.size = sizeof(uint32_t) * 3;  // For M, K, N
    
    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &matmul_descriptor_set_layout_;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant;

    if (vkCreatePipelineLayout(device_, &pipeline_layout_info, nullptr, &it->second.layout) != VK_SUCCESS) {
        std::cerr << "Failed to create MatMul pipeline layout" << std::endl;
        vkDestroyDescriptorSetLayout(device_, matmul_descriptor_set_layout_, nullptr);
        vkDestroyDescriptorPool(device_, matmul_descriptor_pool_, nullptr);
        matmul_descriptor_set_layout_ = nullptr;
        matmul_descriptor_pool_ = nullptr;
        return false;
    }
    std::cout << "Created MatMul pipeline layout with push constants" << std::endl;
    
    // --- 5. Create Compute Pipeline ---
    VkPipelineShaderStageCreateInfo stage_info{};
    stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage_info.module = it->second.module;
    stage_info.pName = "main";

    VkComputePipelineCreateInfo compute_pipeline_info{};
    compute_pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_pipeline_info.layout = it->second.layout;
    compute_pipeline_info.stage = stage_info;

    if (vkCreateComputePipelines(device_, nullptr, 1, &compute_pipeline_info, nullptr, &it->second.pipeline) != VK_SUCCESS) {
        std::cerr << "Failed to create MatMul compute pipeline" << std::endl;
        return false;
    }

    std::cout << "MatMul pipeline and permanent descriptor system initialized successfully" << std::endl;
    return true;
}

bool VulkanCompute::CreateDescriptorSetLayout(uint32_t binding_count, VkDescriptorSetLayout& layout) {
    // This function is deprecated - descriptor layouts are now created in EnsureMatMulPipeline
    // Kept for backward compatibility but should not be used
    std::cerr << "WARNING: CreateDescriptorSetLayout is deprecated. Use EnsureMatMulPipeline instead." << std::endl;
    return false;
}

bool VulkanCompute::AllocateDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorSet& descriptor_set) {
    // This function is deprecated - descriptor set allocation is now handled in DispatchMatMul
    // Kept for backward compatibility but should not be used
    std::cerr << "WARNING: AllocateDescriptorSet is deprecated. Use DispatchMatMul instead." << std::endl;
    return false;
}

bool VulkanCompute::UpdateDescriptorSet(VkDescriptorSet descriptor_set, uint32_t binding, 
                                        VkBuffer buffer, size_t buffer_size) {
    // This function is deprecated - descriptor set updates are now handled in DispatchMatMul
    // Kept for backward compatibility but should not be used
    std::cerr << "WARNING: UpdateDescriptorSet is deprecated. Use DispatchMatMul instead." << std::endl;
    return false;
}

bool VulkanCompute::DispatchMatMul(uint32_t input_a_idx,
                                   uint32_t input_b_idx,
                                   uint32_t output_idx,
                                   uint32_t M,
                                   uint32_t K,
                                   uint32_t N) {
    
    // Validate buffer indices
    if (input_a_idx >= allocated_buffers_.size() || 
        input_b_idx >= allocated_buffers_.size() ||
        output_idx >= allocated_buffers_.size()) {
        std::cerr << "Invalid buffer indices for MatMul dispatch" << std::endl;
        return false;
    }
    
    // Check if matmul pipeline and descriptor system are initialized
    auto it = shaders_.find("matmul");
    if (it == shaders_.end() || !it->second.pipeline || !matmul_descriptor_set_layout_ || !matmul_descriptor_pool_) {
        std::cerr << "MatMul pipeline or descriptor system not initialized. Call EnsureMatMulPipeline first." << std::endl;
        return false;
    }
    
    // Get buffer handles
    VkBuffer buffers[3] = {
        allocated_buffers_[input_a_idx].first,
        allocated_buffers_[input_b_idx].first,
        allocated_buffers_[output_idx].first
    };
    
    // Calculate buffer sizes
    size_t sizes[3] = {
        (size_t)M * K * sizeof(float),
        (size_t)K * N * sizeof(float),
        (size_t)M * N * sizeof(float)
    };

    // --- 1. Allocate Descriptor Set from the PERMANENT Pool ---
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = matmul_descriptor_pool_;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &matmul_descriptor_set_layout_;

    VkDescriptorSet descriptor_set = nullptr;
    if (vkAllocateDescriptorSets(device_, &alloc_info, &descriptor_set) != VK_SUCCESS) {
        std::cerr << "Failed to allocate descriptor set for MatMul" << std::endl;
        return false;
    }

    // --- 2. Update Descriptor Set (Bind Buffers to Bindings 0, 1, 2) ---
    std::vector<VkDescriptorBufferInfo> buffer_infos(3);
    std::vector<VkWriteDescriptorSet> writes(3);

    for (int i = 0; i < 3; ++i) {
        buffer_infos[i].buffer = buffers[i];
        buffer_infos[i].offset = 0;
        buffer_infos[i].range = sizes[i];

        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].pNext = nullptr;
        writes[i].dstSet = descriptor_set;
        writes[i].dstBinding = i;
        writes[i].dstArrayElement = 0;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pImageInfo = nullptr;
        writes[i].pBufferInfo = &buffer_infos[i];
        writes[i].pTexelBufferView = nullptr;
    }

    vkUpdateDescriptorSets(device_, (uint32_t)writes.size(), writes.data(), 0, nullptr);
    std::cout << "Updated descriptor set with 3 storage buffers (A, B, Output)" << std::endl;

    // --- 3. Execute Command Buffer (Dispatch) ---
    bool success = ExecuteSingleTimeCommands([&](VkCommandBuffer cmd_buffer) {
        
        // Push Constants (M, K, N dimensions to shader)
        uint32_t push_data[3] = {M, K, N};
        vkCmdPushConstants(cmd_buffer, 
                           it->second.layout, 
                           VK_SHADER_STAGE_COMPUTE_BIT, 
                           0, 
                           sizeof(push_data), 
                           push_data);

        // Bind Compute Pipeline
        vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, it->second.pipeline);

        // Bind Descriptor Set
        vkCmdBindDescriptorSets(cmd_buffer, 
                                VK_PIPELINE_BIND_POINT_COMPUTE, 
                                it->second.layout, 
                                0, 1, &descriptor_set, 0, nullptr);

        // Dispatch Command (Assuming 16x16 tile workgroup size in shader)
        const uint32_t TILE_SIZE = 16;
        uint32_t group_count_x = (N + TILE_SIZE - 1) / TILE_SIZE;
        uint32_t group_count_y = (M + TILE_SIZE - 1) / TILE_SIZE;
        
        std::cout << "Dispatching: " << group_count_x << "x" << group_count_y << "x1 workgroups" << std::endl;
        vkCmdDispatch(cmd_buffer, group_count_x, group_count_y, 1);
    });
    
    // --- 4. Clean up Descriptor Set ---
    if (descriptor_set) {
        // Free the set back to the pool for reuse
        vkFreeDescriptorSets(device_, matmul_descriptor_pool_, 1, &descriptor_set);
    }
    
    if (success) {
        std::cout << "MatMul dispatch completed successfully (" << M << "x" << K << " * " << K << "x" << N << " -> " << M << "x" << N << ")" << std::endl;
    } else {
        std::cerr << "MatMul GPU dispatch failed." << std::endl;
    }
    
    return success;
}

    bool VulkanCompute::DispatchRaw3Buffers(const std::string& shader_name,
                                            uint32_t input_a_idx,
                                            uint32_t input_b_idx,
                                            uint32_t output_idx,
                                            size_t size_a,
                                            size_t size_b,
                                            size_t size_out,
                                            const void* push_constants,
                                            uint32_t push_constant_size,
                                            uint32_t group_x,
                                            uint32_t group_y,
                                            uint32_t group_z) {
        if (input_a_idx >= allocated_buffers_.size() ||
            input_b_idx >= allocated_buffers_.size() ||
            output_idx >= allocated_buffers_.size()) {
            return false;
        }

        if (group_x == 0 || group_y == 0 || group_z == 0) {
            return false;
        }

        auto it = shaders_.find(shader_name);
        if (it == shaders_.end() || !it->second.pipeline || !it->second.layout ||
            !matmul_descriptor_set_layout_ || !matmul_descriptor_pool_) {
            return false;
        }

        VkBuffer buffers[3] = {
            allocated_buffers_[input_a_idx].first,
            allocated_buffers_[input_b_idx].first,
            allocated_buffers_[output_idx].first,
        };

        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = matmul_descriptor_pool_;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &matmul_descriptor_set_layout_;

        VkDescriptorSet descriptor_set = nullptr;
        if (vkAllocateDescriptorSets(device_, &alloc_info, &descriptor_set) != VK_SUCCESS) {
            return false;
        }

        std::vector<VkDescriptorBufferInfo> infos(3);
        infos[0] = { buffers[0], 0, size_a };
        infos[1] = { buffers[1], 0, size_b };
        infos[2] = { buffers[2], 0, size_out };

        std::vector<VkWriteDescriptorSet> writes(3);
        for (uint32_t i = 0; i < 3; ++i) {
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = descriptor_set;
            writes[i].dstBinding = i;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[i].pBufferInfo = &infos[i];
        }
        vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

        const bool success = ExecuteSingleTimeCommands([&](VkCommandBuffer cmd_buffer) {
            if (push_constants && push_constant_size > 0) {
                vkCmdPushConstants(cmd_buffer,
                                   it->second.layout,
                                   VK_SHADER_STAGE_COMPUTE_BIT,
                                   0,
                                   push_constant_size,
                                   push_constants);
            }

            vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, it->second.pipeline);
            vkCmdBindDescriptorSets(cmd_buffer,
                                    VK_PIPELINE_BIND_POINT_COMPUTE,
                                    it->second.layout,
                                    0,
                                    1,
                                    &descriptor_set,
                                    0,
                                    nullptr);
            vkCmdDispatch(cmd_buffer, group_x, group_y, group_z);
        });

        if (descriptor_set) {
            vkFreeDescriptorSets(device_, matmul_descriptor_pool_, 1, &descriptor_set);
        }

        return success;
    }

bool VulkanCompute::ExecuteMatMul(const float* input_a, const float* input_b,
                                  float* output, uint32_t m, uint32_t k, uint32_t n) {
    // Naive CPU fallback (O(m*k*n)) for benchmarking correctness.
    // If a Vulkan shader/pipeline is loaded named "matmul" we would dispatch it instead.
    // Clear output
    std::fill(output, output + (size_t)m * n, 0.0f);
    for (uint32_t row = 0; row < m; ++row) {
        const float* arow = input_a + (size_t)row * k;
        for (uint32_t col = 0; col < n; ++col) {
            float sum = 0.0f;
            for (uint32_t inner = 0; inner < k; ++inner) {
                sum += arow[inner] * input_b[(size_t)inner * n + col];
            }
            output[(size_t)row * n + col] = sum;
        }
    }
    return true;
}

bool VulkanCompute::DispatchMatMulAsync(uint32_t input_a_idx,
                                        uint32_t input_b_idx,
                                        uint32_t output_idx,
                                        uint32_t M,
                                        uint32_t K,
                                        uint32_t N) {
    
    // Validate buffer indices
    if (input_a_idx >= allocated_buffers_.size() || 
        input_b_idx >= allocated_buffers_.size() ||
        output_idx >= allocated_buffers_.size()) {
        std::cerr << "Invalid buffer indices for MatMul async dispatch" << std::endl;
        return false;
    }
    
    // Check if matmul pipeline and descriptor system are initialized
    auto it = shaders_.find("matmul");
    if (it == shaders_.end() || !it->second.pipeline || !matmul_descriptor_set_layout_ || !matmul_descriptor_pool_) {
        std::cerr << "MatMul pipeline not initialized. Call EnsureMatMulPipeline first." << std::endl;
        return false;
    }
    
    // Get buffer handles
    VkBuffer buffers[3] = {
        allocated_buffers_[input_a_idx].first,
        allocated_buffers_[input_b_idx].first,
        allocated_buffers_[output_idx].first
    };
    
    // Calculate buffer sizes
    size_t sizes[3] = {
        (size_t)M * K * sizeof(float),
        (size_t)K * N * sizeof(float),
        (size_t)M * N * sizeof(float)
    };

    // --- 1. Allocate Descriptor Set from PERMANENT Pool ---
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = matmul_descriptor_pool_;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &matmul_descriptor_set_layout_;

    VkDescriptorSet descriptor_set = nullptr;
    if (vkAllocateDescriptorSets(device_, &alloc_info, &descriptor_set) != VK_SUCCESS) {
        std::cerr << "Failed to allocate descriptor set for async MatMul" << std::endl;
        return false;
    }

    // --- 2. Update Descriptor Set (Bind Buffers A, B, Output) ---
    std::vector<VkDescriptorBufferInfo> buffer_infos(3);
    std::vector<VkWriteDescriptorSet> writes(3);

    for (int i = 0; i < 3; ++i) {
        buffer_infos[i].buffer = buffers[i];
        buffer_infos[i].offset = 0;
        buffer_infos[i].range = sizes[i];

        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].pNext = nullptr;
        writes[i].dstSet = descriptor_set;
        writes[i].dstBinding = i;
        writes[i].dstArrayElement = 0;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pImageInfo = nullptr;
        writes[i].pBufferInfo = &buffer_infos[i];
        writes[i].pTexelBufferView = nullptr;
    }

    vkUpdateDescriptorSets(device_, (uint32_t)writes.size(), writes.data(), 0, nullptr);

    // --- 3. Acquire Command Buffer from Async Pool (NON-BLOCKING) ---
    VkCommandBuffer cmd_buffer = AcquireAsyncCommandBuffer();
    if (!cmd_buffer) {
        std::cerr << "No available command buffers in pool. Consider FlushAsyncCommands()." << std::endl;
        vkFreeDescriptorSets(device_, matmul_descriptor_pool_, 1, &descriptor_set);
        return false;
    }

    // --- 4. Record Commands (Non-blocking recording) ---
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(cmd_buffer, &begin_info) != VK_SUCCESS) {
        std::cerr << "Failed to begin command buffer for async MatMul" << std::endl;
        vkFreeDescriptorSets(device_, matmul_descriptor_pool_, 1, &descriptor_set);
        return false;
    }

    // Push Constants
    uint32_t push_data[3] = {M, K, N};
    vkCmdPushConstants(cmd_buffer, 
                       it->second.layout, 
                       VK_SHADER_STAGE_COMPUTE_BIT, 
                       0, 
                       sizeof(push_data), 
                       push_data);

    // Bind Compute Pipeline
    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, it->second.pipeline);

    // Bind Descriptor Set
    vkCmdBindDescriptorSets(cmd_buffer, 
                            VK_PIPELINE_BIND_POINT_COMPUTE, 
                            it->second.layout, 
                            0, 1, &descriptor_set, 0, nullptr);

    // Dispatch with calculated workgroups
    const uint32_t TILE_SIZE = 16;  // Matches shader compile-time constant
    uint32_t group_count_x = (N + TILE_SIZE - 1) / TILE_SIZE;
    uint32_t group_count_y = (M + TILE_SIZE - 1) / TILE_SIZE;
    
    vkCmdDispatch(cmd_buffer, group_count_x, group_count_y, 1);

    if (vkEndCommandBuffer(cmd_buffer) != VK_SUCCESS) {
        std::cerr << "Failed to end command buffer for async MatMul" << std::endl;
        vkFreeDescriptorSets(device_, matmul_descriptor_pool_, 1, &descriptor_set);
        return false;
    }

    // --- 5. Submit Async (FIRE AND FORGET) ---
    if (!SubmitAsyncCommandBuffer(cmd_buffer)) {
        std::cerr << "Failed to submit async MatMul command buffer" << std::endl;
        vkFreeDescriptorSets(device_, matmul_descriptor_pool_, 1, &descriptor_set);
        return false;
    }

    std::cout << "Async MatMul queued: " << M << "x" << K << " * " << K << "x" << N 
              << " (" << group_count_x << "x" << group_count_y << " workgroups)" << std::endl;
    
    // Note: Descriptor set will be freed after GPU execution completes
    // In a production system, track descriptor sets with command buffers for deferred cleanup
    
    return true;
}

bool VulkanCompute::ExecuteAttention(const float* queries, const float* keys, const float* values,
                                     float* output, uint32_t seq_len, uint32_t head_dim) {
    // CPU scaled dot-product attention (single head)
    // Q: [seq_len, head_dim], K: [seq_len, head_dim], V: [seq_len, head_dim]
    // output = softmax(Q*K^T / sqrt(head_dim)) * V => [seq_len, head_dim]
    if (!queries || !keys || !values || !output || seq_len == 0 || head_dim == 0) return false;
    std::vector<float> scores((size_t)seq_len * seq_len);
    const float scale = 1.0f / std::sqrt((float)head_dim);
    // Compute QK^T
    for (uint32_t i = 0; i < seq_len; ++i) {
        const float* Qi = queries + (size_t)i * head_dim;
        for (uint32_t j = 0; j < seq_len; ++j) {
            const float* Kj = keys + (size_t)j * head_dim;
            float dot = 0.0f;
            for (uint32_t d = 0; d < head_dim; ++d) dot += Qi[d] * Kj[d];
            scores[(size_t)i * seq_len + j] = dot * scale;
        }
    }
    // Softmax per row
    for (uint32_t i = 0; i < seq_len; ++i) {
        float* row = scores.data() + (size_t)i * seq_len;
        float maxv = row[0];
        for (uint32_t j = 1; j < seq_len; ++j) maxv = std::max(maxv, row[j]);
        double sum = 0.0;
        for (uint32_t j = 0; j < seq_len; ++j) { row[j] = std::exp(row[j] - maxv); sum += row[j]; }
        double inv = sum == 0.0 ? 0.0 : 1.0 / sum;
        for (uint32_t j = 0; j < seq_len; ++j) row[j] = (float)(row[j] * inv);
    }
    // Multiply by V: output[i] = Σ_j scores[i,j] * V[j]
    for (uint32_t i = 0; i < seq_len; ++i) {
        float* Outi = output + (size_t)i * head_dim;
        std::fill(Outi, Outi + head_dim, 0.0f);
        float* attRow = scores.data() + (size_t)i * seq_len;
        for (uint32_t j = 0; j < seq_len; ++j) {
            const float* Vj = values + (size_t)j * head_dim;
            float w = attRow[j];
            for (uint32_t d = 0; d < head_dim; ++d) {
                Outi[d] += w * Vj[d];
            }
        }
    }
    return true;
}

bool VulkanCompute::ExecuteRoPE(float* embeddings, uint32_t dim, uint32_t seq_pos, uint32_t rotation_dim) {
    // Rotary Positional Embedding (RoPE) — CPU fallback implementation
    // Applies rotation to pairs of embedding dimensions using sinusoidal frequencies
    // Based on: https://arxiv.org/abs/2104.09864
    
    if (!embeddings || dim == 0 || rotation_dim == 0) {
        return false;
    }
    
    const float freq_base = 10000.0f;
    const uint32_t half_rot = rotation_dim / 2;
    
    for (uint32_t i = 0; i < half_rot; ++i) {
        // Compute frequency for this dimension pair
        float freq = 1.0f / std::pow(freq_base, (float)(2 * i) / (float)rotation_dim);
        float theta = (float)seq_pos * freq;
        float cos_theta = std::cos(theta);
        float sin_theta = std::sin(theta);
        
        // Rotate the pair (embeddings[2*i], embeddings[2*i+1])
        uint32_t idx0 = 2 * i;
        uint32_t idx1 = 2 * i + 1;
        
        if (idx1 < dim) {
            float x0 = embeddings[idx0];
            float x1 = embeddings[idx1];
            
            embeddings[idx0] = x0 * cos_theta - x1 * sin_theta;
            embeddings[idx1] = x0 * sin_theta + x1 * cos_theta;
        }
    }
    
    // Dimensions beyond rotation_dim are left unchanged (pass-through)
    return true;
}

bool VulkanCompute::ExecuteRMSNorm(float* data, uint32_t size, float epsilon) {
    // CPU RMSNorm: y = x / sqrt(mean(x^2) + eps)
    double accum = 0.0;
    for (uint32_t i = 0; i < size; ++i) {
        accum += (double)data[i] * (double)data[i];
    }
    double mean_sq = accum / std::max<uint32_t>(size,1);
    double denom = std::sqrt(mean_sq + (double)epsilon);
    if (denom == 0.0) denom = 1.0; // safety
    float inv = (float)(1.0 / denom);
    for (uint32_t i = 0; i < size; ++i) {
        data[i] = data[i] * inv;
    }
    return true;
}

bool VulkanCompute::ExecuteSiLU(float* data, uint32_t size) {
    // CPU SiLU: x * sigmoid(x)
    for (uint32_t i = 0; i < size; ++i) {
        float x = data[i];
        float s = 1.0f / (1.0f + std::exp(-x));
        data[i] = x * s;
    }
    return true;
}

bool VulkanCompute::ExecuteSoftmax(float* data, uint32_t size) {
    // CPU Softmax (in-place)
    if (size == 0) return true;
    float maxv = data[0];
    for (uint32_t i = 1; i < size; ++i) maxv = std::max(maxv, data[i]);
    double sum = 0.0;
    for (uint32_t i = 0; i < size; ++i) {
        data[i] = std::exp(data[i] - maxv);
        sum += data[i];
    }
    if (sum == 0.0) return true;
    double inv = 1.0 / sum;
    for (uint32_t i = 0; i < size; ++i) data[i] = (float)(data[i] * inv);
    return true;
}

bool VulkanCompute::ExecuteDequantize(const uint8_t* quantized, float* output,
                                      uint32_t elements, const std::string& quant_type) {
    if (!quantized || !output) return false;
    if (elements == 0) return true;
    // Basic branching for common quantization types.
    if (quant_type == "F32") {
        // Assume raw bytes represent floats (size must be elements*sizeof(float)) - copy reinterpret.
        const float* src = reinterpret_cast<const float*>(quantized);
        for (uint32_t i = 0; i < elements; ++i) output[i] = src[i];
        return true;
    } else if (quant_type == "Q2_K") {
        // Very rough: 2-bit values packed in bytes (4 values per byte). Scale to [-1,1].
        uint32_t outIndex = 0;
        for (uint32_t b = 0; outIndex < elements; ++b) {
            uint8_t packed = quantized[b];
            for (int nib = 0; nib < 4 && outIndex < elements; ++nib) {
                uint8_t v = (packed >> (nib * 2)) & 0x3;
                float f = (v / 3.0f) * 2.0f - 1.0f; // map 0..3 -> -1..1
                output[outIndex++] = f;
            }
        }
        return true;
    } else if (quant_type == "Q4_K") {
        // 4-bit values packed (2 per byte). Map 0..15 -> -1..1
        uint32_t outIndex = 0;
        for (uint32_t b = 0; outIndex < elements; ++b) {
            uint8_t packed = quantized[b];
            uint8_t hi = (packed >> 4) & 0xF;
            uint8_t lo = packed & 0xF;
            float fhi = (hi / 15.0f) * 2.0f - 1.0f;
            float flo = (lo / 15.0f) * 2.0f - 1.0f;
            output[outIndex++] = flo;
            if (outIndex < elements) output[outIndex++] = fhi;
        }
        return true;
    } else {
        // Fallback byte->[0,1]
        const float scale = 1.0f / 255.0f;
        for (uint32_t i = 0; i < elements; ++i) output[i] = quantized[i] * scale;
        return true;
    }
}

bool VulkanCompute::LoadSPIRVCode(const std::string& path, std::vector<uint32_t>& code) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open SPIR-V file: " << path << std::endl;
        return false;
    }

    size_t file_size = file.tellg();
    if (file_size % sizeof(uint32_t) != 0) {
        std::cerr << "Invalid SPIR-V file size: " << path << std::endl;
        return false;
    }

    file.seekg(0);
    code.resize(file_size / sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(code.data()), file_size);

    return true;
}

// ==================== KV CACHE INFRASTRUCTURE ====================

bool VulkanCompute::AllocateKVCache(uint32_t num_layers, uint32_t max_seq_len, uint32_t head_dim) {
    if (kv_cache_allocated_) {
        std::cerr << "KV cache already allocated. Call ClearKVCache() first." << std::endl;
        return false;
    }
    
    kv_cache_num_layers_ = num_layers;
    kv_cache_max_seq_len_ = max_seq_len;
    kv_cache_head_dim_ = head_dim;
    
    // Allocate 2 buffers per layer (K and V)
    kv_cache_buffers_.resize(num_layers * 2);
    
    size_t cache_size = static_cast<size_t>(max_seq_len) * head_dim * sizeof(float);
    
    std::cout << "Allocating KV cache: " << num_layers << " layers, "
              << max_seq_len << " max tokens, " << head_dim << " head_dim, "
              << (cache_size / 1024 / 1024) << " MB per buffer" << std::endl;
    
    for (uint32_t layer = 0; layer < num_layers; ++layer) {
        // Allocate K cache buffer
        VkBuffer k_buffer;
        VkDeviceMemory k_memory;
        if (!AllocateBuffer(cache_size, k_buffer, k_memory)) {
            std::cerr << "Failed to allocate K cache for layer " << layer << std::endl;
            ClearKVCache();
            return false;
        }
        kv_cache_buffers_[layer * 2] = {k_buffer, k_memory};
        
        // Zero-initialize K cache
        std::vector<float> zeros(max_seq_len * head_dim, 0.0f);
        if (!CopyHostToBuffer(zeros.data(), k_buffer, cache_size)) {
            std::cerr << "Failed to zero-init K cache for layer " << layer << std::endl;
            ClearKVCache();
            return false;
        }
        
        // Allocate V cache buffer
        VkBuffer v_buffer;
        VkDeviceMemory v_memory;
        if (!AllocateBuffer(cache_size, v_buffer, v_memory)) {
            std::cerr << "Failed to allocate V cache for layer " << layer << std::endl;
            ClearKVCache();
            return false;
        }
        kv_cache_buffers_[layer * 2 + 1] = {v_buffer, v_memory};
        
        // Zero-initialize V cache
        if (!CopyHostToBuffer(zeros.data(), v_buffer, cache_size)) {
            std::cerr << "Failed to zero-init V cache for layer " << layer << std::endl;
            ClearKVCache();
            return false;
        }
    }
    
    kv_cache_allocated_ = true;
    
    std::cout << "KV cache allocated successfully: "
              << (num_layers * 2 * cache_size / 1024 / 1024) << " MB total" << std::endl;
    
    return true;
}

bool VulkanCompute::AppendToKVCache(uint32_t layer_idx, const float* k_new, 
                                    const float* v_new, uint32_t token_pos) {
    if (!kv_cache_allocated_) {
        std::cerr << "KV cache not allocated. Call AllocateKVCache() first." << std::endl;
        return false;
    }
    
    if (layer_idx >= kv_cache_num_layers_) {
        std::cerr << "Invalid layer index: " << layer_idx << " >= " << kv_cache_num_layers_ << std::endl;
        return false;
    }
    
    if (token_pos >= kv_cache_max_seq_len_) {
        std::cerr << "Token position " << token_pos << " exceeds max_seq_len " << kv_cache_max_seq_len_ << std::endl;
        return false;
    }
    
    // Calculate offset and size for this token's K/V vectors
    size_t offset = static_cast<size_t>(token_pos) * kv_cache_head_dim_ * sizeof(float);
    size_t size = kv_cache_head_dim_ * sizeof(float);
    
    // Get K/V cache buffers for this layer
    VkBuffer k_buffer = kv_cache_buffers_[layer_idx * 2].first;
    VkBuffer v_buffer = kv_cache_buffers_[layer_idx * 2 + 1].first;
    
    // Update K cache at token_pos
    if (!CopyHostToBufferOffset(k_new, k_buffer, offset, size)) {
        std::cerr << "Failed to update K cache at layer " << layer_idx << ", pos " << token_pos << std::endl;
        return false;
    }
    
    // Update V cache at token_pos
    if (!CopyHostToBufferOffset(v_new, v_buffer, offset, size)) {
        std::cerr << "Failed to update V cache at layer " << layer_idx << ", pos " << token_pos << std::endl;
        return false;
    }
    
    return true;
}

bool VulkanCompute::GetKVCacheSlice(uint32_t layer_idx, uint32_t start_pos, 
                                    uint32_t end_pos, float* k_out, float* v_out) {
    if (!kv_cache_allocated_) {
        std::cerr << "KV cache not allocated" << std::endl;
        return false;
    }
    
    if (layer_idx >= kv_cache_num_layers_) {
        std::cerr << "Invalid layer index: " << layer_idx << std::endl;
        return false;
    }
    
    if (end_pos > kv_cache_max_seq_len_ || start_pos >= end_pos) {
        std::cerr << "Invalid slice range: [" << start_pos << ", " << end_pos << ")" << std::endl;
        return false;
    }
    
    // Calculate offset and size
    size_t offset = static_cast<size_t>(start_pos) * kv_cache_head_dim_ * sizeof(float);
    size_t size = static_cast<size_t>(end_pos - start_pos) * kv_cache_head_dim_ * sizeof(float);
    
    // Get K/V cache buffers
    VkBuffer k_buffer = kv_cache_buffers_[layer_idx * 2].first;
    VkBuffer v_buffer = kv_cache_buffers_[layer_idx * 2 + 1].first;
    
    // Read K cache slice
    if (!CopyBufferToHostOffset(k_buffer, offset, k_out, size)) {
        std::cerr << "Failed to read K cache slice" << std::endl;
        return false;
    }
    
    // Read V cache slice
    if (!CopyBufferToHostOffset(v_buffer, offset, v_out, size)) {
        std::cerr << "Failed to read V cache slice" << std::endl;
        return false;
    }
    
    return true;
}

void VulkanCompute::ClearKVCache() {
    if (!kv_cache_allocated_) {
        return;
    }
    
    // Clean up all KV cache buffers
    for (auto& [buffer, memory] : kv_cache_buffers_) {
        if (buffer) {
            vkDestroyBuffer(device_, buffer, nullptr);
        }
        if (memory) {
            vkFreeMemory(device_, memory, nullptr);
        }
    }
    
    kv_cache_buffers_.clear();
    kv_cache_num_layers_ = 0;
    kv_cache_max_seq_len_ = 0;
    kv_cache_head_dim_ = 0;
    kv_cache_allocated_ = false;
    
    std::cout << "KV cache cleared" << std::endl;
}

// Helper: Copy host data to buffer at specific offset
bool VulkanCompute::CopyHostToBufferOffset(const void* host_data, VkBuffer device_buffer, 
                                           size_t offset, size_t size) {
    if (!device_ || !host_data || !device_buffer || size == 0) {
        std::cerr << "CopyHostToBufferOffset called with invalid arguments" << std::endl;
        return false;
    }

    // Create staging buffer
    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    
    if (vkCreateBuffer(device_, &buffer_info, nullptr, &staging_buffer) != VK_SUCCESS) {
        std::cerr << "Failed to create staging buffer for offset copy" << std::endl;
        return false;
    }
    
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device_, staging_buffer, &mem_requirements);
    
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = FindMemoryType(mem_requirements.memoryTypeBits,
                                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
                                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (alloc_info.memoryTypeIndex == UINT32_MAX) {
        std::cerr << "Failed to find host-visible memory type for offset staging upload" << std::endl;
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        return false;
    }
    
    if (vkAllocateMemory(device_, &alloc_info, nullptr, &staging_memory) != VK_SUCCESS) {
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        return false;
    }
    
    if (vkBindBufferMemory(device_, staging_buffer, staging_memory, 0) != VK_SUCCESS) {
        std::cerr << "Failed to bind offset staging buffer memory" << std::endl;
        vkFreeMemory(device_, staging_memory, nullptr);
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        return false;
    }
    
    // Map and copy
    void* mapped;
    if (vkMapMemory(device_, staging_memory, 0, size, 0, &mapped) != VK_SUCCESS) {
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        vkFreeMemory(device_, staging_memory, nullptr);
        return false;
    }
    
    std::memcpy(mapped, host_data, size);
    vkUnmapMemory(device_, staging_memory);
    
    // Copy from staging to device buffer at offset
    bool success = ExecuteSingleTimeCommands([&](VkCommandBuffer cmd_buffer) {
        VkBufferCopy copy_region{};
        copy_region.srcOffset = 0;
        copy_region.dstOffset = offset;
        copy_region.size = size;
        vkCmdCopyBuffer(cmd_buffer, staging_buffer, device_buffer, 1, &copy_region);
    });
    
    // Cleanup
    vkDestroyBuffer(device_, staging_buffer, nullptr);
    vkFreeMemory(device_, staging_memory, nullptr);
    
    return success;
}

// Helper: Copy buffer data at offset to host
bool VulkanCompute::CopyBufferToHostOffset(VkBuffer device_buffer, size_t offset, 
                                           void* host_data, size_t size) {
    if (!device_ || !device_buffer || !host_data || size == 0) {
        std::cerr << "CopyBufferToHostOffset called with invalid arguments" << std::endl;
        return false;
    }

    // Create staging buffer
    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    
    if (vkCreateBuffer(device_, &buffer_info, nullptr, &staging_buffer) != VK_SUCCESS) {
        return false;
    }
    
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device_, staging_buffer, &mem_requirements);
    
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = FindMemoryType(mem_requirements.memoryTypeBits,
                                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
                                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (alloc_info.memoryTypeIndex == UINT32_MAX) {
        std::cerr << "Failed to find host-visible memory type for offset staging download" << std::endl;
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        return false;
    }
    
    if (vkAllocateMemory(device_, &alloc_info, nullptr, &staging_memory) != VK_SUCCESS) {
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        return false;
    }
    
    if (vkBindBufferMemory(device_, staging_buffer, staging_memory, 0) != VK_SUCCESS) {
        std::cerr << "Failed to bind offset staging download buffer memory" << std::endl;
        vkFreeMemory(device_, staging_memory, nullptr);
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        return false;
    }
    
    // Copy from device buffer at offset to staging
    bool success = ExecuteSingleTimeCommands([&](VkCommandBuffer cmd_buffer) {
        VkBufferCopy copy_region{};
        copy_region.srcOffset = offset;
        copy_region.dstOffset = 0;
        copy_region.size = size;
        vkCmdCopyBuffer(cmd_buffer, device_buffer, staging_buffer, 1, &copy_region);
    });
    
    if (!success) {
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        vkFreeMemory(device_, staging_memory, nullptr);
        return false;
    }
    
    // Map and read
    void* mapped;
    if (vkMapMemory(device_, staging_memory, 0, size, 0, &mapped) != VK_SUCCESS) {
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        vkFreeMemory(device_, staging_memory, nullptr);
        return false;
    }
    
    std::memcpy(host_data, mapped, size);
    vkUnmapMemory(device_, staging_memory);
    
    // Cleanup
    vkDestroyBuffer(device_, staging_buffer, nullptr);
    vkFreeMemory(device_, staging_memory, nullptr);
    
    return true;
}

// ==================== END KV CACHE INFRASTRUCTURE ====================

uint32_t VulkanCompute::FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) {
    for (uint32_t i = 0; i < device_info_.memory_props.memoryTypeCount; ++i) {
        if ((type_filter & (1 << i)) && 
            (device_info_.memory_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    std::cerr << "Failed to find matching Vulkan memory type for properties mask " << properties << std::endl;
    return UINT32_MAX;
}

void VulkanCompute::Cleanup() {
    if (device_) {
        vkDeviceWaitIdle(device_);
        
        // Clean up KV cache
        ClearKVCache();
        
        // Clean up async command buffer pool
        CleanupCommandBufferPool();
        
        // Clean up shaders and pipelines
        for (auto& shader : shaders_) {
            if (shader.second.pipeline) {
                vkDestroyPipeline(device_, shader.second.pipeline, nullptr);
            }
            if (shader.second.layout) {
                vkDestroyPipelineLayout(device_, shader.second.layout, nullptr);
            }
            if (shader.second.module) {
                vkDestroyShaderModule(device_, shader.second.module, nullptr);
            }
        }
        shaders_.clear();
        
        // Clean up permanent MatMul descriptor system
        if (matmul_descriptor_pool_) {
            vkDestroyDescriptorPool(device_, matmul_descriptor_pool_, nullptr);
            matmul_descriptor_pool_ = nullptr;
        }
        if (matmul_descriptor_set_layout_) {
            vkDestroyDescriptorSetLayout(device_, matmul_descriptor_set_layout_, nullptr);
            matmul_descriptor_set_layout_ = nullptr;
        }
        
        // Clean up Fused MLP descriptor system
        if (fused_mlp_descriptor_pool_) {
            vkDestroyDescriptorPool(device_, fused_mlp_descriptor_pool_, nullptr);
            fused_mlp_descriptor_pool_ = nullptr;
        }
        if (fused_mlp_descriptor_layout_) {
            vkDestroyDescriptorSetLayout(device_, fused_mlp_descriptor_layout_, nullptr);
            fused_mlp_descriptor_layout_ = nullptr;
        }
        
        // Clean up Flash Attention descriptor system
        if (flash_attn_descriptor_pool_) {
            vkDestroyDescriptorPool(device_, flash_attn_descriptor_pool_, nullptr);
            flash_attn_descriptor_pool_ = nullptr;
        }
        if (flash_attn_descriptor_layout_) {
            vkDestroyDescriptorSetLayout(device_, flash_attn_descriptor_layout_, nullptr);
            flash_attn_descriptor_layout_ = nullptr;
        }
        
        // Clean up persistent staging buffer (for optimized transfers)
        if (staging_buffer_) {
            vkDestroyBuffer(device_, staging_buffer_, nullptr);
            staging_buffer_ = nullptr;
        }
        if (staging_memory_) {
            vkFreeMemory(device_, staging_memory_, nullptr);
            staging_memory_ = nullptr;
        }
        
        // Clean up general descriptor pool (if used)
        if (descriptor_pool_) {
            vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
            descriptor_pool_ = nullptr;
        }
        
        // Clean up command pool
        if (command_pool_) {
            vkDestroyCommandPool(device_, command_pool_, nullptr);
            command_pool_ = nullptr;
        }
        
        // Clean up allocated buffers
        for (auto& [buffer, memory] : allocated_buffers_) {
            if (buffer) {
                vkDestroyBuffer(device_, buffer, nullptr);
            }
            if (memory) {
                vkFreeMemory(device_, memory, nullptr);
            }
        }
        allocated_buffers_.clear();
        
        // Clean up uploaded tensors
        ReleaseTensors();
        
        vkDestroyDevice(device_, nullptr);
        device_ = nullptr;
    }
    
    if (instance_) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = nullptr;
    }
    
    std::cout << "Vulkan resources cleaned up successfully" << std::endl;
}

// =============================================================================
// LoadShaderFromMemory — Load SPIR-V from a memory buffer (for hotswap)
// =============================================================================
bool VulkanCompute::LoadShaderFromMemory(const std::string& name, const uint32_t* spirv_code, size_t code_size) {
    if (!spirv_code || code_size == 0 || code_size % sizeof(uint32_t) != 0) {
        std::cerr << "Invalid SPIR-V memory buffer" << std::endl;
        return false;
    }

    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code_size;
    create_info.pCode = spirv_code;

    ComputeShader shader;
    shader.name = name;
    shader.spirv_code.assign(spirv_code, spirv_code + code_size / sizeof(uint32_t));

    if (vkCreateShaderModule(device_, &create_info, nullptr, &shader.module) != VK_SUCCESS) {
        std::cerr << "Failed to create shader module from memory: " << name << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> lock(shader_mutex_);
    shaders_[name] = std::move(shader);
    stats_.shader_load_count.fetch_add(1, std::memory_order_relaxed);
    std::cout << "Loaded shader from memory: " << name << std::endl;
    return true;
}

// =============================================================================
// ReplacePipeline — Destroy old pipeline and create new one from updated shader
// =============================================================================
bool VulkanCompute::ReplacePipeline(const std::string& shader_name, const std::string& new_spirv_path) {
    std::lock_guard<std::mutex> lock(shader_mutex_);

    auto it = shaders_.find(shader_name);
    if (it == shaders_.end()) {
        std::cerr << "Shader not found for replacement: " << shader_name << std::endl;
        return false;
    }

    // Ensure no in-flight work
    if (device_) vkDeviceWaitIdle(device_);

    // Destroy old shader module
    if (it->second.module) {
        vkDestroyShaderModule(device_, it->second.module, nullptr);
        it->second.module = nullptr;
    }
    // Destroy old pipeline (but keep layout — it's reusable)
    if (it->second.pipeline) {
        vkDestroyPipeline(device_, it->second.pipeline, nullptr);
        it->second.pipeline = nullptr;
    }

    // Load new SPIR-V
    std::vector<uint32_t> new_code;
    if (!LoadSPIRVCode(new_spirv_path, new_code)) {
        std::cerr << "Failed to load replacement SPIR-V from: " << new_spirv_path << std::endl;
        return false;
    }

    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = new_code.size() * sizeof(uint32_t);
    create_info.pCode = new_code.data();

    if (vkCreateShaderModule(device_, &create_info, nullptr, &it->second.module) != VK_SUCCESS) {
        std::cerr << "Failed to create replacement shader module" << std::endl;
        return false;
    }
    it->second.spirv_code = std::move(new_code);

    // Recreate pipeline with existing layout
    if (it->second.layout) {
        VkPipeline old_pipeline = it->second.pipeline;

        VkPipelineShaderStageCreateInfo stage_info{};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage_info.module = it->second.module;
        stage_info.pName = "main";

        VkComputePipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.layout = it->second.layout;
        pipeline_info.stage = stage_info;

        if (vkCreateComputePipelines(device_, nullptr, 1, &pipeline_info, nullptr, &it->second.pipeline) != VK_SUCCESS) {
            std::cerr << "Failed to create replacement compute pipeline" << std::endl;
            return false;
        }

        if (old_pipeline) {
            vkDestroyPipeline(device_, old_pipeline, nullptr);
        }
    }

    stats_.pipeline_create_count.fetch_add(1, std::memory_order_relaxed);
    std::cout << "Pipeline replaced: " << shader_name << std::endl;
    return true;
}

// =============================================================================
// HotswapShader — Atomic shader replacement without destroying in-flight work
// =============================================================================
bool VulkanCompute::HotswapShader(const std::string& pipeline_name,
                                   const uint32_t* new_spirv, size_t spirv_size) {
    if (!new_spirv || spirv_size == 0) return false;

    std::lock_guard<std::mutex> lock(shader_mutex_);

    auto it = shaders_.find(pipeline_name);
    if (it == shaders_.end()) {
        // Register as new shader
        return LoadShaderFromMemory(pipeline_name, new_spirv, spirv_size);
    }

    // Wait for GPU idle to ensure no in-flight references
    if (device_) vkDeviceWaitIdle(device_);

    // Create new shader module
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = spirv_size;
    create_info.pCode = new_spirv;

    VkShaderModule new_module = nullptr;
    if (vkCreateShaderModule(device_, &create_info, nullptr, &new_module) != VK_SUCCESS) {
        std::cerr << "HotswapShader: failed to create new module" << std::endl;
        return false;
    }

    // Destroy old module
    VkShaderModule old_module = it->second.module;
    it->second.module = new_module;
    it->second.spirv_code.assign(new_spirv, new_spirv + spirv_size / sizeof(uint32_t));

    if (old_module) {
        vkDestroyShaderModule(device_, old_module, nullptr);
    }

    // Rebuild pipeline if layout exists
    if (it->second.layout) {
        VkPipeline old_pipeline = it->second.pipeline;

        VkPipelineShaderStageCreateInfo stage_info{};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage_info.module = new_module;
        stage_info.pName = "main";

        VkComputePipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.layout = it->second.layout;
        pipeline_info.stage = stage_info;

        if (vkCreateComputePipelines(device_, nullptr, 1, &pipeline_info, nullptr, &it->second.pipeline) != VK_SUCCESS) {
            std::cerr << "HotswapShader: failed to rebuild pipeline" << std::endl;
            it->second.pipeline = old_pipeline; // Rollback
            return false;
        }

        if (old_pipeline) {
            vkDestroyPipeline(device_, old_pipeline, nullptr);
        }
    }

    stats_.pipeline_create_count.fetch_add(1, std::memory_order_relaxed);
    std::cout << "Shader hotswapped: " << pipeline_name << std::endl;
    return true;
}

// =============================================================================
// EnsureFusedMLPPipeline — Linear → GeLU → Linear fused kernel
// =============================================================================
bool VulkanCompute::EnsureFusedMLPPipeline(const std::string& spirv_path) {
    auto it = shaders_.find("fused_mlp");
    if (it != shaders_.end() && it->second.pipeline && fused_mlp_descriptor_layout_) {
        return true; // Already initialized
    }

    if (!LoadShader("fused_mlp", spirv_path)) return false;
    it = shaders_.find("fused_mlp");
    if (it == shaders_.end()) return false;

    // 4 bindings: input, weight1, weight2, output
    std::vector<VkDescriptorSetLayoutBinding> bindings(4);
    for (uint32_t i = 0; i < 4; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[i].pImmutableSamplers = nullptr;
    }

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 4;
    layout_info.pBindings = bindings.data();
    if (vkCreateDescriptorSetLayout(device_, &layout_info, nullptr, &fused_mlp_descriptor_layout_) != VK_SUCCESS) return false;

    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_size.descriptorCount = 40;
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    pool_info.maxSets = 10;
    if (vkCreateDescriptorPool(device_, &pool_info, nullptr, &fused_mlp_descriptor_pool_) != VK_SUCCESS) return false;

    // Push constants: batch_size, hidden_dim, intermediate_dim
    VkPushConstantRange push_constant{};
    push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push_constant.offset = 0;
    push_constant.size = sizeof(uint32_t) * 3;

    VkPipelineLayoutCreateInfo pli{};
    pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pli.setLayoutCount = 1;
    pli.pSetLayouts = &fused_mlp_descriptor_layout_;
    pli.pushConstantRangeCount = 1;
    pli.pPushConstantRanges = &push_constant;
    if (vkCreatePipelineLayout(device_, &pli, nullptr, &it->second.layout) != VK_SUCCESS) return false;

    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = it->second.module;
    stage.pName = "main";

    VkComputePipelineCreateInfo cpi{};
    cpi.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cpi.layout = it->second.layout;
    cpi.stage = stage;
    if (vkCreateComputePipelines(device_, nullptr, 1, &cpi, nullptr, &it->second.pipeline) != VK_SUCCESS) return false;

    stats_.pipeline_create_count.fetch_add(1, std::memory_order_relaxed);
    std::cout << "Fused MLP pipeline initialized" << std::endl;
    return true;
}

bool VulkanCompute::DispatchFusedMLP(uint32_t input_idx, uint32_t weight1_idx,
                                      uint32_t weight2_idx, uint32_t output_idx,
                                      uint32_t batch_size, uint32_t hidden_dim,
                                      uint32_t intermediate_dim) {
    if (input_idx >= allocated_buffers_.size() || weight1_idx >= allocated_buffers_.size() ||
        weight2_idx >= allocated_buffers_.size() || output_idx >= allocated_buffers_.size()) return false;

    auto it = shaders_.find("fused_mlp");
    if (it == shaders_.end() || !it->second.pipeline || !fused_mlp_descriptor_layout_) return false;

    VkBuffer bufs[4] = {
        allocated_buffers_[input_idx].first,
        allocated_buffers_[weight1_idx].first,
        allocated_buffers_[weight2_idx].first,
        allocated_buffers_[output_idx].first
    };
    size_t sizes[4] = {
        (size_t)batch_size * hidden_dim * sizeof(float),
        (size_t)hidden_dim * intermediate_dim * sizeof(float),
        (size_t)intermediate_dim * hidden_dim * sizeof(float),
        (size_t)batch_size * hidden_dim * sizeof(float)
    };

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = fused_mlp_descriptor_pool_;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &fused_mlp_descriptor_layout_;
    VkDescriptorSet ds = nullptr;
    if (vkAllocateDescriptorSets(device_, &alloc_info, &ds) != VK_SUCCESS) return false;

    std::vector<VkDescriptorBufferInfo> bi(4);
    std::vector<VkWriteDescriptorSet> writes(4);
    for (int i = 0; i < 4; ++i) {
        bi[i].buffer = bufs[i]; bi[i].offset = 0; bi[i].range = sizes[i];
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].pNext = nullptr; writes[i].dstSet = ds; writes[i].dstBinding = i;
        writes[i].dstArrayElement = 0; writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pImageInfo = nullptr; writes[i].pBufferInfo = &bi[i];
        writes[i].pTexelBufferView = nullptr;
    }
    vkUpdateDescriptorSets(device_, 4, writes.data(), 0, nullptr);

    bool success = ExecuteSingleTimeCommands([&](VkCommandBuffer cmd) {
        uint32_t pc[3] = { batch_size, hidden_dim, intermediate_dim };
        vkCmdPushConstants(cmd, it->second.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), pc);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, it->second.pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, it->second.layout, 0, 1, &ds, 0, nullptr);
        const uint32_t TILE = 16;
        vkCmdDispatch(cmd, (hidden_dim + TILE - 1) / TILE, (batch_size + TILE - 1) / TILE, 1);
    });

    if (ds) vkFreeDescriptorSets(device_, fused_mlp_descriptor_pool_, 1, &ds);
    if (success) stats_.dispatch_count.fetch_add(1, std::memory_order_relaxed);
    return success;
}

// =============================================================================
// EnsureFlashAttentionPipeline — Flash Attention v2 GPU dispatch
// =============================================================================
bool VulkanCompute::EnsureFlashAttentionPipeline(const std::string& spirv_path) {
    auto it = shaders_.find("flash_attn_v2");
    if (it != shaders_.end() && it->second.pipeline && flash_attn_descriptor_layout_) {
        return true;
    }

    if (!LoadShader("flash_attn_v2", spirv_path)) return false;
    it = shaders_.find("flash_attn_v2");
    if (it == shaders_.end()) return false;

    // 4 bindings: Q, K, V, Output
    std::vector<VkDescriptorSetLayoutBinding> bindings(4);
    for (uint32_t i = 0; i < 4; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[i].pImmutableSamplers = nullptr;
    }

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 4;
    layout_info.pBindings = bindings.data();
    if (vkCreateDescriptorSetLayout(device_, &layout_info, nullptr, &flash_attn_descriptor_layout_) != VK_SUCCESS) return false;

    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_size.descriptorCount = 40;
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    pool_info.maxSets = 10;
    if (vkCreateDescriptorPool(device_, &pool_info, nullptr, &flash_attn_descriptor_pool_) != VK_SUCCESS) return false;

    // Push constants: seq_len, head_dim, num_heads, scale (as uint32_t bit-cast)
    VkPushConstantRange push_constant{};
    push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push_constant.offset = 0;
    push_constant.size = sizeof(uint32_t) * 4;

    VkPipelineLayoutCreateInfo pli{};
    pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pli.setLayoutCount = 1;
    pli.pSetLayouts = &flash_attn_descriptor_layout_;
    pli.pushConstantRangeCount = 1;
    pli.pPushConstantRanges = &push_constant;
    if (vkCreatePipelineLayout(device_, &pli, nullptr, &it->second.layout) != VK_SUCCESS) return false;

    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = it->second.module;
    stage.pName = "main";

    VkComputePipelineCreateInfo cpi{};
    cpi.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cpi.layout = it->second.layout;
    cpi.stage = stage;
    if (vkCreateComputePipelines(device_, nullptr, 1, &cpi, nullptr, &it->second.pipeline) != VK_SUCCESS) return false;

    stats_.pipeline_create_count.fetch_add(1, std::memory_order_relaxed);
    std::cout << "Flash Attention v2 pipeline initialized" << std::endl;
    return true;
}

bool VulkanCompute::DispatchFlashAttentionV2(uint32_t q_idx, uint32_t k_idx, uint32_t v_idx,
                                              uint32_t output_idx, uint32_t seq_len,
                                              uint32_t head_dim, uint32_t num_heads,
                                              float scale) {
    if (q_idx >= allocated_buffers_.size() || k_idx >= allocated_buffers_.size() ||
        v_idx >= allocated_buffers_.size() || output_idx >= allocated_buffers_.size()) return false;

    auto it = shaders_.find("flash_attn_v2");
    if (it == shaders_.end() || !it->second.pipeline || !flash_attn_descriptor_layout_) return false;

    // Auto-compute scale if not provided
    if (scale <= 0.0f) {
        scale = 1.0f / std::sqrt(static_cast<float>(head_dim));
    }

    VkBuffer bufs[4] = {
        allocated_buffers_[q_idx].first,
        allocated_buffers_[k_idx].first,
        allocated_buffers_[v_idx].first,
        allocated_buffers_[output_idx].first
    };
    size_t buf_size = static_cast<size_t>(num_heads) * seq_len * head_dim * sizeof(float);

    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = flash_attn_descriptor_pool_;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &flash_attn_descriptor_layout_;
    VkDescriptorSet ds = nullptr;
    if (vkAllocateDescriptorSets(device_, &ai, &ds) != VK_SUCCESS) return false;

    std::vector<VkDescriptorBufferInfo> bi(4);
    std::vector<VkWriteDescriptorSet> writes(4);
    for (int i = 0; i < 4; ++i) {
        bi[i].buffer = bufs[i]; bi[i].offset = 0; bi[i].range = buf_size;
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].pNext = nullptr; writes[i].dstSet = ds; writes[i].dstBinding = i;
        writes[i].dstArrayElement = 0; writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pImageInfo = nullptr; writes[i].pBufferInfo = &bi[i];
        writes[i].pTexelBufferView = nullptr;
    }
    vkUpdateDescriptorSets(device_, 4, writes.data(), 0, nullptr);

    uint32_t scale_bits;
    std::memcpy(&scale_bits, &scale, sizeof(uint32_t));

    bool success = ExecuteSingleTimeCommands([&](VkCommandBuffer cmd) {
        uint32_t pc[4] = { seq_len, head_dim, num_heads, scale_bits };
        vkCmdPushConstants(cmd, it->second.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), pc);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, it->second.pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, it->second.layout, 0, 1, &ds, 0, nullptr);
        // One workgroup per head, tiled over sequence
        const uint32_t TILE = 64; // Flash Attention tiles
        vkCmdDispatch(cmd, (seq_len + TILE - 1) / TILE, num_heads, 1);
    });

    if (ds) vkFreeDescriptorSets(device_, flash_attn_descriptor_pool_, 1, &ds);
    if (success) {
        stats_.dispatch_count.fetch_add(1, std::memory_order_relaxed);
        stats_.attention_count.fetch_add(1, std::memory_order_relaxed);
    }
    return success;
}

// =============================================================================
// DispatchSpeculativeVerify — Verify draft tokens against target model logits
// =============================================================================
bool VulkanCompute::DispatchSpeculativeVerify(uint32_t draft_logits_idx,
                                               uint32_t target_logits_idx,
                                               uint32_t seq_len, uint32_t vocab_size,
                                               SpeculativeResult* out_result) {
    if (!out_result) return false;
    if (draft_logits_idx >= allocated_buffers_.size() || target_logits_idx >= allocated_buffers_.size()) return false;

    // CPU fallback: compare draft vs target logits token-by-token
    size_t logits_size = static_cast<size_t>(seq_len) * vocab_size * sizeof(float);
    std::vector<float> draft_logits(seq_len * vocab_size);
    std::vector<float> target_logits(seq_len * vocab_size);

    if (!CopyBufferToHost(draft_logits_idx, draft_logits.data(), logits_size)) return false;
    if (!CopyBufferToHost(target_logits_idx, target_logits.data(), logits_size)) return false;

    out_result->accepted_tokens.clear();
    out_result->draft_count = seq_len;
    out_result->accepted_count = 0;

    for (uint32_t t = 0; t < seq_len; ++t) {
        const float* draft_row = draft_logits.data() + static_cast<size_t>(t) * vocab_size;
        const float* target_row = target_logits.data() + static_cast<size_t>(t) * vocab_size;

        // Find argmax of both
        uint32_t draft_token = 0, target_token = 0;
        float draft_max = draft_row[0], target_max = target_row[0];
        for (uint32_t v = 1; v < vocab_size; ++v) {
            if (draft_row[v] > draft_max) { draft_max = draft_row[v]; draft_token = v; }
            if (target_row[v] > target_max) { target_max = target_row[v]; target_token = v; }
        }

        if (draft_token == target_token) {
            out_result->accepted_tokens.push_back(draft_token);
            out_result->accepted_count++;
        } else {
            // First rejection — accept the target token and stop
            out_result->accepted_tokens.push_back(target_token);
            out_result->accepted_count++;
            break;
        }
    }

    out_result->acceptance_rate = (seq_len > 0)
        ? static_cast<float>(out_result->accepted_count) / static_cast<float>(seq_len)
        : 0.0f;

    return true;
}

// =============================================================================
// C-callable exports for MASM bridge
// =============================================================================
static VulkanCompute* g_vulkan_instance = nullptr;

namespace {

std::atomic<uint64_t> g_kv_streamed_tokens{0};
std::atomic<uint64_t> g_kv_bytes_in_flight{0};
constexpr uint64_t kKvPressureSoftLimitBytes = 1024ull * 1024ull * 1024ull;
constexpr uint8_t kLaneCount = 8;
std::array<std::atomic<uint64_t>, kLaneCount> g_lane_dispatch_counts{};
std::array<std::atomic<uint64_t>, kLaneCount> g_lane_error_counts{};

bool isValidLaneId(uint8_t lane_id) {
    return lane_id < kLaneCount;
}

struct VulkanRawDispatchTableV1 {
    uint32_t version;
    uint32_t flags;
    uint32_t groupCountX;
    uint32_t groupCountY;
    uint32_t groupCountZ;
    uint32_t inputAIdx;
    uint32_t inputBIdx;
    uint32_t outputIdx;
    uint32_t M;
    uint32_t K;
    uint32_t N;
};

struct VulkanRawDispatchTableV2 {
    uint32_t version;
    uint32_t flags;
    uint32_t groupCountX;
    uint32_t groupCountY;
    uint32_t groupCountZ;
    uint32_t inputAIdx;
    uint32_t inputBIdx;
    uint32_t outputIdx;
    uint32_t M;
    uint32_t K;
    uint32_t N;
    uint64_t inputABytes;
    uint64_t inputBBytes;
    uint64_t outputBytes;
    uint32_t pushConstantBytes;
};

constexpr uint32_t kVulkanRawDispatchVersion = 1;
constexpr uint32_t kVulkanRawDispatchFlagMatMul = 0x1;
constexpr uint32_t kVulkanRawDispatchVersion2 = 2;
constexpr uint32_t kVulkanRawDispatchFlagGeneric3 = 0x2;

struct RawDispatchCounters {
    std::atomic<uint64_t> attempts{0};
    std::atomic<uint64_t> success{0};
    std::atomic<uint64_t> fallback{0};
    std::atomic<uint64_t> asm_path_calls{0};
    std::atomic<uint64_t> invalid_abi{0};
    std::atomic<uint64_t> pipeline_miss{0};
};

RawDispatchCounters g_raw_dispatch_counters;
std::unordered_map<std::string, std::string> g_uuid_to_shader;
std::mutex g_uuid_to_shader_mutex;

uint64_t fnv1a64(const uint8_t* data, size_t size, uint64_t seed) {
    uint64_t hash = seed;
    for (size_t i = 0; i < size; ++i) {
        hash ^= static_cast<uint64_t>(data[i]);
        hash *= 1099511628211ull;
    }
    return hash;
}

void computeShaderUUID(const uint8_t* spirv_data, size_t spirv_size, uint8_t out_uuid[16]) {
    const uint8_t* data = spirv_data;
    size_t size = spirv_size;
    static const uint8_t fallback_tag[] = "pyre-dispatch-fallback";
    if (!data || size == 0) {
        data = fallback_tag;
        size = sizeof(fallback_tag) - 1;
    }

    const uint64_t h1 = fnv1a64(data, size, 1469598103934665603ull);
    const uint64_t h2 = fnv1a64(data, size, 1099511628211ull);
    std::memcpy(out_uuid + 0, &h1, sizeof(h1));
    std::memcpy(out_uuid + 8, &h2, sizeof(h2));
}

std::string uuidHex(const uint8_t uuid[16]) {
    if (!uuid) {
        return std::string();
    }

    char hex[33] = {};
    for (size_t i = 0; i < 16; ++i) {
        static const char kDigits[] = "0123456789abcdef";
        hex[i * 2 + 0] = kDigits[(uuid[i] >> 4) & 0xF];
        hex[i * 2 + 1] = kDigits[uuid[i] & 0xF];
    }
    return std::string(hex, 32);
}

void cacheUuidShader(const uint8_t uuid[16], const std::string& shader_name) {
    const std::string key = uuidHex(uuid);
    if (key.empty() || shader_name.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_uuid_to_shader_mutex);
    g_uuid_to_shader[key] = shader_name;
}

std::string lookupUuidShader(const uint8_t uuid[16]) {
    const std::string key = uuidHex(uuid);
    if (key.empty()) {
        return std::string();
    }

    std::lock_guard<std::mutex> lock(g_uuid_to_shader_mutex);
    auto it = g_uuid_to_shader.find(key);
    if (it == g_uuid_to_shader.end()) {
        return std::string();
    }
    return it->second;
}

bool ensureMatMulPipelineFromEnv(VulkanCompute* instance) {
    if (!instance) {
        return false;
    }

    static std::once_flag s_once;
    static int s_ok = 0;
    std::call_once(s_once, [&]() {
        char path_buf[MAX_PATH] = {};
        const DWORD len = GetEnvironmentVariableA("RAWRXD_VULKAN_MATMUL_SPV", path_buf, static_cast<DWORD>(sizeof(path_buf)));
        if (len == 0 || len >= sizeof(path_buf)) {
            s_ok = 0;
            return;
        }
        s_ok = instance->EnsureMatMulPipeline(path_buf) ? 1 : 0;
    });

    return s_ok == 1;
}

bool shouldValidateMatMul() {
    static std::once_flag s_once;
    static int s_enabled = 0;
    std::call_once(s_once, []() {
        char value[16] = {};
        const DWORD len = GetEnvironmentVariableA("RAWRXD_VALIDATE_MATMUL", value, static_cast<DWORD>(sizeof(value)));
        if (len == 0 || len >= sizeof(value)) {
            s_enabled = 0;
            return;
        }

        const std::string v(value, len);
        s_enabled = (v == "1" || v == "true" || v == "TRUE" || v == "on" || v == "ON") ? 1 : 0;
    });

    return s_enabled == 1;
}

void validateMatMulOutput(const float* input_a,
                          const float* input_b,
                          const float* gpu_output,
                          uint32_t M,
                          uint32_t N,
                          uint32_t K) {
    const size_t out_count = static_cast<size_t>(M) * static_cast<size_t>(N);
    std::vector<float> cpu_output(out_count, 0.0f);

    for (uint32_t row = 0; row < M; ++row) {
        const size_t a_base = static_cast<size_t>(row) * static_cast<size_t>(K);
        const size_t c_base = static_cast<size_t>(row) * static_cast<size_t>(N);
        for (uint32_t col = 0; col < N; ++col) {
            float sum = 0.0f;
            for (uint32_t inner = 0; inner < K; ++inner) {
                sum += input_a[a_base + inner] * input_b[static_cast<size_t>(inner) * static_cast<size_t>(N) + col];
            }
            cpu_output[c_base + col] = sum;
        }
    }

    float max_abs_error = 0.0f;
    float max_rel_error = 0.0f;
    size_t worst_idx = 0;

    for (size_t i = 0; i < out_count; ++i) {
        const float ref = cpu_output[i];
        const float got = gpu_output[i];
        const float abs_err = std::fabs(ref - got);
        const float denom = std::max(1e-6f, std::fabs(ref));
        const float rel_err = abs_err / denom;
        if (abs_err > max_abs_error) {
            max_abs_error = abs_err;
            worst_idx = i;
        }
        if (rel_err > max_rel_error) {
            max_rel_error = rel_err;
        }
    }

    std::cerr << "[MatMulValidation] M=" << M
              << " N=" << N
              << " K=" << K
              << " max_abs_error=" << max_abs_error
              << " max_rel_error=" << max_rel_error
              << " worst_idx=" << worst_idx
              << " cpu=" << cpu_output[worst_idx]
              << " gpu=" << gpu_output[worst_idx]
              << std::endl;
}

bool loadFileBytes(const char* path, std::vector<uint8_t>& out) {
    if (!path || path[0] == '\0') {
        return false;
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }

    const std::streamsize size = file.tellg();
    if (size <= 0 || (size % static_cast<std::streamsize>(sizeof(uint32_t))) != 0) {
        return false;
    }

    file.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(size));
    return file.read(reinterpret_cast<char*>(out.data()), size).good();
}

} // namespace

extern "C" {

__declspec(dllexport) int VulkanKernel_DispatchRaw(const uint8_t shader_uuid[16],
                                                   const void* descriptor_table,
                                                   const void* push_constants);
__declspec(dllexport) int VulkanKernel_DispatchRaw_Impl(const uint8_t shader_uuid[16],
                                                        const void* descriptor_table,
                                                        const void* push_constants);
#if defined(RAWR_HAS_MASM) && RAWR_HAS_MASM
int VulkanKernel_DispatchRaw_Asm(const uint8_t shader_uuid[16],
                                 const void* descriptor_table,
                                 const void* push_constants);
#endif

// PyreDispatchCompute — C bridge used by dynamic/self-patch compute dispatch.
// Contract:
//   spirv_data/spirv_size: optional SPIR-V payload for hotpatch lane
//   input/output: float buffers
//   count: float element count
// Current behavior:
//   - Ensures Vulkan kernel runtime is initialized (once)
//   - Attempts SPIR-V hotpatch when payload is provided
//   - Uses a safe passthrough fallback until a generic kernel dispatch API is wired
__declspec(dllexport) int PyreDispatchCompute(const uint8_t* spirv_data,
                                              size_t spirv_size,
                                              const void* input,
                                              void* output,
                                              uint32_t count) {
    if (!input || !output || count == 0) {
        return 0;
    }

    static std::once_flag s_initOnce;
    static int s_initOk = 0;
    std::call_once(s_initOnce, []() {
        s_initOk = VulkanKernel_Init();
    });

    if (!s_initOk) {
        return 0;
    }

    const size_t elem_count = static_cast<size_t>(count);
    const size_t byte_count = elem_count * sizeof(float);

    // Optional SPIR-V hotpatch lane (best effort) for the active matmul pipeline.
    if (spirv_data && spirv_size >= sizeof(uint32_t) && (spirv_size % sizeof(uint32_t)) == 0) {
        const uint32_t* words = reinterpret_cast<const uint32_t*>(spirv_data);
        (void)VulkanKernel_HotswapShader("matmul", words, static_cast<uint64_t>(spirv_size));
    }

    g_raw_dispatch_counters.attempts.fetch_add(1, std::memory_order_relaxed);

    // Real GPU dispatch path (v1): use MatMul descriptor lane as a generic ABI backend.
    if (ensureMatMulPipelineFromEnv(g_vulkan_instance)) {
        uint32_t a_idx = 0;
        uint32_t b_idx = 0;
        uint32_t out_idx = 0;

        const float one = 1.0f;
        bool ok = VulkanKernel_AllocBuffer(sizeof(float), &a_idx) &&
                  VulkanKernel_AllocBuffer(byte_count, &b_idx) &&
                  VulkanKernel_AllocBuffer(byte_count, &out_idx) &&
                  VulkanKernel_CopyToDevice(a_idx, &one, sizeof(float)) &&
                  VulkanKernel_CopyToDevice(b_idx, input, static_cast<uint64_t>(byte_count));

        if (ok) {
            uint8_t shader_uuid[16] = {};
            computeShaderUUID(spirv_data, spirv_size, shader_uuid);
            cacheUuidShader(shader_uuid, "matmul");

            VulkanRawDispatchTableV2 desc{};
            desc.version = kVulkanRawDispatchVersion2;
            desc.flags = kVulkanRawDispatchFlagMatMul | kVulkanRawDispatchFlagGeneric3;
            desc.groupCountX = count;
            desc.groupCountY = 1;
            desc.groupCountZ = 1;
            desc.inputAIdx = a_idx;
            desc.inputBIdx = b_idx;
            desc.outputIdx = out_idx;
            desc.M = 1;
            desc.K = 1;
            desc.N = count;
            desc.inputABytes = sizeof(float);
            desc.inputBBytes = static_cast<uint64_t>(byte_count);
            desc.outputBytes = static_cast<uint64_t>(byte_count);
            desc.pushConstantBytes = sizeof(uint32_t) * 3;

            ok = VulkanKernel_DispatchRaw(shader_uuid, &desc, nullptr) &&
                 VulkanKernel_CopyToHost(out_idx, output, static_cast<uint64_t>(byte_count));
        }

        if (ok) {
            g_raw_dispatch_counters.success.fetch_add(1, std::memory_order_relaxed);
            return 1;
        }
    } else {
        g_raw_dispatch_counters.pipeline_miss.fetch_add(1, std::memory_order_relaxed);
    }

    // Deterministic safe fallback.
    std::memcpy(output, input, byte_count);
    g_raw_dispatch_counters.fallback.fetch_add(1, std::memory_order_relaxed);
    return 1;
}

__declspec(dllexport) int PyreDispatchMatMul(const float* input_a,
                                             const float* input_b,
                                             float* output,
                                             uint32_t M,
                                             uint32_t N,
                                             uint32_t K) {
    if (!input_a || !input_b || !output || M == 0 || N == 0 || K == 0) {
        return 0;
    }

    static std::once_flag s_initOnce;
    static int s_initOk = 0;
    std::call_once(s_initOnce, []() {
        s_initOk = VulkanKernel_Init();
    });

    if (!s_initOk || !ensureMatMulPipelineFromEnv(g_vulkan_instance)) {
        return 0;
    }

    const size_t a_bytes = static_cast<size_t>(M) * K * sizeof(float);
    const size_t b_bytes = static_cast<size_t>(K) * N * sizeof(float);
    const size_t out_bytes = static_cast<size_t>(M) * N * sizeof(float);

    uint32_t a_idx = 0;
    uint32_t b_idx = 0;
    uint32_t out_idx = 0;

    bool ok = VulkanKernel_AllocBuffer(a_bytes, &a_idx) &&
              VulkanKernel_AllocBuffer(b_bytes, &b_idx) &&
              VulkanKernel_AllocBuffer(out_bytes, &out_idx) &&
              VulkanKernel_CopyToDevice(a_idx, input_a, static_cast<uint64_t>(a_bytes)) &&
              VulkanKernel_CopyToDevice(b_idx, input_b, static_cast<uint64_t>(b_bytes));

    if (!ok) {
        return 0;
    }

    uint8_t shader_uuid[16] = {};
    computeShaderUUID(nullptr, 0, shader_uuid);
    cacheUuidShader(shader_uuid, "matmul");

    VulkanRawDispatchTableV2 desc{};
    desc.version = kVulkanRawDispatchVersion2;
    desc.flags = kVulkanRawDispatchFlagMatMul | kVulkanRawDispatchFlagGeneric3;
    desc.groupCountX = (N + 15u) / 16u;
    desc.groupCountY = (M + 15u) / 16u;
    desc.groupCountZ = 1;
    desc.inputAIdx = a_idx;
    desc.inputBIdx = b_idx;
    desc.outputIdx = out_idx;
    desc.M = M;
    desc.K = K;
    desc.N = N;
    desc.inputABytes = static_cast<uint64_t>(a_bytes);
    desc.inputBBytes = static_cast<uint64_t>(b_bytes);
    desc.outputBytes = static_cast<uint64_t>(out_bytes);
    desc.pushConstantBytes = sizeof(uint32_t) * 3;

    ok = VulkanKernel_DispatchRaw(shader_uuid, &desc, nullptr) &&
         VulkanKernel_CopyToHost(out_idx, output, static_cast<uint64_t>(out_bytes));

    if (ok && shouldValidateMatMul()) {
        validateMatMulOutput(input_a, input_b, output, M, N, K);
    }

    return ok ? 1 : 0;
}

__declspec(dllexport) int VulkanKernel_DispatchRaw_Impl(const uint8_t shader_uuid[16],
                                                        const void* descriptor_table,
                                                        const void* push_constants) {
    if (!g_vulkan_instance || !descriptor_table) {
        g_raw_dispatch_counters.invalid_abi.fetch_add(1, std::memory_order_relaxed);
        return 0;
    }

    const auto* v1 = reinterpret_cast<const VulkanRawDispatchTableV1*>(descriptor_table);

    if (v1->version == kVulkanRawDispatchVersion2) {
        const auto* v2 = reinterpret_cast<const VulkanRawDispatchTableV2*>(descriptor_table);
        if (v2->groupCountX == 0 || v2->groupCountY == 0 || v2->groupCountZ == 0) {
            g_raw_dispatch_counters.invalid_abi.fetch_add(1, std::memory_order_relaxed);
            return 0;
        }

        if ((v2->flags & kVulkanRawDispatchFlagMatMul) != 0) {
            if (v2->M == 0 || v2->K == 0 || v2->N == 0) {
                g_raw_dispatch_counters.invalid_abi.fetch_add(1, std::memory_order_relaxed);
                return 0;
            }
            return g_vulkan_instance->DispatchMatMul(v2->inputAIdx,
                                                     v2->inputBIdx,
                                                     v2->outputIdx,
                                                     v2->M,
                                                     v2->K,
                                                     v2->N) ? 1 : 0;
        }

        if ((v2->flags & kVulkanRawDispatchFlagGeneric3) != 0) {
            const std::string shader_name = lookupUuidShader(shader_uuid);
            if (shader_name.empty()) {
                g_raw_dispatch_counters.pipeline_miss.fetch_add(1, std::memory_order_relaxed);
                return 0;
            }

            return g_vulkan_instance->DispatchRaw3Buffers(
                shader_name,
                v2->inputAIdx,
                v2->inputBIdx,
                v2->outputIdx,
                static_cast<size_t>(v2->inputABytes),
                static_cast<size_t>(v2->inputBBytes),
                static_cast<size_t>(v2->outputBytes),
                push_constants,
                v2->pushConstantBytes,
                v2->groupCountX,
                v2->groupCountY,
                v2->groupCountZ) ? 1 : 0;
        }

        g_raw_dispatch_counters.invalid_abi.fetch_add(1, std::memory_order_relaxed);
        return 0;
    }

    if (v1->version != kVulkanRawDispatchVersion) {
        g_raw_dispatch_counters.invalid_abi.fetch_add(1, std::memory_order_relaxed);
        return 0;
    }

    if ((v1->flags & kVulkanRawDispatchFlagMatMul) == 0) {
        g_raw_dispatch_counters.invalid_abi.fetch_add(1, std::memory_order_relaxed);
        return 0;
    }

    if (v1->M == 0 || v1->K == 0 || v1->N == 0) {
        g_raw_dispatch_counters.invalid_abi.fetch_add(1, std::memory_order_relaxed);
        return 0;
    }

    return g_vulkan_instance->DispatchMatMul(v1->inputAIdx,
                                             v1->inputBIdx,
                                             v1->outputIdx,
                                             v1->M,
                                             v1->K,
                                             v1->N) ? 1 : 0;
}

__declspec(dllexport) int VulkanKernel_DispatchRaw(const uint8_t shader_uuid[16],
                                                   const void* descriptor_table,
                                                   const void* push_constants) {
#if defined(RAWR_HAS_MASM) && RAWR_HAS_MASM
    g_raw_dispatch_counters.asm_path_calls.fetch_add(1, std::memory_order_relaxed);
    return VulkanKernel_DispatchRaw_Asm(shader_uuid, descriptor_table, push_constants);
#else
    return VulkanKernel_DispatchRaw_Impl(shader_uuid, descriptor_table, push_constants);
#endif
}

__declspec(dllexport) uint32_t VulkanKernel_GetRawDispatchAbiVersion(void) {
    return kVulkanRawDispatchVersion2;
}

__declspec(dllexport) int VulkanKernel_IsMatMulPipelineReady(void) {
    return ensureMatMulPipelineFromEnv(g_vulkan_instance) ? 1 : 0;
}

__declspec(dllexport) int VulkanKernel_EnsureMatMulPipelineFromEnv(void) {
    return ensureMatMulPipelineFromEnv(g_vulkan_instance) ? 1 : 0;
}

__declspec(dllexport) int VulkanKernel_ComputeShaderUUID(const uint8_t* spirv_data,
                                                         uint64_t spirv_size,
                                                         uint8_t out_uuid[16]) {
    if (!out_uuid) {
        return 0;
    }
    computeShaderUUID(spirv_data, static_cast<size_t>(spirv_size), out_uuid);
    return 1;
}

__declspec(dllexport) void VulkanKernel_GetRawDispatchCounters(uint64_t* attempts,
                                                               uint64_t* success,
                                                               uint64_t* fallback,
                                                               uint64_t* asm_path_calls,
                                                               uint64_t* invalid_abi,
                                                               uint64_t* pipeline_miss) {
    if (attempts) *attempts = g_raw_dispatch_counters.attempts.load(std::memory_order_relaxed);
    if (success) *success = g_raw_dispatch_counters.success.load(std::memory_order_relaxed);
    if (fallback) *fallback = g_raw_dispatch_counters.fallback.load(std::memory_order_relaxed);
    if (asm_path_calls) *asm_path_calls = g_raw_dispatch_counters.asm_path_calls.load(std::memory_order_relaxed);
    if (invalid_abi) *invalid_abi = g_raw_dispatch_counters.invalid_abi.load(std::memory_order_relaxed);
    if (pipeline_miss) *pipeline_miss = g_raw_dispatch_counters.pipeline_miss.load(std::memory_order_relaxed);
}

__declspec(dllexport) void VulkanKernel_ResetRawDispatchCounters(void) {
    g_raw_dispatch_counters.attempts.store(0, std::memory_order_relaxed);
    g_raw_dispatch_counters.success.store(0, std::memory_order_relaxed);
    g_raw_dispatch_counters.fallback.store(0, std::memory_order_relaxed);
    g_raw_dispatch_counters.asm_path_calls.store(0, std::memory_order_relaxed);
    g_raw_dispatch_counters.invalid_abi.store(0, std::memory_order_relaxed);
    g_raw_dispatch_counters.pipeline_miss.store(0, std::memory_order_relaxed);
}

__declspec(dllexport) int VulkanKernel_GetReadinessSnapshot(uint32_t* abi_version,
                                                             uint32_t* matmul_ready,
                                                             uint64_t* attempts,
                                                             uint64_t* success,
                                                             uint64_t* fallback) {
    if (abi_version) {
        *abi_version = VulkanKernel_GetRawDispatchAbiVersion();
    }
    if (matmul_ready) {
        *matmul_ready = VulkanKernel_IsMatMulPipelineReady() ? 1u : 0u;
    }
    if (attempts) {
        *attempts = g_raw_dispatch_counters.attempts.load(std::memory_order_relaxed);
    }
    if (success) {
        *success = g_raw_dispatch_counters.success.load(std::memory_order_relaxed);
    }
    if (fallback) {
        *fallback = g_raw_dispatch_counters.fallback.load(std::memory_order_relaxed);
    }
    return 1;
}

int VulkanKernel_Init(void) {
    if (g_vulkan_instance) return 1; // Already initialized
    g_vulkan_instance = new VulkanCompute();
    return g_vulkan_instance->Initialize() ? 1 : 0;
}

int VulkanKernel_LoadShader(const char* name, const char* spirv_path) {
    if (!g_vulkan_instance || !name || !spirv_path) return 0;
    return g_vulkan_instance->LoadShader(name, spirv_path) ? 1 : 0;
}

int VulkanKernel_CreatePipeline(const char* shader_name) {
    if (!g_vulkan_instance || !shader_name) return 0;
    return g_vulkan_instance->CreateComputePipeline(shader_name) ? 1 : 0;
}

int VulkanKernel_AllocBuffer(uint64_t size, uint32_t* out_idx) {
    if (!g_vulkan_instance || !out_idx) return 0;
    size_t mem_size = 0;
    return g_vulkan_instance->AllocateBuffer(static_cast<size_t>(size), *out_idx, mem_size) ? 1 : 0;
}

int VulkanKernel_CopyToDevice(uint32_t buf_idx, const void* data, uint64_t size) {
    if (!g_vulkan_instance || !data) return 0;
    return g_vulkan_instance->CopyHostToBuffer(const_cast<void*>(data), buf_idx,
                                                static_cast<size_t>(size)) ? 1 : 0;
}

int VulkanKernel_CopyToHost(uint32_t buf_idx, void* data, uint64_t size) {
    if (!g_vulkan_instance || !data) return 0;
    return g_vulkan_instance->CopyBufferToHost(buf_idx, data,
                                                static_cast<size_t>(size)) ? 1 : 0;
}

// Historical/ASM-facing aliases (some MASM modules reference these names).
int VulkanKERNEL_TYPE_COPYToDevice(uint32_t buf_idx, const void* data, uint64_t size) {
    return VulkanKernel_CopyToDevice(buf_idx, data, size);
}

int VulkanKERNEL_TYPE_COPYToHost(uint32_t buf_idx, void* data, uint64_t size) {
    return VulkanKernel_CopyToHost(buf_idx, data, size);
}

int VulkanKernel_DispatchMatMul(uint32_t a, uint32_t b, uint32_t out,
                                 uint32_t M, uint32_t K, uint32_t N) {
    if (!g_vulkan_instance) return 0;
    return g_vulkan_instance->DispatchMatMul(a, b, out, M, K, N) ? 1 : 0;
}

int VulkanKernel_DispatchFlashAttn(uint32_t q, uint32_t k, uint32_t v,
                                    uint32_t out, uint32_t seq_len,
                                    uint32_t head_dim, uint32_t num_heads) {
    if (!g_vulkan_instance) return 0;
    return g_vulkan_instance->DispatchFlashAttentionV2(q, k, v, out, seq_len, head_dim, num_heads) ? 1 : 0;
}

int VulkanKernel_HotswapShader(const char* name, const uint32_t* spirv, uint64_t size) {
    if (!g_vulkan_instance || !name || !spirv) return 0;
    return g_vulkan_instance->HotswapShader(name, spirv, static_cast<size_t>(size)) ? 1 : 0;
}

int VulkanKernel_TryHotReloadFromFile(const char* name, const char* spirv_path, uint8_t out_uuid[16]) {
    if (!g_vulkan_instance || !name || !spirv_path) {
        return 0;
    }

    std::vector<uint8_t> raw;
    if (!loadFileBytes(spirv_path, raw)) {
        return 0;
    }

    uint8_t computed_uuid[16] = {};
    computeShaderUUID(raw.data(), raw.size(), computed_uuid);
    cacheUuidShader(computed_uuid, name);

    if (out_uuid) {
        std::memcpy(out_uuid, computed_uuid, sizeof(computed_uuid));
    }

    const uint32_t* words = reinterpret_cast<const uint32_t*>(raw.data());
    return g_vulkan_instance->HotswapShader(name, words, raw.size()) ? 1 : 0;
}

int VulkanKernel_KVStreamUpdate(uint32_t tokens_added,
                                uint64_t bytes_in_flight,
                                float* out_pressure) {
    g_kv_streamed_tokens.fetch_add(static_cast<uint64_t>(tokens_added), std::memory_order_relaxed);
    g_kv_bytes_in_flight.store(bytes_in_flight, std::memory_order_relaxed);

    if (out_pressure) {
        const float pressure = static_cast<float>(bytes_in_flight) /
            static_cast<float>(kKvPressureSoftLimitBytes);
        *out_pressure = std::clamp(pressure, 0.0f, 8.0f);
    }
    return 1;
}

int VulkanKernel_DispatchSpeculativeVerify(uint32_t draft_logits_idx,
                                           uint32_t target_logits_idx,
                                           uint32_t seq_len,
                                           uint32_t vocab_size,
                                           uint32_t* out_accepted,
                                           float* out_acceptance_rate) {
    if (!g_vulkan_instance) {
        return 0;
    }

    VulkanCompute::SpeculativeResult result{};
    const bool ok = g_vulkan_instance->DispatchSpeculativeVerify(
        draft_logits_idx,
        target_logits_idx,
        seq_len,
        vocab_size,
        &result);

    if (!ok) {
        return 0;
    }

    if (out_accepted) {
        *out_accepted = result.accepted_count;
    }
    if (out_acceptance_rate) {
        *out_acceptance_rate = result.acceptance_rate;
    }
    return 1;
}

__declspec(dllexport) int VulkanKernel_QueryLaneCapacity(uint8_t lane_id,
                                                          uint32_t* out_slots,
                                                          uint64_t* out_est_latency_ns) {
    if (!isValidLaneId(lane_id)) {
        return 0;
    }

    if (out_slots) {
        // Conservative static lane slot estimate for now.
        *out_slots = 4;
    }
    if (out_est_latency_ns) {
        // Basic heuristic per lane ID.
        *out_est_latency_ns = 80000ull + (static_cast<uint64_t>(lane_id) * 5000ull);
    }
    return 1;
}

__declspec(dllexport) int VulkanKernel_GetKVPressure(float* out_pressure,
                                                      uint64_t* out_bytes_in_flight,
                                                      uint64_t* out_tokens_streamed) {
    const uint64_t bytes = g_kv_bytes_in_flight.load(std::memory_order_relaxed);
    const uint64_t tokens = g_kv_streamed_tokens.load(std::memory_order_relaxed);

    if (out_pressure) {
        const float pressure = static_cast<float>(bytes) /
            static_cast<float>(kKvPressureSoftLimitBytes);
        *out_pressure = std::clamp(pressure, 0.0f, 8.0f);
    }
    if (out_bytes_in_flight) {
        *out_bytes_in_flight = bytes;
    }
    if (out_tokens_streamed) {
        *out_tokens_streamed = tokens;
    }
    return 1;
}

__declspec(dllexport) void VulkanKernel_ResetKVPressure(void) {
    g_kv_bytes_in_flight.store(0, std::memory_order_relaxed);
    g_kv_streamed_tokens.store(0, std::memory_order_relaxed);
}

__declspec(dllexport) int VulkanKernel_PrefetchLanePipeline(uint8_t lane_id,
                                                             const char* pipeline_name,
                                                             const uint32_t* spirv,
                                                             uint64_t spirv_size) {
    if (!g_vulkan_instance || !pipeline_name || !spirv || spirv_size == 0) {
        if (isValidLaneId(lane_id)) {
            g_lane_error_counts[lane_id].fetch_add(1, std::memory_order_relaxed);
        }
        return 0;
    }
    if (!isValidLaneId(lane_id)) {
        return 0;
    }

    const bool ok = g_vulkan_instance->HotswapShader(
        pipeline_name,
        spirv,
        static_cast<size_t>(spirv_size));

    if (ok) {
        g_lane_dispatch_counts[lane_id].fetch_add(1, std::memory_order_relaxed);
        return 1;
    }

    g_lane_error_counts[lane_id].fetch_add(1, std::memory_order_relaxed);
    return 0;
}

__declspec(dllexport) int VulkanKernel_VerifySPIRVFile(const char* spirv_path,
                                                        uint64_t* out_size_bytes) {
    if (!spirv_path || spirv_path[0] == '\0') {
        return 0;
    }

    std::vector<uint8_t> raw;
    if (!loadFileBytes(spirv_path, raw)) {
        return 0;
    }

    if (out_size_bytes) {
        *out_size_bytes = static_cast<uint64_t>(raw.size());
    }
    return 1;
}

__declspec(dllexport) int VulkanKernel_DispatchSpeculativeVerifyBatch(
    const uint32_t* draft_logits_idx,
    const uint32_t* target_logits_idx,
    uint32_t batch_size,
    uint32_t seq_len,
    uint32_t vocab_size,
    uint32_t* out_total_accepted,
    float* out_mean_acceptance_rate) {
    if (!g_vulkan_instance || !draft_logits_idx || !target_logits_idx || batch_size == 0) {
        return 0;
    }

    uint64_t accepted_sum = 0;
    double rate_sum = 0.0;

    for (uint32_t i = 0; i < batch_size; ++i) {
        VulkanCompute::SpeculativeResult result{};
        const bool ok = g_vulkan_instance->DispatchSpeculativeVerify(
            draft_logits_idx[i],
            target_logits_idx[i],
            seq_len,
            vocab_size,
            &result);
        if (!ok) {
            return 0;
        }
        accepted_sum += result.accepted_count;
        rate_sum += static_cast<double>(result.acceptance_rate);
    }

    if (out_total_accepted) {
        *out_total_accepted = static_cast<uint32_t>(accepted_sum);
    }
    if (out_mean_acceptance_rate) {
        *out_mean_acceptance_rate = static_cast<float>(rate_sum / static_cast<double>(batch_size));
    }
    return 1;
}

__declspec(dllexport) int VulkanKernel_QueryDispatchLaneMetrics(uint8_t lane_id,
                                                                 uint64_t* out_dispatches,
                                                                 uint64_t* out_errors,
                                                                 uint64_t* out_kv_tokens) {
    if (!isValidLaneId(lane_id)) {
        return 0;
    }

    if (out_dispatches) {
        *out_dispatches = g_lane_dispatch_counts[lane_id].load(std::memory_order_relaxed);
    }
    if (out_errors) {
        *out_errors = g_lane_error_counts[lane_id].load(std::memory_order_relaxed);
    }
    if (out_kv_tokens) {
        *out_kv_tokens = g_kv_streamed_tokens.load(std::memory_order_relaxed);
    }
    return 1;
}

void VulkanKernel_GetStats(uint64_t* dispatches, uint64_t* matmuls,
                            uint64_t* attentions, uint64_t* errors) {
    if (!g_vulkan_instance) return;
    const auto& s = g_vulkan_instance->GetStats();
    if (dispatches) *dispatches = s.dispatch_count.load(std::memory_order_relaxed);
    if (matmuls) *matmuls = s.matmul_count.load(std::memory_order_relaxed);
    if (attentions) *attentions = s.attention_count.load(std::memory_order_relaxed);
    if (errors) *errors = s.error_count.load(std::memory_order_relaxed);
}

void VulkanKernel_Cleanup(void) {
    if (g_vulkan_instance) {
        g_vulkan_instance->Cleanup();
        delete g_vulkan_instance;
        g_vulkan_instance = nullptr;
    }
}

} // extern "C"
