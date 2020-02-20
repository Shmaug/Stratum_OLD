#pragma kernel Draw

#pragma multi_compile LIGHTING
#pragma multi_compile PHYSICAL_SHADING
#pragma multi_compile SBS_HORIZONTAL SBS_VERTICAL

#pragma static_sampler Sampler max_lod=0 addressMode=clamp_border borderColor=float_transparent_black

#define PI 3.1415926535897932

[[vk::binding(0, 0)]] RWTexture2D<float4> RenderTarget : register(u0);
[[vk::binding(1, 0)]] RWTexture2D<float4> DepthNormal : register(u1);

[[vk::binding(2, 0)]] Texture3D<float4> BakedVolumeS : register(t0);
[[vk::binding(3, 0)]] Texture3D<float> BakedOpticalDensityS : register(t1);

[[vk::binding(4, 0)]] Texture2D<float4> NoiseTex : register(t2);
[[vk::binding(5, 0)]] SamplerState Sampler : register(s0);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4x4 InvViewProj;
	float3 CameraPosition;

	float2 ScreenResolution;
	float3 VolumeResolution;
	float3 VolumePosition;
	float4 InvVolumeRotation;
	float3 InvVolumeScale;
	uint2 WriteOffset;

	float3 LightColor;
	float3 LightDirection;

	float RemapMin;
	float InvRemapRange;
	float Cutoff;

	float Density;
	float Scattering;
	float Extinction;
	float HG;

	float StepSize;
	uint FrameIndex;
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

#ifdef PHYSICAL_SHADING
float3 Sample(float3 p) {
	float4 s = BakedVolumeS.SampleLevel(Sampler, p, 0);
	return s.rgb * (Density * s.a);
}
#else
float4 Sample(float3 p) {
	float4 s = BakedVolumeS.SampleLevel(Sampler, p, 0);
	return float4(s.rgb, s.a * Density);
}
#endif

float HenyeyGreenstein(float angle, float g) {
	return (1 - g * g) / (pow(1 + g * g - 2 * g * angle, 1.5) * 4 * PI);
}

[numthreads(8, 8, 1)]
void Draw(uint3 index : SV_DispatchThreadID) {
	float2 clip = 2 * index.xy / ScreenResolution - 1;

	float4 unprojected = mul(InvViewProj, float4(clip, 0, 1));

	float3 ro = CameraPosition;
	float3 rd = unprojected.xyz / unprojected.w - ro;
	rd = normalize(rd);

	float3 f = WorldToVolume(ro + rd * length(DepthNormal[WriteOffset + index.xy].xyz));

	float rdl = dot(rd, LightDirection);
	
	ro = WorldToVolume(ro);
	rd = WorldToVolumeV(rd);

	float2 isect = RayBox(ro, rd, -.5, .5);
	isect.x = max(0, isect.x);
	isect.y = min(isect.y, length(f - ro));

	if (isect.x >= isect.y) return;
	
	// jitter samples
	float2 jitter = NoiseTex.Load(uint3((index.xy ^ FrameIndex + FrameIndex) % 256, 0)).xy;
	isect.x -= StepSize * jitter.x;

	ro += .5;
	ro += rd * isect.x;
	isect.y -= isect.x;

	#ifdef PHYSICAL_SHADING
	
	float scaledStep = StepSize * length(rd / InvVolumeScale) / 2;

	float3 opticalDensity = 0;
	float3 scatter = 0;
	float3 prevDensity = 0;
	float3 prevScatter = 0;

	for (float t = StepSize; t < isect.y;) {
		float3 sp = ro + rd * t;

		float3 localDensity = Sample(sp);
		opticalDensity += (localDensity + prevDensity) * scaledStep;
		prevDensity = localDensity;

		float densityPA = BakedOpticalDensityS.SampleLevel(Sampler, sp, 0);
		float3 densityCPA = opticalDensity + densityPA;
		float3 localInscatter = localDensity * exp(-densityCPA * Extinction);

		scatter += localInscatter + prevScatter;
		prevScatter = localInscatter;

		t += StepSize;
	}

	scatter *= scaledStep * HenyeyGreenstein(rdl, HG);

	float3 lightInscatter = scatter * Scattering * LightColor.xyz;
	float3 lightExtinction = exp(-opticalDensity * Extinction);

	RenderTarget[WriteOffset + index.xy] = float4(RenderTarget[WriteOffset + index.xy].rgb * lightExtinction + lightInscatter, 1);

	#else

	float4 sum = 0;

	for (float t = StepSize; t < isect.y; t += StepSize) {
		float3 sp = ro + rd * t;
		float4 localDensity = Sample(sp);
		localDensity.a *= StepSize;

		#ifdef LIGHTING
		float densityPA = BakedOpticalDensityS.SampleLevel(Sampler, sp, 0);
		localDensity.rgb *= exp(-densityPA * Extinction);
		#endif

		localDensity.rgb *= localDensity.a;
		sum += (1 - sum.a) * localDensity;
	}

	RenderTarget[WriteOffset + index.xy] = RenderTarget[WriteOffset + index.xy] * (1 - sum.a) + sum * sum.a;

	#endif
}