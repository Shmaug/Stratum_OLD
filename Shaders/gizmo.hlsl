#pragma vertex vsmain
#pragma fragment fsmain

#pragma render_queue 5000
#pragma cull false
#pragma zwrite false
#pragma blend alpha

#pragma static_sampler Sampler
#pragma array MainTexture 128

#include "include/shadercompat.h"

struct Gizmo {
	float4 Color;
	float4 Rotation;
	float4 TextureST;
	float3 Position;
	uint TextureIndex;
	float3 Scale;
	uint Type;
};

[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);

[[vk::binding(OBJECT_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<Gizmo> Gizmos : register(t0);
[[vk::binding(BINDING_START + 0, PER_OBJECT)]] SamplerState Sampler : register(s0);
[[vk::binding(BINDING_START + 1, PER_OBJECT)]] Texture2D<float4> MainTexture[128] : register(t1);

#include "include/util.hlsli"

struct v2f {
	float4 position : SV_Position;
	float depth : TEXCOORD0;
	float3 normal : NORMAL;
	float4 color : COLOR0;
	float2 texcoord : TEXCOORD1;
	uint textureIndex : TEXCOORD2;
};

float3 rotate(float4 q, float3 v){
	return 2 * dot(q.xyz, v) * q.xyz + (q.w * q.w - dot(q.xyz, q.xyz)) * v + 2 * q.w * cross(q.xyz, v);
}

v2f vsmain(
	[[vk::location(0)]] float3 vertex : Position,  
	[[vk::location(3)]] float2 texcoord : TEXCOORD0,
	uint i : SV_InstanceID ) {
	Gizmo g = Gizmos[i];

	float3 worldPos = g.Position + rotate(g.Rotation, vertex * g.Scale);
	worldPos.xyz -= Camera.Position;

	v2f o;
	o.position = mul(Camera.ViewProjection, float4(worldPos, 1));
	o.depth = LinearDepth01(o.position.z);
	o.normal = rotate(g.Rotation, float3(0,0,1));
	o.color = g.Color;
	o.texcoord = texcoord * g.TextureST.xy + g.TextureST.zw;
	o.textureIndex = g.TextureIndex;
	return o;
}

void fsmain(v2f i,
	out float4 color : SV_Target0,
	out float4 depthNormal : SV_Target1 ) {
	color = MainTexture[i.textureIndex].Sample(Sampler, i.texcoord) * i.color;
	depthNormal = float4(normalize(i.normal * .5 + .5), i.depth);
}