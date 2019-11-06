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

#include <shadercompat.h>
#include "pbr.hlsli"

// per-object
[[vk::binding(OBJECT_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<ObjectBuffer> Objects : register(t0);
[[vk::binding(LIGHT_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<GPULight> Lights : register(t1);
// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);
// per-material
[[vk::binding(BINDING_START + 0, PER_MATERIAL)]] Texture2D<float4> MainTexture : register(t2);
[[vk::binding(BINDING_START + 1, PER_MATERIAL)]] Texture2D<float4> NormalTexture : register(t3);
[[vk::binding(BINDING_START + 2, PER_MATERIAL)]] Texture2D<float4> BrdfTexture : register(t4);
[[vk::binding(BINDING_START + 3, PER_MATERIAL)]] Texture2D<float4> EnvironmentTexture : register(t5);
[[vk::binding(BINDING_START + 4, PER_MATERIAL)]] Texture2D<float4> EmissionTexture : register(t6);
[[vk::binding(BINDING_START + 5, PER_MATERIAL)]] Texture2D<float4> SpecGlossTexture : register(t7);
[[vk::binding(BINDING_START + 6, PER_MATERIAL)]] Texture2D<float4> OcclusionTexture : register(t8);
[[vk::binding(BINDING_START + 7, PER_MATERIAL)]] SamplerState Sampler : register(s0);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4 Color;
	float Metallic;
	float Roughness;
	float EnvironmentStrength;
	uint LightCount;
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
	float3 normal : NORMAL;
#ifdef NORMAL_MAP
	float4 tangent : TANGENT;
#endif
#if defined(NORMAL_MAP) || defined(COLOR_MAP) || defined(EMISSION) || defined(SPECGLOSS_MAP) || defined(OCCLUSION_MAP)
	float2 texcoord : TEXCOORD1;
#endif
};

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
	
	float4 wp = mul(Objects[instance].ObjectToWorld, float4(vertex, 1.0));
	o.position = mul(Camera.ViewProjection, wp);
	o.worldPos = wp.xyz;

	o.normal = mul(float4(normal, 1), Objects[instance].WorldToObject).xyz;
	#ifdef NORMAL_MAP
	o.tangent = mul(tangent, Objects[instance].WorldToObject) * tangent.w;
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

	float3 view = Camera.Position - i.worldPos;
	float depth = length(view);
	view /= depth;

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
		float3 toLight = Lights[l].Direction;
		float attenuation = 1;
		if (Lights[l].Type > LIGHT_SUN) {
			toLight = Lights[l].WorldPosition - i.worldPos;
			float d2 = dot(toLight, toLight);
			toLight /= sqrt(d2);

			attenuation = 1 / max(d2, .0001);
			float f = d2 * Lights[l].InvSqrRange;
			f = saturate(1 - f * f);
			attenuation *= f * f;

			if (Lights[l].Type == LIGHT_SPOT) {
				float a = saturate(dot(toLight, Lights[l].Direction) * Lights[l].SpotAngleScale + Lights[l].SpotAngleOffset);
				attenuation *= a * a;
			}
		}
		eval += attenuation * Lights[l].Color * ShadePoint(material, toLight, normal, view);
	}

	float3 reflection = normalize(reflect(-view, normal));

	uint texWidth, texHeight, numMips;
	EnvironmentTexture.GetDimensions(0, texWidth, texHeight, numMips);
	float2 envuv = float2(atan2(reflection.z, reflection.x) * INV_PI * .5 + .5, acos(reflection.y) * INV_PI);
	float3 env = EnvironmentTexture.SampleLevel(Sampler, envuv, saturate(material.perceptualRoughness) * numMips).rgb * EnvironmentStrength;
	
	eval += ShadeIndirect(material, normal, view, env, env);

#	ifdef OCCLUSION_MAP
	eval *= OcclusionTexture.Sample(Sampler, i.texcoord).rgb;
	#endif

	#ifdef EMISSION
	eval.rgb += Emission * EmissionTexture.Sample(Sampler, i.texcoord).rgb;
	#endif

	color = float4(eval, 1);
	depthNormal = float4(normal * .5 + .5, depth / Camera.Viewport.w);
}