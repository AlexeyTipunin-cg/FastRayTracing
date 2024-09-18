#pragma once
#include "Walnut/Image.h"
#include "Walnut/Random.h"
#include <memory>
#include <glm/glm.hpp>
#include "Camera.h"
#include "Ray.h"
#include "Scene.h"


class Renderer
{
public:
	struct Settings
	{
		bool Accumalate = true;
		bool SlowRandom = true;
	};
public:
	Renderer() = default;
	void OnResize(uint32_t width, uint32_t height);
	void Render(const Scene& scene, const Camera&  camera);
	void SetLightPosition(glm::vec3 pos);
	void ResetFrameIndex() { m_FrameIndex = 1; }
	Settings& GetSettings() { return m_Settings; }

	std::shared_ptr<Walnut::Image> GetFinalImage() const { return m_FinalImage; }
private:
	struct HitPayload
	{
		float HitDistance;
		glm::vec3 WorldPosition;
		glm::vec3 WorldNormal;

		int32_t ObjectIndex;
	};

	glm::vec4 PerPixel(uint32_t x, uint32_t y);
	HitPayload TraceRay(const Ray& ray);
	HitPayload ClosestHit(const Ray& ray, float hitDistance, int objectIndex);
	HitPayload Miss(const Ray& ray);
private:
	const Scene* m_Scene;
	const Camera* m_Camera;

	std::vector<uint32_t> m_HorizontalIterator, m_VerticalIterator;

	Settings m_Settings;
	std::shared_ptr<Walnut::Image> m_FinalImage;
	uint32_t* m_ImageData = nullptr;
	glm::vec4* m_AccumulationData = nullptr;
	glm::vec3 m_LightPos = {3.0f,3.0f,3.0f};

	uint32_t m_FrameIndex = 1;
};
