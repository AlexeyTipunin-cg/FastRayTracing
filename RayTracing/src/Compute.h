#pragma once
#include <vulkan/vulkan.hpp>
#include <optional>
#include "Scene.h"
#include "Camera.h"

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
    float FrameIndex;
    CameraDirections CameraPosition;
    Sphere Spheres[3];
    Material Materials[3];
};

class Compute
{


public:
    Compute(Scene scene, float width, float height, uint32_t frameIndex, const Camera& camera);
    void InitBegiin();
    void Init(Scene scene, uint32_t frameIndex, glm::vec4* accumulateData);
    void Update(uint32_t frameIndex);
    void Draw();
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    void DrawFrame(float currentFrame);

    void CreateComputePipeline();
    void recordComputeCommandBuffer(VkCommandBuffer commandBuffer);
    VkShaderModule createShaderModule(const std::vector<char>& code);

    void createSyncObjects();

    uint32_t* RenderResult;
    OutPut* AccumulationData;
    vk::Fence Fence;
    vk::Device device;
    vk::PhysicalDevice physicalDevice;


private:
    int MAX_FRAMES_IN_FLIGHT = 2;

    float m_width;
    float m_height;
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
    Scene m_Scene;

    vk::BufferCreateInfo BufferCreateInfoIn;
    vk::BufferCreateInfo BufferCreateInfoOut;
    vk::BufferCreateInfo BufferCreateInfoCameraDirections;
    vk::BufferCreateInfo  AccumulatedCreateInfo;

    vk::Buffer InBuffer;
    vk::Buffer OutBuffer;
        //vk::Buffer UniformBuf
    vk::Buffer CameraBuffer;
    vk::Buffer AccumulatedBuffer;
    vk::ShaderModule ShaderModule;

    FrameData* InBufferPtr;
    CameraDirections* CameraBufferPtr;

    VkDescriptorSetLayout computeDescriptorSetLayout;
    VkPipelineLayout computePipelineLayout;
    VkPipeline computePipeline;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkSemaphore> computeFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> computeInFlightFences;
    vk::DescriptorSetLayout DescriptorSetLayout;
    vk::CommandBuffer CmdBuffer;
};

