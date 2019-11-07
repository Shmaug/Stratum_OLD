#pragma vertex vsmain
#pragma fragment fsmain

#pragma render_queue 0
#pragma cull false
#pragma zwrite false

#pragma static_sampler Sampler

#include <shadercompat.h>

#define INV_PI 0.31830988618

// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);
// per-material
[[vk::binding(BINDING_START + 0, PER_MATERIAL)]] Texture2D<float4> EnvironmentTexture : register(t3);
[[vk::binding(BINDING_START + 1, PER_MATERIAL)]] SamplerState Sampler : register(s0);

struct v2f {
	float4 position : SV_Position;
	float3 viewRay : TEXCOORD0;
};

v2f vsmain([[vk::location(0)]] float3 vertex : POSITION ) {
	v2f o;

	float3 worldPos = vertex + Camera.Position;
	o.position = mul(Camera.ViewProjection, float4(worldPos, 1));

	float4 p0 = mul(Camera.InvViewProjection, float4(o.position.xy / o.position.w, 0, 1));
	o.viewRay = worldPos - p0.xyz / p0.w;

	return o;
}

void fsmain(v2f i,
	out float4 color : SV_Target0,
	out float4 depthNormal : SV_Target1) {

	float3 view = normalize(i.viewRay);
	float2 uv = float2(atan2(view.z, view.x) * INV_PI * .5 + .5, acos(view.y) * INV_PI);
	color = float4(pow(EnvironmentTexture.SampleLevel(Sampler, uv, 0).rgb, 1 / 2.2), 1);
	depthNormal = float4(0,0,0, Camera.Viewport.w);
}