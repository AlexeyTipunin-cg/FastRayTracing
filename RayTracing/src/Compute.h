#pragma once
#include <vulkan/vulkan.hpp>
#include <optional>
#include "Scene.h"

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsAndComputeFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsAndComputeFamily.has_value() && presentFamily.has_value();
    }
};

class Compute
{


public:
	Compute(Scene scene);
    void Init(Scene scene);
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    void DrawFrame(float currentFrame);

    void CreateComputePipeline();
    void recordComputeCommandBuffer(VkCommandBuffer commandBuffer);
    VkShaderModule createShaderModule(const std::vector<char>& code);

    void createSyncObjects();

private:
    int MAX_FRAMES_IN_FLIGHT = 2;

    VkSurfaceKHR surface;
    VkQueue computeQueue;
    VkDevice device;

    VkDescriptorSetLayout computeDescriptorSetLayout;
    VkPipelineLayout computePipelineLayout;
    VkPipeline computePipeline;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkSemaphore> computeFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> computeInFlightFences;
};

