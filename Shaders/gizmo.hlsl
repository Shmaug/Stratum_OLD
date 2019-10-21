#pragma vertex vsmain
#pragma fragment fsmain

#pragma render_queue 5000
#pragma cull false
#pragma zwrite false
#pragma blend alpha

#include <shadercompat.h>

[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4 Color;
	float3 Position;
	float4 Rotation;
	float3 Scale;
}

struct v2f {
	float4 pos : SV_Position;
	float3 viewRay : TEXCOORD0;
	float3 normal : NORMAL;
};

v2f vsmain([[vk::location(0)]] float3 vertex : POSITION) {
	v2f o;
	float3 worldPos = vertex * Scale;
	worldPos = 2 * dot(Rotation.xyz, worldPos) * Rotation.xyz + (Rotation.w * Rotation.w - dot(Rotation.xyz, Rotation.xyz)) * worldPos + 2 * Rotation.w * cross(Rotation.xyz, worldPos);
	worldPos += Position;
	o.viewRay = worldPos - Camera.Position;

	float3 normal = float3(0, 0, 1);
	o.normal = 2 * dot(Rotation.xyz, normal) * Rotation.xyz + (Rotation.w * Rotation.w - dot(Rotation.xyz, Rotation.xyz)) * normal + 2 * Rotation.w * cross(Rotation.xyz, normal);
	o.pos = mul(Camera.ViewProjection, float4(worldPos, 1));
	return o;
}

void fsmain(v2f i,
	out float4 color : SV_Target0,
	out float4 depthNormal : SV_Target1 ) {
	color = Color;
	depthNormal = float4(normalize(i.normal * .5 + .5), length(i.viewRay) / Camera.Viewport.w);
}