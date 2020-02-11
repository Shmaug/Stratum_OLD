#pragma kernel Combine

#pragma multi_compile MULTI_COMBINE

#define DEPTH_WEIGHT    2
#define NORMAL_WEIGHT   3
#define PLANE_WEIGHT    5

#define BLUR_RADIUS 1
// 1 / (Radius * .5)
#define FILTER_INVSIGMA (1/(.5*BLUR_RADIUS))

[[vk::binding(0, 0)]] Texture2D<float4> Primary : register(t0);
[[vk::binding(1, 0)]] Texture2D<float4> Secondary : register(t1);
[[vk::binding(2, 0)]] Texture2D<float4> Meta : register(t2);
[[vk::binding(3, 0)]] RWTexture2D<float4> Output : register(u0);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4x4 InvViewProj;
	float3 CameraPosition;
	uint2 Resolution;
	uint BlurAxis;
}

float3 Unproject(uint2 index, float t) {
	float4 unprojected = mul(InvViewProj, float4(index.xy * 2 / (float2)Resolution - 1, 0, 1));
	return CameraPosition + t*normalize(unprojected.xyz / unprojected.w);
}

[numthreads(8, 8, 1)]
void Combine(uint3 index : SV_DispatchThreadId) {
	float4 nt = Meta[index.xy];
	float3 pos = Unproject(index, nt.w);

	float3 primary = 0;
	float3 secondary = 0;
	float total = 0;

	for (int i = -BLUR_RADIUS; i <= BLUR_RADIUS; i++) {
		int2 idx = index.xy;
		idx[BlurAxis] += i;

		float4 tap_nt = Meta[idx];
		float3 tap_pos = Unproject(idx, tap_nt.w);

		float weight = exp(-i*i * FILTER_INVSIGMA*FILTER_INVSIGMA);

		#if DEPTH_WEIGHT > 0
		weight *= max(0, 1 - abs(tap_nt.w - nt.w) * DEPTH_WEIGHT);
		#endif

		#if NORMAL_WEIGHT > 0
		float normalCloseness = max(0, dot(tap_nt.xyz, nt.xyz));
		normalCloseness *= normalCloseness;
		normalCloseness *= normalCloseness;
		float normalError = 1 - normalCloseness;
		weight *= max(0, (1 - normalError * NORMAL_WEIGHT));
		#endif

		#if PLANE_WEIGHT>0
		float3 dq = pos - tap_pos;
		float distance2 = dot(dq, dq);
		float planeError = max(abs(dot(dq, tap_nt.xyz)), abs(dot(dq, nt.xyz)));
		float dw = max(0, 1 - 2 * PLANE_WEIGHT * planeError / sqrt(distance2));
		weight *= (distance2 < 0.0001) ? 1 : dw*dw;
		#endif

		float4 tap_p = Primary[idx];
		primary += weight * tap_p.rgb / tap_p.w;
		#ifdef MULTI_COMBINE
		float4 tap_s = Secondary[idx];
		secondary += weight * tap_s.rgb / tap_s.w;
		#endif
		total += weight;
	}

	Output[index.xy] = float4((primary + secondary) / total, 1);
}