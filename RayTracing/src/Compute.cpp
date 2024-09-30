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




Compute::Compute(Scene scene, float width, float height, uint32_t frameIndex, const Camera& camera)
{
	m_width = width;
	m_height = height;
	m_frameIndex = frameIndex;
	m_Camera = &camera;
	m_Scene = scene;

	//Init(scene);
	InitBegiin();
}

void Compute::InitBegiin()
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
	BufferSizeOut = NumElements * sizeof(OutPut);

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

	InBuffer = device.createBuffer(BufferCreateInfoIn);
	OutBuffer = device.createBuffer(BufferCreateInfoOut);
	//vk::Buffer UniformBuffer = device.createBuffer(BufferCreateInfoUniform);
	CameraBuffer = device.createBuffer(BufferCreateInfoCameraDirections);
	//vk::Buffer MaterialBuffer = device.createBuffer(BufferCreateInfoMaterials);


	vk::MemoryRequirements InBufferMemoryRequirements = device.getBufferMemoryRequirements(InBuffer);
	vk::MemoryRequirements OutBufferMemoryRequirements = device.getBufferMemoryRequirements(OutBuffer);
	//vk::MemoryRequirements UniformBufferMemoryRequirements = device.getBufferMemoryRequirements(UniformBuffer);
	vk::MemoryRequirements CameraBufferMemoryRequirements = device.getBufferMemoryRequirements(CameraBuffer);
	//vk::MemoryRequirements MaterialBufferMemoryRequirements = device.getBufferMemoryRequirements(MaterialBuffer);


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
	//vk::MemoryAllocateInfo UniformBufferAllocateInfo(UniformBufferMemoryRequirements.size, MemoryTypeIndex);
	vk::MemoryAllocateInfo CameraBufferAllocateInfo(CameraBufferMemoryRequirements.size, MemoryTypeIndex);
	//vk::MemoryAllocateInfo MaterialBufferAllocateInfo(MaterialBufferMemoryRequirements.size, MemoryTypeIndex);


	InBufferMemory = device.allocateMemory(InBufferMemoryAllocateInfo);
	OutBufferMemory = device.allocateMemory(OutBufferMemoryAllocateInfo);
	//vk::DeviceMemory UniformBufferMemory = device.allocateMemory(UniformBufferAllocateInfo);
	CameraBufferMemory = device.allocateMemory(CameraBufferAllocateInfo);

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
//{3, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
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
}

void Compute::Init(Scene scene, uint32_t frameIndex)
{

	//const uint32_t FrameDataSize = sizeof(FrameData);
	//const uint32_t MaterialBufferSize = scene.Materials.size() * sizeof(Material);

	//vk::BufferCreateInfo BufferCreateInfoMaterials{
	//vk::BufferCreateFlags(),
	//MaterialBufferSize,
	//vk::BufferUsageFlagBits::eStorageBuffer,
	//vk::SharingMode::eExclusive,
	//1,
	//&ComputeQueueFamilyIndex
	//};

	//vk::DeviceMemory MaterialBufferMemory = device.allocateMemory(MaterialBufferAllocateInfo);


	InBufferPtr = static_cast<FrameData*>(device.mapMemory(InBufferMemory, 0, BufferSizeIn));

	auto pos = m_Camera->GetPosition();
	CameraDirections cameraPosition{ pos };
	FrameData data;
	data.ScreenWidth = m_width;
	data.FrameIndex = frameIndex;
	data.CameraPosition = cameraPosition;

	for (size_t i = 0; i < 3; i++)
	{
		data.Spheres[i] = scene.Spheres[i];
	}

	for (size_t i = 0; i < 3; i++)
	{
		data.Materials[i] = scene.Materials[i];
	}

	InBufferPtr[0] = data;
	//for (int32_t I = 0; I < scene.Spheres.size(); ++I)
	//{
	//    CameraDirections cameraPosition{ m_Camera->GetPosition() };
	//    FrameData data;
	//    data.ScreenWidth = m_width;
	//    data.FrameIndex = m_frameIndex;
	//    data.CameraPosition = cameraPosition;
	//    data.Spheres = scene.Spheres;
	//    data.Materials = scene.Materials

	//    SphereComputeData data{ glm::vec4(scene.Spheres[I].Position, 0.0f),scene.Spheres[I].Radius, scene.Spheres[I].MaterialIndex };
	//    InBufferPtr[I] = data;
	//}

	device.unmapMemory(InBufferMemory);

	//FrameData* UniformBufferPtr = static_cast<FrameData*>(device.mapMemory(UniformBufferMemory, 0, FrameDataSize));
	//UniformBufferPtr->FrameIndex = m_frameIndex;
	//UniformBufferPtr->ScreenWidth = m_width;

	//device.unmapMemory(UniformBufferMemory);

	CameraBufferPtr = static_cast<CameraDirections*>(device.mapMemory(CameraBufferMemory, 0, CameraDirectionsBufferSize));
	for (int32_t I = 0; I < m_Camera->GetRayDirections().size(); ++I)
	{
		CameraDirections dir{ m_Camera->GetRayDirections()[I] };
		CameraBufferPtr[I] = dir;
	}

	device.unmapMemory(CameraBufferMemory);

	//Material* MaterialBufferPtr = static_cast<Material*>(device.mapMemory(MaterialBufferMemory, 0, MaterialBufferSize));
	//for (int32_t I = 0; I < scene.Materials.size(); ++I)
	//{
	//    MaterialBufferPtr[I] = scene.Materials[I];
	//}

	//device.unmapMemory(MaterialBufferMemory);



	device.bindBufferMemory(InBuffer, InBufferMemory, 0);
	device.bindBufferMemory(OutBuffer, OutBufferMemory, 0);
	//device.bindBufferMemory(UniformBuffer, UniformBufferMemory, 0);
	device.bindBufferMemory(CameraBuffer, CameraBufferMemory, 0);
	//device.bindBufferMemory(MaterialBuffer, MaterialBufferMemory, 0);


	vk::DescriptorPoolSize DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 3);
	vk::DescriptorPoolCreateInfo DescriptorPoolCreateInfo(vk::DescriptorPoolCreateFlags(), 1, DescriptorPoolSize);
	vk::DescriptorPool DescriptorPool = device.createDescriptorPool(DescriptorPoolCreateInfo);



	vk::DescriptorSetAllocateInfo DescriptorSetAllocInfo(DescriptorPool, 1, &DescriptorSetLayout);
	const std::vector<vk::DescriptorSet> DescriptorSets = device.allocateDescriptorSets(DescriptorSetAllocInfo);
	DescriptorSet = DescriptorSets.front();
	vk::DescriptorBufferInfo InBufferInfo(InBuffer, 0, 1 * sizeof(FrameData));
	vk::DescriptorBufferInfo OutBufferInfo(OutBuffer, 0, NumElements * sizeof(OutPut));
	//vk::DescriptorBufferInfo UniformBufferInfo(UniformBuffer, 0, FrameDataSize);
	vk::DescriptorBufferInfo CameraBufferInfo(CameraBuffer, 0, CameraDirectionsBufferSize);
	//vk::DescriptorBufferInfo MaterialBufferInfo(MaterialBuffer, 0, MaterialBufferSize);

	WriteDescriptorSet = {
		{DescriptorSet, 0, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &InBufferInfo},
		{DescriptorSet, 1, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &OutBufferInfo},
		//{DescriptorSet, 2, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &UniformBufferInfo},
		{DescriptorSet, 2, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &CameraBufferInfo},
		//{DescriptorSet, 4, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &MaterialBufferInfo},
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
	CmdBuffer.dispatch(m_width, m_height, 1);
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

	// Timeout

	InBufferPtr = static_cast<FrameData*>(device.mapMemory(InBufferMemory, 0, BufferSizeIn));
	//for (uint32_t I = 0; I < scene.Materials.size(); ++I)
	//{
	//	std::cout << InBufferPtr[0].FrameIndex << " ";
	//}
	std::cout << std::endl;
	device.unmapMemory(InBufferMemory);

	OutPut* OutBufferPtr = static_cast<OutPut*>(device.mapMemory(OutBufferMemory, 0, BufferSizeOut));
	RenderResult = OutBufferPtr;
	//for (uint32_t I = 0; I < NumElements; ++I)
	//{
	//	std::cout << glm::to_string(OutBufferPtr[I].direction) << "\n";
	//	break;
	//}
	std::cout << std::endl;
	device.unmapMemory(OutBufferMemory);
}

void Compute::Update(uint32_t frameIndex)
{
	InBufferPtr = static_cast<FrameData*>(device.mapMemory(InBufferMemory, 0, BufferSizeIn));

	auto pos = m_Camera->GetPosition();
	CameraDirections cameraPosition{ pos };
	FrameData data;
	data.ScreenWidth = m_width;
	data.FrameIndex = frameIndex;
	data.CameraPosition = cameraPosition;

	for (size_t i = 0; i < 3; i++)
	{
		data.Spheres[i] = m_Scene.Spheres[i];
	}

	for (size_t i = 0; i < 3; i++)
	{
		data.Materials[i] = m_Scene.Materials[i];
	}

	InBufferPtr[0] = data;
	//for (int32_t I = 0; I < scene.Spheres.size(); ++I)
	//{
	//    CameraDirections cameraPosition{ m_Camera->GetPosition() };
	//    FrameData data;
	//    data.ScreenWidth = m_width;
	//    data.FrameIndex = m_frameIndex;
	//    data.CameraPosition = cameraPosition;
	//    data.Spheres = scene.Spheres;
	//    data.Materials = scene.Materials

	//    SphereComputeData data{ glm::vec4(scene.Spheres[I].Position, 0.0f),scene.Spheres[I].Radius, scene.Spheres[I].MaterialIndex };
	//    InBufferPtr[I] = data;
	//}

	device.unmapMemory(InBufferMemory);

	//FrameData* UniformBufferPtr = static_cast<FrameData*>(device.mapMemory(UniformBufferMemory, 0, FrameDataSize));
	//UniformBufferPtr->FrameIndex = m_frameIndex;
	//UniformBufferPtr->ScreenWidth = m_width;

	//device.unmapMemory(UniformBufferMemory);

	CameraBufferPtr = static_cast<CameraDirections*>(device.mapMemory(CameraBufferMemory, 0, CameraDirectionsBufferSize));
	for (int32_t I = 0; I < m_Camera->GetRayDirections().size(); ++I)
	{
		CameraDirections dir{ m_Camera->GetRayDirections()[I] };
		CameraBufferPtr[I] = dir;
	}

	device.unmapMemory(CameraBufferMemory);
}

void Compute::Draw()
{
	device.updateDescriptorSets(WriteDescriptorSet, {});

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
	CmdBuffer.dispatch(m_width, m_height, 1);
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
		uint64_t(-1));      // Timeout

	InBufferPtr = static_cast<FrameData*>(device.mapMemory(InBufferMemory, 0, BufferSizeIn));
	for (uint32_t I = 0; I < 3; ++I)
	{
		std::cout << InBufferPtr[0].FrameIndex << " ";
	}
	std::cout << std::endl;
	device.unmapMemory(InBufferMemory);

	OutPut* OutBufferPtr = static_cast<OutPut*>(device.mapMemory(OutBufferMemory, 0, BufferSizeOut));
	RenderResult = OutBufferPtr;
	for (uint32_t I = 0; I < NumElements; ++I)
	{
		std::cout << glm::to_string(OutBufferPtr[I].direction) << "\n";
		break;
	}
	std::cout << std::endl;
	device.unmapMemory(OutBufferMemory);
}
