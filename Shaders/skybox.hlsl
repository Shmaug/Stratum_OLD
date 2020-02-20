#pragma vertex vsmain
#pragma fragment fsmain

#pragma render_queue 0
#pragma cull false
#pragma zwrite false
#pragma ztest false

#pragma static_sampler Sampler maxAnisotropy=0 addressMode=clamp_edge

#pragma multi_compile ENABLE_SCATTERING ENVIRONMENT_TEXTURE ENVIRONMENT_TEXTURE_HDR

#include "include/shadercompat.h"

#define PI 3.1415926535897932
#define INV_PI 0.31830988618

// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);
// per-material
[[vk::binding(BINDING_START + 0, PER_MATERIAL)]] Texture3D<float3> SkyboxLUTR : register(t3);
[[vk::binding(BINDING_START + 1, PER_MATERIAL)]] Texture3D<float3> SkyboxLUTM : register(t3);
[[vk::binding(BINDING_START + 2, PER_MATERIAL)]] Texture2D<float4> MoonTexture : register(t4);
[[vk::binding(BINDING_START + 3, PER_MATERIAL)]] Texture2D<float>  LightShaftLUT : register(t5);
[[vk::binding(BINDING_START + 4, PER_MATERIAL)]] TextureCube<float4> StarTexture : register(t6);

[[vk::binding(BINDING_START + 5, PER_MATERIAL)]] Texture2D<float4> EnvironmentTexture : register(t7);

[[vk::binding(BINDING_START + 6, PER_MATERIAL)]] SamplerState Sampler : register(s0);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	STRATUM_PUSH_CONSTANTS

	float4 _MoonRotation;
	float _MoonSize;

	float3 _IncomingLight;

	float3 _SunDir;

	float _PlanetRadius;
	float _AtmosphereHeight;

	float _SunIntensity;
	float _MieG;

	float3 _ScatteringR;
	float3 _ScatteringM;

	float4 _StarRotation;
	float _StarFade;
};

#include "include/util.hlsli"

float3 rotate(float4 q, float3 v) {
	return 2 * dot(q.xyz, v) * q.xyz + (q.w * q.w - dot(q.xyz, q.xyz)) * v + 2 * q.w * cross(q.xyz, v);
}

void RenderMoon(inout float3 color, float3 ray) {
	float3 moonray = -rotate(_MoonRotation, ray);
	if (moonray.z < 0) return;
	float2 moonuv = moonray.xy;
	if (abs(moonuv.x) < _MoonSize && abs(moonuv.y) < _MoonSize) {
		float4 moontex = MoonTexture.SampleLevel(Sampler, moonuv / _MoonSize * .5 + .5, 0);
		color += moontex.rgb * moontex.a * saturate(1 - .5 * length(color));
	}
}
float3 RenderSun(in float3 scatterM, float cosAngle) {
	float g = 0.98;
	float g2 = g * g;

	float sun = (1 - g) * (1 - g) / (4 * PI * pow(1.0 + g2 - 2.0 * g * cosAngle, 1.5));
	return sun * 0.0015 * scatterM;// 5;
}

void ApplyPhaseFunctionElek(inout float3 scatterR, inout float3 scatterM, float cosAngle) {
	// r
	float phase = (8.0 / 10.0) / (4 * PI) * ((7.0 / 5.0) + 0.5 * cosAngle);
	scatterR *= phase;

	// m
	float g = _MieG;
	float g2 = g * g;
	phase = (1.0 / (4.0 * PI)) * ((3.0 * (1.0 - g2)) / (2.0 * (2.0 + g2))) * ((1 + cosAngle * cosAngle) / (pow((1 + g2 - 2 * g * cosAngle), 3.0 / 2.0)));
	scatterM *= phase;
}

void vsmain(
	float3 vertex : POSITION,
	out float4 position : SV_Position,
	out float4 screenPos : TEXCOORD0,
	out float3 viewRay : TEXCOORD1) {
	if (Camera.ProjParams.w) {
		position = float4(vertex.xy, 0, 1);
		viewRay = float3(STRATUM_MATRIX_V[0].z, STRATUM_MATRIX_V[1].z, STRATUM_MATRIX_V[2].z);
	} else {
		position = mul(STRATUM_MATRIX_P, float4(vertex, 1));
		viewRay = mul(vertex, (float3x3)STRATUM_MATRIX_V);
	}
	StratumOffsetClipPosStereo(position);
	screenPos = ComputeScreenPos(position);
}

void fsmain(
	in float4 screenPos : TEXCOORD0,
	in float3 viewRay : TEXCOORD1,
	out float4 color : SV_Target0,
	out float4 depthNormal : SV_Target1 ) {
	float3 ray = normalize(viewRay);

#ifdef ENABLE_SCATTERING
	float3 rayStart = Camera.Position;

	float3 planetCenter = float3(0, -_PlanetRadius, 0);

	float rp = length(rayStart - planetCenter);
	float height = max(0, rp - _PlanetRadius);
	float3 normal = (rayStart - planetCenter) / rp;

	float viewZenith = abs(dot(normal, ray));
	float sunZenith = dot(normal, _SunDir);

	float3 coords = float3(height / _AtmosphereHeight, viewZenith * 0.5 + 0.5, sunZenith * 0.5 + 0.5);

	coords.x = sqrt(height / _AtmosphereHeight);
	float ch = -(sqrt(height * (2 * _PlanetRadius + height)) / (_PlanetRadius + height));
	if (viewZenith > ch)
		coords.y = 0.5 * pow((viewZenith - ch) / (1 - ch), 0.2) + 0.5;
	else
		coords.y = 0.5 * pow((ch - viewZenith) / (ch + 1), 0.2);
	coords.z = 0.5 * ((atan(max(sunZenith, -0.1975) * tan(1.26 * 1.1)) / 1.1) + (1 - 0.26));

	coords = saturate(coords);
	float3 scatterR = SkyboxLUTR.SampleLevel(Sampler, coords, 0);
	float3 scatterM = SkyboxLUTM.SampleLevel(Sampler, coords, 0);

	float3 m = scatterM;

	ApplyPhaseFunctionElek(scatterR.xyz, scatterM.xyz, dot(ray, _SunDir));
	float3 lightInscatter = (scatterR * _ScatteringR + scatterM * _ScatteringM) * _IncomingLight;

	// light shafts
	//float shadow = LightShaftLUT.SampleLevel(Sampler, screenPos.xy / screenPos.w, 0);
	//float shadow4 = shadow*shadow;
	//shadow4 *= shadow4;
	//lightInscatter *= shadow * .2 + shadow4 * .8;
	
	// sun and moon
	lightInscatter += RenderSun(m, dot(ray, _SunDir)) * _SunIntensity;
	RenderMoon(lightInscatter, ray);

	// stars
	float3 star = StarTexture.SampleLevel(Sampler, rotate(_StarRotation, ray), .25).rgb;
	lightInscatter += star * (1 - saturate(_StarFade * dot(lightInscatter, lightInscatter)));

	color = float4(lightInscatter, 1);
#elif defined(ENVIRONMENT_TEXTURE) || defined(ENVIRONMENT_TEXTURE_HDR)
	uint texWidth, texHeight, numMips;
	EnvironmentTexture.GetDimensions(0, texWidth, texHeight, numMips);
	float2 envuv = float2(atan2(ray.z, ray.x) * INV_PI * .5 + .5, acos(ray.y) * INV_PI);
	color = float4(EnvironmentTexture.SampleLevel(Sampler, envuv, 0).rgb, 1);
#ifdef ENVIRONMENT_TEXTURE_HDR
	color = pow(color, 1 / 2.2);
#endif
#else
	color = float4(AmbientLight, 1);
#endif

	depthNormal = float4(normalize(float3(1)) * Camera.Viewport.w, 1);
}