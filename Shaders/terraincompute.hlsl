#pragma kernel GenHeight
#pragma kernel DrawDetails

#include "include/shadercompat.h"

struct VkDrawIndexedIndirectCommand {
	uint indexCount;
	uint instanceCount;
	uint firstIndex;
	int  vertexOffset;
	uint firstInstance;
};

[[vk::binding(0, 0)]] RWTexture2D<float3> Heightmap : register(u0);

[[vk::binding(1, 0)]] RWStructuredBuffer<VkDrawIndexedIndirectCommand> IndirectCommands : register(u1);
[[vk::binding(2, 0)]] StructuredBuffer<DetailTransform> DetailTransforms : register(u2);
[[vk::binding(3, 0)]] RWStructuredBuffer<ObjectBuffer> DetailBuffers : register(u3);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float Scale;
	float Offset;
}

#include "include/noise.hlsli"

float SampleTerrain(float2 p) {
	float m = saturate(pow(1 - (billow4(p * .0005, .5, 6534)*.5+.5), 25));
	m *= .7 + .3 * (ridged4(p * .01, .5, 5214) * .5 + .5);

	float n = multi8(p * .006, .5, 6325) * .5 + .5;
	n = .98 * n + .02 * (fbm6(p * .1, .5, 7818) * .5 + .5);
	return .6 * m + .4 * n;
}

[numthreads(8, 8, 1)]
void GenHeight(uint3 id : SV_DispatchThreadID) {
	Heightmap[id.xy] = SampleTerrain((float2)id.xy * Scale + Offset);
}

[numthreads(64, 1, 1)]
void DrawDetails(uint3 id : SV_DispatchThreadID) {
	
}