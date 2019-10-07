#pragma vertex vsmain
#pragma fragment fsmain

#pragma render_queue 1000

#pragma static_sampler Sampler

#include <shadercompat.h>

#define PI 3.1415926535897932
#define unity_ColorSpaceDielectricSpec float4(0.04, 0.04, 0.04, 1.0 - 0.04) // standard dielectric reflectivity coef at incident angle (= 4%)

// per-object
[[vk::binding(OBJECT_BUFFER_BINDING, PER_OBJECT)]] ConstantBuffer<ObjectBuffer> Object : register(b0);
// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);
// per-material
[[vk::binding(BINDING_START + 0, PER_MATERIAL)]] Texture2D<float4> BrdfTexture : register(t0);
[[vk::binding(BINDING_START + 1, PER_MATERIAL)]] Texture2D<float4> DiffuseTexture : register(t1);
[[vk::binding(BINDING_START + 2, PER_MATERIAL)]] Texture2D<float4> NormalTexture : register(t2);
[[vk::binding(BINDING_START + 3, PER_MATERIAL)]] SamplerState Sampler : register(s0);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4 Color;
	float Metallic;
	float Roughness;
}

struct v2f {
	float4 position : SV_Position;
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
	float3 worldPos : TEXCOORD0;
	float2 texcoord : TEXCOORD1;
};
struct fs_out {
	float4 color : SV_Target0;
	float4 depthNormal : SV_Target1;
};

float microfacetDistribution(float roughness, float NdotH) {
	float roughness2 = roughness * roughness;
	float f = (NdotH * roughness2 - NdotH) * NdotH + 1;
	return roughness2 / (PI * f * f + 1e-7);
}
float OneMinusReflectivityFromMetallic(float metallic) {
	// We'll need oneMinusReflectivity, so
	//   1-reflectivity = 1-lerp(dielectricSpec, 1, metallic) = lerp(1-dielectricSpec, 0, metallic)
	// store (1-dielectricSpec) in unity_ColorSpaceDielectricSpec.a, then
	//   1-reflectivity = lerp(alpha, 0, metallic) = alpha + metallic*(0 - alpha) =
	//                  = alpha - metallic * alpha
	float oneMinusDielectricSpec = unity_ColorSpaceDielectricSpec.a;
	return oneMinusDielectricSpec - metallic * oneMinusDielectricSpec;
}
float3 DiffuseAndSpecularFromMetallic(float3 albedo, float metallic, out float3 specColor, out float oneMinusReflectivity) {
	specColor = lerp(unity_ColorSpaceDielectricSpec.rgb, albedo, metallic);
	oneMinusReflectivity = OneMinusReflectivityFromMetallic(metallic);
	return albedo * oneMinusReflectivity;
}

struct MaterialInfo {
	float3 diffuseColor;
	float3 specularColor;
	float perceptualRoughness;
	float oneMinusReflectivity;
};

float pow5(float x) {
	float x2 = x * x;
	return x2 * x2 * x;
}

float PerceptualRoughnessToRoughness(float perceptualRoughness) {
	return perceptualRoughness * perceptualRoughness;
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

float3 SampleBackground(float3 D, float roughness) {
	return .5;
}

float3 ShadePoint(MaterialInfo material, float3 light, float3 normal, float3 viewDir) {
	float3 halfDir = normalize(light + viewDir);

	float nv = abs(dot(normal, viewDir));
	float nl = saturate(dot(normal, light));
	float nh = saturate(dot(normal, halfDir));

	float lv = saturate(dot(light, viewDir));
	float lh = saturate(dot(light, halfDir));

	// Diffuse term
	float diffuseTerm = DisneyDiffuse(nv, nl, lh, material.perceptualRoughness) * nl;

	// Specular term
	// HACK: theoretically we should divide diffuseTerm by Pi and not multiply specularTerm!
	// BUT 1) that will make shader look significantly darker than Legacy ones
	// and 2) on engine side "Non-important" lights have to be divided by Pi too in cases when they are injected into ambient SH
	float roughness = PerceptualRoughnessToRoughness(material.perceptualRoughness);

	// GGX with roughtness to 0 would mean no specular at all, using max(roughness, 0.002) here to match HDrenderloop roughtness remapping.
	roughness = max(roughness, 0.002);
	float V = SmithJointGGXVisibilityTerm(nl, nv, roughness);
	float D = microfacetDistribution(nh, roughness);

	float specularTerm = V * D * PI; // Torrance-Sparrow model, Fresnel is applied later

	// specularTerm * nl can be NaN on Metal in some cases, use max() to make sure it's a sane value
	specularTerm = max(0, specularTerm * nl);

	// surfaceReduction = Int D(NdotH) * NdotH * Id(NdotL>0) dH = 1/(roughness^2+1)
	float surfaceReduction = 1.0 / (roughness * roughness + 1.0); // fade \in [0.5;1]


	// To provide true Lambert lighting, we need to be able to kill specular completely.
	specularTerm *= any(material.specularColor) ? 1.0 : 0.0;

	float grazingTerm = saturate((1 - material.perceptualRoughness) + (1 - material.oneMinusReflectivity));
	return material.diffuseColor * diffuseTerm + specularTerm * FresnelTerm(material.specularColor, lh);
}
float3 ShadeIndirect(MaterialInfo material, float3 normal, float3 viewDir, float3 diffuseLight, float3 specularLight) {
	float roughness = PerceptualRoughnessToRoughness(material.perceptualRoughness);
	roughness = max(roughness, 0.002);

	// surfaceReduction = Int D(NdotH) * NdotH * Id(NdotL>0) dH = 1/(roughness^2+1)
	float surfaceReduction = 1.0 / (roughness * roughness + 1.0); // fade \in [0.5;1]

	float nv = abs(dot(normal, viewDir));
	float grazingTerm = saturate((1 - material.perceptualRoughness) + (1 - material.oneMinusReflectivity));
	return material.diffuseColor * diffuseLight + surfaceReduction * specularLight * FresnelLerp(material.specularColor, grazingTerm, nv);
}

v2f vsmain(
	[[vk::location(0)]] float3 vertex : POSITION,
	[[vk::location(1)]] float3 normal : NORMAL,
	[[vk::location(2)]] float4 tangent : TANGENT,
	[[vk::location(3)]] float2 texcoord : TEXCOORD0 ) {
	v2f o;
	
	float4 wp = mul(Object.ObjectToWorld, float4(vertex, 1.0));

	o.position = mul(Camera.ViewProjection, wp);
	o.worldPos = wp.xyz;
	o.normal = mul(float4(normal, 1), Object.WorldToObject).xyz;
	o.tangent = mul(tangent, Object.WorldToObject) * tangent.w;
	o.texcoord = texcoord;

	return o;
}

fs_out fsmain(v2f i, bool front : SV_IsFrontFace) {
	float4 col = DiffuseTexture.Sample(Sampler, i.texcoord) * Color;
	clip(col.a - .5);

	float4 bump = NormalTexture.Sample(Sampler, i.texcoord);
	bump.xyz = bump.xyz * 2 - 1;

	float3 normal = normalize(front ? i.normal : -i.normal);
	float3 tangent = normalize(i.tangent);
	float3 bitangent = normalize(cross(i.normal, i.tangent));

	normal = normalize(tangent * bump.x + bitangent * bump.y + normal * bump.z);

	MaterialInfo material;
	material.perceptualRoughness = Roughness;
	material.diffuseColor = DiffuseAndSpecularFromMetallic(col.rgb, Metallic, material.specularColor, material.oneMinusReflectivity);

	float3 view = Camera.Position - i.worldPos.xyz;
	float depth = length(view);
	view /= depth;

	float3 eval = 0;
	float3 reflection = normalize(reflect(-view, normal));
	float3 diffuseLight = 0.5;// SampleBackground(reflection, 1).rgb;
	float3 specularLight = SampleBackground(reflection, material.perceptualRoughness).rgb;

	eval += ShadePoint(material, normalize(float3(.5, 1, .25)), normal, view);
	eval += ShadeIndirect(material, normal, view, diffuseLight, specularLight);

	fs_out o;
	o.color = float4(normal, col.a);
	o.depthNormal = float4(normal * .5 + .5, depth / Camera.Viewport.w);
	return o;
}