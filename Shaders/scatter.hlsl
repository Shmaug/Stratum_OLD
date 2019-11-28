#pragma kernel SkyboxLUT
#pragma kernel InscatteringLUT
#pragma kernel ParticleDensityLUT
#pragma kernel AmbientLightLUT
#pragma kernel DirectLightLUT

#pragma static_sampler PointClampSampler maxAnisotropy=0 maxLod=0 addressMode=clamp_edge filter=nearest
#pragma static_sampler LinearClampSampler maxAnisotropy=0 maxLod=0 addressMode=clamp_edge filter=linear

[[vk::binding(0, 0)]] RWTexture3D<float4> _SkyboxLUT : register(u0);
[[vk::binding(1, 0)]] RWTexture3D<float2> _SkyboxLUT2 : register(u1);

[[vk::binding(2, 0)]] RWTexture3D<float4> _InscatteringLUT : register(u2);
[[vk::binding(3, 0)]] RWTexture3D<float4> _ExtinctionLUT : register(u3);

[[vk::binding(4, 0)]] RWTexture2D<float2> _RWParticleDensityLUT : register(u4);
[[vk::binding(5, 0)]] RWTexture2D<float4> _RWAmbientLightLUT : register(u5);
[[vk::binding(6, 0)]] RWTexture2D<float4> _RWDirectLightLUT : register(u6);

[[vk::binding(7, 0)]] RWTexture2D<float4> _RandomVectors : register(u7);
[[vk::binding(8, 0)]] Texture2D<float2> _ParticleDensityLUT : register(t0);

[[vk::binding(9, 0)]] SamplerState PointClampSampler : register(s0);
[[vk::binding(10, 0)]] SamplerState LinearClampSampler : register(s1);

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

#define PI 3.14159265359

float2 RaySphereIntersection(float3 ro, float3 rd, float3 p, float r) {
	float3 f = ro - p;
	float r2 = r * r;

	float a = dot(rd, rd);
	float b = dot(f, rd);

	float a2 = a * a;

	float3 l = f * a - rd * b;
	float a2r2 = a2 * r2;
	float ll = dot(l, l);
	if (a2r2 < ll) return -1.0;

	float det = a2r2 - ll;
	float rcpa = 1.0 / a;
	det = sqrt(det * rcpa);
	return (-b - det, -b + det) * rcpa;
}

void GetAtmosphereDensity(float3 position, float3 planetCenter, float3 lightDir, out float2 localDensity, out float2 densityToAtmTop) {
	float height = length(position - planetCenter) - _PlanetRadius;
	localDensity = exp(-height.xx / _DensityScaleHeight.xy);
	float cosAngle = dot(normalize(position - planetCenter), -lightDir.xyz);
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

void IntegrateInscattering(float3 rayStart, float3 rayDir, float rayLength, float3 planetCenter, float3 lightDir, out float scatterR, out float scatterM) {
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

void PrecomputeLightScattering(float3 rayStart, float3 rayDir, float rayLength, float3 planetCenter, float3 lightDir, uint3 coords, uint sampleCount) {
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

		ApplyPhaseFunction(currentScatterR, currentScatterM, dot(rayDir, -lightDir.xyz));
		float3 lightInscatter = (currentScatterR * _ScatteringR + currentScatterM * _ScatteringM) * _IncomingLight.xyz;
		float3 lightExtinction = exp(-(densityCP.x * _ExtinctionR + densityCP.y * _ExtinctionM));

		_InscatteringLUT[coords] = float4(lightInscatter, 1);
		_ExtinctionLUT[coords] = float4(lightExtinction, 1);
	}
}
float2 PrecomputeParticleDensity(float3 rayStart, float3 rayDir) {
	float3 planetCenter = float3(0, -_PlanetRadius, 0);

	float stepCount = 250;

	float2 intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, _PlanetRadius);
	if (intersection.x > 0)
		// intersection with planet, write high density
		return 1e20;

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

	return density;
}
float4 PrecomputeAmbientLight(float3 lightDir) {
	float startHeight = 0;
	float3 rayStart = float3(0, startHeight, 0);
	float3 planetCenter = float3(0, -_PlanetRadius + startHeight, 0);

	float4 color = 0;

	int sampleCount = 255;

	for (int ii = 0; ii < sampleCount; ++ii) {
		float3 rayDir = _RandomVectors[uint2(ii % 16, ii / 16)];
		rayDir.y = abs(rayDir.y);

		float2 intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, _PlanetRadius + _AtmosphereHeight);
		float rayLength = intersection.y;

		intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, _PlanetRadius);
		if (intersection.x > 0)
			rayLength = min(rayLength, intersection.x);

		float sampleCount = 32;
		float4 extinction;

		float4 scattering = (IntegrateInscattering(rayStart, rayDir, rayLength, planetCenter, 1, lightDir, sampleCount, extinction, _SunIntensity));

		color += scattering * dot(rayDir, float3(0, 1, 0));
	}

	return color * 2 * PI / sampleCount;
}
float4 PrecomputeDirectLight(float3 rayDir) {
	float startHeight = 500;

	float3 rayStart = float3(0, startHeight, 0);

	float3 planetCenter = float3(0, -_PlanetRadius + startHeight, 0);

	float2 localDensity;
	float2 densityToAtmosphereTop;

	GetAtmosphereDensity(rayStart, planetCenter, -rayDir, localDensity, densityToAtmosphereTop);
	float4 color;
	color.xyz = ComputeOpticalDepth(densityToAtmosphereTop);
	color.w = 1;
	return color;
}

[numthreads(1, 1, 1)]
void SkyboxLUT(uint3 id : SV_DispatchThreadID) {
	float w, h, d;
	_SkyboxLUT.GetDimensions(w, h, d);

	// linear parameters
	float3 coords = float3(id.x / (w - 1), id.y / (h - 1), id.z / (d - 1));

	float height = coords.x * coords.x * _AtmosphereHeight;
	//float height = coords.x * _AtmosphereHeight;
	float ch = -(sqrt(height * (2 * _PlanetRadius + height)) / (_PlanetRadius + height));

	float viewZenithAngle = coords.y;
	//float viewZenithAngle = coords.y * 2.0 - 1.0;

	if (viewZenithAngle > 0.5)
		viewZenithAngle = ch + pow((viewZenithAngle - 0.5) * 2, 5) * (1 - ch);
	else
		viewZenithAngle = ch - pow(viewZenithAngle * 2, 5) * (1 + ch);

	float sunZenithAngle = (tan((2 * coords.z - 1 + 0.26) * 0.75)) / (tan(1.26 * 0.75));// coords.z * 2.0 - 1.0;
	//float sunZenithAngle = coords.z * 2.0 - 1.0;

	float3 planetCenter = float3(0, -_PlanetRadius, 0);
	float3 rayStart = float3(0, height, 0);

	float3 rayDir = float3(sqrt(saturate(1 - viewZenithAngle * viewZenithAngle)), viewZenithAngle, 0);
	float3 lightDir = -float3(sqrt(saturate(1 - sunZenithAngle * sunZenithAngle)), sunZenithAngle, 0);

	float rayLength = 1e20;
	float2 intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, _PlanetRadius + _AtmosphereHeight);
	rayLength = intersection.y;

	intersection = RaySphereIntersection(rayStart, rayDir, planetCenter, _PlanetRadius);
	if (intersection.x > 0)
		rayLength = min(rayLength, intersection.x);

	float4 rayleigh;
	float4 mie;
	IntegrateInscattering(rayStart, rayDir, rayLength, planetCenter, lightDir, rayleigh, mie);

	//color.inscattering.z = coords.z;
	_SkyboxLUT[id.xyz] = float4(rayleigh.xyz, mie.x);
	_SkyboxLUT2[id.xyz] = mie.yz;
}

[numthreads(1, 1, 1)]
void InscatteringLUT(uint3 id : SV_DispatchThreadID) {
	float w, h, d;
	_InscatteringLUT.GetDimensions(w, h, d);

	float2 coords = float2(id.x / (w - 1), id.y / (h - 1));

	float3 v1 = lerp(_BottomLeftCorner.xyz, _BottomRightCorner.xyz, coords.x);
	float3 v2 = lerp(_TopLeftCorner.xyz, _TopRightCorner.xyz, coords.x);

	float3 rayEnd = lerp(v1, v2, coords.y);
	float3 rayStart = _CameraPos;

	float3 rayDir = rayEnd - rayStart;
	float rayLength = length(rayDir);
	rayDir /= rayLength;

	float3 planetCenter = float3(0, -_PlanetRadius, 0);
	PrecomputeLightScattering(rayStart, rayDir, rayLength, planetCenter, normalize(_LightDir), id, d);
}

[numthreads(1, 1, 1)]
void ParticleDensityLUT(uint3 id : SV_DispatchThreadID) {
	float w, h;
	_RWParticleDensityLUT.GetDimensions(w, h);

	float2 uv = float2(id.x / (w - 1), id.y / (h - 1));

	float cosAngle = uv.x * 2 - 1;
	float sinAngle = sqrt(saturate(1 - cosAngle * cosAngle));
	float startHeight = _AtmosphereHeight * uv.y;

	float3 rayStart = float3(0, startHeight, 0);
	float3 rayDir = float3(sinAngle, cosAngle, 0);

	_RWParticleDensityLUT[id.xy] = PrecomputeParticleDensity(rayStart, rayDir);
}

[numthreads(1, 1, 1)]
void AmbientLightLUT(uint3 id : SV_DispatchThreadID) {
	float w, h;
	_RWAmbientLightLUT.GetDimensions(w, h);

	float2 uv = float2(id.x / (w - 1), id.y / (h - 1));

	float cosAngle = uv.x * 1.1 - 0.1;// *2.0 - 1.0;
	float sinAngle = sqrt(saturate(1 - cosAngle * cosAngle));
	float3 lightDir = -normalize(float3(sinAngle, cosAngle, 0));

	_RWAmbientLightLUT[id.xy] = PrecomputeAmbientLight(lightDir);
}

[numthreads(1, 1, 1)]
void DirectLightLUT(uint3 id : SV_DispatchThreadID) {
	float w, h;
	_RWDirectLightLUT.GetDimensions(w, h);

	float2 uv = float2(id.x / (w - 1), id.y / (h - 1));

	float cosAngle = uv.x * 1.1 - 0.1;// *2.0 - 1.0;
	float sinAngle = sqrt(saturate(1 - cosAngle * cosAngle));
	float3 rayDir = normalize(float3(sinAngle, cosAngle, 0));

	_RWDirectLightLUT[id.xy] = PrecomputeDirectLight(rayDir);
}