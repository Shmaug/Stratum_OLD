#pragma vertex vsmain
#pragma fragment fsmain

#pragma render_queue 5000
#pragma cull false
#pragma zwrite false
#pragma blend alpha

#pragma static_sampler Sampler

#include "include/shadercompat.h"

// per-object
[[vk::binding(BINDING_START + 0, PER_OBJECT)]] Texture2D<float4> MainTexture : register(t0);
[[vk::binding(BINDING_START + 1, PER_OBJECT)]] SamplerState Sampler : register(s0);
// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4x4 ObjectToWorld;
	float4x4 WorldToObject;
	float4 Color;
	float2 Offset;
	float2 Extent;
	float2 Bounds;
}

#include "include/util.hlsli"

struct v2f {
	float4 position : SV_Position;
	float4 texcoord : TEXCOORD0;
	float4 worldPos : TEXCOORD1;
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
	float4x4 ct = float4x4(1,0,0,-Camera.Position.x, 0,1,0,-Camera.Position.y, 0,0,1,-Camera.Position.z, 0,0,0,1);
	float4 worldPos = mul(mul(ct, ObjectToWorld), float4(p, 0, 1.0));

	v2f o;
	o.position = mul(Camera.ViewProjection, worldPos);
	o.worldPos = float4(worldPos.xyz, LinearDepth01(o.position.z));
	o.texcoord.xy = positions[index];
	o.texcoord.zw = abs(p);

	return o;
}

void fsmain(v2f i,
	out float4 color : SV_Target0,
	out float4 depthNormal : SV_Target1) {
	depthNormal = float4(cross(ddx(i.worldPos.xyz), ddy(i.worldPos.xyz)), i.worldPos.w);
	clip(any(Bounds - i.texcoord.zw));
	color = MainTexture.SampleLevel(Sampler, i.texcoord.xy, 0) * Color;
}