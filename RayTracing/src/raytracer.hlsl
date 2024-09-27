
struct SphereData
{
	float4 Position;
	float Radius;
	int MaterialIndex;
};

[[vk::binding(0, 0)]] RWStructuredBuffer<SphereData> InBuffer;
[[vk::binding(1, 0)]] RWStructuredBuffer<float> OutBuffer;


struct HitPayload
{
	float HitDistance;
	float3 WorldPosition;
    float3 WorldNormal;
    uint ObjectIndex;
};

struct Ray
{
	float3 Origin;
	float3 Direction;
};

[numthreads(1, 1, 1)]
void Main(uint3 DTid : SV_DispatchThreadID)
{
    OutBuffer[DTid.x] = InBuffer[DTid.x].Radius;
}


// float4 PerPixel(uint x, uint y)
// {
// 	Ray ray;
// 	// ray.Origin = m_Camera->GetPosition();
// 	// ray.Direction = m_Camera->GetRayDirections()[x + y * m_FinalImage->GetWidth()];

// 	uint seed = x + y * m_FinalImage->GetWidth();
// 	seed *= m_FrameIndex;

// 	int bounce = 5;
// 	float3 light{0.0f};

// 	float3 contribution{1.0f};
// 	for (int i = 0; i < bounce; i++)
// 	{
// 		seed += i;

// 		HitPayload payload = TraceRay(ray);

// 		if (payload.HitDistance < 0.0f)
// 		{
// 			float3 skyColor = float3(0.06, 0.7f, 0.9f);
// 			//light += skyColor * contribution;
// 			return float4(light, 1.0f);
// 		}

// 		const Sphere& sphere = m_Scene->Spheres[payload.ObjectIndex];
// 		const Material& mat = m_Scene->Materials[sphere.MaterialIndex];

// 		//light += mat.Albedo * contribution;
// 		contribution *= mat.Albedo;
// 		light += mat.GetEmission();

// 		ray.Origin = payload.WorldPosition + payload.WorldNormal * 0.0001f;

// 		if (m_Settings.SlowRandom)
// 		{
// 			ray.Direction = normalize(payload.WorldNormal + Walnut::Random::InUnitSphere());
// 		}
// 		else {
// 			ray.Direction = normalize(payload.WorldNormal + Utils::InUnitSphere(seed));

// 		}
// 	}



// 	return float4(light, 1.0f);
// }

// HitPayload ClosestHit(inout Ray ray, float hitDistance, int objectIndex)
// {
// 	HitPayload payload;
// 	payload.HitDistance = hitDistance;
// 	payload.ObjectIndex = objectIndex;

// 	Sphere closestSphere = InBuffer[objectIndex];

// 	float3 origin = ray.Origin - closestSphere.Position;
// 	payload.WorldPosition = origin + ray.Direction * hitDistance;
// 	payload.WorldNormal = normalize(payload.WorldPosition);

// 	payload.WorldPosition += closestSphere.Position;

// 	return  payload;
// }

// HitPayload Miss(inout Ray ray)
// {
// 	HitPayload payload;
// 	payload.HitDistance = -1.0f;
// 	return payload;
// }

// HitPayload TraceRay(inout Ray ray)
// {
// 	float3 rayOrigin = ray.Origin;

// 	int closestSphere = -1;
// 	float hitDistance = 1.0 / 0.0;

// 	float3 rayDirection = ray.Direction;
// 	float a = dot(rayDirection, rayDirection);

//     uint numStructs, stride;
//     InBuffer.GetDimensions(numStructs, stride);

// 	for (int i = 0 ; i < numStructs; i++)
// 	{
// 		Sphere sphere = InBuffer[i];
// 		float3 origin = rayOrigin - sphere.Position;

// 		float b = 2.0f * dot(origin, rayDirection);
// 		float c = dot(origin, origin) - sphere.Radius * sphere.Radius;

// 		float discrim = b * b - 4.0f * a * c;

// 		if (discrim < 0.0f)
// 		{
// 			continue;
// 		}

// 		float discrimRoot = sqrt(discrim);
// 		float t2 = (-b - discrimRoot) / 2 / a;

// 		if (t2 < 0)
// 		{
// 			continue;
// 		}

// 		float3 p2 = origin + rayDirection * t2;

// 		if (t2 < hitDistance)
// 		{
// 			hitDistance = t2;
// 			closestSphere = i;
// 		}
// 	}

// 	if (closestSphere == -1)
// 	{
// 		return Miss(ray);
// 	}

// 	return ClosestHit(ray, hitDistance, closestSphere);
// }