#include "Compute.h"
#include "Walnut/Application.h"
#include <fstream>
#include <iostream>
#include "Scene.h"
#include "glm/gtx/string_cast.hpp"
#include "Renderer.h"
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




Compute::Compute(const Scene& scene, uint32_t width, uint32_t height, uint32_t frameIndex, const Camera& camera)
{
	m_width = width;
	m_height = height;
	m_frameIndex = frameIndex;
	m_Camera = &camera;
	m_Scene = &scene;

	Initialize();
}

void Compute::Initialize()
{
	device = Walnut::Application::GetDevice();
	physicalDevice = Walnut::Application::GetPhysicalDevice();
	//auto indices = findQueueFamilies(physicalDevice);
	//vkGetDeviceQueue(device, indices.graphicsAndComputeFamily.value(),
	//    0, &computeQueue);

	std::vector<vk::QueueFamilyProperties> QueueFamilyProps = physicalDevice.getQueueFamilyProperties();
	auto PropIt = std::find_if(QueueFamilyProps.begin(), QueueFamilyProps.end(), [](const vk::QueueFamilyProperties& Prop)
		{
			return Prop.queueFlags & vk::QueueFlagBits::eCompute;
		});
	ComputeQueueFamilyIndex = std::distance(QueueFamilyProps.begin(), PropIt);
	std::cout << "Compute Queue Family Index: " << ComputeQueueFamilyIndex << std::endl;

	CameraDirectionsBufferSize = m_Camera->GetRayDirections().size() * sizeof(CameraDirections);



	NumElements = m_width * m_height;
	BufferSizeIn = 1 * sizeof(FrameData);
	BufferSizeOut = NumElements * sizeof(uint32_t);
	AccumulatedColorBufferSize = NumElements * sizeof(OutPut);

	BufferCreateInfoIn = {
	vk::BufferCreateFlags(),                    // Flags
	BufferSizeIn,                                 // Size
	vk::BufferUsageFlagBits::eStorageBuffer,    // Usage
	vk::SharingMode::eExclusive,                // Sharing mode
	1,                                          // Number of queue family indices
	&ComputeQueueFamilyIndex                    // List of queue family indices
	};

	BufferCreateInfoOut = {
		vk::BufferCreateFlags(),                    // Flags
		BufferSizeOut,                                 // Size
		vk::BufferUsageFlagBits::eStorageBuffer,    // Usage
		vk::SharingMode::eExclusive,                // Sharing mode
		1,                                          // Number of queue family indices
		&ComputeQueueFamilyIndex                    // List of queue family indices
	};

	BufferCreateInfoCameraDirections = {
	vk::BufferCreateFlags(),
	CameraDirectionsBufferSize,
	vk::BufferUsageFlagBits::eStorageBuffer,
	vk::SharingMode::eExclusive,
	1,
	&ComputeQueueFamilyIndex
	};

	AccumulatedCreateInfo = {
		vk::BufferCreateFlags(),
		AccumulatedColorBufferSize,
		vk::BufferUsageFlagBits::eStorageBuffer,
		vk::SharingMode::eExclusive,
		1,
		&ComputeQueueFamilyIndex
			};



	InBuffer = device.createBuffer(BufferCreateInfoIn);
	OutBuffer = device.createBuffer(BufferCreateInfoOut);
	//vk::Buffer UniformBuffer = device.createBuffer(BufferCreateInfoUniform);
	CameraBuffer = device.createBuffer(BufferCreateInfoCameraDirections);
	AccumulatedBuffer = device.createBuffer(AccumulatedCreateInfo);
	//vk::Buffer MaterialBuffer = device.createBuffer(BufferCreateInfoMaterials);


	vk::MemoryRequirements InBufferMemoryRequirements = device.getBufferMemoryRequirements(InBuffer);
	vk::MemoryRequirements OutBufferMemoryRequirements = device.getBufferMemoryRequirements(OutBuffer);
	//vk::MemoryRequirements UniformBufferMemoryRequirements = device.getBufferMemoryRequirements(UniformBuffer);
	vk::MemoryRequirements CameraBufferMemoryRequirements = device.getBufferMemoryRequirements(CameraBuffer);
	vk::MemoryRequirements AccumulatedBufferMemoryRequirements = device.getBufferMemoryRequirements(AccumulatedBuffer);
	//vk::MemoryRequirements MaterialBufferMemoryRequirements = device.getBufferMemoryRequirements(MaterialBuffer);


	vk::PhysicalDeviceMemoryProperties MemoryProperties = physicalDevice.getMemoryProperties();
	vk::PhysicalDeviceProperties Properties = physicalDevice.getProperties();

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
	std::cout << "Name : " << Properties.deviceName << std::endl;


	vk::MemoryAllocateInfo InBufferMemoryAllocateInfo(InBufferMemoryRequirements.size, MemoryTypeIndex);
	vk::MemoryAllocateInfo OutBufferMemoryAllocateInfo(OutBufferMemoryRequirements.size, MemoryTypeIndex);
	//vk::MemoryAllocateInfo UniformBufferAllocateInfo(UniformBufferMemoryRequirements.size, MemoryTypeIndex);
	vk::MemoryAllocateInfo CameraBufferAllocateInfo(CameraBufferMemoryRequirements.size, MemoryTypeIndex);
	vk::MemoryAllocateInfo AccumulatedBufferAllocateInfo(AccumulatedBufferMemoryRequirements.size, MemoryTypeIndex);
	//vk::MemoryAllocateInfo MaterialBufferAllocateInfo(MaterialBufferMemoryRequirements.size, MemoryTypeIndex);


	InBufferMemory = device.allocateMemory(InBufferMemoryAllocateInfo);
	OutBufferMemory = device.allocateMemory(OutBufferMemoryAllocateInfo);
	//vk::DeviceMemory UniformBufferMemory = device.allocateMemory(UniformBufferAllocateInfo);
	CameraBufferMemory = device.allocateMemory(CameraBufferAllocateInfo);
	AccumulatedBufferMemory = device.allocateMemory(AccumulatedBufferAllocateInfo);

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
	ShaderModule = device.createShaderModule(ShaderModuleCreateInfo);

	const std::vector<vk::DescriptorSetLayoutBinding> DescriptorSetLayoutBinding = {
{0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
{1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
{2, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
{3, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
//{4, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute}
	};

	vk::DescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(
		vk::DescriptorSetLayoutCreateFlags(),
		DescriptorSetLayoutBinding);
	DescriptorSetLayout = device.createDescriptorSetLayout(DescriptorSetLayoutCreateInfo);

		vk::PipelineLayoutCreateInfo PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(), DescriptorSetLayout);
	PipelineLayout = device.createPipelineLayout(PipelineLayoutCreateInfo);
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
	ComputePipeline = device.createComputePipeline(PipelineCache, ComputePipelineCreateInfo).value;

		device.bindBufferMemory(InBuffer, InBufferMemory, 0);
	device.bindBufferMemory(OutBuffer, OutBufferMemory, 0);
	device.bindBufferMemory(CameraBuffer, CameraBufferMemory, 0);
	device.bindBufferMemory(AccumulatedBuffer, AccumulatedBufferMemory, 0);

	InBufferInfo = vk::DescriptorBufferInfo(InBuffer, 0, 1 * sizeof(FrameData));
	OutBufferInfo = vk::DescriptorBufferInfo(OutBuffer, 0, BufferSizeOut);
	CameraBufferInfo = vk::DescriptorBufferInfo(CameraBuffer, 0, CameraDirectionsBufferSize);
	AccumulatedBufferInfo = vk::DescriptorBufferInfo(AccumulatedBuffer, 0, AccumulatedColorBufferSize);

	vk::DescriptorPoolSize DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 4);
	DescriptorPoolCreateInfo = vk::DescriptorPoolCreateInfo (vk::DescriptorPoolCreateFlags(), 1, DescriptorPoolSize);
}

void Compute::CalculateScreenPixels(uint32_t frameIndex, std::vector<uint32_t>* vertilcaIterator, std::vector<uint32_t>* horizontalIterator)
{
	InBufferPtr = static_cast<FrameData*>(device.mapMemory(InBufferMemory, 0, BufferSizeIn));

	auto pos = m_Camera->GetPosition();
	CameraDirections cameraPosition{ pos };
	FrameData data;
	data.ScreenWidth = m_width;
	data.ScreenHeight = m_height;
	data.FrameIndex = frameIndex;
	data.CameraPosition = cameraPosition;

	for (size_t i = 0; i < m_Scene->Spheres.size(); i++)
	{
		data.Spheres[i] = m_Scene->Spheres[i];
	}

	for (size_t i = 0; i < m_Scene->Materials.size(); i++)
	{
		data.Materials[i] = m_Scene->Materials[i];
	}

	InBufferPtr[0] = data;

	device.unmapMemory(InBufferMemory);



	if (frameIndex < 3)
	{
		CameraBufferPtr = static_cast<CameraDirections*>(device.mapMemory(CameraBufferMemory, 0, CameraDirectionsBufferSize));

		for (int32_t I = 0; I < m_Camera->GetRayDirections().size(); ++I)
		{
			CameraDirections dir{ m_Camera->GetRayDirections()[I] };
			CameraBufferPtr[I] = dir;
		}

		device.unmapMemory(CameraBufferMemory);

	}

	if (frameIndex == 1)
	{
		AccumulationData = static_cast<OutPut*>(device.mapMemory(AccumulatedBufferMemory, 0, AccumulatedColorBufferSize));
		memset(AccumulationData, 0, AccumulatedColorBufferSize);
		device.unmapMemory(AccumulatedBufferMemory);
	}

	vk::DescriptorPool DescriptorPool = device.createDescriptorPool(DescriptorPoolCreateInfo);

	vk::DescriptorSetAllocateInfo DescriptorSetAllocInfo(DescriptorPool, 1, &DescriptorSetLayout);
	const std::vector<vk::DescriptorSet> DescriptorSets = device.allocateDescriptorSets(DescriptorSetAllocInfo);
	DescriptorSet = DescriptorSets.front();

	WriteDescriptorSet = {
		{DescriptorSet, 0, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &InBufferInfo},
		{DescriptorSet, 1, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &OutBufferInfo},
		{DescriptorSet, 2, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &CameraBufferInfo},
		{DescriptorSet, 3, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &AccumulatedBufferInfo},
	};
	device.updateDescriptorSets(WriteDescriptorSet, {});

	if (!CmdBuffer)
	{
		vk::CommandPoolCreateInfo CommandPoolCreateInfo(vk::CommandPoolCreateFlags(), ComputeQueueFamilyIndex);
		vk::CommandPool CommandPool = device.createCommandPool(CommandPoolCreateInfo);

		vk::CommandBufferAllocateInfo CommandBufferAllocInfo(
			CommandPool,                         // Command Pool
			vk::CommandBufferLevel::ePrimary,    // Level
			1);                                  // Num Command Buffers
		const std::vector<vk::CommandBuffer> CmdBuffers = device.allocateCommandBuffers(CommandBufferAllocInfo);
		CmdBuffer = CmdBuffers.front();
	}



	vk::CommandBufferBeginInfo CmdBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	CmdBuffer.begin(CmdBufferBeginInfo);
	CmdBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, ComputePipeline);
	CmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,    // Bind point
		PipelineLayout,                  // Pipeline Layout
		0,                               // First descriptor set
		{ DescriptorSet },               // List of descriptor sets
		{});                             // Dynamic offsets
	CmdBuffer.dispatch(m_width / 32, m_height/ 32, 1);
	CmdBuffer.end();

	vk::Queue Queue = device.getQueue(ComputeQueueFamilyIndex, 0);
	Fence = device.createFence(vk::FenceCreateInfo());

	vk::SubmitInfo SubmitInfo(0,                // Num Wait Semaphores
		nullptr,        // Wait Semaphores
		nullptr,        // Pipeline Stage Flags
		1,              // Num Command Buffers
		&CmdBuffer);    // List of command buffers
	Queue.submit({ SubmitInfo }, Fence);
	device.waitForFences({ Fence },             // List of fences
		true,               // Wait All
		uint64_t(-1));

	uint32_t* OutBufferPtr = static_cast<uint32_t*>(device.mapMemory(OutBufferMemory, 0, BufferSizeOut));
	RenderResult = OutBufferPtr;
	device.unmapMemory(OutBufferMemory);
	AccumulationData = static_cast<OutPut*>(device.mapMemory(AccumulatedBufferMemory, 0, AccumulatedColorBufferSize));
	device.unmapMemory(AccumulatedBufferMemory);
}

