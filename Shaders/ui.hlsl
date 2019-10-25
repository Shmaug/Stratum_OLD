#pragma vertex vsmain
#pragma fragment fsmain

#pragma render_queue 5000
#pragma cull false
#pragma zwrite false
#pragma blend alpha

#pragma static_sampler Sampler

#include <shadercompat.h>

// per-object
[[vk::binding(OBJECT_BUFFER_BINDING, PER_OBJECT)]] ConstantBuffer<ObjectBuffer> Object : register(b0);
[[vk::binding(BINDING_START + 0, PER_OBJECT)]] Texture2D<float4> MainTexture : register(t0);
[[vk::binding(BINDING_START + 1, PER_OBJECT)]] SamplerState Sampler : register(s0);
// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4 Color;
	float2 Offset;
	float2 Extent;
	float2 Bounds;
}

struct v2f {
	float4 position : SV_Position;
	float4 texcoord : TEXCOORD0;
	float3 worldPos : TEXCOORD1;
	float3 normal : NORMAL;
};

v2f vsmain(uint index : SV_VertexID) {
	static const float2 positions[6] = {
		float2(0,0),
		float2(1,0),
		float2(1,1),
		float2(0,1),
		float2(0,0),
		float2(1,1)
	};

	float2 p = Offset + Extent * (positions[index] * 2 - 1);
	float4 wp = mul(Object.ObjectToWorld, float4(p, 0, 1.0));

	v2f o;
	o.position = mul(Camera.ViewProjection, wp);
	o.texcoord.xy = positions[index];
	o.texcoord.zw = abs(p);
	o.normal = mul(float4(0, 0, 1, 1), Object.WorldToObject).xyz;
	o.worldPos = wp.xyz;

	return o;
}

void fsmain(v2f i,
	out float4 color : SV_Target0,
	out float4 depthNormal : SV_Target1) {
	clip(any(Bounds - i.texcoord.zw));
	color = MainTexture.SampleLevel(Sampler, i.texcoord.xy, 0) * Color;
	depthNormal = float4(normalize(i.normal) * .5 + .5, length(Camera.Position - i.worldPos.xyz) / Camera.Viewport.w);
}