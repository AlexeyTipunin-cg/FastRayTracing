#include "Compute.h"
#include "Walnut/Application.h"
#include <fstream>
#include <iostream>
#include "Scene.h"
namespace Utils
{
    static std::vector<char> readFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }
}

Compute::Compute(Scene scene)
{
    Init(scene);
}

void Compute::Init(Scene scene)
{
    vk::Device device = Walnut::Application::GetDevice();
    vk::PhysicalDevice physicalDevice = Walnut::Application::GetPhysicalDevice();
    //auto indices = findQueueFamilies(physicalDevice);
    //vkGetDeviceQueue(device, indices.graphicsAndComputeFamily.value(),
    //    0, &computeQueue);

    std::vector<vk::QueueFamilyProperties> QueueFamilyProps = physicalDevice.getQueueFamilyProperties();
    auto PropIt = std::find_if(QueueFamilyProps.begin(), QueueFamilyProps.end(), [](const vk::QueueFamilyProperties& Prop)
        {
            return Prop.queueFlags & vk::QueueFlagBits::eCompute;
        });
    const uint32_t ComputeQueueFamilyIndex = std::distance(QueueFamilyProps.begin(), PropIt);
    std::cout << "Compute Queue Family Index: " << ComputeQueueFamilyIndex << std::endl;


    const uint32_t NumElements = scene.Spheres.size();
    const uint32_t BufferSizeIn = NumElements * sizeof(Sphere);
    const uint32_t BufferSizeOut = NumElements * sizeof(float);



    vk::BufferCreateInfo BufferCreateInfoIn{
        vk::BufferCreateFlags(),                    // Flags
        BufferSizeIn,                                 // Size
        vk::BufferUsageFlagBits::eStorageBuffer,    // Usage
        vk::SharingMode::eExclusive,                // Sharing mode
        1,                                          // Number of queue family indices
        &ComputeQueueFamilyIndex                    // List of queue family indices
    };

    vk::BufferCreateInfo BufferCreateInfoOut{
        vk::BufferCreateFlags(),                    // Flags
        BufferSizeOut,                                 // Size
        vk::BufferUsageFlagBits::eStorageBuffer,    // Usage
        vk::SharingMode::eExclusive,                // Sharing mode
        1,                                          // Number of queue family indices
        &ComputeQueueFamilyIndex                    // List of queue family indices
    };



    vk::Buffer InBuffer = device.createBuffer(BufferCreateInfoIn);
    vk::Buffer OutBuffer = device.createBuffer(BufferCreateInfoOut);

    vk::MemoryRequirements InBufferMemoryRequirements = device.getBufferMemoryRequirements(InBuffer);
    vk::MemoryRequirements OutBufferMemoryRequirements = device.getBufferMemoryRequirements(OutBuffer);

    vk::PhysicalDeviceMemoryProperties MemoryProperties = physicalDevice.getMemoryProperties();

    uint32_t MemoryTypeIndex = uint32_t(~0);
    vk::DeviceSize MemoryHeapSize = uint32_t(~0);
    for (uint32_t CurrentMemoryTypeIndex = 0; CurrentMemoryTypeIndex < MemoryProperties.memoryTypeCount; ++CurrentMemoryTypeIndex)
    {
        vk::MemoryType MemoryType = MemoryProperties.memoryTypes[CurrentMemoryTypeIndex];
        if ((vk::MemoryPropertyFlagBits::eHostVisible & MemoryType.propertyFlags) &&
            (vk::MemoryPropertyFlagBits::eHostCoherent & MemoryType.propertyFlags))
        {
            MemoryHeapSize = MemoryProperties.memoryHeaps[MemoryType.heapIndex].size;
            MemoryTypeIndex = CurrentMemoryTypeIndex;
            break;
        }
    }

    std::cout << "Memory Type Index: " << MemoryTypeIndex << std::endl;
    std::cout << "Memory Heap Size : " << MemoryHeapSize / 1024 / 1024 / 1024 << " GB" << std::endl;

    vk::MemoryAllocateInfo InBufferMemoryAllocateInfo(InBufferMemoryRequirements.size, MemoryTypeIndex);
    vk::MemoryAllocateInfo OutBufferMemoryAllocateInfo(OutBufferMemoryRequirements.size, MemoryTypeIndex);
    vk::DeviceMemory InBufferMemory = device.allocateMemory(InBufferMemoryAllocateInfo);
    vk::DeviceMemory OutBufferMemory = device.allocateMemory(OutBufferMemoryAllocateInfo);

    Sphere* InBufferPtr = static_cast<Sphere*>(device.mapMemory(InBufferMemory, 0, BufferSizeIn));
    for (int32_t I = 0; I < NumElements; ++I)
    {
        InBufferPtr[I] = scene.Spheres[I];
    }
    device.unmapMemory(InBufferMemory);

    device.bindBufferMemory(InBuffer, InBufferMemory, 0);
    device.bindBufferMemory(OutBuffer, OutBufferMemory, 0);

    std::vector<char> ShaderContents;
    if (std::ifstream ShaderFile{ "C:/Raytracing/FastRayTracing/RayTracing/src/raytracer.vert.spv", std::ios::binary | std::ios::ate })
    {
        const size_t FileSize = ShaderFile.tellg();
        ShaderFile.seekg(0);
        ShaderContents.resize(FileSize, '\0');
        ShaderFile.read(ShaderContents.data(), FileSize);
    }

    vk::ShaderModuleCreateInfo ShaderModuleCreateInfo(
        vk::ShaderModuleCreateFlags(),                                // Flags
        ShaderContents.size(),                                        // Code size
        reinterpret_cast<const uint32_t*>(ShaderContents.data()));    // Code
    vk::ShaderModule ShaderModule = device.createShaderModule(ShaderModuleCreateInfo);

    const std::vector<vk::DescriptorSetLayoutBinding> DescriptorSetLayoutBinding = {
    {0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
    {1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute}
    };
    vk::DescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(
        vk::DescriptorSetLayoutCreateFlags(),
        DescriptorSetLayoutBinding);
    vk::DescriptorSetLayout DescriptorSetLayout = device.createDescriptorSetLayout(DescriptorSetLayoutCreateInfo);

    vk::PipelineLayoutCreateInfo PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(), DescriptorSetLayout);
    vk::PipelineLayout PipelineLayout = device.createPipelineLayout(PipelineLayoutCreateInfo);
    vk::PipelineCache PipelineCache = device.createPipelineCache(vk::PipelineCacheCreateInfo());

    vk::PipelineShaderStageCreateInfo PipelineShaderCreateInfo(
        vk::PipelineShaderStageCreateFlags(),  // Flags
        vk::ShaderStageFlagBits::eCompute,     // Stage
        ShaderModule,                          // Shader Module
        "Main");                               // Shader Entry Point
    vk::ComputePipelineCreateInfo ComputePipelineCreateInfo(
        vk::PipelineCreateFlags(),    // Flags
        PipelineShaderCreateInfo,     // Shader Create Info struct
        PipelineLayout);              // Pipeline Layout
    auto ComputePipeline = device.createComputePipeline(PipelineCache, ComputePipelineCreateInfo).value;


    vk::DescriptorPoolSize DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 2);
    vk::DescriptorPoolCreateInfo DescriptorPoolCreateInfo(vk::DescriptorPoolCreateFlags(), 1, DescriptorPoolSize);
    vk::DescriptorPool DescriptorPool = device.createDescriptorPool(DescriptorPoolCreateInfo);

    vk::DescriptorSetAllocateInfo DescriptorSetAllocInfo(DescriptorPool, 1, &DescriptorSetLayout);
    const std::vector<vk::DescriptorSet> DescriptorSets = device.allocateDescriptorSets(DescriptorSetAllocInfo);
    vk::DescriptorSet DescriptorSet = DescriptorSets.front();
    vk::DescriptorBufferInfo InBufferInfo(InBuffer, 0, NumElements * sizeof(Sphere));
    vk::DescriptorBufferInfo OutBufferInfo(OutBuffer, 0, NumElements * sizeof(float));

    const std::vector<vk::WriteDescriptorSet> WriteDescriptorSets = {
        {DescriptorSet, 0, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &InBufferInfo},
        {DescriptorSet, 1, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &OutBufferInfo},
    };
    device.updateDescriptorSets(WriteDescriptorSets, {});

    vk::CommandPoolCreateInfo CommandPoolCreateInfo(vk::CommandPoolCreateFlags(), ComputeQueueFamilyIndex);
    vk::CommandPool CommandPool = device.createCommandPool(CommandPoolCreateInfo);

    vk::CommandBufferAllocateInfo CommandBufferAllocInfo(
        CommandPool,                         // Command Pool
        vk::CommandBufferLevel::ePrimary,    // Level
        1);                                  // Num Command Buffers
    const std::vector<vk::CommandBuffer> CmdBuffers = device.allocateCommandBuffers(CommandBufferAllocInfo);
    vk::CommandBuffer CmdBuffer = CmdBuffers.front();


    vk::CommandBufferBeginInfo CmdBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    CmdBuffer.begin(CmdBufferBeginInfo);
    CmdBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, ComputePipeline);
    CmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,    // Bind point
        PipelineLayout,                  // Pipeline Layout
        0,                               // First descriptor set
        { DescriptorSet },               // List of descriptor sets
        {});                             // Dynamic offsets
    CmdBuffer.dispatch(NumElements, 1, 1);
    CmdBuffer.end();

    vk::Queue Queue = device.getQueue(ComputeQueueFamilyIndex, 0);
    vk::Fence Fence = device.createFence(vk::FenceCreateInfo());

    vk::SubmitInfo SubmitInfo(0,                // Num Wait Semaphores
        nullptr,        // Wait Semaphores
        nullptr,        // Pipeline Stage Flags
        1,              // Num Command Buffers
        &CmdBuffer);    // List of command buffers
    Queue.submit({ SubmitInfo }, Fence);
    device.waitForFences({ Fence },             // List of fences
        true,               // Wait All
        uint64_t(-1));      // Timeout

    InBufferPtr = static_cast<Sphere*>(device.mapMemory(InBufferMemory, 0, BufferSizeIn));
    for (uint32_t I = 0; I < NumElements; ++I)
    {
        std::cout << InBufferPtr[I].Radius << " ";
    }
    std::cout << std::endl;
    device.unmapMemory(InBufferMemory);

    float* OutBufferPtr = static_cast<float*>(device.mapMemory(OutBufferMemory, 0, BufferSizeOut));
    for (uint32_t I = 0; I < NumElements; ++I)
    {
        std::cout << OutBufferPtr[I] << " ";
    }
    std::cout << std::endl;
    device.unmapMemory(OutBufferMemory);

}

QueueFamilyIndices Compute::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;
    uint32_t queueFamilyCount = 0;

    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            indices.graphicsAndComputeFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}

void Compute::DrawFrame(float currentFrame) {
    //VkSubmitInfo submitInfo{};
    //submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    //// Compute submission        
    //vkWaitForFences(device, 1, &computeInFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    //updateUniformBuffer(currentFrame);

    //vkResetFences(device, 1, &computeInFlightFences[currentFrame]);

    //vkResetCommandBuffer(computeCommandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
    //recordComputeCommandBuffer(computeCommandBuffers[currentFrame]);

    //submitInfo.commandBufferCount = 1;
    //submitInfo.pCommandBuffers = &computeCommandBuffers[currentFrame];
    //submitInfo.signalSemaphoreCount = 1;
    //submitInfo.pSignalSemaphores = &computeFinishedSemaphores[currentFrame];

    //if (vkQueueSubmit(computeQueue, 1, &submitInfo, computeInFlightFences[currentFrame]) != VK_SUCCESS) {
    //    throw std::runtime_error("failed to submit compute command buffer!");
    //};

    //// Graphics submission
    //vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    //uint32_t imageIndex;
    //VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    //if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    //    recreateSwapChain();
    //    return;
    //}
    //else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    //    throw std::runtime_error("failed to acquire swap chain image!");
    //}

    //vkResetFences(device, 1, &inFlightFences[currentFrame]);

    //vkResetCommandBuffer(commandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
    //recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

    //VkSemaphore waitSemaphores[] = { computeFinishedSemaphores[currentFrame], imageAvailableSemaphores[currentFrame] };
    //VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    //submitInfo = {};
    //submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    //submitInfo.waitSemaphoreCount = 2;
    //submitInfo.pWaitSemaphores = waitSemaphores;
    //submitInfo.pWaitDstStageMask = waitStages;
    //submitInfo.commandBufferCount = 1;
    //submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
    //submitInfo.signalSemaphoreCount = 1;
    //submitInfo.pSignalSemaphores = &renderFinishedSemaphores[currentFrame];

    ////if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
    ////    throw std::runtime_error("failed to submit draw command buffer!");
    ////}

    //VkPresentInfoKHR presentInfo{};
    //presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    //presentInfo.waitSemaphoreCount = 1;
    //presentInfo.pWaitSemaphores = &renderFinishedSemaphores[currentFrame];

    //VkSwapchainKHR swapChains[] = { swapChain };
    //presentInfo.swapchainCount = 1;
    //presentInfo.pSwapchains = swapChains;

    //presentInfo.pImageIndices = &imageIndex;

    //result = vkQueuePresentKHR(presentQueue, &presentInfo);

    //if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
    //    framebufferResized = false;
    //    recreateSwapChain();
    //}
    //else if (result != VK_SUCCESS) {
    //    throw std::runtime_error("failed to present swap chain image!");
    //}

    //currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Compute::CreateComputePipeline() {
    auto computeShaderCode = Utils::readFile("raytracer.spv");

    VkShaderModule computeShaderModule = createShaderModule(computeShaderCode);

    VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
    computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderStageInfo.module = computeShaderModule;
    computeShaderStageInfo.pName = "main";

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &computeDescriptorSetLayout;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &computePipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipeline layout!");
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = computePipelineLayout;
    pipelineInfo.stage = computeShaderStageInfo;

    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipeline!");
    }

    vkDestroyShaderModule(device, computeShaderModule, nullptr);
}

void Compute::recordComputeCommandBuffer(VkCommandBuffer commandBuffer) {
    //VkCommandBufferBeginInfo beginInfo{};
    //beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    //if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
    //    throw std::runtime_error("failed to begin recording compute command buffer!");
    //}

    //vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);

    //vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeDescriptorSets[currentFrame], 0, nullptr);

    //vkCmdDispatch(commandBuffer, PARTICLE_COUNT / 256, 1, 1);

    //if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
    //    throw std::runtime_error("failed to record compute command buffer!");
    //}

}


VkShaderModule Compute::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(Walnut::Application::GetDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

void Compute::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    computeFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    computeInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics synchronization objects for a frame!");
        }
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &computeFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &computeInFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute synchronization objects for a frame!");
        }
    }
}