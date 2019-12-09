#pragma kernel GenHeight

#include "include/shadercompat.h"

[[vk::binding(0, 0)]] RWTexture2D<float3> Heightmap : register(u0);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float Scale;
	float Offset;
}

#include "include/noise.hlsli"

float SampleTerrain(float2 p) {
	float m = 1 - saturate(2*pow(1 - (billow4(p * .0001, .5, 12345)*.5+.5), 80));
	m *= .8 + .2 * (ridged4(p * .01, .5, 54321) * .5 + .5);

	float n = multi8(p * .004, .5, 12345) * .5 + .5;
	return .6 * m + .4 * n;
}

[numthreads(8, 8, 1)]
void GenHeight(uint3 id : SV_DispatchThreadID) {
	float h = 0;

	for (int i = -1; i <= 1; i++)
		for (int j = -1; j <= 1; j++)
			h += SampleTerrain((id.xy + int2(i, j)) * Scale + Offset);
	Heightmap[id.xy] = h / 9;
}