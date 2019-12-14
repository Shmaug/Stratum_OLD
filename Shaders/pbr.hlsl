#pragma vertex vsmain
#pragma fragment fsmain

#pragma multi_compile PASS_DEPTH
#pragma multi_compile ALPHA_CLIP

#pragma multi_compile TWO_SIDED
#pragma multi_compile COLOR_MAP
#pragma multi_compile NORMAL_MAP
#pragma multi_compile MASK_MAP
#pragma multi_compile EMISSION

#pragma multi_compile ENABLE_SCATTERING ENVIRONMENT_TEXTURE

#pragma render_queue 1000

#pragma static_sampler Sampler
#pragma static_sampler ShadowSampler maxAnisotropy=0 maxLod=0 addressMode=clamp_border borderColor=float_opaque_white compareOp=less
#pragma static_sampler AtmosphereSampler maxAnisotropy=0 addressMode=clamp_edge

#if defined(ALPHA_CLIP) || (!defined(PASS_DEPTH) && (defined(NORMAL_MAP) || defined(COLOR_MAP) || defined(EMISSION) || defined(MASK_MAP)))
#define NEED_TEXCOORD
#endif
#ifdef NORMAL_MAP
#define NEED_TANGENT
#endif

#include "include/shadercompat.h"

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
[[vk::binding(BINDING_START + 2, PER_MATERIAL)]] Texture2D<float4> MaskTexture			: register(t6); // rgba ->ao, rough, metallic (glTF spec.)
[[vk::binding(BINDING_START + 3, PER_MATERIAL)]] Texture2D<float4> EmissionTexture		: register(t8);

[[vk::binding(BINDING_START + 4, PER_MATERIAL)]] Texture3D<float3> InscatteringLUT		: register(t9);
[[vk::binding(BINDING_START + 5, PER_MATERIAL)]] Texture3D<float3> ExtinctionLUT		: register(t10);
[[vk::binding(BINDING_START + 6, PER_MATERIAL)]] Texture2D<float>  LightShaftLUT		: register(t11);

[[vk::binding(BINDING_START + 7, PER_MATERIAL)]] Texture2D<float4> EnvironmentTexture	: register(t12);

[[vk::binding(BINDING_START + 8 , PER_MATERIAL)]] SamplerState Sampler : register(s0);
[[vk::binding(BINDING_START + 9 , PER_MATERIAL)]] SamplerComparisonState ShadowSampler : register(s1);
[[vk::binding(BINDING_START + 10, PER_MATERIAL)]] SamplerState AtmosphereSampler : register(s2);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4 Color;
	float Metallic;
	float Roughness;
#ifdef NORMAL_MAP
	float BumpStrength;
#endif
#ifdef EMISSION
	float3 Emission;
#endif

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
	#ifndef PASS_DEPTH
	float4 screenPos : TEXCOORD1;
	float3 normal : NORMAL;
	#endif
	#ifdef NEED_TANGENT
	float3 tangent : TANGENT;
	#endif
	#ifdef NEED_TEXCOORD
	float2 texcoord : TEXCOORD2;
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
	
	float4x4 ct = float4x4(
		1,0,0,-Camera.Position.x,
		0,1,0,-Camera.Position.y,
		0,0,1,-Camera.Position.z,
		0,0,0,1);
	float4 worldPos = mul(mul(ct, Instances[instance].ObjectToWorld), float4(vertex, 1.0));

	o.position = mul(Camera.ViewProjection, worldPos);
	o.worldPos = float4(worldPos.xyz, LinearDepth01(o.position.z));
	
	#ifndef PASS_DEPTH
	o.screenPos = ComputeScreenPos(o.position);
	o.normal = mul(float4(normal, 1), Instances[instance].WorldToObject).xyz;
	#endif
	
	#ifdef NEED_TANGENT
	o.tangent = mul(tangent, Instances[instance].WorldToObject).xyz * tangent.w;
	#endif
	
	#ifdef NEED_TEXCOORD
	o.texcoord = texcoord;
	#endif

	return o;
}

#ifdef PASS_DEPTH
#ifdef ALPHA_CLIP
float fsmain(in float4 worldPos : TEXCOORD0, in float2 texcoord : TEXCOORD2) : SV_Target0{
	clip((MainTexture.Sample(Sampler, texcoord) * Color).a - .75);
#else
float fsmain(in float4 worldPos : TEXCOORD0) : SV_Target0 {
#endif
	return worldPos.w;
}
#else
void fsmain(v2f i,
	out float4 color : SV_Target0,
	out float4 depthNormal : SV_Target1) {

	float3 view = ComputeView(i.worldPos.xyz, i.screenPos);

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

	#ifdef MASK_MAP
	float4 mask = MaskTexture.Sample(Sampler, i.texcoord);
	#else
	float4 mask = 1;
	#endif

	MaterialInfo material;
	material.diffuse = DiffuseAndSpecularFromMetallic(col.rgb, Metallic*mask.b, material.specular, material.oneMinusReflectivity);
	material.perceptualRoughness = Roughness * mask.g * .99;
	material.occlusion = mask.r;
	material.roughness = max(.002, material.perceptualRoughness * material.perceptualRoughness);
	#ifdef EMISSION
	material.emission = Emission * EmissionTexture.Sample(Sampler, i.texcoord).rgb;
	#else
	material.emission = 0;
	#endif
	
	float3 eval = EvaluateLighting(material, i.worldPos.xyz, normal, view, i.worldPos.w);

	#ifdef ENABLE_SCATTERING
	ApplyScattering(eval, i.screenPos.xy / i.screenPos.w, i.worldPos.w);
	#endif

	color = float4(eval, col.a);
	depthNormal = float4(normal * .5 + .5, i.worldPos.w);
}
#endif