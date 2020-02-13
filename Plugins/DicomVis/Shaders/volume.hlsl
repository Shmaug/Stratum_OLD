#pragma kernel PreProcess
#pragma kernel Draw

#pragma multi_compile READ_RAW READ_MASK READ_RAW_MASK

#pragma static_sampler Sampler max_lod=0

#define PI 3.1415926535897932

[[vk::binding(0, 0)]] RWTexture3D<float> RawVolume : register(u0);
[[vk::binding(1, 0)]] RWTexture3D<float> RawMask : register(u2);
[[vk::binding(2, 0)]] RWTexture3D<float2> BakedVolume : register(u3);
[[vk::binding(3, 0)]] RWTexture3D<float4> BakedOpticalDensity : register(u4);

[[vk::binding(4, 0)]] RWTexture2D<float4> RenderTarget : register(u5);
[[vk::binding(5, 0)]] RWTexture2D<float4> DepthNormal : register(u6);

[[vk::binding(6, 0)]] Texture3D<float> BakedVolumeS : register(t0);
[[vk::binding(7, 0)]] Texture3D<float3> BakedOpticalDensityS : register(t1);

[[vk::binding(8, 0)]] Texture2D<float4> NoiseTex : register(t2);
[[vk::binding(9, 0)]] SamplerState Sampler : register(s0);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float3 VolumePosition;
	float4 InvVolumeRotation;
	float3 InvVolumeScale;
	float4x4 InvViewProj;
	float3 CameraPosition;
	float2 ScreenResolution;
	float3 VolumeResolution;
	float Far;

	float3 LightColor;
	float3 LightDirection;

	float Threshold;
	float InvThreshold;
	float Density;
	float Scattering;
	float Extinction;

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

float3 HuetoRGB(float h) {
	return saturate(float3(abs(h * 6 - 3) - 1, 2 - abs(h * 6 - 2), 2 - abs(h * 6 - 4)));
}
float3 HSVtoRGB(float3 hsv) {
	float3 rgb = HuetoRGB(hsv.x);
	return ((rgb - 1) * hsv.y + 1) * hsv.z;
}
float3 RGBtoHCV(float3 rgb) {
	// Based on work by Sam Hocevar and Emil Persson
	float4 P = (rgb.g < rgb.b) ? float4(rgb.bg, -1, 2.0 / 3.0) : float4(rgb.gb, 0, -1.0 / 3.0);
	float4 Q = (rgb.r < P.x) ? float4(P.xyw, rgb.r) : float4(rgb.r, P.yzx);
	float C = Q.x - min(Q.w, Q.y);
	float H = abs((Q.w - Q.y) / (6 * C + 1e-5) + Q.z);
	return float3(H, C, Q.x);
}

float3 Sample(float3 p) {
	float s = BakedVolumeS.SampleLevel(Sampler, p, 0);
	return Density * HSVtoRGB(float3(.75 - s * .5, .75, 1)) * max(0, s - Threshold) * InvThreshold;
}
float3 SampleFast(uint3 p) {
	float s = BakedVolume[p];
	return Density * HSVtoRGB(float3(.75 - s * .5, .75, 1)) * max(0, s - Threshold) * InvThreshold;
}

float HenyeyGreenstein(float angle, float g) {
	return (1 - g * g) / (pow(1 + g * g - 2 * g * angle, 1.5) * 4 * PI);
}


[numthreads(4, 4, 4)]
void PreProcess(uint3 index : SV_DispatchThreadID) {
	#if defined(READ_RAW)
	BakedVolume[index.xyz] = float2(RawVolume[index.xyz], 1);
	AllMemoryBarrierWithGroupSync();
	#elif defined(READ_MASK)
	BakedVolume[index.xyz] = float2(BakedVolume[index.xyz].r, RawMask[index.xyz]);
	AllMemoryBarrierWithGroupSync();
	#elif defined(READ_RAW_MASK)
	BakedVolume[index.xyz] = float2(RawVolume[index.xyz], RawMask[index.xyz]);
	AllMemoryBarrierWithGroupSync();
	#endif

	float3 ld = WorldToVolumeV(LightDirection);
	float scaledLightStep = StepSize * length(ld / InvVolumeScale);

	float3 opticalDensity = 0;
	float li = RayBox(index.xyz / VolumeResolution, ld, 0, 1).y;
	for (float lt = StepSize; lt < li; lt += StepSize)
		opticalDensity += SampleFast((uint3)(index.xyz + VolumeResolution*lt*ld + .5));

	BakedOpticalDensity[index.xyz] = float4(opticalDensity*scaledLightStep, 1);
}

[numthreads(8, 8, 1)]
void Draw(uint3 index : SV_DispatchThreadID) {
	float4 unprojected = mul(InvViewProj, float4(index.xy * 2 / ScreenResolution - 1, 0, 1));
	float3 ro = CameraPosition;
	float3 rd = normalize(unprojected.xyz / unprojected.w);

	float rdl = saturate(dot(rd, LightDirection));

	float3 f = ro + rd * length(DepthNormal[index.xy].xyz)*Far;

	ro = WorldToVolume(ro);
	rd = WorldToVolumeV(rd);
	f  = WorldToVolume(f);

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

	float scaledStep = StepSize * length(rd / InvVolumeScale) / 2;

	float3 opticalDensity = 0;
	float3 scatter = 0;
	float3 prevDensity = 0;
	float3 prevScatter = 0;

	for (float t = StepSize; t < isect.y;) {
		float3 sp = ro + rd * t;

		float3 localDensity = Sample(sp);
		float3 densityPA = BakedOpticalDensityS[(uint3)(sp * VolumeResolution + .5)];
		
		opticalDensity += (localDensity + prevDensity) * scaledStep;
		prevDensity = localDensity;

		float3 densityCPA = opticalDensity + densityPA;
		float3 localInscatter = localDensity * exp(-densityCPA * Extinction);

		scatter += localInscatter + prevScatter;
		prevScatter = localInscatter;

		t += StepSize;
	}

	scatter *= scaledStep * HenyeyGreenstein(rdl, .5);
	float3 lightInscatter = scatter * Scattering * LightColor.xyz;
	float3 lightExtinction = exp(-opticalDensity * Extinction);

	RenderTarget[index.xy] = float4(RenderTarget[index.xy].rgb * lightExtinction + lightInscatter, 1);
}