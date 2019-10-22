#pragma vertex vsmain
#pragma fragment fsmain

#pragma multi_compile NORMAL_MAP 
#pragma multi_compile COLOR_MAP
#pragma multi_compile TWO_SIDED
#pragma multi_compile EMISSION
#pragma multi_compile SPECULAR_MAP

#pragma render_queue 1000

#pragma static_sampler Sampler

#include <shadercompat.h>

#define PI 3.1415926535897932
#define INV_PI 0.31830988618
#define MIN_ROUGHNESS 0.04
#define unity_ColorSpaceDielectricSpec float4(0.04, 0.04, 0.04, 1.0 - 0.04) // standard dielectric reflectivity coef at incident angle (= 4%)

// per-object
[[vk::binding(OBJECT_BUFFER_BINDING, PER_OBJECT)]] ConstantBuffer<ObjectBuffer> Object : register(b0);
[[vk::binding(BINDING_START + 5, PER_OBJECT)]] StructuredBuffer<GPULight> Lights : register(t3);
// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);
// per-material
[[vk::binding(BINDING_START + 0, PER_MATERIAL)]] Texture2D<float4> MainTexture : register(t0);
[[vk::binding(BINDING_START + 1, PER_MATERIAL)]] Texture2D<float4> NormalTexture : register(t1);
[[vk::binding(BINDING_START + 2, PER_MATERIAL)]] Texture2D<float4> BrdfTexture : register(t2);
[[vk::binding(BINDING_START + 3, PER_MATERIAL)]] Texture2D<float4> EnvironmentTexture : register(t3);
[[vk::binding(BINDING_START + 4, PER_MATERIAL)]] Texture2D<float4> EmissionTexture : register(t4);
[[vk::binding(BINDING_START + 5, PER_MATERIAL)]] Texture2D<float4> SpecGlossTexture : register(t5);
[[vk::binding(BINDING_START + 6, PER_MATERIAL)]] SamplerState Sampler : register(s0);

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
#if defined(NORMAL_MAP) || defined(COLOR_MAP) || defined(EMISSION) || defined(SPECULAR_MAP)
	float2 texcoord : TEXCOORD1;
#endif
};

float3 DiffuseAndSpecularFromMetallic(float3 albedo, float metallic, out float3 specColor, out float oneMinusReflectivity) {
	specColor = lerp(unity_ColorSpaceDielectricSpec.rgb, albedo, metallic);
	float oneMinusDielectricSpec = unity_ColorSpaceDielectricSpec.a;
	oneMinusReflectivity = oneMinusDielectricSpec - metallic * oneMinusDielectricSpec;
	return albedo * oneMinusReflectivity;
}
float3 DiffuseAndSpecularFromSpecular(float3 diffuse, float3 specular, out float3 specColor, out float oneMinusReflectivity) {
	float maxSpecular = max(max(specular.r, specular.g), specular.b);

	float metallic = 0;
	float perceivedDiffuse = sqrt(0.299 * diffuse.r * diffuse.r + 0.587 * diffuse.g * diffuse.g + 0.114 * diffuse.b * diffuse.b);
	float perceivedSpecular = sqrt(0.299 * specular.r * specular.r + 0.587 * specular.g * specular.g + 0.114 * specular.b * specular.b);
	if (perceivedSpecular > MIN_ROUGHNESS) {
		float a = MIN_ROUGHNESS;
		float b = perceivedDiffuse * (1.0 - maxSpecular) / (1.0 - MIN_ROUGHNESS) + perceivedSpecular - 2.0 * MIN_ROUGHNESS;
		float c = MIN_ROUGHNESS - perceivedSpecular;
		float D = max(b * b - 4.0 * a * c, 0.0);
		float metallic = clamp((-b + sqrt(D)) / (2.0 * a), 0.0, 1.0);
	}
	specColor = lerp(unity_ColorSpaceDielectricSpec.rgb, diffuse, metallic);

	float3 baseColorDiffusePart = diffuse.rgb * ((1.0 - maxSpecular) / (1 - MIN_ROUGHNESS) / max(1 - metallic, .0001));
	float3 baseColorSpecularPart = specular - (float3(MIN_ROUGHNESS) * (1 - metallic) * (1 / max(metallic, .0001)));
	float oneMinusDielectricSpec = unity_ColorSpaceDielectricSpec.a;
	oneMinusReflectivity = oneMinusDielectricSpec - metallic * oneMinusDielectricSpec;
	return lerp(baseColorDiffusePart, baseColorSpecularPart, metallic * metallic);
}

float pow5(float x) {
	float x2 = x * x;
	return x2 * x2 * x;
}

float MicrofacetDistribution(float roughness, float NdotH) {
	float roughness2 = roughness * roughness;
	float f = (NdotH * roughness2 - NdotH) * NdotH + 1;
	return roughness2 / (PI * f * f + 1e-7);
}
float3 FresnelTerm(float3 F0, float cosA) {
	return F0 + (1 - F0) * pow5(1 - cosA);
}
float3 FresnelLerp(float3 F0, float3 F90, float cosA) {
	return lerp(F0, F90, pow5(1 - cosA));
}
float DisneyDiffuse(float NdotV, float NdotL, float LdotH, float perceptualRoughness) {
	float fd90 = 0.5 + 2 * LdotH * LdotH * perceptualRoughness;
	// Two schlick fresnel term
	float lightScatter = (1 + (fd90 - 1) * pow5(1 - NdotL));
	float viewScatter = (1 + (fd90 - 1) * pow5(1 - NdotV));

	return lightScatter * viewScatter;
}
float SmithJointGGXVisibilityTerm(float NdotL, float NdotV, float roughness) {
	float a = roughness;
	float lambdaV = NdotL * (NdotV * (1 - a) + a);
	float lambdaL = NdotV * (NdotL * (1 - a) + a);

	return 0.5f / (lambdaV + lambdaL + 1e-5f);
}

struct MaterialInfo {
	float3 diffuseColor;
	float3 specularColor;
	float perceptualRoughness;
	float roughness;
	float oneMinusReflectivity;
	float nv;
};

float3 ShadePoint(MaterialInfo material, float3 light, float3 normal, float3 viewDir) {
	float3 halfDir = normalize(light + viewDir);

	float nl = saturate(dot(normal, light));
	float nh = saturate(dot(normal, halfDir));

	float lv = saturate(dot(light, viewDir));
	float lh = saturate(dot(light, halfDir));

	float diffuseTerm = DisneyDiffuse(material.nv, nl, lh, material.perceptualRoughness) * nl;

	float V = SmithJointGGXVisibilityTerm(nl, material.nv, material.roughness);
	float D = MicrofacetDistribution(nh, material.roughness);

	float specularTerm = V * D * PI; // Torrance-Sparrow model, Fresnel is applied later
	specularTerm = max(0, specularTerm * nl);
	specularTerm *= any(material.specularColor) ? 1.0 : 0.0;

	return material.diffuseColor * diffuseTerm + specularTerm * FresnelTerm(material.specularColor, lh);
}
float3 ShadeIndirect(MaterialInfo material, float3 normal, float3 viewDir, float3 diffuseLight, float3 specularLight) {
	float surfaceReduction = 1.0 / (material.roughness * material.roughness + 1.0);
	float grazingTerm = saturate((1 - material.perceptualRoughness) + (1 - material.oneMinusReflectivity));
	return material.diffuseColor * diffuseLight + surfaceReduction * specularLight * FresnelLerp(material.specularColor, grazingTerm, material.nv);
}

v2f vsmain(
	[[vk::location(0)]] float3 vertex : POSITION,
	[[vk::location(1)]] float3 normal : NORMAL
#ifdef NORMAL_MAP
	,[[vk::location(2)]] float4 tangent : TANGENT
#endif
#if defined(NORMAL_MAP) || defined(COLOR_MAP) || defined(EMISSION) || defined(SPECULAR_MAP)
	,[[vk::location(3)]] float2 texcoord : TEXCOORD0
#endif
	) {
	v2f o;
	
	float4 wp = mul(Object.ObjectToWorld, float4(vertex, 1.0));

	o.position = mul(Camera.ViewProjection, wp);
	o.worldPos = wp.xyz;
	o.normal = mul(float4(normal, 1), Object.WorldToObject).xyz;
	#ifdef NORMAL_MAP
	o.tangent = mul(tangent, Object.WorldToObject) * tangent.w;
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
	#ifdef SPECULAR_MAP
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

	#ifdef EMISSION
	eval.rgb += Emission* EmissionTexture.Sample(Sampler, i.texcoord).rgb;
	#endif

	color = float4(eval, col.a);
	depthNormal = float4(normal * .5 + .5, depth / Camera.Viewport.w);
}