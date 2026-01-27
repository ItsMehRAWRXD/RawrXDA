#include "vulkan_compute.h"
#include <vulkan/vulkan.h>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstring>

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

    if (vkAllocateMemory(device_, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate memory" << std::endl;
        vkDestroyBuffer(device_, buffer, nullptr);
        return false;
    }

    vkBindBufferMemory(device_, buffer, memory, 0);
    return true;
}

// ==================== STAGING BUFFER HELPER ====================
bool VulkanCompute::CreateStagingBuffer(size_t size, VkBuffer& buffer, VkDeviceMemory& memory) {
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

    if (vkAllocateMemory(device_, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate staging memory" << std::endl;
        vkDestroyBuffer(device_, buffer, nullptr);
        return false;
    }

    vkBindBufferMemory(device_, buffer, memory, 0);
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
    // Placeholder for RoPE implementation
    std::cout << "Executing RoPE: Dim=" << dim << ", SeqPos=" << seq_pos << std::endl;
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
    
    if (vkAllocateMemory(device_, &alloc_info, nullptr, &staging_memory) != VK_SUCCESS) {
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        return false;
    }
    
    vkBindBufferMemory(device_, staging_buffer, staging_memory, 0);
    
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
    
    if (vkAllocateMemory(device_, &alloc_info, nullptr, &staging_memory) != VK_SUCCESS) {
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        return false;
    }
    
    vkBindBufferMemory(device_, staging_buffer, staging_memory, 0);
    
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
    return 0;
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
