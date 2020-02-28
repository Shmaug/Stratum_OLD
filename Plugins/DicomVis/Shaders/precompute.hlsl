#pragma kernel CopyRaw

#pragma multi_compile READ_MASK
#pragma multi_compile INVERT
#pragma multi_compile COLORED COLORIZE

#pragma static_sampler Sampler max_lod=0 addressMode=clamp_border borderColor=float_transparent_black

#ifdef COLORED
[[vk::binding(0, 0)]] RWTexture3D<float3> RawVolume : register(u0);
#else
[[vk::binding(0, 0)]] RWTexture3D<float> RawVolume : register(u0);
#endif
[[vk::binding(1, 0)]] RWTexture3D<uint> RawMask : register(u1);
[[vk::binding(2, 0)]] RWTexture3D<float4> BakedVolume : register(u2);
[[vk::binding(3, 0)]] RWTexture3D<float4> PrevBakedInscatter : register(u3);
[[vk::binding(4, 0)]] RWTexture3D<float4> BakedInscatter : register(u4);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float TransferMin;
	float TransferMax;
	float RemapMin;
	float InvRemapRange;
	float Cutoff;
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
	return HSVtoRGB(float3(TransferMin + density * (TransferMax - TransferMin), .5, 1));
	#else
	return density;
	#endif
}
float Threshold(float x) {
	float h = 1 - 100 * max(0, x - Cutoff);
	x = (x - RemapMin) * InvRemapRange * h;
	return saturate(x);
}

const float4 MaskColors[8] = {
	float4( 1,  1,  1, 0),
	float4( 1, .1, .1, 1),
	float4(.1,  1, .1, 1),
	float4(.1, .1,  1, 1),
	float4( 1,  1, .1, 1),
	float4( 1, .1,  1, 1),
	float4(.1,  1,  1, 1),
	float4( 1,  1,  1, 1),
};

[numthreads(4, 4, 4)]
void CopyRaw(uint3 index : SV_DispatchThreadID) {
	#ifdef COLORED

	float4 col = float4(RawVolume[index.xyz], 1);

	#else

	#ifdef INVERT
	float r = 1 - RawVolume[index.xyz];
	#else
	float r = RawVolume[index.xyz];
	#endif

	#ifdef READ_MASK
	float4 col = MaskColors[RawMask[index.xyz] % 8];
	col.a *= r;
	#else
	float4 col = float4(Transfer(r), r);
	#endif

	#endif

	col.a = Threshold(col.a);
	BakedVolume[index.xyz] = col;
}