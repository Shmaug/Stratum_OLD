#pragma vertex vsmain
#pragma fragment fsmain

#pragma multi_compile DEPTH_PASS

#pragma render_queue 1000

#pragma static_sampler Sampler
#pragma static_sampler ShadowSampler maxAnisotropy=0 maxLod=0 addressMode=clamp_border borderColor=float_opaque_white compareOp=greater

#define GRID_SIZE 16

#include <shadercompat.h>

// per-object
[[vk::binding(OBJECT_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<float4> Nodes : register(t8);
[[vk::binding(LIGHT_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<GPULight> Lights : register(t1);
[[vk::binding(SHADOW_ATLAS_BINDING, PER_OBJECT)]] Texture2D<float> ShadowAtlas : register(t2);
[[vk::binding(SHADOW_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<ShadowData> Shadows : register(t3);
// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);
// per-material
[[vk::binding(BINDING_START + 0, PER_MATERIAL)]] Texture2D<float4> MainTexture : register(t4);
[[vk::binding(BINDING_START + 1, PER_MATERIAL)]] Texture2D<float4> NormalTexture : register(t5);
[[vk::binding(BINDING_START + 2, PER_MATERIAL)]] Texture2D<float4> ReflectionTexture : register(t6);
[[vk::binding(BINDING_START + 3, PER_MATERIAL)]] Texture2D<float4> MaskTexture : register(t7); // rgb -> rgh, hgt, ao
[[vk::binding(BINDING_START + 6, PER_MATERIAL)]] SamplerState Sampler : register(s0);
[[vk::binding(BINDING_START + 7, PER_MATERIAL)]] SamplerComparisonState ShadowSampler : register(s1);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4x4 ObjectToWorld;
	float4x4 WorldToObject;
	float ReflectionStrength;
	uint LightCount;
	float2 ShadowTexelSize;
};

#include "brdf.hlsli"

struct v2f {
	float4 position : SV_Position;
#ifdef DEPTH_PASS
	float depth : TEXCOORD0;
#else
	float3 worldPos : TEXCOORD0;
	float4 screenPos : TEXCOORD1;
	float3 normal : NORMAL;
#endif
};

v2f vsmain(
	uint v : SV_VertexID,
	uint instance : SV_InstanceID ) {

	float3 vertex = float3(v % GRID_SIZE, 0, v / GRID_SIZE) / (GRID_SIZE - 1) - .5;
	float3 normal = float3(0, 1, 0);

	vertex = vertex * Nodes[instance].w + Nodes[instance].xyz;

	v2f o;
	float4 worldPos = mul(ObjectToWorld, float4(vertex, 1.0));
	o.position = mul(Camera.ViewProjection, worldPos);
	#ifdef DEPTH_PASS
	o.depth = (Camera.ProjParams.w ? o.position.z * (Camera.Viewport.w - Camera.Viewport.z) + Camera.Viewport.z : o.position.w) / Camera.Viewport.w;
	#else
	o.worldPos = worldPos.xyz;
	o.screenPos = o.position;
	o.normal = mul(float4(normal, 1), WorldToObject).xyz;
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
		view = normalize(Camera.Position - i.worldPos);
		depth = i.screenPos.w;
	}
	depth /= Camera.Viewport.w;

	float4 col = MainTexture.Sample(Sampler, i.worldPos.xz);
	float4 bump = NormalTexture.Sample(Sampler, i.worldPos.xz);
	float4 mask = MaskTexture.Sample(Sampler, i.worldPos.xz);
	bump.xyz = bump.xyz * 2 - 1;

	float3 normal = normalize(i.normal);


	MaterialInfo material;
	material.diffuse = DiffuseAndSpecularFromMetallic(col.rgb, 0, material.specular, material.oneMinusReflectivity);
	material.perceptualRoughness = mask.r;
	material.roughness = max(.002, material.perceptualRoughness * material.perceptualRoughness);
	material.occlusion = mask.b;
	material.emission = 0;

	float3 eval = EvaluateLighting(material, i.worldPos, normal, view, depth);
	color = float4(eval, 1);
	depthNormal = float4(normal * .5 + .5, depth);
}

#endif