#include "Renderer.h"
#include <execution>
#include <vulkan/vulkan.hpp>
#include <iostream>
#include <fstream>
#include "Compute.h"

namespace Utils {
	static uint32_t ConvertToRGBA(glm::vec4 color) {
		uint8_t r = uint8_t(255.0f * color.r);
		uint8_t g = uint8_t(255.0f * color.g);
		uint8_t b = uint8_t(255.0f * color.b);
		uint8_t a = uint8_t(255 * color.a);

		return (a << 24) | (b << 16) | (g << 8) | r;
	}

	static uint32_t PCG_Hash(uint32_t input)
	{
		uint32_t state = input * 747796405u + 2891336453u;
		uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
		return (word >> 22u) ^ word;
	}

	static float RandomFloat(uint32_t& seed)
	{
		seed = PCG_Hash(seed);
		return (float)seed / (float)std::numeric_limits<uint32_t>::max();
	}

	static glm::vec3 InUnitSphere(uint32_t& seed)
	{
		return glm::normalize(glm::vec3(RandomFloat(seed) * 2.0f - 1.0f, RandomFloat(seed) * 2.0f - 1.0f, RandomFloat(seed) * 2.0f - 1.0f));
	}

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


void Renderer::Render(const Scene& scene, const Camera& camera) {

	m_Camera = &camera;
	m_Scene = &scene;

	if (m_FrameIndex == 1)
	{
		memset(m_AccumulationData, 0, m_FinalImage->GetWidth() * m_FinalImage->GetHeight() * sizeof(glm::vec4));
	}

#define GPU 1
#if GPU
	if (m_Compute == nullptr)
	{
		m_Compute = new Compute(scene, m_FinalImage->GetWidth(), m_FinalImage->GetHeight(), m_FrameIndex, camera);
	}

	m_Compute->CalculateScreenPixels(m_FrameIndex, &m_VerticalIterator, &m_HorizontalIterator);

	m_FinalImage->SetData(m_Compute->RenderResult);

	if (m_Settings.Accumalate)
	{
		m_FrameIndex++;
	}
	else {
		m_FrameIndex = 1;
	}

	return;

#else

#define MT 1
#if MT

	std::for_each(std::execution::par, m_VerticalIterator.begin(), m_VerticalIterator.end(),
		[this](uint32_t y)
		{
			std::for_each(std::execution::par, m_HorizontalIterator.begin(), m_HorizontalIterator.end(),
				[this, y](uint32_t x)
				{
					auto color = PerPixel(x, y);
					m_AccumulationData[x + y * m_FinalImage->GetWidth()] += color;

					auto sampledColor = m_AccumulationData[x + y * m_FinalImage->GetWidth()] / (float)m_FrameIndex;
					sampledColor = glm::clamp(sampledColor, glm::vec4(0.0f), glm::vec4(1.0f));
					m_ImageData[x + y * m_FinalImage->GetWidth()] = Utils::ConvertToRGBA(sampledColor);

				});

		});
#else

	for (uint32_t y = 0; y < m_FinalImage->GetHeight(); y++)
	{
		for (uint32_t x = 0; x < m_FinalImage->GetWidth(); x++)
		{
			auto color = PerPixel(x, y);
			m_AccumulationData[x + y * m_FinalImage->GetWidth()] += color;

			auto sampledColor = m_AccumulationData[x + y * m_FinalImage->GetWidth()] / (float)m_FrameIndex;
			sampledColor = glm::clamp(sampledColor, glm::vec4(0.0f), glm::vec4(1.0f));
			m_ImageData[x + y * m_FinalImage->GetWidth()] = Utils::ConvertToRGBA(sampledColor);
		}
	}
#endif
	m_FinalImage->SetData(m_ImageData);

	if (m_Settings.Accumalate)
	{
		m_FrameIndex++;
	}
	else {
		m_FrameIndex = 1;
	}
#endif
}

void Renderer::SetLightPosition(glm::vec3 pos)
{
	m_LightPos = pos;
}

glm::vec4 Renderer::PerPixel(uint32_t x, uint32_t y)
{
	Ray ray;
	ray.Origin = m_Camera->GetPosition();
	ray.Direction = m_Camera->GetRayDirections()[x + y * m_FinalImage->GetWidth()];

	uint32_t seed = x + y * m_FinalImage->GetWidth();
	seed *= m_FrameIndex;

	int bounce = 5;
	glm::vec3 light{ 0.0f };

	glm::vec3 contribution{ 1.0f };
	for (size_t i = 0; i < bounce; i++)
	{
		seed += i;

		Renderer::HitPayload payload = TraceRay(ray);

		if (payload.HitDistance < 0.0f)
		{
			glm::vec3 skyColor = glm::vec3(0.06, 0.7f, 0.9f);
			light += skyColor * contribution;
			return glm::vec4(light, 1.0f);
		}

		const Sphere& sphere = m_Scene->Spheres[payload.ObjectIndex];
		const Material& mat = m_Scene->Materials[sphere.MaterialIndex];

		//light += mat.Albedo * contribution;
		contribution *= mat.Albedo;


		light += mat.GetEmission();

		ray.Origin = payload.WorldPosition + payload.WorldNormal * 0.0001f;

		if (m_Settings.SlowRandom)
		{
			ray.Direction = glm::normalize(payload.WorldNormal + Walnut::Random::InUnitSphere());
		}
		else {
			ray.Direction = glm::normalize(payload.WorldNormal + Utils::InUnitSphere(seed));

		}
	}



	return glm::vec4(light, 1.0f);
}

Renderer::HitPayload Renderer::ClosestHit(const Ray& ray, float hitDistance, int objectIndex)
{
	Renderer::HitPayload payload;
	payload.HitDistance = hitDistance;
	payload.ObjectIndex = objectIndex;

	const Sphere& closestSphere = m_Scene->Spheres[objectIndex];

	glm::vec3 origin = ray.Origin - closestSphere.Position;
	payload.WorldPosition = origin + ray.Direction * hitDistance;
	payload.WorldNormal = glm::normalize(payload.WorldPosition);

	payload.WorldPosition += closestSphere.Position;

	return  payload;
}

Renderer::HitPayload Renderer::Miss(const Ray& ray)
{
	Renderer::HitPayload payload;
	payload.HitDistance = -1.0f;
	return payload;
}


Renderer::HitPayload Renderer::TraceRay(const Ray& ray)
{
	glm::vec3 rayOrigin = ray.Origin;

	int closestSphere = -1;
	float hitDistance = FLT_MAX;

	glm::vec3 rayDirection = ray.Direction;
	float a = glm::dot(rayDirection, rayDirection);

	for (size_t i = 0; i < m_Scene->Spheres.size(); i++)
	{
		const Sphere& sphere = m_Scene->Spheres[i];
		glm::vec3 origin = rayOrigin - sphere.Position;

		float b = 2.0f * glm::dot(origin, rayDirection);
		float c = glm::dot(origin, origin) - sphere.Radius * sphere.Radius;

		float discrim = b * b - 4.0f * a * c;

		if (discrim < 0.0f)
		{
			continue;
		}

		float discrimRoot = glm::sqrt(discrim);
		float t2 = (-b - discrimRoot) / 2 / a;

		if (t2 < 0)
		{
			continue;
		}

		glm::vec3 p2 = origin + rayDirection * t2;

		if (t2 < hitDistance)
		{
			hitDistance = t2;
			closestSphere = i;
		}
	}

	if (closestSphere == -1)
	{
		return Miss(ray);
	}

	return ClosestHit(ray, hitDistance, closestSphere);
}

void Renderer::OnResize(uint32_t width, uint32_t height)
{

	if (m_FinalImage)
	{
		if (m_FinalImage->GetWidth() == width && m_FinalImage->GetHeight() == height)
		{
			return;
		}
		m_FinalImage->Resize(width, height);
	}
	else {
		m_FinalImage = std::make_shared<Walnut::Image>(width, height, Walnut::ImageFormat::RGBA);
	}

	delete[] m_ImageData;
	m_ImageData = new uint32_t[width * height];

	delete[] m_AccumulationData;
	m_AccumulationData = new glm::vec4[width * height];

	m_HorizontalIterator.resize(width);
	m_VerticalIterator.resize(height);

	for (size_t i = 0; i < width; i++)
	{
		m_HorizontalIterator[i] = i;
	}

	for (size_t i = 0; i < height; i++)
	{
		m_VerticalIterator[i] = i;
	}
}
