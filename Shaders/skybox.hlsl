#pragma vertex vsmain
#pragma fragment fsmain

#pragma render_queue 0
#pragma cull false
#pragma zwrite false
#pragma ztest false

#pragma static_sampler Sampler

#include <shadercompat.h>

#define INV_PI 0.31830988618

// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);
// per-material
[[vk::binding(BINDING_START + 0, PER_MATERIAL)]] Texture2D<float4> EnvironmentTexture : register(t3);
[[vk::binding(BINDING_START + 1, PER_MATERIAL)]] SamplerState Sampler : register(s0);


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
	
	float2 uv = float2(atan2(viewRay.z, viewRay.x) * INV_PI * .5 + .5, acos(viewRay.y / length(viewRay)) * INV_PI);
	color = float4(pow(EnvironmentTexture.SampleLevel(Sampler, uv, 0).rgb, 1 / 2.2), 1);
	depthNormal = float4(0, 0, 0, 1);
}