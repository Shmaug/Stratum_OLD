#pragma kernel ComputeSkyboxLUT
#pragma kernel ComputeInscatteringLUT
#pragma kernel ComputeParticleDensityLUT
#pragma kernel ComputeAmbientLightLUT
#pragma kernel ComputeDirectLightLUT

#pragma staticsampler ShadowSampler maxAnisotropy=0 maxLod=0 addressMode=clampborder borderColor=floatopaquewhite compareOp=less
#pragma staticsampler PointClampSampler maxAnisotropy=0 maxLod=0 addressMode=clampedge filter=nearest
#pragma staticsampler LinearClampSampler maxAnisotropy=0 maxLod=0 addressMode=clampedge filter=linear
#pragma staticsampler Sampler maxAnisotropy=0 maxLod=0

#include <include/shadercompat.h>

[[vk::binding(0, 0)]] RWTexture3D<float3> SkyboxLUTR : register(u0);
[[vk::binding(1, 0)]] RWTexture3D<float3> SkyboxLUTM : register(u1);

[[vk::binding(2, 0)]] RWTexture3D<float3> InscatteringLUT : register(u2);
[[vk::binding(3, 0)]] RWTexture3D<float3> ExtinctionLUT : register(u3);

[[vk::binding(4, 0)]] RWTexture2D<float> LightShaftLUT : register(u4);

[[vk::binding(5, 0)]] RWTexture2D<float2> RWParticleDensityLUT : register(u5);
[[vk::binding(6, 0)]] RWStructuredBuffer<float4> RWAmbientLightLUT : register(u6);
[[vk::binding(7, 0)]] RWStructuredBuffer<float4> RWDirectLightLUT : register(u7);

[[vk::binding(8, 0)]] RWTexture2D<float4> RandomVectors : register(u8);
[[vk::binding(9, 0)]] Texture2D<float2> ParticleDensityLUT : register(t0);

[[vk::binding(10, 0)]] StructuredBuffer<GPULight> Lights : register(t1);
[[vk::binding(11, 0)]] Texture2D<float> ShadowAtlas : register(t2);
[[vk::binding(12, 0)]] Texture2D<float> DepthTexture : register(t3);
[[vk::binding(13, 0)]] StructuredBuffer<ShadowData> Shadows : register(t4);

[[vk::binding(14, 0)]] SamplerComparisonState ShadowSampler : register(s0);
[[vk::binding(15, 0)]] SamplerState PointClampSampler : register(s1);
[[vk::binding(16, 0)]] SamplerState LinearClampSampler : register(s2);
[[vk::binding(17, 0)]] SamplerState Sampler : register(s2);

[[vk::binding(18, 0)]] cbuffer Inputs : register(b2) {
	float4 DensityScaleHeight;
	float4 ScatteringR;
	float4 ScatteringM;
	float4 ExtinctionR;
	float4 ExtinctionM;

	float4 BottomLeftCorner;
	float4 BottomRightCorner;
	float4 TopLeftCorner;
	float4 TopRightCorner;

	float3 LightDir;
	float SunIntensity;

	float3 CameraPos;
	float MieG;

	float4 IncomingLight;

	float DistanceScale;
	float AtmosphereHeight;
	float PlanetRadius;
	float pad;
}

#include <include/shadow.hlsli>

#define PI 3.14159265359

float2 RaySphereIntersection(float3 ro, float3 rd, float3 p, float r) {
	float3 f = ro - p;
	float a = dot(rd, rd);
	float b = dot(f, rd);
	float3 l = a * f - rd * b;
	float det = a * a * r * r - dot(l, l);
	if (det < 0.0) return -1;
	float ra = 1.0 / a;
	det = sqrt(det * ra);
	return (-b + float2(-det, det)) * ra;
}

void GetAtmosphereDensity(float3 position, float3 planetCenter, float3 lightDir, out float2 localDensity, out float2 densityToAtmTop) {
	float height = length(position - planetCenter) - PlanetRadius;
	localDensity = exp(-height.xx / DensityScaleHeight.xy);
	float cosAngle = dot(normalize(position - planetCenter), lightDir.xyz);
	densityToAtmTop = ParticleDensityLUT.SampleLevel(PointClampSampler, float2(cosAngle * 0.5 + 0.5, height / AtmosphereHeight), 0).xy;
}

void ComputeLocalInscattering(float2 localDensity, float2 densityPA, float2 densityCP, out float3 localInscatterR, out float3 localInscatterM) {
	float2 densityCPA = densityCP + densityPA;

	float3 Tr = densityCPA.x * ExtinctionR;
	float3 Tm = densityCPA.y * ExtinctionM;

	float3 extinction = exp(-(Tr + Tm));

	localInscatterR = localDensity.x * extinction;
	localInscatterM = localDensity.y * extinction;
}

void ApplyPhaseFunction(inout float3 scatterR, inout float3 scatterM, float cosAngle) {
	// r
	float phase = (3.0 / (16.0 * PI)) * (1 + (cosAngle * cosAngle));
	scatterR *= phase;

	// m
	float g = MieG;
	float g2 = g * g;
	phase = (1.0 / (4.0 * PI)) * ((3.0 * (1.0 - g2)) / (2.0 * (2.0 + g2))) * ((1 + cosAngle * cosAngle) / (pow((1 + g2 - 2 * g * cosAngle), 3.0 / 2.0)));
	scatterM *= phase;
}

void IntegrateInscattering(float3 rayStart, float3 rayDir, float rayLength, float3 planetCenter, float3 lightDir, out float3 scatterR, out float3 scatterM) {
	float sampleCount = 64;
	float3 step = rayDir * (rayLength / sampleCount);
	float stepSize = length(step);

	float2 densityCP = 0;
	scatterR = 0;
	scatterM = 0;

	float2 localDensity;
	float2 densityPA;

	float2 prevLocalDensity;
	float3 prevLocalInscatterR, prevLocalInscatterM;
	GetAtmosphereDensity(rayStart, planetCenter, lightDir, prevLocalDensity, densityPA);
	ComputeLocalInscattering(prevLocalDensity, densityPA, densityCP, prevLocalInscatterR, prevLocalInscatterM);

	// P - current integration point
	// C - camera position
	// A - top of the atmosphere
	for (float s = 1.0; s < sampleCount; s += 1) {
		float3 p = rayStart + step * s;

		GetAtmosphereDensity(p, planetCenter, lightDir, localDensity, densityPA);
		densityCP += (localDensity + prevLocalDensity) * (stepSize / 2.0);

		prevLocalDensity = localDensity;

		float3 localInscatterR, localInscatterM;
		ComputeLocalInscattering(localDensity, densityPA, densityCP, localInscatterR, localInscatterM);

		scatterR += (localInscatterR + prevLocalInscatterR) * (stepSize / 2.0);
		scatterM += (localInscatterM + prevLocalInscatterM) * (stepSize / 2.0);

		prevLocalInscatterR = localInscatterR;
		prevLocalInscatterM = localInscatterM;
	}
}

float3 RenderSun(in float3 scatterM, float cosAngle) {
	float g = 0.98;
	float g2 = g * g;

	float sun = (1 - g) * (1 - g) / (4 * PI * pow(1.0 + g2 - 2.0 * g * cosAngle, 1.5));
	return sun * 0.003 * scatterM;// 5;
}

float3 ComputeOpticalDepth(float2 density) {
	float3 Tr = density.x * ExtinctionR;
	float3 Tm = density.y * ExtinctionM;

	float3 extinction = exp(-(Tr + Tm));

	return IncomingLight.xyz * extinction;
}
float4 IntegrateInscattering(float3 rayStart, float3 rayDir, float rayLength, float3 planetCenter, float distanceScale, float3 lightDir, float sampleCount, out float4 extinction, float sunintensity) {
	float3 step = rayDir * (rayLength / sampleCount);
	float stepSize = length(step) * distanceScale;

	float2 densityCP = 0;
	float3 scatterR = 0;
	float3 scatterM = 0;

	float2 localDensity;
	float2 densityPA;

	float2 prevLocalDensity;
	float3 prevLocalInscatterR, prevLocalInscatterM;
	GetAtmosphereDensity(rayStart, planetCenter, lightDir, prevLocalDensity, densityPA);
	ComputeLocalInscattering(prevLocalDensity, densityPA, densityCP, prevLocalInscatterR, prevLocalInscatterM);

	// P - current integration point
	// C - camera position
	// A - top of the atmosphere
	for (float s = 1.0; s < sampleCount; s += 1) {
		float3 p = rayStart + step * s;

		GetAtmosphereDensity(p, planetCenter, lightDir, localDensity, densityPA);
		densityCP += (localDensity + prevLocalDensity) * (stepSize / 2.0);

		prevLocalDensity = localDensity;

		float3 localInscatterR, localInscatterM;
		ComputeLocalInscattering(localDensity, densityPA, densityCP, localInscatterR, localInscatterM);

		scatterR += (localInscatterR + prevLocalInscatterR) * (stepSize / 2.0);
		scatterM += (localInscatterM + prevLocalInscatterM) * (stepSize / 2.0);

		prevLocalInscatterR = localInscatterR;
		prevLocalInscatterM = localInscatterM;
	}

	float3 m = scatterM;
	// phase function
	ApplyPhaseFunction(scatterR, scatterM, dot(rayDir, -lightDir.xyz));
	//scatterR = 0;
	float3 lightInscatter = (scatterR * ScatteringR + scatterM * ScatteringM) * IncomingLight.xyz;
	lightInscatter += RenderSun(m, dot(rayDir, -lightDir.xyz)) * sunintensity;
	float3 lightExtinction = exp(-(densityCP.x * ExtinctionR + densityCP.y * ExtinctionM));

	extinction = float4(lightExtinction, 0);
	return float4(lightInscatter, 1);
}

[numthreads(4, 4, 4)]
void ComputeSkyboxLUT(uint3 id : SVDispatchThreadID) {
	float w, h, d;
	SkyboxLUTR.GetDimensions(w, h, d);
	float3 coords = float3(id.x / (w - 1), id.y / (h - 1), id.z / (d - 1));
	
	float height = coords.x * coords.x * AtmosphereHeight;
	float ch = -(sqrt(height * (2 * PlanetRadius + height)) / (PlanetRadius + height));

	float viewZenithAngle = coords.y;

	if (viewZenithAngle > 0.5)
		viewZenithAngle = ch + pow((viewZenithAngle - 0.5) * 2, 5) * (1 - ch);
	else
		viewZenithAngle = ch - pow(viewZenithAngle * 2, 5) * (1 + ch);

	float sunZenithAngle = (tan((2 * coords.z - 1 + 0.26) * 0.75)) / (tan(1.26 * 0.75));// coords.z * 2.0 - 1.0;

	float3 planetCenter = float3(0, -PlanetRadius, 0);
	float3 rayStart = float3(0, height, 0);

	float3 rayDir   = float3(sqrt(saturate(1 - viewZenithAngle * viewZenithAngle)), viewZenithAngle, 0);
	float3 lightDir = float3(sqrt(saturate(1 - sunZenithAngle * sunZenithAngle)), sunZenithAngle, 0);

	float rayLength = 1e20;
	float2 intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, PlanetRadius + AtmosphereHeight);
	rayLength = intersection.y;

	intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, PlanetRadius);
	if (intersection.x > 0)
		rayLength = min(rayLength, intersection.x);

	float3 rayleigh;
	float3 mie;
	IntegrateInscattering(rayStart, rayDir, rayLength, planetCenter, lightDir, rayleigh, mie);

	SkyboxLUTR[id.xyz] = rayleigh;
	SkyboxLUTM[id.xyz] = mie;
}

[numthreads(8, 8, 1)]
void ComputeInscatteringLUT(uint3 id : SVDispatchThreadID) {
	uint w, h, d;
	InscatteringLUT.GetDimensions(w, h, d);

	uint3 coords = id;
	uint sampleCount = d;

	float2 uv = float2(id.x / float(w - 1), id.y / float(h - 1));
	float3 v1 = lerp(BottomLeftCorner.xyz, BottomRightCorner.xyz, uv.x);
	float3 v2 = lerp(TopLeftCorner.xyz, TopRightCorner.xyz, uv.x);

	float3 rayEnd = lerp(v1, v2, uv.y);
	float3 rayDir = normalize(rayEnd);

	float3 step = rayEnd / (float)(sampleCount - 1);
	float stepSize = length(step) * DistanceScale;

	float3 planetCenter = float3(0, -PlanetRadius, 0) - CameraPos;
	float3 lightDir = normalize(LightDir);
	float rdl = saturate(dot(rayDir, lightDir));
	
	float2 densityCP = 0;
	float3 scatterR = 0;
	float3 scatterM = 0;

	float2 localDensity;
	float2 densityPA;

	float2 prevLocalDensity;
	float3 prevLocalInscatterR, prevLocalInscatterM;
	GetAtmosphereDensity(0, planetCenter, lightDir, prevLocalDensity, densityPA);
	ComputeLocalInscattering(prevLocalDensity, densityPA, densityCP, prevLocalInscatterR, prevLocalInscatterM);

	InscatteringLUT[coords] = 0;
	ExtinctionLUT[coords] = 1;

	// P - current integration point
	// C - camera position
	// A - top of the atmosphere
	for (coords.z = 1; coords.z < sampleCount; coords.z += 1) {
		GetAtmosphereDensity(step * coords.z, planetCenter, lightDir, localDensity, densityPA);
		densityCP += (localDensity + prevLocalDensity) * (stepSize / 2.0);

		prevLocalDensity = localDensity;

		float3 localInscatterR, localInscatterM;
		ComputeLocalInscattering(localDensity, densityPA, densityCP, localInscatterR, localInscatterM);
		
		scatterR += (localInscatterR + prevLocalInscatterR) * (stepSize / 2.0);
		scatterM += (localInscatterM + prevLocalInscatterM) * (stepSize / 2.0);

		prevLocalInscatterR = localInscatterR;
		prevLocalInscatterM = localInscatterM;

		float3 currentScatterR = scatterR;
		float3 currentScatterM = scatterM;

		ApplyPhaseFunction(currentScatterR, currentScatterM, rdl);
		float3 lightInscatter = (currentScatterR * ScatteringR + currentScatterM * ScatteringM) * IncomingLight.xyz;
		float3 lightExtinction = exp(-(densityCP.x * ExtinctionR + densityCP.y * ExtinctionM));

		InscatteringLUT[coords] = lightInscatter;
		ExtinctionLUT[coords] = lightExtinction;
	}
}

[numthreads(8, 8, 1)]
void ComputeParticleDensityLUT(uint3 id : SVDispatchThreadID) {
	float w, h;
	RWParticleDensityLUT.GetDimensions(w, h);
	float2 uv = float2(id.x / (w - 1), id.y / (h - 1));

	float cosAngle = uv.x * 2 - 1;
	float sinAngle = sqrt(saturate(1 - cosAngle * cosAngle));
	float startHeight = AtmosphereHeight * uv.y;

	float3 rayStart = float3(0, startHeight, 0);
	float3 rayDir = float3(sinAngle, cosAngle, 0);
	float3 planetCenter = float3(0, -PlanetRadius, 0);

	float stepCount = 250;

	float2 intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, PlanetRadius);
	if (intersection.x > 0){
		// intersection with planet, write high density
		RWParticleDensityLUT[id.xy] = 1e20;
		return;
	}

	intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, PlanetRadius + AtmosphereHeight);
	float3 rayEnd = rayStart + rayDir * intersection.y;

	// compute density along the ray
	float3 step = (rayEnd - rayStart) / stepCount;
	float stepSize = length(step);
	float2 density = 0;

	for (float s = 0.5; s < stepCount; s += 1.0) {
		float3 position = rayStart + step * s;
		float height = abs(length(position - planetCenter) - PlanetRadius);
		float2 localDensity = exp(-(height.xx / DensityScaleHeight));

		density += localDensity * stepSize;
	}

	RWParticleDensityLUT[id.xy] = density;
}

[numthreads(64, 1, 1)]
void ComputeAmbientLightLUT(uint3 id : SVDispatchThreadID) {
	float cosAngle = id.x / 128.0 * 1.1 - 0.1;
	float sinAngle = sqrt(saturate(1 - cosAngle * cosAngle));
	float3 lightDir = normalize(float3(sinAngle, cosAngle, 0));
	float startHeight = 0;
	float3 rayStart = float3(0, startHeight, 0);
	float3 planetCenter = float3(0, -PlanetRadius + startHeight, 0);

	float4 color = 0;

	for (int ii = 0; ii < 255; ++ii) {
		float3 rayDir = normalize(RandomVectors[uint2(ii % 16, ii / 16)].xyz * 2 - 1);
		rayDir.y = abs(rayDir.y);

		float2 intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, PlanetRadius + AtmosphereHeight);
		float rayLength = intersection.y;

		intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, PlanetRadius);
		if (intersection.x > 0)
			rayLength = min(rayLength, intersection.x);

		float4 extinction;
		float4 scattering = IntegrateInscattering(rayStart, rayDir, rayLength, planetCenter, 1, lightDir, 32, extinction, SunIntensity);
		
		color.rgb += scattering* dot(rayDir, float3(0, 1, 0));
	}

	RWAmbientLightLUT[id.x] = color * 2 * PI / 255;
}

[numthreads(64, 1, 1)]
void ComputeDirectLightLUT(uint3 id : SVDispatchThreadID) {
	float cosAngle = id.x / 128.0 * 1.1 - 0.1;
	float sinAngle = sqrt(saturate(1 - cosAngle * cosAngle));
	float3 rayDir = normalize(float3(sinAngle, cosAngle, 0));

	float startHeight = 500;

	float3 rayStart = float3(0, startHeight, 0);

	float3 planetCenter = float3(0, -PlanetRadius + startHeight, 0);

	float2 localDensity;
	float2 densityToAtmosphereTop;

	GetAtmosphereDensity(rayStart, planetCenter, rayDir, localDensity, densityToAtmosphereTop);
	float4 color;
	color.xyz = ComputeOpticalDepth(densityToAtmosphereTop);
	color.w = 1;

	RWDirectLightLUT[id.x] = color;
}

/*
[numthreads(8, 8, 1)]
void LightShaftLUT(uint3 id : SVDispatchThreadID) {
	if (Lights[0].ShadowIndex < 0 || Lights[0].Type != LIGHTSUN) {
		LightShaftLUT[id.xy] = 1;
		return;
	}

	uint w, h;
	LightShaftLUT.GetDimensions(w, h);
	float2 uv = float2(id.x / float(w - 1), id.y / float(h - 1));

	uv.y = 1 - uv.y;

	float3 v1 = lerp(BottomLeftCorner.xyz, BottomRightCorner.xyz, uv.x);
	float3 v2 = lerp(TopLeftCorner.xyz, TopRightCorner.xyz, uv.x);
	float3 rayEnd = lerp(v1, v2, 1 - uv.y);

	float maxDepth = DepthTexture.Gather(LinearClampSampler, uv) / 4;
	
	float attenuation = 0;
	for (uint i = 0; i < 32; i++) {
		float depth = maxDepth * i / 31.0;
		attenuation += SampleShadow(Lights[0], CameraPos, rayEnd * depth, depth);
	}
	LightShaftLUT[id.xy] = attenuation / 32.0;
}
*/