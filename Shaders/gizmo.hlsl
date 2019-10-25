#pragma vertex vsmain
#pragma fragment fsmain

#pragma multi_compile TEXTURED_QUAD

#pragma render_queue 5000
#pragma cull false
#pragma zwrite false
#pragma blend alpha

#pragma static_sampler Sampler

#include <shadercompat.h>

[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);

[[vk::binding(BINDING_START + 0, PER_OBJECT)]] Texture2D<float4> MainTexture : register(t0);
[[vk::binding(BINDING_START + 1, PER_OBJECT)]] SamplerState Sampler : register(s0);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4 Color;
	float3 Position;
	float4 Rotation;
	float3 Scale;
	#ifdef TEXTURED_QUAD
	float4 MainTexture_ST;
	#endif
}

struct v2f {
	float4 pos : SV_Position;
	float3 viewRay : TEXCOORD0;
	float3 normal : NORMAL;
	#ifdef TEXTURED_QUAD
	float2 uv : TEXCOORD1;
	#endif
};

v2f vsmain(
	#ifdef TEXTURED_QUAD
	uint index : SV_VertexID
	#else
	[[vk::location(0)]] float3 vertex : POSITION
	#endif
	) {
	v2f o;
	
	#ifdef TEXTURED_QUAD
	static const float2 positions[6] = {
		float2(0,0),
		float2(1,0),
		float2(0,1),
		float2(1,0),
		float2(1,1),
		float2(0,1)
	};
	float3 worldPos = float3((positions[index]*2-1), 0) * Scale;
	worldPos = Camera.Right * worldPos.x + Camera.Up * worldPos.y;
	o.uv = positions[index] * MainTexture_ST.xy + MainTexture_ST.zw;
	#else
	float3 worldPos = vertex * Scale;
	worldPos = 2 * dot(Rotation.xyz, worldPos) * Rotation.xyz + (Rotation.w * Rotation.w - dot(Rotation.xyz, Rotation.xyz)) * worldPos + 2 * Rotation.w * cross(Rotation.xyz, worldPos);
	#endif

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
	#ifdef TEXTURED_QUAD
	color *= MainTexture.Sample(Sampler, i.uv);
	#endif
	depthNormal = float4(normalize(i.normal * .5 + .5), length(i.viewRay) / Camera.Viewport.w);
}