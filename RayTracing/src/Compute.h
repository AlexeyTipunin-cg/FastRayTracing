#pragma once
#include <vulkan/vulkan.hpp>
#include <optional>
#include "Scene.h"
#include "Camera.h"
#include <execution>

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsAndComputeFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsAndComputeFamily.has_value() && presentFamily.has_value();
    }
};

struct CameraDirections
{
    alignas(16) glm::vec3 direction;
};

struct OutPut
{
    alignas(16) glm::vec4 direction;
};

struct FrameData
{
    float ScreenWidth;
    float ScreenHeight;
    float FrameIndex;
    CameraDirections CameraPosition;
    Sphere Spheres[4];
    Material Materials[4];
};

class Compute
{


public:
    Compute(const Scene& scene, uint32_t width, uint32_t height, uint32_t frameIndex, const Camera& camera);
    void Initialize();
    void CalculateScreenPixels(uint32_t frameIndex, std::vector<uint32_t>* vertilcaIterator, std::vector<uint32_t>* horizontalIterator);

    uint32_t* RenderResult;
    OutPut* AccumulationData = nullptr;
    vk::Fence Fence;
    vk::Device device;
    vk::PhysicalDevice physicalDevice;


private:
    int MAX_FRAMES_IN_FLIGHT = 2;

    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_frameIndex;
    const Camera* m_Camera;

    VkSurfaceKHR surface;
    VkQueue computeQueue;
    vk::Pipeline ComputePipeline;
    std::vector<vk::WriteDescriptorSet> WriteDescriptorSet;
    uint32_t ComputeQueueFamilyIndex;
    vk::DescriptorSet DescriptorSet;
    vk::PipelineLayout PipelineLayout;
    vk::DeviceMemory InBufferMemory;
    vk::DeviceMemory OutBufferMemory;
    vk::DeviceMemory CameraBufferMemory;
    vk::DeviceMemory AccumulatedBufferMemory;
    uint32_t NumElements;
    uint32_t BufferSizeIn;
    uint32_t BufferSizeOut;
    uint32_t AccumulatedColorBufferSize;
    uint32_t CameraDirectionsBufferSize;
    const Scene* m_Scene;

    vk::BufferCreateInfo BufferCreateInfoIn;
    vk::BufferCreateInfo BufferCreateInfoOut;
    vk::BufferCreateInfo BufferCreateInfoCameraDirections;
    vk::BufferCreateInfo  AccumulatedCreateInfo;

    vk::Buffer InBuffer;
    vk::Buffer OutBuffer;

    vk::Buffer CameraBuffer;
    vk::Buffer AccumulatedBuffer;
    vk::ShaderModule ShaderModule;

    FrameData* InBufferPtr;
    CameraDirections* CameraBufferPtr;

    vk::DescriptorSetAllocateInfo DescriptorSetAllocInfo;
    vk::DescriptorBufferInfo InBufferInfo;
    vk::DescriptorBufferInfo OutBufferInfo;
    vk::DescriptorBufferInfo CameraBufferInfo;
    vk::DescriptorBufferInfo AccumulatedBufferInfo;
    vk::DescriptorPoolCreateInfo DescriptorPoolCreateInfo;

    VkPipelineLayout computePipelineLayout;
    VkPipeline computePipeline;

    vk::DescriptorSetLayout DescriptorSetLayout;
    vk::CommandBuffer CmdBuffer;
};

