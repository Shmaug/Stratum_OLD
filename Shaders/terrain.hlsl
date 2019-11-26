#pragma vertex vsmain
#pragma fragment fsmain

#pragma array MainTextures 8
#pragma array NormalTextures 8
#pragma array MaskTextures 8

#pragma multi_compile DEPTH_PASS

#pragma render_queue 1000

#pragma static_sampler Sampler
#pragma static_sampler ShadowSampler maxAnisotropy=0 maxLod=0 addressMode=clamp_border borderColor=float_opaque_white compareOp=greater

#include <shadercompat.h>

// per-object
[[vk::binding(OBJECT_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<float4> Nodes : register(t8);
[[vk::binding(LIGHT_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<GPULight> Lights : register(t1);
[[vk::binding(SHADOW_ATLAS_BINDING, PER_OBJECT)]] Texture2D<float> ShadowAtlas : register(t2);
[[vk::binding(SHADOW_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<ShadowData> Shadows : register(t3);
// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);
// per-material
[[vk::binding(BINDING_START + 0, PER_MATERIAL)]] Texture2D<float4> ReflectionTexture  : register(t4);
[[vk::binding(BINDING_START + 1, PER_MATERIAL)]] Texture2D<float4> MainTextures[8]    : register(t5);
[[vk::binding(BINDING_START + 9, PER_MATERIAL)]] Texture2D<float4> NormalTextures[8]  : register(t13);
[[vk::binding(BINDING_START + 17, PER_MATERIAL)]] Texture2D<float4> MaskTextures[8]    : register(t21); // rgb -> rough, height, ao
[[vk::binding(BINDING_START + 18, PER_MATERIAL)]] SamplerState Sampler : register(s0);
[[vk::binding(BINDING_START + 19, PER_MATERIAL)]] SamplerComparisonState ShadowSampler : register(s1);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4x4 ObjectToWorld;
	float4x4 WorldToObject;
	float ReflectionStrength;
	uint LightCount;
	float2 ShadowTexelSize;
	float TerrainHeight;
};

#include "brdf.hlsli"
#include "noise.hlsli"

struct v2f {
	float4 position : SV_Position;
#ifdef DEPTH_PASS
	float depth : TEXCOORD0;
#else
	float3 worldPos : TEXCOORD0;
	float4 screenPos : TEXCOORD1;
	float2 terrainPos : TEXCOORD2;
#endif
};

float SampleTerrain(float2 p) {
	float n = .25*fbm3(p * .01);

	float b = billow2(p * .00015);
	b = tanh(70 * (b + .1));

	n += .75*ridged8(p * .0015) * (b*.5 + .5);

	return TerrainHeight * n;
}

void triplanar(uint index, float3 p, float3 blend, out float4 color, out float4 mask, out float3 bump) {
	color = 0;
	mask = 0;
	bump = 0;
	if (blend.x > 0) {
		color += blend.x * MainTextures[index].Sample(Sampler, p.yz);
		mask  += blend.x * MaskTextures[index].Sample(Sampler, p.yz);
		bump  += blend.x * (NormalTextures[index].Sample(Sampler, p.yz).xyz * 2 - 1);
	}
	if (blend.y > 0) {
		color += blend.y * MainTextures[index].Sample(Sampler, p.xz);
		mask  += blend.y * MaskTextures[index].Sample(Sampler, p.xz);
		bump  += blend.y * (NormalTextures[index].Sample(Sampler, p.xz).xyz * 2 - 1);
	}
	if (blend.z > 0) {
		color += blend.z * MainTextures[index].Sample(Sampler, p.xy);
		mask  += blend.z * MaskTextures[index].Sample(Sampler, p.xy);
		bump  += blend.z * (NormalTextures[index].Sample(Sampler, p.xy).xyz * 2 - 1);
	}
}

v2f vsmain(
	uint v : SV_VertexID,
	uint instance : SV_InstanceID ) {
	
	float3 vertex = float3(v % 17, 0, v / 17) / 16.0;
	vertex.xz -= .5;
	vertex = vertex * Nodes[instance].w + Nodes[instance].xyz;
	vertex.y = SampleTerrain(vertex.xz);

	float4 worldPos = mul(ObjectToWorld, float4(vertex, 1.0));
	worldPos.xyz -= Camera.Position;

	v2f o;
	o.position = mul(Camera.ViewProjection, worldPos);
	#ifdef DEPTH_PASS
	o.depth = (Camera.ProjParams.w ? o.position.z * (Camera.Viewport.w - Camera.Viewport.z) + Camera.Viewport.z : o.position.w) / Camera.Viewport.w;
	#else
	o.worldPos = worldPos.xyz;
	o.screenPos = o.position;
	o.terrainPos = vertex.xz;
	#endif
	return o;
}

#ifdef DEPTH_PASS
float4 fsmain(in float depth : TEXCOORD0) : SV_Target0 { return depth; }
#else
void fsmain(v2f i,
	out float4 color : SV_Target0,
	out float4 depthNormal : SV_Target1) {
	
	float3 view;
	float depth;
	if (Camera.ProjParams.w) {
		view = float3(i.screenPos.xy / i.screenPos.w, Camera.Viewport.z);
		view.x *= Camera.ProjParams.x; // aspect
		view.xy *= Camera.ProjParams.y; // ortho size
		view = mul(float4(view, 1), Camera.View).xyz;
		view = -view;
		depth = i.screenPos.z * (Camera.Viewport.w - Camera.Viewport.z) + Camera.Viewport.z;
	} else {
		view = normalize(-i.worldPos.xyz);
		depth = i.screenPos.w;
	}
	depth /= Camera.Viewport.w;

	const float e = .1;
	float y = SampleTerrain(i.terrainPos);
	float3 tangent   = normalize(float3(e, SampleTerrain(i.terrainPos + float2(e, 0)) - y, 0));
	float3 bitangent = normalize(float3(0, SampleTerrain(i.terrainPos - float2(0, e)) - y, -e));
	float3 normal    = normalize(cross(tangent, bitangent));

	float4 col = 0;
	float4 mask = 0;
	float3 bump = 0;


	// tri-planar
	float3 blend = abs(normal);
	blend *= blend;
	blend /= dot(blend, 1);

	float3 wp = frac(i.worldPos + Camera.Position);

	triplanar(0, wp, blend, col, mask, bump);

	bump = normalize(bump);
	normal = normalize(tangent * bump.x + bitangent * bump.y + normal * bump.z);

	MaterialInfo material;
	material.diffuse = DiffuseAndSpecularFromMetallic(col.rgb, 0, material.specular, material.oneMinusReflectivity);
	material.perceptualRoughness = mask.r;
	material.roughness = max(.002, material.perceptualRoughness * material.perceptualRoughness);
	material.occlusion = mask.b;
	material.emission = 0;
	float3 eval = EvaluateLighting(material, i.worldPos + Camera.Position, normal, view, depth);
	color = float4(eval, 1);
	depthNormal = float4(normal * .5 + .5, depth);
}
#endif