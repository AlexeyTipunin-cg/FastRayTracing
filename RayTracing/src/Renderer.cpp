#include "Renderer.h"

namespace Utils {
	static uint32_t ConvertToRGBA(glm::vec4 color) {
		uint8_t r = uint8_t(255.0f * color.r);
		uint8_t g = uint8_t(255.0f * color.g);
		uint8_t b = uint8_t(255.0f * color.b);
		uint8_t a = uint8_t(255 * color.a);

		return (a << 24) | (b << 16) | (g << 8) | r;
	}
}

void Renderer::Render(const Scene& scene, const Camera& camera) {
	m_Camera = &camera;
	m_Scene = &scene;

	for (uint32_t y = 0; y < m_FinalImage->GetHeight(); y++)
	{
		for (uint32_t x = 0; x < m_FinalImage->GetWidth(); x++)
		{
			auto color = PerPixel(x, y);
			color = glm::clamp(color, glm::vec4(0.0f), glm::vec4(1.0f));
			m_ImageData[x + y * m_FinalImage->GetWidth()] = Utils::ConvertToRGBA(color);
		}
	}

	m_FinalImage->SetData(m_ImageData);
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

	int bounce = 2;
	glm::vec3 color{0.0f};

	float multiplier = 1.0f;
	for (size_t i = 0; i < bounce; i++)
	{
		Renderer::HitPayload payload = TraceRay(ray);

		if (payload.HitDistance < 0.0f)
		{
			glm::vec3 skyColor = glm::vec3(0.0f);
			color += skyColor * multiplier;
			return glm::vec4(color, 1.0f);
		}

		glm::vec3 lightDir = glm::normalize(glm::vec3(-1, -1, -1));
		float lightIntensity = glm::max(glm::dot(payload.WorldNormal, -lightDir), 0.0f);

		const Sphere& sphere = m_Scene->Spheres[payload.ObjectIndex];
		glm::vec3 sphereColor = sphere.Albedo;
		sphereColor *= lightIntensity;
		color += sphereColor * multiplier;
		multiplier *= 0.5;
		//sphereColor *= lightIntensity;

		ray.Origin = payload.WorldPosition + payload.WorldNormal * 0.0001f;
		ray.Direction = glm::reflect(ray.Direction, payload.WorldNormal);
	}



	return glm::vec4(color, 1.0f);
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


Renderer::HitPayload Renderer::TraceRay( const Ray& ray)
{
	glm::vec3 rayOrigin = ray.Origin;

	int closestSphere = -1;
	float hitDistance = FLT_MAX;

	glm::vec3 rayDirection = ray.Direction;
	float a = glm::dot(rayDirection, rayDirection);

	for (size_t i = 0 ; i < m_Scene->Spheres.size(); i++)
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
}
