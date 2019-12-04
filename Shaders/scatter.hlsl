#pragma kernel SkyboxLUT
#pragma kernel InscatteringLUT
#pragma kernel ParticleDensityLUT
#pragma kernel AmbientLightLUT
#pragma kernel DirectLightLUT
#pragma kernel LightShaftLUT

#pragma static_sampler ShadowSampler maxAnisotropy=0 maxLod=0 addressMode=clamp_border borderColor=float_opaque_white compareOp=less
#pragma static_sampler PointClampSampler maxAnisotropy=0 maxLod=0 addressMode=clamp_edge filter=nearest
#pragma static_sampler LinearClampSampler maxAnisotropy=0 maxLod=0 addressMode=clamp_edge filter=linear

#include "include/shadercompat.h"

[[vk::binding(0, 0)]] RWTexture3D<float4> _SkyboxLUT : register(u0);
[[vk::binding(1, 0)]] RWTexture3D<float2> _SkyboxLUT2 : register(u1);

[[vk::binding(2, 0)]] RWTexture3D<float4> _InscatteringLUT : register(u2);
[[vk::binding(3, 0)]] RWTexture3D<float4> _ExtinctionLUT : register(u3);

[[vk::binding(4, 0)]] RWTexture2D<float> _LightShaftLUT : register(u4);

[[vk::binding(5, 0)]] RWTexture2D<float2> _RWParticleDensityLUT : register(u5);
[[vk::binding(6, 0)]] RWStructuredBuffer<float4> _RWAmbientLightLUT : register(u6);
[[vk::binding(7, 0)]] RWStructuredBuffer<float4> _RWDirectLightLUT : register(u7);

[[vk::binding(8, 0)]] RWTexture2D<float4> _RandomVectors : register(u8);
[[vk::binding(9, 0)]] Texture2D<float2> _ParticleDensityLUT : register(t0);

[[vk::binding(10, 0)]] StructuredBuffer<GPULight> Lights : register(t1);
[[vk::binding(11, 0)]] Texture2D<float> ShadowAtlas : register(t2);
[[vk::binding(12, 0)]] Texture2D<float> DepthTexture : register(t3);
[[vk::binding(13, 0)]] StructuredBuffer<ShadowData> Shadows : register(t4);

[[vk::binding(14, 0)]] SamplerComparisonState ShadowSampler : register(s0);
[[vk::binding(15, 0)]] SamplerState PointClampSampler : register(s1);
[[vk::binding(16, 0)]] SamplerState LinearClampSampler : register(s2);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4 _DensityScaleHeight;
	float4 _ScatteringR;
	float4 _ScatteringM;
	float4 _ExtinctionR;
	float4 _ExtinctionM;

	float4 _BottomLeftCorner;
	float4 _BottomRightCorner;
	float4 _TopLeftCorner;
	float4 _TopRightCorner;

	float3 _LightDir;
	float3 _CameraPos;

	float4 _IncomingLight;

	float _SunIntensity;
	float _MieG;

	float _DistanceScale;

	float _AtmosphereHeight;
	float _PlanetRadius;
}

#include "include/shadow.hlsli"

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
	float height = length(position - planetCenter) - _PlanetRadius;
	localDensity = exp(-height.xx / _DensityScaleHeight.xy);
	float cosAngle = dot(normalize(position - planetCenter), lightDir.xyz);
	densityToAtmTop = _ParticleDensityLUT.SampleLevel(PointClampSampler, float2(cosAngle * 0.5 + 0.5, height / _AtmosphereHeight), 0).xy;
}

void ComputeLocalInscattering(float2 localDensity, float2 densityPA, float2 densityCP, out float3 localInscatterR, out float3 localInscatterM) {
	float2 densityCPA = densityCP + densityPA;

	float3 Tr = densityCPA.x * _ExtinctionR;
	float3 Tm = densityCPA.y * _ExtinctionM;

	float3 extinction = exp(-(Tr + Tm));

	localInscatterR = localDensity.x * extinction;
	localInscatterM = localDensity.y * extinction;
}

void ApplyPhaseFunction(inout float3 scatterR, inout float3 scatterM, float cosAngle) {
	// r
	float phase = (3.0 / (16.0 * PI)) * (1 + (cosAngle * cosAngle));
	scatterR *= phase;

	// m
	float g = _MieG;
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
	float3 Tr = density.x * _ExtinctionR;
	float3 Tm = density.y * _ExtinctionM;

	float3 extinction = exp(-(Tr + Tm));

	return _IncomingLight.xyz * extinction;
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
	float3 lightInscatter = (scatterR * _ScatteringR + scatterM * _ScatteringM) * _IncomingLight.xyz;
	lightInscatter += RenderSun(m, dot(rayDir, -lightDir.xyz)) * sunintensity;
	float3 lightExtinction = exp(-(densityCP.x * _ExtinctionR + densityCP.y * _ExtinctionM));

	extinction = float4(lightExtinction, 0);
	return float4(lightInscatter, 1);
}

[numthreads(4, 4, 4)]
void SkyboxLUT(uint3 id : SV_DispatchThreadID) {
	float w, h, d;
	_SkyboxLUT.GetDimensions(w, h, d);
	float3 coords = float3(id.x / (w - 1), id.y / (h - 1), id.z / (d - 1));
	
	float height = coords.x * coords.x * _AtmosphereHeight;
	float ch = -(sqrt(height * (2 * _PlanetRadius + height)) / (_PlanetRadius + height));

	float viewZenithAngle = coords.y;

	if (viewZenithAngle > 0.5)
		viewZenithAngle = ch + pow((viewZenithAngle - 0.5) * 2, 5) * (1 - ch);
	else
		viewZenithAngle = ch - pow(viewZenithAngle * 2, 5) * (1 + ch);

	float sunZenithAngle = (tan((2 * coords.z - 1 + 0.26) * 0.75)) / (tan(1.26 * 0.75));// coords.z * 2.0 - 1.0;

	float3 planetCenter = float3(0, -_PlanetRadius, 0);
	float3 rayStart = float3(0, height, 0);

	float3 rayDir   = float3(sqrt(saturate(1 - viewZenithAngle * viewZenithAngle)), viewZenithAngle, 0);
	float3 lightDir = float3(sqrt(saturate(1 - sunZenithAngle * sunZenithAngle)), sunZenithAngle, 0);

	float rayLength = 1e20;
	float2 intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, _PlanetRadius + _AtmosphereHeight);
	rayLength = intersection.y;

	intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, _PlanetRadius);
	if (intersection.x > 0)
		rayLength = min(rayLength, intersection.x);

	float3 rayleigh;
	float3 mie;
	IntegrateInscattering(rayStart, rayDir, rayLength, planetCenter, lightDir, rayleigh, mie);

	_SkyboxLUT[id.xyz] = float4(rayleigh, mie.x);
	_SkyboxLUT2[id.xyz] = mie.yz;
}

[numthreads(8, 8, 1)]
void InscatteringLUT(uint3 id : SV_DispatchThreadID) {
	uint w, h, d;
	_InscatteringLUT.GetDimensions(w, h, d);
	float2 uv = float2(id.x / float(w - 1), id.y / float(h - 1));

	float3 v1 = lerp(_BottomLeftCorner.xyz, _BottomRightCorner.xyz, uv.x);
	float3 v2 = lerp(_TopLeftCorner.xyz, _TopRightCorner.xyz, uv.x);

	float3 rayEnd = lerp(v1, v2, uv.y);
	float3 rayStart = _CameraPos;

	float3 rayDir = rayEnd - rayStart;
	float rayLength = length(rayDir);
	rayDir /= rayLength;

	float3 planetCenter = float3(0, -_PlanetRadius, 0);
	float3 lightDir = normalize(_LightDir);
	uint3 coords = id;
	uint sampleCount = d;
	
	float3 step = rayDir * (rayLength / (float)(sampleCount - 1));
	float stepSize = length(step) * _DistanceScale;

	float2 densityCP = 0;
	float3 scatterR = 0;
	float3 scatterM = 0;

	float2 localDensity;
	float2 densityPA;

	float2 prevLocalDensity;
	float3 prevLocalInscatterR, prevLocalInscatterM;
	GetAtmosphereDensity(rayStart, planetCenter, lightDir, prevLocalDensity, densityPA);
	ComputeLocalInscattering(prevLocalDensity, densityPA, densityCP, prevLocalInscatterR, prevLocalInscatterM);

	_InscatteringLUT[coords] = float4(0, 0, 0, 1);
	_ExtinctionLUT[coords] = float4(1, 1, 1, 1);

	// P - current integration point
	// C - camera position
	// A - top of the atmosphere
	for (coords.z = 1; coords.z < sampleCount; coords.z += 1) {
		float3 p = rayStart + step * coords.z;

		GetAtmosphereDensity(p, planetCenter, lightDir, localDensity, densityPA);
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

		ApplyPhaseFunction(currentScatterR, currentScatterM, saturate(dot(rayDir, lightDir)));
		float3 lightInscatter = (currentScatterR * _ScatteringR + currentScatterM * _ScatteringM) * _IncomingLight.xyz;
		float3 lightExtinction = exp(-(densityCP.x * _ExtinctionR + densityCP.y * _ExtinctionM));

		_InscatteringLUT[coords] = float4(lightInscatter, 1);
		_ExtinctionLUT[coords] = float4(lightExtinction, 1);
	}
}

[numthreads(8, 8, 1)]
void ParticleDensityLUT(uint3 id : SV_DispatchThreadID) {
	float w, h;
	_RWParticleDensityLUT.GetDimensions(w, h);
	float2 uv = float2(id.x / (w - 1), id.y / (h - 1));

	float cosAngle = uv.x * 2 - 1;
	float sinAngle = sqrt(saturate(1 - cosAngle * cosAngle));
	float startHeight = _AtmosphereHeight * uv.y;

	float3 rayStart = float3(0, startHeight, 0);
	float3 rayDir = float3(sinAngle, cosAngle, 0);
	float3 planetCenter = float3(0, -_PlanetRadius, 0);

	float stepCount = 250;

	float2 intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, _PlanetRadius);
	if (intersection.x > 0){
		// intersection with planet, write high density
		_RWParticleDensityLUT[id.xy] = 1e20;
		return;
	}

	intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, _PlanetRadius + _AtmosphereHeight);
	float3 rayEnd = rayStart + rayDir * intersection.y;

	// compute density along the ray
	float3 step = (rayEnd - rayStart) / stepCount;
	float stepSize = length(step);
	float2 density = 0;

	for (float s = 0.5; s < stepCount; s += 1.0) {
		float3 position = rayStart + step * s;
		float height = abs(length(position - planetCenter) - _PlanetRadius);
		float2 localDensity = exp(-(height.xx / _DensityScaleHeight));

		density += localDensity * stepSize;
	}

	_RWParticleDensityLUT[id.xy] = density;
}

[numthreads(64, 1, 1)]
void AmbientLightLUT(uint3 id : SV_DispatchThreadID) {
	float cosAngle = id.x / 128.0 * 1.1 - 0.1;
	float sinAngle = sqrt(saturate(1 - cosAngle * cosAngle));
	float3 lightDir = normalize(float3(sinAngle, cosAngle, 0));
	float startHeight = 0;
	float3 rayStart = float3(0, startHeight, 0);
	float3 planetCenter = float3(0, -_PlanetRadius + startHeight, 0);

	float4 color = 0;

	for (int ii = 0; ii < 255; ++ii) {
		float3 rayDir = normalize(_RandomVectors[uint2(ii % 16, ii / 16)].xyz * 2 - 1);
		rayDir.y = abs(rayDir.y);

		float2 intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, _PlanetRadius + _AtmosphereHeight);
		float rayLength = intersection.y;

		intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, _PlanetRadius);
		if (intersection.x > 0)
			rayLength = min(rayLength, intersection.x);

		float4 extinction;
		float4 scattering = IntegrateInscattering(rayStart, rayDir, rayLength, planetCenter, 1, lightDir, 32, extinction, _SunIntensity);
		
		color.rgb += scattering* dot(rayDir, float3(0, 1, 0));
	}

	_RWAmbientLightLUT[id.x] = color * 2 * PI / 255;
}

[numthreads(64, 1, 1)]
void DirectLightLUT(uint3 id : SV_DispatchThreadID) {
	float cosAngle = id.x / 128.0 * 1.1 - 0.1;
	float sinAngle = sqrt(saturate(1 - cosAngle * cosAngle));
	float3 rayDir = normalize(float3(sinAngle, cosAngle, 0));

	float startHeight = 500;

	float3 rayStart = float3(0, startHeight, 0);

	float3 planetCenter = float3(0, -_PlanetRadius + startHeight, 0);

	float2 localDensity;
	float2 densityToAtmosphereTop;

	GetAtmosphereDensity(rayStart, planetCenter, rayDir, localDensity, densityToAtmosphereTop);
	float4 color;
	color.xyz = ComputeOpticalDepth(densityToAtmosphereTop);
	color.w = 1;

	_RWDirectLightLUT[id.x] = color;
}

[numthreads(8, 8, 1)]
void LightShaftLUT(uint3 id : SV_DispatchThreadID) {
	uint w, h;
	_LightShaftLUT.GetDimensions(w, h);
	float2 uv = float2(id.x / float(w - 1), id.y / float(h - 1));

	float3 v1 = lerp(_BottomLeftCorner.xyz, _BottomRightCorner.xyz, uv.x);
	float3 v2 = lerp(_TopLeftCorner.xyz, _TopRightCorner.xyz, uv.x);

	float2 screenUV = uv;
	screenUV.y = 1 - screenUV.y;
	float maxDepth = 0;
	maxDepth += DepthTexture.SampleLevel(LinearClampSampler, screenUV, 0);
	maxDepth += DepthTexture.SampleLevel(LinearClampSampler, screenUV, 0, int2(1,0));
	maxDepth += DepthTexture.SampleLevel(LinearClampSampler, screenUV, 0, int2(0,1));
	maxDepth += DepthTexture.SampleLevel(LinearClampSampler, screenUV, 0, int2(1,1));
	maxDepth /= 4;

	float3 rayEnd = lerp(v1, v2, uv.y);
	rayEnd *= maxDepth;
	
	const uint SampleCount = 512;

	float attenuation = 0;
	
	for (uint i = 0; i < SampleCount; i++) {
		float depth = i / ((float)SampleCount - 1.0);
		depth *= depth; // bias samples towards 0
		float3 worldPos = rayEnd * depth;
		attenuation += 1 - SampleShadow(Lights[0], _CameraPos, worldPos, depth);
	}

	attenuation /= SampleCount;
	attenuation = 1 - attenuation;

	attenuation* attenuation;

	_LightShaftLUT[id.xy] = attenuation * attenuation;
}