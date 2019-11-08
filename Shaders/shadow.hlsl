#pragma vertex vsmain
#pragma fragment fsmain

#include <shadercompat.h>

[[vk::binding(OBJECT_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<ObjectBuffer> Instances : register(t0);
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);

void vsmain(
	[[vk::location(0)]] float3 vertex : POSITION,
	uint instance : SV_InstanceID,
	out float4 position : SV_Position,
	out float depth : TEXCOORD0) {
	float4 worldPos = mul(Instances[instance].ObjectToWorld, float4(vertex, 1.0));
	position = mul(Camera.ViewProjection, worldPos);
	depth = (Camera.ProjParams.w ? position.z * (Camera.Viewport.w - Camera.Viewport.z) + Camera.Viewport.z : position.w) / Camera.Viewport.w;
}

float4 fsmain(in float depth : TEXCOORD0) : SV_Target0 {
	return depth;
}