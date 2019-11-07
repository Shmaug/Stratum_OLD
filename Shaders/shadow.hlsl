#pragma vertex vsmain
#pragma fragment fsmain

#pragma color_mask r

#include <shadercompat.h>

[[vk::binding(OBJECT_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<ObjectBuffer> Instances : register(t0);
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);

struct v2f {
	float4 position : SV_Position;
	float3 viewRay : TEXCOORD0;
};

v2f vsmain(
	[[vk::location(0)]] float3 vertex : POSITION,
	uint instance : SV_InstanceID) {
	v2f o;

	float4 worldPos = mul(Instances[instance].ObjectToWorld, float4(vertex, 1.0));
	o.position = mul(Camera.ViewProjection, worldPos);

	float4 p0 = mul(Camera.InvViewProjection, float4(o.position.xy / o.position.w, 0, 1));
	o.viewRay = worldPos.xyz - p0.xyz / p0.w;

	return o;
}

float fsmain(in float3 viewRay : TEXCOORD0) : SV_Target0 {
	return length(viewRay);
}