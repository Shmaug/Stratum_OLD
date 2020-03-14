#pragma kernel ComputeLUT
#pragma kernel ComputeGradient

#pragma multi_compile READ_MASK
#pragma multi_compile COLORIZE

#pragma static_sampler Sampler max_lod=0 addressMode=clamp_border borderColor=float_transparent_black

[[vk::binding(0, 0)]] RWTexture3D<float> RawVolume : register(u0);
[[vk::binding(1, 0)]] RWTexture3D<float> RawMask : register(u1);
[[vk::binding(2, 0)]] RWTexture3D<float4> GradientAlpha : register(u2);
[[vk::binding(3, 0)]] RWTexture1D<float4> TransferLUT : register(u3);
[[vk::binding(4, 0)]] SamplerState Sampler : register(s0);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	uint3 Resolution;
	float TransferMin;
	float TransferMax;
	float RemapMin;
	float InvRemapRange;
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

float Threshold(float x) {
	x = (x - RemapMin) * InvRemapRange;
	return saturate(x);// * saturate(x);
}

[numthreads(64, 1, 1)]
void ComputeLUT(uint3 index : SV_DispatchThreadID) {
	if (index.x >= Resolution.x) return;
	float a = (float)index.x / (float)(Resolution.x - 1);
	a = Threshold(a);

	#ifdef COLORIZE
	TransferLUT[index.x] = float4(HSVtoRGB(float3(TransferMin + a * (TransferMax - TransferMin), .5, 1)), a);
	#else
	TransferLUT[index.x] = a;
	#endif
}

[numthreads(4, 4, 4)]
void ComputeGradient(uint3 index : SV_DispatchThreadID) {
	float3 gradient = 0;

	for (uint i = 0; i < 3; i++) {
		uint3 idx0 = index;
		uint3 idx1 = index;

		if (index[i] > 0) idx0[i]--;
		if (index[i] < Resolution[i]-1) idx1[i]++;

		#ifdef READ_MASK
		gradient[i] = RawMask[idx1] * Threshold(RawVolume[idx1]) - RawMask[idx0] * Threshold(RawVolume[idx0]);
		#else
		gradient[i] = Threshold(RawVolume[idx1]) - Threshold(RawVolume[idx0]);
		#endif
	}

	float r = RawVolume[index];
	#ifdef READ_MASK
	r *= RawMask[index];
	#endif
	GradientAlpha[index.xyz] = float4(gradient*.5+.5, r);
}
