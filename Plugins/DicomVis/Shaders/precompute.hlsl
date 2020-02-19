#pragma kernel CopyRaw
#pragma kernel ComputeOpticalDensity

#pragma multi_compile READ_MASK
#pragma multi_compile INVERT
#pragma multi_compile COLORED COLORIZE

#pragma static_sampler Sampler max_lod=0 addressMode=clamp_border borderColor=float_transparent_black

#ifdef COLORED
[[vk::binding(0, 0)]] RWTexture3D<float3> RawVolume : register(u0);
#else
[[vk::binding(0, 0)]] RWTexture3D<float> RawVolume : register(u0);
#endif
[[vk::binding(1, 0)]] RWTexture3D<float> RawMask : register(u2);
[[vk::binding(2, 0)]] RWTexture3D<float4> BakedVolume : register(u3);
[[vk::binding(3, 0)]] RWTexture3D<float> BakedOpticalDensity : register(u4);

[[vk::binding(4, 0)]] Texture3D<float4> BakedVolumeS : register(t0);

[[vk::binding(5, 0)]] RWTexture2D<float4> RenderTarget : register(u5);
[[vk::binding(6, 0)]] RWTexture2D<float4> DepthNormal : register(u6);

[[vk::binding(7, 0)]] Texture2D<float4> NoiseTex : register(t2);
[[vk::binding(8, 0)]] SamplerState Sampler : register(s0);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float3 VolumeResolution;
	float3 VolumePosition;
	float4 InvVolumeRotation;
	float3 InvVolumeScale;
	float Far;

	float3 LightColor;
	float3 LightDirection;

	float RemapMin;
	float InvRemapRange;
	float Cutoff;

	float Density;

	float StepSize;
	uint FrameIndex;
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

float3 Transfer(float density) {
	#ifdef COLORIZE
	return HSVtoRGB(float3(density * .45, .8, 1));
	#else
	return density;
	#endif
}
float Threshold(float x) {
	float h = 1 - 100 * max(0, x - Cutoff);
	x = (x - RemapMin) * InvRemapRange * h;
	return saturate(x);
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
float3 WorldToVolumeV(float3 vec) {
	return qmul(InvVolumeRotation, vec) * InvVolumeScale;
}

[numthreads(4, 4, 4)]
void CopyRaw(uint3 index : SV_DispatchThreadID) {
	#ifdef COLORED
	float4 col = float4(RawVolume[index.xyz], 1);
	#ifdef INVERT
	col.a = 1 - col.a;
	#endif

	#else

	float r = RawVolume[index.xyz];
	#ifdef INVERT
	r = 1 - r;
	#endif
	float4 col = float4(Transfer(r), r);
	#endif

	#if defined(READ_MASK)
	col.a *= RawMask[index.xyz];
	#endif

	col.a = Threshold(col.a);
	BakedVolume[index.xyz] = col;
}

[numthreads(4, 4, 4)]
void ComputeOpticalDensity(uint3 index : SV_DispatchThreadID) {
	float3 ld = WorldToVolumeV(LightDirection);

	float3 ro = (index.xyz + .5) / VolumeResolution;

	// jitter samples
	float2 jitter = NoiseTex.Load(uint3(index.xy % 256, 0)).xy;
	float start = StepSize * jitter.x;

	float li = RayBox(ro, ld, 0, 1).y;
	ro += ld * start;
	ld *= StepSize;

	float opticalDensity = 0;
	for (float lt = start; lt < li; lt += StepSize) {
		opticalDensity += BakedVolumeS.SampleLevel(Sampler, ro, 0).a;
		ro += ld;
	}

	BakedOpticalDensity[index.xyz] = opticalDensity * Density * StepSize * length(LightDirection);
}