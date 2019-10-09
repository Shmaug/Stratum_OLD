#pragma vertex vsmain
#pragma fragment fsmain

#pragma render_queue 1000

#include <shadercompat.h>

[[vk::binding(OBJECT_BUFFER_BINDING, PER_OBJECT)]] ConstantBuffer<ObjectBuffer> Object : register(b0);
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4 Color;
}

float4 vsmain([[vk::location(0)]] float3 vertex : POSITION) : SV_Position {
	return mul(Camera.ViewProjection, mul(Object.ObjectToWorld, float4(vertex, 1.0)));
}

float4 fsmain(float4 position : SV_Position) : SV_Target0 {
	return Color;
}