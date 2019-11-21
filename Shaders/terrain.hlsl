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
[[vk::binding(BINDING_START + 3, PER_MATERIAL)]] Texture2D<float4> MaskTexture : register(t7); // rgb -> rough, height, ao
[[vk::binding(BINDING_START + 6, PER_MATERIAL)]] SamplerState Sampler : register(s0);
[[vk::binding(BINDING_START + 7, PER_MATERIAL)]] SamplerComparisonState ShadowSampler : register(s1);

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
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
#endif
};

v2f vsmain(
	uint v : SV_VertexID,
	uint instance : SV_InstanceID ) {

	float3 vertex = float3(v % GRID_SIZE, 0, v / GRID_SIZE);
	vertex.xz = vertex.xz / (GRID_SIZE - 2) - .5;

	vertex = vertex * Nodes[instance].w + Nodes[instance].xyz;

	float3 noise = fbm(vertex.xz);
	noise.x *= .5;
	noise *= TerrainHeight;
	vertex.y = noise.x;
	float3 tangent = normalize(float3(1, noise.y, 0));
	float3 bitangent = normalize(float3(0, noise.z, 1));
	float3 normal = normalize(cross(bitangent, tangent));

	v2f o;
	float4 worldPos = mul(ObjectToWorld, float4(vertex, 1.0));
	worldPos.xyz -= Camera.Position;
	o.position = mul(Camera.ViewProjection, worldPos);
	#ifdef DEPTH_PASS
	o.depth = (Camera.ProjParams.w ? o.position.z * (Camera.Viewport.w - Camera.Viewport.z) + Camera.Viewport.z : o.position.w) / Camera.Viewport.w;
	#else
	o.worldPos = worldPos.xyz;
	o.screenPos = o.position;
	o.normal = mul(float4(normal, 1), WorldToObject).xyz;
	o.tangent = mul(float4(tangent, 1), WorldToObject).xyz;
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
		view = normalize(-i.worldPos);
		depth = i.screenPos.w;
	}
	depth /= Camera.Viewport.w;

	float3 normal = normalize(i.normal);
	float3 tangent = normalize(i.tangent);
	float3 bitangent = normalize(cross(tangent, normal));
	
	float4 col = 0;
	float3 bump = 0;
	float4 mask = 0;

	// tri-planar
	float3 blend = abs(normal);
	blend /= dot(blend * blend, 1);
	float3 sgn = sign(normal);

	float3 wp = frac(i.worldPos + Camera.Position);

	if (blend.x > 0) {
		col  += blend.x * MainTexture.Sample(Sampler, wp.yz);
		mask += blend.x * MaskTexture.Sample(Sampler, wp.yz);
		bump += blend.x * sgn.x * (NormalTexture.Sample(Sampler, wp.yz).xyz * 2 - 1);
	}
	if (blend.y > 0) {
		col  += blend.y * MainTexture.Sample(Sampler, wp.xz);
		mask += blend.y * MaskTexture.Sample(Sampler, wp.xz);
		bump += blend.y * sgn.y * (NormalTexture.Sample(Sampler, wp.xz).xyz * 2 - 1);
	}
	if (blend.z > 0) {
		col  += blend.z * MainTexture.Sample(Sampler, wp.xy);
		mask += blend.z * MaskTexture.Sample(Sampler, wp.xy);
		bump += blend.z * sgn.z * (NormalTexture.Sample(Sampler, wp.xy).xyz * 2 - 1);
	}
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