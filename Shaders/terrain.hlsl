#pragma vertex vsmain
#pragma fragment fsmain

#pragma array MainTextures 8
#pragma array NormalTextures 8
#pragma array MaskTextures 8

#pragma multi_compile DEPTH_PASS

#pragma render_queue 1000

#pragma static_sampler Sampler
#pragma static_sampler ShadowSampler maxAnisotropy=0 maxLod=0 addressMode=clamp_border borderColor=float_opaque_white compareOp=less
#pragma static_sampler AtmosphereSampler maxAnisotropy=0 addressMode=clamp_edge

#include "include/shadercompat.h"

// per-object
[[vk::binding(OBJECT_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<float4> Nodes : register(t8);
[[vk::binding(LIGHT_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<GPULight> Lights : register(t1);
[[vk::binding(SHADOW_ATLAS_BINDING, PER_OBJECT)]] Texture2D<float> ShadowAtlas : register(t2);
[[vk::binding(SHADOW_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<ShadowData> Shadows : register(t3);
// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);
// per-material
[[vk::binding(BINDING_START + 0, PER_MATERIAL)]] Texture3D<float4> InscatteringLUT		: register(t4);
[[vk::binding(BINDING_START + 1, PER_MATERIAL)]] Texture3D<float4> ExtinctionLUT		: register(t5);
[[vk::binding(BINDING_START + 2, PER_MATERIAL)]] Texture2D<float>  LightShaftLUT		: register(t6);
[[vk::binding(BINDING_START + 3, PER_MATERIAL)]] Texture2D<float4> MainTextures[8]		: register(t7);
[[vk::binding(BINDING_START + 4, PER_MATERIAL)]] Texture2D<float4> NormalTextures[8]	: register(t8);
[[vk::binding(BINDING_START + 5, PER_MATERIAL)]] Texture2D<float4> MaskTextures[8]		: register(t9); // rgb -> rough, height, ao
[[vk::binding(BINDING_START + 6, PER_MATERIAL)]] SamplerState Sampler : register(s0);
[[vk::binding(BINDING_START + 7, PER_MATERIAL)]] SamplerComparisonState ShadowSampler : register(s1);
[[vk::binding(BINDING_START + 8, PER_MATERIAL)]] SamplerState AtmosphereSampler : register(s2);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4x4 ObjectToWorld;
	float4x4 WorldToObject;
	uint LightCount;
	float2 ShadowTexelSize;
	float TerrainHeight;
	float3 AmbientLight;
};

#include "include/util.hlsli"
#include "include/shadow.hlsli"
#include "include/brdf.hlsli"
#include "include/scatter.hlsli"
#include "include/noise.hlsli"

struct v2f {
	float4 position : SV_Position;
	float4 worldPos : TEXCOORD0;
	float4 screenPos : TEXCOORD1;
	#ifndef DEPTH_PASS
	float2 terrainPos : TEXCOORD2;
	#endif
};

void triplanar(uint index, uint topIndex, float3 p, float3 blend, inout float4 color, inout float4 mask, inout float3 bump, float weight) {
	if (blend.x > 0) {
		color += blend.x * MainTextures[index].Sample(Sampler, p.yz) * weight;
		mask  += blend.x * MaskTextures[index].Sample(Sampler, p.yz) * weight;
		bump  += blend.x * (NormalTextures[index].Sample(Sampler, p.yz).xyz * 2 - 1) * weight;
	}
	if (blend.y > 0) {
		color += blend.y * MainTextures[topIndex].Sample(Sampler, p.xz) * weight;
		mask  += blend.y * MaskTextures[topIndex].Sample(Sampler, p.xz) * weight;
		bump  += blend.y * (NormalTextures[topIndex].Sample(Sampler, p.xz).xyz * 2 - 1) * weight;
	}
	if (blend.z > 0) {
		color += blend.z * MainTextures[index].Sample(Sampler, p.xy) * weight;
		mask  += blend.z * MaskTextures[index].Sample(Sampler, p.xy) * weight;
		bump  += blend.z * (NormalTextures[index].Sample(Sampler, p.xy).xyz * 2 - 1) * weight;
	}
}

v2f vsmain(
	uint v : SV_VertexID,
	uint instance : SV_InstanceID ) {
	
	float tmp;

	float3 vertex = float3(v % 17, 0, v / 17) / 16.0;
	vertex.xz -= .5;
	vertex = vertex * Nodes[instance].w + Nodes[instance].xyz;
	vertex.y = TerrainHeight * SampleTerrain(vertex.xz, tmp, tmp);
	
	float4x4 ct = float4x4(1,0,0,-Camera.Position.x, 0,1,0,-Camera.Position.y, 0,0,1,-Camera.Position.z, 0,0,0,1);
	float4 worldPos = mul(mul(ct, ObjectToWorld), float4(vertex, 1.0));

	v2f o;
	o.position = mul(Camera.ViewProjection, worldPos);
	o.worldPos = float4(worldPos.xyz, LinearDepth01(o.position.z));
	o.screenPos = ComputeScreenPos(o.position);
	#ifndef DEPTH_PASS
	o.terrainPos = vertex.xz;
	#endif
	return o;
}

#ifdef DEPTH_PASS
float fsmain(in float4 worldPos : TEXCOORD0) : SV_Target0 {
	return worldPos.w;// + max(ddx(worldPos).w, ddy(worldPos).w);
}
#else
void fsmain(v2f i,
	out float4 color : SV_Target0,
	out float4 depthNormal : SV_Target1) {
	
	float3 view = ComputeView(i.worldPos.xyz, i.screenPos);

	float tmp;
	float mountain, lake;

	const float e = .1;
	float y = TerrainHeight * SampleTerrain(i.terrainPos, mountain, lake);
	float3 tangent   = normalize(float3(e, TerrainHeight * SampleTerrain(i.terrainPos + float2(e, 0), tmp, tmp) - y, 0));
	float3 bitangent = normalize(float3(0, TerrainHeight * SampleTerrain(i.terrainPos - float2(0, e), tmp, tmp) - y, -e));
	float3 normal    = normalize(cross(tangent, bitangent));

	float4 col = 0;
	float4 mask = 0;
	float3 bump = 0;

	// tri-planar
	float3 blend = abs(normal);
	blend *= blend * blend;
	blend *= blend * blend;
	blend /= dot(blend, 1);

	float3 wp = .2 * (i.worldPos.xyz + Camera.Position);
	
	triplanar(2, 0, wp, blend, col, mask, bump, (1 - mountain) * (1 - lake)); // grass
	triplanar(2, 1, wp, blend, col, mask, bump, lake); // dirt
	triplanar(2, 3, wp, blend, col, mask, bump, mountain * (1 - lake)); // snow

	bump = normalize(bump);
	normal = normalize(tangent * bump.x + bitangent * bump.y + normal * bump.z);

	MaterialInfo material;
	material.diffuse = DiffuseAndSpecularFromMetallic(col.rgb, 0, material.specular, material.oneMinusReflectivity);
	material.perceptualRoughness = mask.r;
	material.roughness = max(.002, material.perceptualRoughness * material.perceptualRoughness);
	material.occlusion = mask.b;
	material.emission = 0;
	float3 eval = EvaluateLighting(material, i.worldPos.xyz, normal, view, i.worldPos.w);
	ApplyScattering(eval, i.screenPos.xy / i.screenPos.w, i.worldPos.w);
	color = float4(eval, 1);
	depthNormal = float4(normal * .5 + .5, i.worldPos.w);
}
#endif