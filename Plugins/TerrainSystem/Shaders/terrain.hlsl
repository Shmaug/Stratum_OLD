#pragma vertex vsmain
#pragma fragment fsmain main
#pragma fragment fsdepth depth

#pragma array MainTextures 8
#pragma array NormalTextures 8
#pragma array MaskTextures 8

#pragma multi_compile ENABLE_SCATTERING ENVIRONMENT_TEXTURE

#pragma render_queue 1000

#pragma static_sampler Sampler
#pragma static_sampler ShadowSampler maxAnisotropy=0 maxLod=0 addressMode=clamp_border borderColor=float_opaque_white compareOp=less
#pragma static_sampler AtmosphereSampler maxAnisotropy=0 addressMode=clamp_edge

#include "include/shadercompat.h"

// per-object
[[vk::binding(INSTANCE_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<float4> Nodes : register(t8);
[[vk::binding(LIGHT_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<GPULight> Lights : register(t1);
[[vk::binding(SHADOW_ATLAS_BINDING, PER_OBJECT)]] Texture2D<float> ShadowAtlas : register(t2);
[[vk::binding(SHADOW_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<ShadowData> Shadows : register(t3);
// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);
// per-material
[[vk::binding(BINDING_START + 0, PER_MATERIAL)]] Texture2D<float4> MainTextures[8]		: register(t8);
[[vk::binding(BINDING_START + 1, PER_MATERIAL)]] Texture2D<float4> NormalTextures[8]	: register(t16);
[[vk::binding(BINDING_START + 2, PER_MATERIAL)]] Texture2D<float4> MaskTextures[8]		: register(t24); // rgba -> rough, height, ao, 1-metallic

[[vk::binding(BINDING_START + 3, PER_MATERIAL)]] Texture2D<float4> Heightmap			: register(t25);
[[vk::binding(BINDING_START + 4, PER_MATERIAL)]] Texture3D<float3> InscatteringLUT		: register(t26);
[[vk::binding(BINDING_START + 5, PER_MATERIAL)]] Texture3D<float3> ExtinctionLUT		: register(t27);
[[vk::binding(BINDING_START + 6, PER_MATERIAL)]] Texture2D<float>  LightShaftLUT		: register(t28);

[[vk::binding(BINDING_START + 7, PER_MATERIAL)]] Texture2D<float4> EnvironmentTexture	: register(t29);

[[vk::binding(BINDING_START + 8 , PER_MATERIAL)]] SamplerState Sampler : register(s0);
[[vk::binding(BINDING_START + 9 , PER_MATERIAL)]] SamplerComparisonState ShadowSampler : register(s1);
[[vk::binding(BINDING_START + 10, PER_MATERIAL)]] SamplerState AtmosphereSampler : register(s2);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	STRATUM_PUSH_CONSTANTS

	float4x4 ObjectToWorld;
	float4x4 WorldToObject;
	float TerrainSize;
	float TerrainHeight;
};

//#define SHOW_CASCADE_SPLITS

#include "include/util.hlsli"
#include "include/shadow.hlsli"
#include "include/brdf.hlsli"
#include "include/scatter.hlsli"

struct v2f {
	float4 position : SV_Position;
	float4 worldPos : TEXCOORD0;
	float4 screenPos : TEXCOORD1;
	float2 terrainPos : TEXCOORD2;
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

	float3 vertex = float3(v % 17, 0, v / 17) / 16.0;
	vertex.xz -= .5;
	vertex = vertex * Nodes[instance].w + Nodes[instance].xyz;

	float2 terrainPos = vertex.xz / TerrainSize * .5 + .5;
	vertex.y = TerrainHeight * Heightmap.SampleLevel(Sampler, terrainPos, 0);
	
	float4x4 ct = float4x4(1,0,0,-Camera.Position.x, 0,1,0,-Camera.Position.y, 0,0,1,-Camera.Position.z, 0,0,0,1);
	float4 worldPos = mul(mul(ct, ObjectToWorld), float4(vertex, 1.0));

	v2f o;
	o.position = mul(STRATUM_MATRIX_VP, worldPos);
	StratumOffsetClipPosStereo(o.position);
	o.worldPos = float4(worldPos.xyz, LinearDepth01(o.position.z));
	o.screenPos = ComputeScreenPos(o.position);
	o.terrainPos = terrainPos;
	return o;
}

float fsdepth(in float4 worldPos : TEXCOORD0) : SV_Target0 {
	return worldPos.w;
}

void fsmain(v2f i,
	out float4 color : SV_Target0,
	out float4 depthNormal : SV_Target1) {
	depthNormal = float4(cross(ddx(i.worldPos.xyz), ddy(i.worldPos.xyz)), i.worldPos.w);
	
	float3 view = ComputeView(i.worldPos.xyz, i.screenPos);
	
	float2 res;
	Heightmap.GetDimensions(res.x, res.y);
	res = TerrainSize / res;

	float4 yt = TerrainHeight * Heightmap.SampleLevel(Sampler, i.terrainPos, 0);
	float3 tangent   = normalize(float3(res.x, TerrainHeight * Heightmap.SampleLevel(Sampler, i.terrainPos, 0, int2(1,0)).x - yt.x,  0));
	float3 bitangent = normalize(float3(0,     TerrainHeight * Heightmap.SampleLevel(Sampler, i.terrainPos, 0, int2(0,-1)).x - yt.x, res.y));
	float3 normal    = normalize(cross(bitangent, tangent));

	float4 col = 0;
	float4 mask = 0;
	float3 bump = 0;

	// tri-planar
	float3 blend = abs(normal);
	blend *= blend * blend;
	blend *= blend * blend;
	blend /= dot(blend, 1);

	float3 wp = .2 * (i.worldPos.xyz + Camera.Position);
	
	triplanar(0, 1, wp, blend, col, mask, bump, 1);

	bump = normalize(bump);
	normal = normalize(tangent * bump.x + bitangent * bump.y + normal * bump.z);

	MaterialInfo material;
	material.diffuse = DiffuseAndSpecularFromMetallic(col.rgb, 1-mask.a, material.specular, material.oneMinusReflectivity);
	material.perceptualRoughness = mask.r;
	material.roughness = max(.002, material.perceptualRoughness * material.perceptualRoughness);
	material.occlusion = mask.b;
	material.emission = 0;
	float3 eval = EvaluateLighting(material, i.worldPos.xyz, normal, view, i.worldPos.w);

	#ifdef ENABLE_SCATTERING
	ApplyScattering(eval, i.screenPos.xy / i.screenPos.w, i.worldPos.w);
	#endif

	color = float4(eval, 1);
}