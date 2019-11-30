#pragma vertex vsmain
#pragma fragment fsmain

#pragma multi_compile DEPTH_PASS

#pragma multi_compile TWO_SIDED
#pragma multi_compile NORMAL_MAP
#pragma multi_compile COLOR_MAP
#pragma multi_compile SPECGLOSS_MAP
#pragma multi_compile OCCLUSION_MAP
#pragma multi_compile EMISSION

#pragma render_queue 1000

#pragma static_sampler Sampler
#pragma static_sampler ShadowSampler maxAnisotropy=0 maxLod=0 addressMode=clamp_border borderColor=float_opaque_white compareOp=greater


#if defined(NORMAL_MAP) || defined(COLOR_MAP) || defined(EMISSION) || defined(SPECGLOSS_MAP) || defined(OCCLUSION_MAP)
#define NEED_TEXCOORD
#endif
#ifdef NORMAL_MAP
#define NEED_TANGENT
#endif

#include <shadercompat.h>

// per-object
[[vk::binding(OBJECT_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<ObjectBuffer> Instances : register(t0);
[[vk::binding(LIGHT_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<GPULight> Lights : register(t1);
[[vk::binding(SHADOW_ATLAS_BINDING, PER_OBJECT)]] Texture2D<float> ShadowAtlas : register(t2);
[[vk::binding(SHADOW_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<ShadowData> Shadows : register(t3);
// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);
// per-material
[[vk::binding(BINDING_START + 0, PER_MATERIAL)]] Texture2D<float4> EnvironmentTexture	: register(t4);
[[vk::binding(BINDING_START + 1, PER_MATERIAL)]] Texture2D<float4> MainTexture			: register(t5);
[[vk::binding(BINDING_START + 2, PER_MATERIAL)]] Texture2D<float4> NormalTexture		: register(t6);
[[vk::binding(BINDING_START + 3, PER_MATERIAL)]] Texture2D<float4> EmissionTexture		: register(t7);
[[vk::binding(BINDING_START + 4, PER_MATERIAL)]] Texture2D<float4> SpecGlossTexture		: register(t8);
[[vk::binding(BINDING_START + 5, PER_MATERIAL)]] Texture2D<float4> OcclusionTexture		: register(t9);
[[vk::binding(BINDING_START + 6, PER_MATERIAL)]] SamplerState Sampler : register(s0);
[[vk::binding(BINDING_START + 7, PER_MATERIAL)]] SamplerComparisonState ShadowSampler : register(s1);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4 Color;
	float Metallic;
	float Roughness;
	float3 AmbientLight;
	uint LightCount;
	float2 ShadowTexelSize;
#ifdef NORMAL_MAP
	float BumpStrength;
#endif
#ifdef EMISSION
	float3 Emission;
#endif
};

#include "util.hlsli"
#include "brdf.hlsli"

struct v2f {
	float4 position : SV_Position;
#ifdef DEPTH_PASS
	float depth : TEXCOORD0;
#else
	float3 worldPos : TEXCOORD0;
	float4 screenPos : TEXCOORD1;
	float3 normal : NORMAL;
	#ifdef NEED_TANGENT
	float3 tangent : TANGENT;
	#endif
	#ifdef NEED_TEXCOORD
	float2 texcoord : TEXCOORD2;
	#endif
#endif
};

v2f vsmain(
	[[vk::location(0)]] float3 vertex : POSITION,
	[[vk::location(1)]] float3 normal : NORMAL,
	#ifdef NEED_TANGENT
	[[vk::location(2)]] float4 tangent : TANGENT,
	#endif
	#ifdef NEED_TEXCOORD
	[[vk::location(3)]] float2 texcoord : TEXCOORD0,
	#endif
	uint instance : SV_InstanceID ) {
	v2f o;
	
	float4 worldPos = mul(Instances[instance].ObjectToWorld, float4(vertex, 1.0));
	worldPos.xyz -= Camera.Position;

	o.position = mul(Camera.ViewProjection, worldPos);
	#ifdef DEPTH_PASS
	o.depth = (Camera.ProjParams.w ? o.position.z * (Camera.Viewport.w - Camera.Viewport.z) + Camera.Viewport.z : o.position.w) / Camera.Viewport.w;
	#else
	o.screenPos = ComputeScreenPos(o.position);
	o.worldPos = worldPos.xyz;

	o.normal = mul(float4(normal, 1), Instances[instance].WorldToObject).xyz;
	#ifdef NEED_TANGENT
	o.tangent = mul(tangent, Instances[instance].WorldToObject).xyz * tangent.w;
	#endif
	#ifdef NEED_TEXCOORD
	o.texcoord = texcoord;
	#endif

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
	ComputeDepth(i.worldPos, i.screenPos, view, depth);

	#ifdef COLOR_MAP
	float4 col = MainTexture.Sample(Sampler, i.texcoord) * Color;
	#else
	float4 col = Color;
	#endif

	float3 normal = normalize(i.normal);
	#ifdef TWO_SIDED
	if (dot(normal, view) < 0) normal = -normal;
	#endif

	#ifdef NORMAL_MAP
	float4 bump = NormalTexture.Sample(Sampler, i.texcoord);
	bump.xyz = bump.xyz * 2 - 1;
	float3 tangent = normalize(i.tangent);
	float3 bitangent = normalize(cross(i.normal, i.tangent));
	bump.xy *= BumpStrength;
	normal = normalize(tangent * bump.x + bitangent * bump.y + normal * bump.z);
	#endif

	MaterialInfo material;
	#ifdef SPECGLOSS_MAP
	float4 specGloss = SpecGlossTexture.Sample(Sampler, i.texcoord);
	material.perceptualRoughness = (1.0 - (1.0 - Roughness) * specGloss.a);
	material.diffuse = DiffuseAndSpecularFromSpecular(col.rgb, specGloss.rgb, material.specular, material.oneMinusReflectivity);
	#else
	material.diffuse = DiffuseAndSpecularFromMetallic(col.rgb, Metallic, material.specular, material.oneMinusReflectivity);
	material.perceptualRoughness = Roughness * .99;
	#endif
	material.roughness = max(.002, material.perceptualRoughness * material.perceptualRoughness);
	#ifdef OCCLUSION_MAP
	material.occlusion = OcclusionTexture.Sample(Sampler, i.texcoord).r;
	#else
	material.occlusion = 1;
	#endif
	#ifdef EMISSION
	material.emission = Emission * EmissionTexture.Sample(Sampler, i.texcoord).rgb;
	#else
	material.emission = 0;
	#endif
	
	float3 eval = EvaluateLighting(material, i.worldPos + Camera.Position, normal, view, depth);
	color = float4(eval, 1);
	depthNormal = float4(normal * .5 + .5, depth);
}
#endif