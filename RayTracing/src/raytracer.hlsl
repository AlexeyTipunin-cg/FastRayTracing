
struct SphereData
{
	float3 Position;
	float Radius;
	int MaterialIndex;
};

struct Material
{
	float3 Albedo;
	float3 EmissionColor;
	float Roughness;
	float Metalic;
	float EmissionPower;

	float3 GetEmission()
	{
		return EmissionColor * EmissionPower;
	}
};

struct SceneData
{
	float ScreenWidth;
    float FrameIndex;
	float3 CameraPosition;
	SphereData Spheres[3];
	Material Materials[3];
};

[[vk::binding(0, 0)]] RWStructuredBuffer<SceneData> InBuffer;
[[vk::binding(1, 0)]] RWStructuredBuffer<uint> OutBuffer;
[[vk::binding(2, 0)]] RWStructuredBuffer<float3> CameraDirections;
[[vk::binding(3, 0)]] RWStructuredBuffer<float4> AccumulatedColor;


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

uint PCG_Hash(uint input)
{
	uint state = input * 747796405u + 2891336453u;
	uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	return (word >> 22u) ^ word;
}

float RandomFloat(inout uint seed) 
{
	seed = PCG_Hash(seed);
	return (float)seed / 10000000000;
}

float3 InUnitSphere(inout uint seed)
{
	return normalize(float3(RandomFloat(seed) * 2.0f - 1.0f, RandomFloat(seed) * 2.0f - 1.0f, RandomFloat(seed) * 2.0f - 1.0f));
}

HitPayload ClosestHit(inout Ray ray, float hitDistance, int objectIndex)
{
	HitPayload payload;
	payload.HitDistance = hitDistance;
	payload.ObjectIndex = objectIndex;

	SphereData closestSphere = InBuffer[0].Spheres[objectIndex];

	float3 origin = ray.Origin - closestSphere.Position;
	payload.WorldPosition = origin + ray.Direction * hitDistance;
	payload.WorldNormal = normalize(payload.WorldPosition);

	payload.WorldPosition += closestSphere.Position;

	return  payload;
}

HitPayload Miss(inout Ray ray)
{
	HitPayload payload;
	payload.HitDistance = -1.0f;
	return payload;
}

HitPayload TraceRay(inout Ray ray)
{
	float3 rayOrigin = ray.Origin;

	int closestSphere = -1;
	float hitDistance = 10000000000;

	float3 rayDirection = ray.Direction;
	float a = dot(rayDirection, rayDirection);

    uint numStructs, stride;
    // InBuffer[0].Spheres.GetDimensions(numStructs, stride);

	for (int i = 0 ; i < 3; i++)
	{
		SphereData sphere = InBuffer[0].Spheres[i];
		float3 origin = rayOrigin - sphere.Position;

		float b = 2.0f * dot(origin, rayDirection);
		float c = dot(origin, origin) - sphere.Radius * sphere.Radius;

		float discrim = b * b - 4.0f * a * c;

		if (discrim < 0.0f)
		{
			continue;
		}

		float discrimRoot = sqrt(discrim);
		float t2 = (-b - discrimRoot) / 2 / a;

		if (t2 < 0)
		{
			continue;
		}

		float3 p2 = origin + rayDirection * t2;

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


float4 PerPixel(uint x, uint y)
{
	Ray ray;
	ray.Origin = InBuffer[0].CameraPosition;
	ray.Direction = CameraDirections[x + y * InBuffer[0].ScreenWidth];

	uint seed = x + y * InBuffer[0].ScreenWidth;
	seed *= InBuffer[0].FrameIndex;

	int bounce = 5;
	float3 light = float3(0.0f, 0.0f, 0.0f);

	float3 contribution = float3(1.0f, 1.0f, 1.0f);
	for (int i = 0; i < bounce; i++)
	{
		seed += i;

		HitPayload payload = TraceRay(ray);

		if (payload.HitDistance < 0.0f)
		{
			float3 skyColor = float3(0.06, 0.7f, 0.9f);
			//light += skyColor * contribution;
			return float4(light, 1.0f);
		}

		SphereData sphere = InBuffer[0].Spheres[payload.ObjectIndex];
		Material mat = InBuffer[0].Materials[sphere.MaterialIndex];

		//light += mat.Albedo * contribution;
		contribution *= mat.Albedo;
		light += mat.GetEmission();

		ray.Origin = payload.WorldPosition + payload.WorldNormal * 0.0001f;

		ray.Direction = normalize(payload.WorldNormal + InUnitSphere(seed));
	}



	return float4(light, 1.0f);
}

uint ConvertToRGBA(float4 color) {
	uint r = uint(255.0f * color.r);
	uint g = uint(255.0f * color.g);
	uint b = uint(255.0f * color.b);
	uint a = uint(255 * color.a);
	return (a << 24) | (b << 16) | (g << 8) | r;
}

[numthreads(2, 1, 1)]
void Main(uint3 DTid : SV_DispatchThreadID)
{
    // OutBuffer[DTid.x + DTid.y * InBuffer[0].ScreenWidth] = PerPixel(DTid.x, DTid.y);
	float4 color =  PerPixel(DTid.x, DTid.y);
	AccumulatedColor[DTid.x + DTid.y * InBuffer[0].ScreenWidth] += color;
	float4 sampledColor = AccumulatedColor[DTid.x + DTid.y * InBuffer[0].ScreenWidth] / (float)InBuffer[0].FrameIndex;
	sampledColor = clamp(sampledColor, float4(0.0f, 0.0f, 0.0f, 0.0f), float4(1.0f, 1.0f, 1.0f, 1.0f));
	OutBuffer[DTid.x + DTid.y * InBuffer[0].ScreenWidth] = ConvertToRGBA(sampledColor);
	// OutBuffer[DTid.x + DTid.y * InBuffer[0].ScreenWidth] = float4(InBuffer[0].Materials[2].GetEmission(), ((float)1.0 / (float)0.0);
}
