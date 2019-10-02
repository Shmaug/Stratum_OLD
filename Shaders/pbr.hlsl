#pragma vertex vsmain
#pragma fragment fsmain

#pragma render_queue 1000

#pragma static_sampler Sampler

#include <shadercompat.h>

#define PI 3.1415926535897932

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

struct MaterialInfo {
	float3 diffuseColor;	// color contribution from diffuse lighting
	float roughness;		// roughness mapped to a more linear change in the roughness
	float3 reflectance0;	// full reflectance color (normal incidence angle)
	float3 reflectance90;	// reflectance color at grazing angle
};

float3 specularReflection(float3 reflectance0, float3 reflectance90, float VdotH) {
	float x = saturate(1 - VdotH);
	float x2 = x * x;
	return lerp(reflectance0, reflectance90, x2 * x2 * x);
}
float visibilityOcclusion(float roughness, float NdotL, float NdotV) {
	float roughness2 = roughness * roughness;

	float GGXV = NdotL * sqrt(NdotV * NdotV * (1 - roughness2) + roughness2);
	float GGXL = NdotV * sqrt(NdotL * NdotL * (1 - roughness2) + roughness2);

	float GGX = GGXV + GGXL;

	return GGX > 0 ? .5 / GGX : 0;
}
float microfacetDistribution(float roughness, float NdotH) {
	float roughness2 = roughness * roughness;
	float f = (NdotH * roughness2 - NdotH) * NdotH + 1;
	return roughness2 / (PI * f * f + 0.000001);
}

float3 ShadePoint(MaterialInfo material, float3 l, float3 n, float3 v) {
	float NdotL = saturate(dot(n, l));
	float NdotV = saturate(dot(n, v));

	if (NdotL > 0 || NdotV > 0) {
		float3 h = normalize(l + v);
		float NdotH = saturate(dot(n, h));
		float LdotH = saturate(dot(l, h));
		float VdotH = saturate(dot(v, h));

		// Calculate the shading terms for the microfacet specular shading model
		float3 F = specularReflection(material.reflectance0, material.reflectance90, VdotH);
		float Vis = visibilityOcclusion(material.roughness, NdotL, NdotV);
		float D = microfacetDistribution(material.roughness, NdotH);

		// Calculation of analytical lighting contribution
		float3 diffuseContrib = (1 - F) * material.diffuseColor / PI;
		float3 specContrib = F * Vis * D;

		// Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
		return NdotL * (diffuseContrib + specContrib);
	}

	return 0;
}

float3 SampleBackground(float3 D, float roughness) {
	return .5;
}

float3 getIBLContribution(float3 diffuseColor, float3 specularColor, float3 n, float3 v, float perceptualRoughness) {
	float3 reflection = normalize(reflect(-v, n));

	float2 brdfSamplePoint = saturate(float2(dot(n, v), perceptualRoughness));
	// retrieve a scale and bias to F0. See [1], Figure 3
	float2 brdf = BrdfTexture.SampleLevel(Sampler, brdfSamplePoint, 0).rg;

	float3 diffuseLight = 0;//SampleBackground(reflection, perceptualRoughness).rgb;
	float3 specularLight = SampleBackground(reflection, perceptualRoughness).rgb;

	float3 diffuse = diffuseLight * diffuseColor;
	float3 specular = specularLight * (specularColor * brdf.x + brdf.y);

	return diffuse + specular;
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

fs_out fsmain(v2f i) {
	float4 col = DiffuseTexture.Sample(Sampler, i.texcoord) * Color;
	clip(col.a - .5);

	float4 bump = NormalTexture.Sample(Sampler, i.texcoord);
	float metallic = Metallic;// bump.b;
	float perceptualRoughness = Roughness;// bump.a;
	//bump.z = sqrt(1 - saturate(dot(bump.xy, bump.xy)));

	bump.xy = bump.xy * 2 - 1;
	bump.xyz = normalize(bump.xyz);

	float3 normal = normalize(i.normal);
	float3 tangent = normalize(i.tangent);
	float3 bitangent = normalize(cross(i.normal, i.tangent));

	normal = normalize(tangent * bump.x + bitangent * bump.y + normal * bump.z);

	fs_out o = {};
	float f0 = 0.04; // specular
	float3 diffuseColor = col.rgb * (1 - f0) * (1 - metallic);
	float3 specularColor = lerp(f0, col.rgb, metallic);

	MaterialInfo material = {
		diffuseColor,
		perceptualRoughness * perceptualRoughness,
		specularColor,
		saturate(max(max(specularColor.r, specularColor.g), specularColor.b) * 50).xxx, // reflectance
	};

	float3 view = Camera.Position - i.worldPos.xyz;
	float depth = length(view);
	view /= depth;

	float3 eval = 0;
	eval += ShadePoint(material, normalize(float3(.5, 1.0, .2)), normal, view);
	eval += getIBLContribution(diffuseColor, specularColor, normal, view, perceptualRoughness);

	o.color = col;// float4(normal * .5 + .5, col.a);// float4(eval, col.a);
	o.depthNormal = float4(normal * .5 + .5, view / Camera.Viewport.w);
	return o;
}