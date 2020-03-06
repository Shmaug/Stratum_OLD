#define PI 3.1415926535897932
#define INV_PI 0.31830988618
#define MIN_ROUGHNESS 0.04
#define unity_ColorSpaceDielectricSpec float4(0.04, 0.04, 0.04, 1.0 - 0.04) // standard dielectric reflectivity coef at incident angle (= 4%)

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
	float3 diffuse;
	float3 specular;
	float3 emission;
	float perceptualRoughness;
	float roughness;
	float oneMinusReflectivity;
	float occlusion;
};

float3 BRDF(MaterialInfo material, float nv, float3 omega, float3 normal, float3 view) {
	float3 H = normalize(omega + view);

	float nl = saturate(dot(normal, omega));
	float nh = saturate(dot(normal, H));
	
	float lv = saturate(dot(omega, view));
	float lh = saturate(dot(omega, H));

	float diffuseTerm = DisneyDiffuse(nv, nl, lh, material.perceptualRoughness) * nl;

	float V = SmithJointGGXVisibilityTerm(nl, nv, material.roughness);
	float D = MicrofacetDistribution(nh, material.roughness);

	float specularTerm = V * D * PI; // Torrance-Sparrow model, Fresnel is applied later
	specularTerm = max(0, specularTerm * nl);
	specularTerm *= any(material.specular) ? 1 : 0;

	return material.diffuse * diffuseTerm + specularTerm * FresnelTerm(material.specular, lh);
}
float3 BRDFIndirect(MaterialInfo material, float3 N, float3 V, float nv, float3 diffuseLight, float3 specularLight) {
	float surfaceReduction = 1.0 / (material.roughness * material.roughness + 1.0);
	float grazingTerm = saturate((1 - material.perceptualRoughness) + (1 - material.oneMinusReflectivity));
	return material.diffuse * diffuseLight + surfaceReduction * specularLight * FresnelLerp(material.specular, grazingTerm, nv);
}

float3 EvaluateLighting(MaterialInfo material, float3 worldPos, float3 normal, float3 view, float depth){
	#ifdef SHOW_CASCADE_SPLITS
	static const float4 CascadeSplitColors[4] = {
		float4(.5,  1, .5, 1),
		float4(1, .5, .5, 1),
		float4(1,  1, .5, 1),
		float4(.5, .5,  1, 1),
	};
	float4 cascadeColor = 0;
	#endif

	float nv = abs(dot(normal, view));

	float3 eval = 0;

	for (uint l = 0; l < LightCount; l++) {
		float3 L;
		float attenuation = LightAttenuation(l, Camera.Position, worldPos, normal, depth, L);

		#ifdef SHOW_CASCADE_SPLITS
		if (Lights[l].Type == LIGHT_SUN) {
			float ct = CascadeSplit(Lights[l].CascadeSplits, depth);
			if (ct >= 0) {
				uint ci = (uint)ct;
				float4 cc = CascadeSplitColors[ci];
				if (ci < 3) cc = lerp(cc, CascadeSplitColors[ci + 1], saturate(((ct - ci) - .92) / .08));
				cascadeColor += cc;
			}
		}
		#endif

		eval += attenuation * Lights[l].Color * BRDF(material, nv, L, normal, view);
	}

	eval.rgb += material.emission;

	float3 env_spec = AmbientLight * lerp(.5, 1, normal.y * .5 + .5);
	float3 env_diff = AmbientLight * lerp(.5, 1, normal.y * .5 + .5);

	#ifdef ENVIRONMENT_TEXTURE
	uint texWidth, texHeight, numMips;
	EnvironmentTexture.GetDimensions(0, texWidth, texHeight, numMips);
	float3 reflection = normalize(reflect(-view, normal));
	float2 envuv = float2(atan2(reflection.z, reflection.x) * INV_PI * .5 + .5, acos(reflection.y) * INV_PI);
	env_spec *= EnvironmentTexture.SampleLevel(Sampler, envuv, saturate(material.perceptualRoughness) * numMips).rgb;
	env_diff *= EnvironmentTexture.SampleLevel(Sampler, envuv, .75 * numMips).rgb;
	#endif

	eval += BRDFIndirect(material, normal, view, nv, env_diff, env_spec) * material.occlusion;

	#ifdef SHOW_CASCADE_SPLITS
	if (cascadeColor.w) eval = lerp(eval.rgb, cascadeColor.rgb / cascadeColor.w, .5);
	#endif

	return eval;
}