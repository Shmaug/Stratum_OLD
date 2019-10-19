#pragma vertex vsmain
#pragma fragment fsmain

#pragma zwrite false
#pragma blend_fac src_alpha inv_src_alpha

#pragma render_queue 1000

#include <shadercompat.h>

[[vk::binding(OBJECT_BUFFER_BINDING, PER_OBJECT)]] ConstantBuffer<ObjectBuffer> Object : register(b0);
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4 Color;
	float3 Position;
	float4 Rotation;
	float3 Scale;
}

float4 vsmain([[vk::location(0)]] float3 vertex : POSITION) : SV_Position {
	float3 vec = vertex * Scale;
	vec = 2 * dot(Rotation.xyz, vec) * Rotation.xyz + (Rotation.w * Rotation.w - dot(Rotation.xyz, Rotation.xyz)) * vec + 2 * Rotation.w * cross(Rotation.xyz, vec);
	vec += Position;
	return mul(Camera.ViewProjection, float4(vec, 1));
}

void fsmain(out float4 color : SV_Target0, out float4 depthNormal : SV_Target1) {
	color = Color;
	depthNormal = float4(0, 0, 0, 1);
}