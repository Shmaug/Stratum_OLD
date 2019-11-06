#pragma vertex vsmain
#pragma fragment fsmain

#pragma render_queue 1000

#include <shadercompat.h>

[[vk::binding(OBJECT_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<ObjectBuffer> Objects : register(t0);
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);

struct v2f {
	float4 position : SV_Position;
	float3 view : TEXCOORD0;
};

v2f vsmain(
	[[vk::location(0)]] float3 vertex : POSITION,
	uint instance : SV_InstanceID) {
	v2f o;

	float4 worldPos = mul(Objects[instance].ObjectToWorld, float4(vertex, 1.0));
	o.position = mul(Camera.ViewProjection, worldPos);
	o.view = Camera.Position - worldPos.xyz;
	return o;
}

float fsmain(in float3 view : TEXCOORD0) : SV_Target0 {
	return length(view);
}