#pragma once

#include <vector>
#include "glm/glm.hpp"

struct Material
{
	alignas(16)glm::vec3 Albedo{ 1.0f };
	alignas(16) glm::vec3 EmissionColor{ 0.0f };
	float Roughness{ 1.0f };
	float Metalic{ 1.0f };
	float EmissionPower = 0.0f;

	glm::vec3 GetEmission() const
	{
		return EmissionColor * EmissionPower;
	}
};

struct Sphere
{
	alignas(16) glm::vec3 Position{ 0.0f };
	float Radius = 0.5f;
	
	int MaterialIndex = 0;
};

struct SphereComputeData
{
	alignas(16) glm::vec3 Position{ 0.0f };
	float Radius = 0.5f;
	int MaterialIndex = 0;
};

struct Scene
{
	std::vector<Sphere> Spheres;
	std::vector<Material> Materials;
};