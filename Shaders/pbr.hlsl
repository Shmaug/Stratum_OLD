#pragma vertex vsmain
#pragma fragment fsmain

#pragma multi_compile NORMAL_MAP
#pragma multi_compile COLOR_MAP
#pragma multi_compile TWO_SIDED
#pragma multi_compile SPECGLOSS_MAP
#pragma multi_compile OCCLUSION_MAP
#pragma multi_compile EMISSION

#pragma render_queue 1000

#pragma static_sampler Sampler
#pragma static_sampler ShadowSampler maxAnisotropy=0 maxLod=0 addressMode=clamp_border borderColor=float_opaque_white compareOp=greater

#include <shadercompat.h>
#include "brdf.hlsli"

// per-object
[[vk::binding(OBJECT_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<ObjectBuffer> Instances : register(t0);
[[vk::binding(LIGHT_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<GPULight> Lights : register(t1);
[[vk::binding(SHADOW_ATLAS_BINDING, PER_OBJECT)]] Texture2D<float> ShadowAtlas : register(t2);
[[vk::binding(SHADOW_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<ShadowData> Shadows : register(t3);
// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);
// per-material
[[vk::binding(BINDING_START + 0, PER_MATERIAL)]] Texture2D<float4> MainTexture : register(t4);
[[vk::binding(BINDING_START + 1, PER_MATERIAL)]] Texture2D<float4> NormalTexture : register(t5);
[[vk::binding(BINDING_START + 2, PER_MATERIAL)]] Texture2D<float4> BrdfTexture : register(t6);
[[vk::binding(BINDING_START + 3, PER_MATERIAL)]] Texture2D<float4> EnvironmentTexture : register(t7);
[[vk::binding(BINDING_START + 4, PER_MATERIAL)]] Texture2D<float4> EmissionTexture : register(t8);
[[vk::binding(BINDING_START + 5, PER_MATERIAL)]] Texture2D<float4> SpecGlossTexture : register(t9);
[[vk::binding(BINDING_START + 6, PER_MATERIAL)]] Texture2D<float4> OcclusionTexture : register(t10);
[[vk::binding(BINDING_START + 7, PER_MATERIAL)]] SamplerState Sampler : register(s0);
[[vk::binding(BINDING_START + 8, PER_MATERIAL)]] SamplerComparisonState ShadowSampler : register(s1);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4 Color;
	float Metallic;
	float Roughness;
	float EnvironmentStrength;
	uint LightCount;
	float2 ShadowTexelSize;
#ifdef NORMAL_MAP
	float BumpStrength;
#endif
#ifdef EMISSION
	float3 Emission;
#endif
};

struct v2f {
	float4 position : SV_Position;
	float3 worldPos : TEXCOORD0;
	float4 screenPos : TEXCOORD1;
	float3 normal : NORMAL;
#ifdef NORMAL_MAP
	float4 tangent : TANGENT;
#endif
#if defined(NORMAL_MAP) || defined(COLOR_MAP) || defined(EMISSION) || defined(SPECGLOSS_MAP) || defined(OCCLUSION_MAP)
	float2 texcoord : TEXCOORD2;
#endif
};

float LightAttenuation(uint li, float3 worldPos, float3 normal, float depth, out float3 L) {
	GPULight l = Lights[li];
	L = l.Direction;
	float attenuation = 1;

	int si = -1;
	if (depth < l.CascadeSplits[3]) si = l.ShadowIndex[3];
	if (depth < l.CascadeSplits[2]) si = l.ShadowIndex[2];
	if (depth < l.CascadeSplits[1]) si = l.ShadowIndex[1];
	if (depth < l.CascadeSplits[0]) si = l.ShadowIndex[0];

	if (si >= 0) {
		ShadowData s = Shadows[si];

		float4 shadowPos = mul(s.WorldToShadow, float4(worldPos + normal * .005, 1));

		float z = s.Proj.x ? shadowPos.z * (s.Proj.w - s.Proj.z) + s.Proj.z : shadowPos.w;
		z *= s.Proj.y;
		z -= .001;

		shadowPos /= shadowPos.w;

		if (z > 0 && z < 1 && shadowPos.x > -1 && shadowPos.y > -1 && shadowPos.x < 1 && shadowPos.y < 1) {
			float2 shadowUV = shadowPos.xy * .5 + .5;
			shadowUV = shadowUV * s.ShadowST.xy + s.ShadowST.zw;

			float shadow = 0;
			shadow += ShadowAtlas.SampleCmpLevelZero(ShadowSampler, shadowUV, z);
			shadow += ShadowAtlas.SampleCmpLevelZero(ShadowSampler, shadowUV + ShadowTexelSize, z);
			shadow += ShadowAtlas.SampleCmpLevelZero(ShadowSampler, shadowUV - ShadowTexelSize, z);
			shadow += ShadowAtlas.SampleCmpLevelZero(ShadowSampler, shadowUV + float2(-ShadowTexelSize.x,  ShadowTexelSize.y), z);
			shadow += ShadowAtlas.SampleCmpLevelZero(ShadowSampler, shadowUV + float2( ShadowTexelSize.x, -ShadowTexelSize.y), z);
			shadow += ShadowAtlas.SampleCmpLevelZero(ShadowSampler, shadowUV + float2( ShadowTexelSize.x, 0), z);
			shadow += ShadowAtlas.SampleCmpLevelZero(ShadowSampler, shadowUV + float2(-ShadowTexelSize.x, 0), z);
			shadow += ShadowAtlas.SampleCmpLevelZero(ShadowSampler, shadowUV + float2(0,  ShadowTexelSize.y), z);
			shadow += ShadowAtlas.SampleCmpLevelZero(ShadowSampler, shadowUV + float2(0, -ShadowTexelSize.y), z);
			shadow *= 1.0 / 9.0;

			attenuation = 1 - shadow;
		}
	}

	if (l.Type > LIGHT_SUN) {
		L = l.WorldPosition - worldPos;
		float d2 = dot(L, L);
		L /= sqrt(d2);
		attenuation *= 1 / max(d2, .0001);
		float f = d2 * l.InvSqrRange;
		f = saturate(1 - f * f);
		attenuation *= f * f;
		if (l.Type == LIGHT_SPOT) {
			float a = saturate(dot(L, l.Direction) * l.SpotAngleScale + l.SpotAngleOffset);
			attenuation *= a * a;
		}
	}

	return attenuation;
}

v2f vsmain(
	[[vk::location(0)]] float3 vertex : POSITION,
	[[vk::location(1)]] float3 normal : NORMAL,
	#ifdef NORMAL_MAP
	[[vk::location(2)]] float4 tangent : TANGENT,
	#endif
	#if defined(NORMAL_MAP) || defined(COLOR_MAP) || defined(EMISSION) || defined(SPECGLOSS_MAP) || defined(OCCLUSION_MAP)
	[[vk::location(3)]] float2 texcoord : TEXCOORD0,
	#endif
	uint instance : SV_InstanceID
	) {
	v2f o;
	
	float4 worldPos = mul(Instances[instance].ObjectToWorld, float4(vertex, 1.0));
	o.position = mul(Camera.ViewProjection, worldPos);
	o.screenPos = o.position;
	o.worldPos = worldPos.xyz;

	o.normal = mul(float4(normal, 1), Instances[instance].WorldToObject).xyz;
	#ifdef NORMAL_MAP
	o.tangent = mul(tangent, Instances[instance].WorldToObject) * tangent.w;
	#endif
	#if defined(NORMAL_MAP) || defined(COLOR_MAP) || defined(EMISSION)
	o.texcoord = texcoord;
	#endif
	return o;
}

void fsmain(v2f i,
	out float4 color : SV_Target0,
	out float4 depthNormal : SV_Target1) {
	#ifdef COLOR_MAP
	float4 col = MainTexture.Sample(Sampler, i.texcoord) * Color;
	#else
	float4 col = Color;
	#endif

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
	material.diffuseColor = DiffuseAndSpecularFromSpecular(col.rgb, specGloss.rgb, material.specularColor, material.oneMinusReflectivity);
	#else
	material.diffuseColor = DiffuseAndSpecularFromMetallic(col.rgb, Metallic, material.specularColor, material.oneMinusReflectivity);
	material.perceptualRoughness = Roughness * .99;
	#endif
	material.roughness = max(.002, material.perceptualRoughness * material.perceptualRoughness);
	material.nv = abs(dot(normal, view));

	float3 eval = 0;

	for (uint l = 0; l < LightCount; l++) {
		float3 L;
		float attenuation = LightAttenuation(l, i.worldPos, normal, depth, L);
		if (attenuation > 0.001)
			eval += attenuation * Lights[l].Color* BRDF(material, L, normal, view);
	}

	float3 reflection = normalize(reflect(-view, normal));

	uint texWidth, texHeight, numMips;
	EnvironmentTexture.GetDimensions(0, texWidth, texHeight, numMips);
	float2 envuv = float2(atan2(reflection.z, reflection.x) * INV_PI * .5 + .5, acos(reflection.y) * INV_PI);
	float3 env = EnvironmentTexture.SampleLevel(Sampler, envuv, saturate(material.perceptualRoughness) * numMips).rgb * EnvironmentStrength;
	
	eval += BRDFIndirect(material, normal, view, env, env);

	#ifdef OCCLUSION_MAP
	eval *= OcclusionTexture.Sample(Sampler, i.texcoord).rgb;
	#endif

	#ifdef EMISSION
	eval.rgb += Emission * EmissionTexture.Sample(Sampler, i.texcoord).rgb;
	#endif

	color = float4(eval, 1);
	depthNormal = float4(normal * .5 + .5, depth);
}