#pragma vertex vsmain
#pragma fragment fsmain

#pragma multi_compile DEPTH_PASS

#pragma render_queue 1000

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
[[vk::binding(BINDING_START + 1, PER_MATERIAL)]] StructuredBuffer<TreeVertex> Tree      : register(t5);

[[vk::binding(BINDING_START + 4, PER_MATERIAL)]] Texture3D<float3> InscatteringLUT		: register(t9);
[[vk::binding(BINDING_START + 5, PER_MATERIAL)]] Texture3D<float3> ExtinctionLUT		: register(t10);
[[vk::binding(BINDING_START + 6, PER_MATERIAL)]] Texture2D<float>  LightShaftLUT		: register(t11);
[[vk::binding(BINDING_START + 7, PER_MATERIAL)]] SamplerState Sampler : register(s0);
[[vk::binding(BINDING_START + 8, PER_MATERIAL)]] SamplerComparisonState ShadowSampler : register(s1);
[[vk::binding(BINDING_START + 9, PER_MATERIAL)]] SamplerState AtmosphereSampler : register(s2);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float3 AmbientLight;
	uint LightCount;
	float2 ShadowTexelSize;
};

//#define SHOW_CASCADE_SPLITS

#include "include/util.hlsli"
#include "include/shadow.hlsli"
#include "include/brdf.hlsli"
#include "include/scatter.hlsli"

struct v2f {
	float4 position : SV_Position;
	float4 worldPos : TEXCOORD0;
#ifndef DEPTH_PASS
	float4 screenPos : TEXCOORD1;
	float3 normal : NORMAL;
	float2 texcoord : TEXCOORD2;
#endif
};

v2f vsmain(
	uint vertexID : SV_VertexID,
	uint instance : SV_InstanceID ) {
	v2f o;
	
	float3 vertex = Tree[vertexID].position;
	float3 normal = Tree[vertexID].normal;

	float4x4 ct = float4x4(1,0,0,-Camera.Position.x, 0,1,0,-Camera.Position.y, 0,0,1,-Camera.Position.z, 0,0,0,1);
	float4 worldPos = mul(mul(ct, Instances[instance].ObjectToWorld), float4(vertex, 1.0));

	o.position = mul(Camera.ViewProjection, worldPos);
	o.worldPos = float4(worldPos.xyz, LinearDepth01(o.position.z));
	
	#ifndef DEPTH_PASS
	o.screenPos = ComputeScreenPos(o.position);
	o.normal = mul(float4(normal, 1), Instances[instance].WorldToObject).xyz;
	o.texcoord = 0;
	#endif

	return o;
}

#ifdef DEPTH_PASS
float fsmain(in float4 worldPos : TEXCOORD0) : SV_Target0{
	return worldPos.w;
}
#else
void fsmain(v2f i,
	out float4 color : SV_Target0,
	out float4 depthNormal : SV_Target1) {

	float3 view = ComputeView(i.worldPos.xyz, i.screenPos);
	float4 col = MainTexture.Sample(Sampler, i.texcoord);

	clip(col.a - .9);

	float3 normal = normalize(i.normal);
	if (dot(normal, view) < 0) normal = -normal;

	MaterialInfo material;
	material.diffuse = DiffuseAndSpecularFromMetallic(col.rgb, 0, material.specular, material.oneMinusReflectivity);
	material.perceptualRoughness = .5;
	material.occlusion = 1;
	material.roughness = max(.002, material.perceptualRoughness * material.perceptualRoughness);
	material.emission = 0;
	
	float3 eval = EvaluateLighting(material, i.worldPos.xyz, normal, view, i.worldPos.w);
	ApplyScattering(eval, i.screenPos.xy / i.screenPos.w, i.worldPos.w);
	color = float4(eval, 1);
	depthNormal = float4(normal * .5 + .5, i.worldPos.w);
}
#endif