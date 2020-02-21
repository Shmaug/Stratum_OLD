#pragma kernel Draw

#pragma multi_compile PHYSICAL_SHADING
#pragma multi_compile SBS_HORIZONTAL SBS_VERTICAL

#pragma static_sampler Sampler max_lod=0 addressMode=clamp_border borderColor=float_transparent_black

#define PI 3.1415926535897932
#define INV_PI (1 / PI)

[[vk::binding(0, 0)]] RWTexture2D<float4> RenderTarget : register(u0);
[[vk::binding(1, 0)]] RWTexture2D<float4> DepthNormal : register(u1);

[[vk::binding(2, 0)]] Texture3D<float4> BakedVolume : register(t0);
[[vk::binding(3, 0)]] Texture3D<float4> BakedInscatter : register(t1);

[[vk::binding(4, 0)]] Texture2D<float4> EnvironmentTexture	: register(t2);

[[vk::binding(5, 0)]] Texture2D<float4> NoiseTex : register(t3);
[[vk::binding(6, 0)]] SamplerState Sampler : register(s0);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4x4 InvViewProj;
	float3 CameraPosition;

	float2 ScreenResolution;
	float3 VolumeResolution;
	float3 VolumePosition;
	float4 VolumeRotation;
	float3 VolumeScale;
	float4 InvVolumeRotation;
	float3 InvVolumeScale;
	uint2 WriteOffset;

	float3 AmbientLight;

	float Density;
	float Scattering;
	float Extinction;
	float HG;

	float StepSize;
	uint FrameIndex;
}

#define CMJ_DIM 16
struct RandomSampler {
	uint index;
	uint dimension;
	uint scramble;
	uint pad;
};
uint permute(uint i, uint l, uint p) {
	uint w = l - 1;
	w |= w >> 1;
	w |= w >> 2;
	w |= w >> 4;
	w |= w >> 8;
	w |= w >> 16;
	do {
		i ^= p;
		i *= 0xe170893d;
		i ^= p >> 16;
		i ^= (i & w) >> 4;
		i ^= p >> 8;
		i *= 0x0929eb3f;
		i ^= p >> 23;
		i ^= (i & w) >> 1;
		i *= 1 | p >> 27;
		i *= 0x6935fa69;
		i ^= (i & w) >> 11;
		i *= 0x74dcb303;
		i ^= (i & w) >> 2;
		i *= 0x9e501cc3;
		i ^= (i & w) >> 2;
		i *= 0xc860a3df;
		i &= w;
		i ^= i >> 5;
	} while (i >= l);
	return (i + p) % l;
}
float randfloat(uint i, uint p) {
	i ^= p;
	i ^= i >> 17;
	i ^= i >> 10;
	i *= 0xb36534e5;
	i ^= i >> 12;
	i ^= i >> 21;
	i *= 0x93fc4795;
	i ^= 0xdf6e307f;
	i ^= i >> 17;
	i *= 1 | p >> 18;
	return i * (1.0 / 4294967808.0f);
}
float2 cmj(int s, int n, int p) {
	int sx = permute(s % n, n, p * 0xa511e9b3);
	int sy = permute(s / n, n, p * 0x63d83595);
	float jx = randfloat(s, p * 0xa399d265);
	float jy = randfloat(s, p * 0x711ad6a5);
	return float2((s % n + (sy + jx) / n) / n, (s / n + (sx + jy) / n) / n);
}

float2 SampleRNG(inout RandomSampler rng) {
	int idx = permute(rng.index, CMJ_DIM * CMJ_DIM, 0xa399d265 * rng.dimension * rng.scramble);
	float2 s = cmj(idx, CMJ_DIM, rng.dimension * rng.scramble);
	rng.dimension++;
	return s;
}

float2 RayBox(float3 ro, float3 rd, float3 mn, float3 mx) {
	float3 id = 1 / rd;
	float3 t0 = (mn - ro) * id;
	float3 t1 = (mx - ro) * id;
	float3 tmin = min(t0, t1);
	float3 tmax = max(t0, t1);
	return float2(max(max(tmin.x, tmin.y), tmin.z), min(min(tmax.x, tmax.y), tmax.z));
}

float3 qmul(float4 q, float3 vec) {
	return 2 * dot(q.xyz, vec) * q.xyz + (q.w * q.w - dot(q.xyz, q.xyz)) * vec + 2 * q.w * cross(q.xyz, vec);
}
float3 WorldToVolume(float3 pos) {
	return qmul(InvVolumeRotation, pos - VolumePosition) * InvVolumeScale;
}
float3 WorldToVolumeV(float3 vec) {
	return qmul(InvVolumeRotation, vec) * InvVolumeScale;
}
float3 VolumeToWorldV(float3 vec) {
	return qmul(VolumeRotation, vec) * VolumeScale;
}

float HenyeyGreenstein(float angle, float g) {
	return (1 - g * g) / (pow(1 + g * g - 2 * g * angle, 1.5) * 4 * PI);
}

#define unity_ColorSpaceDielectricSpec float4(0.04, 0.04, 0.04, 1.0 - 0.04)
float pow5(float x){
	float x2 = x*x;
	return x2*x2*x;
}
float3 FresnelLerp(float3 F0, float3 F90, float cosA) {
	return lerp(F0, F90, pow5(1 - cosA));
}
float3 DiffuseAndSpecularFromMetallic(float3 albedo, float metallic, out float3 specColor, out float oneMinusReflectivity) {
	specColor = lerp(unity_ColorSpaceDielectricSpec.rgb, albedo, metallic);
	float oneMinusDielectricSpec = unity_ColorSpaceDielectricSpec.a;
	oneMinusReflectivity = oneMinusDielectricSpec - metallic * oneMinusDielectricSpec;
	return albedo * oneMinusReflectivity;
}
float3 BRDFIndirect(float3 albedo, float metallic, float roughness, float3 view, float3 normal) {
	float oneMinusReflectivity;
	float3 specular;
	float3 diffuse = DiffuseAndSpecularFromMetallic(albedo, metallic, specular, oneMinusReflectivity);

	uint texWidth, texHeight, numMips;
	EnvironmentTexture.GetDimensions(0, texWidth, texHeight, numMips);
	numMips--;
	float3 reflection = normalize(reflect(-view, normal));
	float2 envuv = float2(atan2(reflection.z, reflection.x) * INV_PI * .5 + .5, acos(reflection.y) * INV_PI);
	float3 diffuseLight  = AmbientLight * EnvironmentTexture.SampleLevel(Sampler, envuv, .75 * numMips).rgb;
	float3 specularLight = AmbientLight * EnvironmentTexture.SampleLevel(Sampler, envuv, saturate(roughness) * numMips).rgb;

	float surfaceReduction = 1.0 / (roughness * roughness + 1.0);
	float grazingTerm = saturate((1 - roughness) + (1 - oneMinusReflectivity));
	return diffuse * diffuseLight + surfaceReduction * specularLight * FresnelLerp(specular, grazingTerm, dot(normal, view));
}

[numthreads(8, 8, 1)]
void Draw(uint3 index : SV_DispatchThreadID) {
	float2 clip = 2 * index.xy / ScreenResolution - 1;

	float4 unprojected = mul(InvViewProj, float4(clip, 0, 1));

	float3 ro = CameraPosition;
	float3 rd_w = unprojected.xyz / unprojected.w - ro;
	rd_w = normalize(rd_w);

	float3 f = WorldToVolume(ro + rd_w * length(DepthNormal[WriteOffset + index.xy].xyz));

	ro = WorldToVolume(ro);
	float3 rd = WorldToVolumeV(rd_w);

	float2 isect = RayBox(ro, rd, -.5, .5);
	isect.x = max(0, isect.x);
	isect.y = min(isect.y, length(f - ro));

	if (isect.x >= isect.y) return;
	
	// jitter samples
	isect.x -= StepSize * NoiseTex.Load(uint3((index.xy ^ FrameIndex + FrameIndex) % 256, 0)).x;

	ro += .5;
	ro += rd * isect.x;
	isect.y -= isect.x;

	#ifdef PHYSICAL_SHADING
	
	float scaledStep = StepSize * length(rd * VolumeScale);

	float3 sp = ro + rd * StepSize;
	float3 dsp = rd * StepSize;

	float3 opticalDensity = 0;
	float3 inscatter = 0;

	for (float t = StepSize; t < isect.y;) {
		float4 localSample = BakedVolume.SampleLevel(Sampler, sp, 0);
		float3 gradient = float3(
			BakedVolume.SampleLevel(Sampler, sp, 0, int3(1, 0, 0)).a - localSample.a,
			BakedVolume.SampleLevel(Sampler, sp, 0, int3(0, 1, 0)).a - localSample.a,
			BakedVolume.SampleLevel(Sampler, sp, 0, int3(0, 0, 1)).a - localSample.a );
		gradient = VolumeToWorldV(gradient);
		float l = length(gradient);

		float3 localEval = localSample.rgb;
		float4 lightEval = BakedVolume.SampleLevel(Sampler, sp, 2);

		if (l > .01) localEval = BRDFIndirect(localSample.rgb, 0, 1 - localSample.a * .1, -rd_w, gradient / l);

		localEval *= exp(-lightEval.rgb * lightEval.a * Density * Extinction);

		localSample.a *= scaledStep * Density;
		inscatter += localEval * localSample.a * exp(-opticalDensity * Extinction);
		opticalDensity += localSample.rgb * localSample.a;

		t += StepSize;
		sp += dsp;
	}

	float3 extinction = exp(-opticalDensity * Extinction);
	inscatter *= Scattering;

	RenderTarget[WriteOffset + index.xy] = float4(RenderTarget[WriteOffset + index.xy].rgb * extinction + inscatter, 1);

	#else

	// traditional alpha blending
	float4 sum = 0;
	for (float t = StepSize; t < isect.y; t += StepSize) {
		float3 sp = ro + rd * t;
		float4 localDensity = BakedVolume.SampleLevel(Sampler, sp, 0);
		localDensity.a *= StepSize * Density;

		localDensity.rgb *= localDensity.a;
		sum += (1 - sum.a) * localDensity;
	}
	RenderTarget[WriteOffset + index.xy] = RenderTarget[WriteOffset + index.xy] * (1 - sum.a) + sum * sum.a;

	#endif
}