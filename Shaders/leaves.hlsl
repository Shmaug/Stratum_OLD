#pragma vertex vsmain
#pragma fragment fsmain

#pragma multi_compile DEPTH_PASS
#pragma multi_compile GEN_LEAVES

#pragma render_queue 1000

#pragma cull false
#pragma blend alpha

#pragma static_sampler Sampler
#pragma static_sampler ShadowSampler maxAnisotropy=0 maxLod=0 addressMode=clamp_border borderColor=float_opaque_white compareOp=less
#pragma static_sampler AtmosphereSampler maxAnisotropy=0 addressMode=clamp_edge

#include "include/shadercompat.h"

struct TreeVertex {
	float3 position;
	float3 normal;
	float4 tangent;
	float2 uv;
};

// per-object
[[vk::binding(OBJECT_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<ObjectBuffer> Instances : register(t0);
[[vk::binding(LIGHT_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<GPULight> Lights : register(t1);
[[vk::binding(SHADOW_ATLAS_BINDING, PER_OBJECT)]] Texture2D<float> ShadowAtlas : register(t2);
[[vk::binding(SHADOW_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<ShadowData> Shadows : register(t3);
// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);
// per-material
[[vk::binding(BINDING_START + 0, PER_MATERIAL)]] Texture2D<float4> MainTexture			: register(t4);
[[vk::binding(BINDING_START + 1, PER_MATERIAL)]] Texture2D<float4> NormalTexture		: register(t5);
[[vk::binding(BINDING_START + 2, PER_MATERIAL)]] Texture2D<float4> MaskTexture			: register(t6);
[[vk::binding(BINDING_START + 3, PER_MATERIAL)]] StructuredBuffer<TreeVertex> Tree      : register(t7);

[[vk::binding(BINDING_START + 4, PER_MATERIAL)]] Texture3D<float3> InscatteringLUT		: register(t9);
[[vk::binding(BINDING_START + 5, PER_MATERIAL)]] Texture3D<float3> ExtinctionLUT		: register(t10);
[[vk::binding(BINDING_START + 6, PER_MATERIAL)]] Texture2D<float>  LightShaftLUT		: register(t11);
[[vk::binding(BINDING_START + 7, PER_MATERIAL)]] SamplerState Sampler : register(s0);
[[vk::binding(BINDING_START + 8, PER_MATERIAL)]] SamplerComparisonState ShadowSampler : register(s1);
[[vk::binding(BINDING_START + 9, PER_MATERIAL)]] SamplerState AtmosphereSampler : register(s2);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float BumpStrength;

	float Time;
	float3 AmbientLight;
	uint LightCount;
	float2 ShadowTexelSize;
};

//#define SHOW_CASCADE_SPLITS

#include "include/util.hlsli"
#include "include/shadow.hlsli"
#include "include/brdf.hlsli"
#include "include/scatter.hlsli"
#include "include/noise.hlsli"

struct v2f {
	float4 position : SV_Position;
	float4 worldPos : TEXCOORD0;
	float2 texcoord : TEXCOORD1;
#ifndef DEPTH_PASS
	float4 screenPos : TEXCOORD2;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
#endif
};

float2 Sway(float h, float2 xz){
	return h * .1 * float2(noise(xz + 4*Time), noise(xz + 4*Time + 100));
}

#ifdef GEN_LEAVES
v2f vsmain(
	uint vertexId : SV_VertexID,
	uint instance : SV_InstanceID) {
	TreeVertex tv = Tree[vertexId / 6];
	static const float2 positions[6] = {
		float2(0,0),
		float2(1,0),
		float2(1,1),
		float2(0,1),
		float2(0,0),
		float2(1,1)
	};
	float2 p = positions[vertexId % 6];

	float3 tvertex = tv.position;
	float4 tangent = tv.tangent;
	float3 normal = tv.normal;
	float2 texcoord = p * float2(.333333, 1);
	float3 bitangent = cross(tangent.xyz, normal) * tangent.w;

	tvertex.xz += Sway(tvertex.y, Instances[instance].ObjectToWorld[3].xz);

	float3 vertex = tvertex + ((p.x * 2 - 1) * tangent + p.y * normal) * .1;
	normal = bitangent;

	vertex.xz += Sway(p.y, tvertex.xz);

#else

v2f vsmain(
	[[vk::location(0)]] float3 vertex : POSITION,
	[[vk::location(1)]] float3 normal : NORMAL,
	[[vk::location(2)]] float4 tangent : TANGENT,
	[[vk::location(3)]] float2 texcoord : TEXCOORD0,
	uint instance : SV_InstanceID ) {

	vertex.xz += Sway(vertex.y, Instances[instance].ObjectToWorld[3].xz);
#endif
	v2f o;

	float4x4 ct = float4x4(1,0,0,-Camera.Position.x, 0,1,0,-Camera.Position.y, 0,0,1,-Camera.Position.z, 0,0,0,1);
	float4 worldPos = mul(mul(ct, Instances[instance].ObjectToWorld), float4(vertex, 1.0));

	o.position = mul(Camera.ViewProjection, worldPos);
	o.worldPos = float4(worldPos.xyz, LinearDepth01(o.position.z));
	o.texcoord = texcoord;
	
	#ifndef DEPTH_PASS
	o.screenPos = ComputeScreenPos(o.position);
	o.normal = mul(float4(normal, 1), Instances[instance].WorldToObject).xyz;
	o.tangent = mul(float4(tangent.xyz, 1), Instances[instance].WorldToObject).xyz * tangent.w;
	#endif

	return o;
}

#ifdef DEPTH_PASS
float fsmain(in float4 worldPos : TEXCOORD0, in float2 texcoord : TEXCOORD1) : SV_Target0{
	clip(MainTexture.Sample(Sampler, texcoord).a - .75);
	return worldPos.w;
}
#else
void fsmain(v2f i,
	out float4 color : SV_Target0,
	out float4 depthNormal : SV_Target1) {

	float3 view = ComputeView(i.worldPos.xyz, i.screenPos);
	float4 col = MainTexture.Sample(Sampler, i.texcoord);

	float3 normal = normalize(i.normal);
	if (dot(normal, view) < 0) normal = -normal;

	float3 bump = NormalTexture.Sample(Sampler, i.texcoord).rgb * 2 - 1;
	float3 tangent = normalize(i.tangent);
	float3 bitangent = normalize(cross(i.normal, i.tangent));
	bump.xy *= BumpStrength;
	normal = normalize(tangent * bump.x + bitangent * bump.y + normal * bump.z);

	float4 mask = MaskTexture.Sample(Sampler, i.texcoord);

	MaterialInfo material;
	material.diffuse = DiffuseAndSpecularFromMetallic(col.rgb, 0, material.specular, material.oneMinusReflectivity);
	material.perceptualRoughness = mask.r * .99;
	material.occlusion = mask.b;
	material.roughness = max(.002, material.perceptualRoughness * material.perceptualRoughness);
	material.emission = 0;
	
	float3 eval = EvaluateLighting(material, i.worldPos.xyz, normal, view, i.worldPos.w);
	ApplyScattering(eval, i.screenPos.xy / i.screenPos.w, i.worldPos.w);
	color = float4(eval, col.a);
	depthNormal = float4(normal * .5 + .5, i.worldPos.w);
}
#endif