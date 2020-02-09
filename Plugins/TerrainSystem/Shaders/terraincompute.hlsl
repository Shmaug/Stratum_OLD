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

struct DetailTransform{
	float3 position;
	float scale;
	float4 rotation;
};

[[vk::binding(0, 0)]] RWTexture2D<float4> Heightmap : register(u0);

[[vk::binding(1, 0)]] RWStructuredBuffer<VkDrawIndexedIndirectCommand> IndirectCommands : register(u1);
[[vk::binding(2, 0)]] RWStructuredBuffer<InstanceBuffer> DetailInstances : register(u2);
[[vk::binding(3, 0)]] RWStructuredBuffer<InstanceBuffer> ImposterInstances : register(u1);
[[vk::binding(4, 0)]] RWStructuredBuffer<DetailTransform> DetailTransforms : register(t0);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float3 CameraPosition;
	float ImposterRange;
	uint DetailCount;

	float Scale;
	float Offset;
}

#include "include/noise.hlsli"

float CompositeHeight(float2 p, out float4 w) {
	w = 0;

	float m = saturate(pow(1 - (billow4(p * .001, .5, 6534) * .5 + .5), 25));
	w.x = m;
	m *= .7 + .3 * (ridged4(p * .02, .5, 5214) * .5 + .5);

	float n = multi8(p * .01, .5, 6325) * .5 + .5;
	w.y = n;
	n = .98 * n + .02 * (fbm6(p * .2, .5, 7818) * .5 + .5);

	return .6 * m + .4 * n;
}

float4 SampleTerrain(float2 p) {
	float4 w;
	float h1 = CompositeHeight(0, w);
	float h0 = CompositeHeight(p, w);

	h1 = saturate(h1 - h0) * saturate(.03*(length(p) - 35));

	w *= w;
	w *= w;
	float s = dot(w, 1);
	return float4(h1, s > .0001 ? w.yzw / s : float3(1,0,0));
}

[numthreads(8, 8, 1)]
void GenHeight(uint3 id : SV_DispatchThreadID) {
	Heightmap[id.xy] = SampleTerrain((float2)id.xy * Scale + Offset);
}

float4 qinv(float4 q){
	return float4(-q.xyz, q.w) / dot(q, q);
}
float4x4 qtom(float4 q) {
	float3 c  = q.xxy * q.zyz;
	float3 q2 = q.xyz * q.xyz;
	float3 qw = q.xyz * q.w;

	float4x4 transform = float4x4(
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1 );
	transform[0][0] = 1 - 2 * (q2.y + q2.z);
	transform[0][1] = 2 * (c.y + qw.z);
	transform[0][2] = 2 * (c.x - qw.y);
	transform[1][0] = 2 * (c.y - qw.z);
	transform[1][1] = 1 - 2 * (q2.x + q2.z);
	transform[1][2] = 2 * (c.z + qw.x);
	transform[2][0] = 2 * (c.x + qw.y);
	transform[2][1] = 2 * (c.z - qw.x);
	transform[2][2] = 1 - 2 * (q2.x + q2.y);
	return transform;
}

[numthreads(64, 1, 1)]
void DrawDetails(uint3 id : SV_DispatchThreadID) {
	if (id.x >= DetailCount) return;

	DetailTransform d = DetailTransforms[id.x];

	float4x4 transform    = qtom(d.rotation);
	float4x4 invTransform = qtom(qinv(d.rotation));

	transform *= d.scale;
	transform[3].xyz = d.position;

	invTransform /= d.scale;
	invTransform[3].xyz = -d.position;

	if (length(CameraPosition - d.position) < ImposterRange) {
		uint index;
		InterlockedAdd(IndirectCommands[0].instanceCount, 1, index);
		DetailInstances[index].ObjectToWorld = transform;
		DetailInstances[index].WorldToObject = invTransform;
	}
}