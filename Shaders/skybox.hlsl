#pragma vertex vsmain
#pragma fragment fsmain

#pragma render_queue 0
#pragma cull false
#pragma zwrite false
#pragma ztest false

#pragma static_sampler Sampler

#include <shadercompat.h>

#define PI 3.1415926535897932
#define INV_PI 0.31830988618

// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);
// per-material
[[vk::binding(BINDING_START + 0, PER_MATERIAL)]] Texture3D<float4> _SkyboxLUT : register(t3);
[[vk::binding(BINDING_START + 1, PER_MATERIAL)]] Texture3D<float2> _SkyboxLUT2 : register(t3);
[[vk::binding(BINDING_START + 2, PER_MATERIAL)]] Texture2D<float4> _MoonTex : register(t4);
[[vk::binding(BINDING_START + 3, PER_MATERIAL)]] TextureCube<float4> _StarCube : register(t5);
[[vk::binding(BINDING_START + 4, PER_MATERIAL)]] SamplerState Sampler : register(s0);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float3 _MoonDir;
	float3 _MoonRight;
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


float3 rotate(float4 q, float3 v) {
	return 2 * dot(q.xyz, v) * q.xyz + (q.w * q.w - dot(q.xyz, q.xyz)) * v + 2 * q.w * cross(q.xyz, v);
}

void RenderMoon(inout float3 color, float3 ray) {
	if (dot(ray, _MoonDir) < 0) return;
	float3 moonray = _MoonDir - ray;
	float2 moonuv = float2(dot(moonray, _MoonRight), dot(moonray, cross(_MoonDir, _MoonRight)));
	if (abs(moonuv.x) < _MoonSize && abs(moonuv.y) < _MoonSize) {
		float4 moontex = _MoonTex.SampleLevel(Sampler, moonuv / _MoonSize * .5 + .5, 0);
		color += moontex.rgb * moontex.a * saturate(1 - .5 * length(color));
	}
}
float3 RenderSun(in float3 scatterM, float cosAngle) {
	float g = 0.98;
	float g2 = g * g;

	float sun = (1 - g) * (1 - g) / (4 * PI * pow(1.0 + g2 - 2.0 * g * cosAngle, 1.5));
	return sun * 0.003 * scatterM;// 5;
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
	[[vk::location(0)]] float3 vertex : POSITION,
	out float4 position : SV_Position,
	out float3 viewRay : TEXCOORD0) {
	if (Camera.ProjParams.w) {
		position = float4(vertex.xy, 0, 1);
		viewRay = float3(Camera.View[0].z, Camera.View[1].z, Camera.View[2].z);
	} else {
		position = mul(Camera.Projection, float4(vertex, 1));
		viewRay = mul(vertex, (float3x3)Camera.View);
	}
}

void fsmain(
	float3 viewRay : TEXCOORD0,
	out float4 color : SV_Target0,
	out float4 depthNormal : SV_Target1 ) {

	float3 rayStart = Camera.Position;
	float3 ray = normalize(viewRay);

	float3 planetCenter = float3(0, -_PlanetRadius, 0);

	float4 scatterR = 0;
	float4 scatterM = 0;

	float height = max(0, length(rayStart - planetCenter) - _PlanetRadius);
	float3 normal = normalize(rayStart - planetCenter);

	float viewZenith = dot(normal, ray);
	float sunZenith = dot(normal, _SunDir);

	float3 coords = float3(height / _AtmosphereHeight, viewZenith * 0.5 + 0.5, sunZenith * 0.5 + 0.5);

	coords.x = pow(height / _AtmosphereHeight, 0.5);
	float ch = -(sqrt(height * (2 * _PlanetRadius + height)) / (_PlanetRadius + height));
	if (viewZenith > ch)
		coords.y = 0.5 * pow((viewZenith - ch) / (1 - ch), 0.2) + 0.5;
	else
		coords.y = 0.5 * pow((ch - viewZenith) / (ch + 1), 0.2);
	coords.z = 0.5 * ((atan(max(sunZenith, -0.1975) * tan(1.26 * 1.1)) / 1.1) + (1 - 0.26));

	coords = saturate(coords);
	scatterR = _SkyboxLUT.Sample(Sampler, coords);
	scatterM.x = scatterR.w;
	scatterM.yz = _SkyboxLUT2.Sample(Sampler, coords).xy;

	float3 m = scatterM;
	// scatterR = 0;
	// phase function
	ApplyPhaseFunctionElek(scatterR.xyz, scatterM.xyz, dot(ray, _SunDir));
	float3 lightInscatter = (scatterR * _ScatteringR + scatterM * _ScatteringM) * _IncomingLight;

	lightInscatter += RenderSun(m, dot(ray, _SunDir.xyz)) * _SunIntensity;

	color = float4(lightInscatter, 1);
	RenderMoon(color.rgb, ray);
	float3 star = _StarCube.SampleLevel(Sampler, rotate(_StarRotation, ray), .25);
	color.rgb += star * (1 - saturate(_StarFade * dot(color, color)));

	depthNormal = float4(0, 0, 0, 1);
}