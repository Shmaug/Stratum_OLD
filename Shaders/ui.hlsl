#pragma vertex vsmain
#pragma fragment fsmain

#pragma render_queue 5000
#pragma cull false
#pragma zwrite false
#pragma blend alpha

#pragma static_sampler Sampler

#pragma multi_compile OUTLINE

#include <shadercompat.h>

// per-object
[[vk::binding(OBJECT_BUFFER_BINDING, PER_OBJECT)]] ConstantBuffer<ObjectBuffer> Object : register(b0);
[[vk::binding(BINDING_START + 0, PER_OBJECT)]] Texture2D<float4> MainTexture : register(t0);
[[vk::binding(BINDING_START + 1, PER_OBJECT)]] SamplerState Sampler : register(s0);
// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4 Color;
	float2 Extent;
}

struct v2f {
	float4 position : SV_Position;
	float3 normal : NORMAL;
	float2 texcoord : TEXCOORD0;
	float3 worldPos : TEXCOORD1;
};

v2f vsmain(uint index : SV_VertexID) {
	#ifdef OUTLINE
	static const float2 positions[8] = {
		float2(0,0),
		float2(1,0),

		float2(1,0),
		float2(1,1),

		float2(1,1),
		float2(0,1),

		float2(0,1),
		float2(0,0)
	};
	#else
	static const float2 positions[6] = {
		float2(0,0),
		float2(1,0),
		float2(0,1),
		float2(1,0),
		float2(1,1),
		float2(0,1)
	};
	#endif
	
	float4 wp = mul(Object.ObjectToWorld, float4(Extent * (positions[index] * 2 - 1), 0, 1.0));

	v2f o;
	o.position = mul(Camera.ViewProjection, wp);
	o.texcoord = positions[index];
	o.normal = mul(float4(0, 0, 1, 1), Object.WorldToObject).xyz;
	o.worldPos = wp.xyz;

	return o;
}

void fsmain(v2f i,
	out float4 color : SV_Target0,
	out float4 depthNormal : SV_Target1) {
	color = MainTexture.SampleLevel(Sampler, i.texcoord, 0) * Color;
	depthNormal = float4(normalize(i.normal) * .5 + .5, length(Camera.Position - i.worldPos.xyz) / Camera.Viewport.w);
}