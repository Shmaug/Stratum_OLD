#pragma kernel skin

#include <include/shadercompat.h>

[[vk::binding(0, 0)]] RWStructuredBuffer<float> InputVertices	: register(u0);
[[vk::binding(1, 0)]] RWStructuredBuffer<float> OutputVertices	: register(u1);
[[vk::binding(2, 0)]] RWStructuredBuffer<VertexWeight> Weights	: register(u2);
[[vk::binding(3, 0)]] RWStructuredBuffer<float4x4> Pose			: register(u3);

[[vk::push_constant]] cbuffer PushConstants : register(b0) {
	uint VertexCount;
	uint VertexStride;
}

[numthreads(64, 1, 1)]
void skin(uint3 index : SV_DispatchThreadID) {
	if (index.x >= VertexCount) return;
	/*
	VertexWeight w = Weights[index.x];

	float4x4 pose = 0;
	float sum = dot(w.Weights, 1);

	if (sum < .0001) return;

	for (uint i = 0; i < 4; i++)
		if (w.Weights[i] > 0)
			pose += w.Weights[i] * Pose[w.Indices[i]];
	pose /= sum;

	float3x3 pose3 = (float3x3)pose;
	*/

	uint address = VertexStride * index.x;
	for (uint i = 0; i < VertexStride/4; i += 4) {
		uint x = InputVertices.Load(address + i);
		OutputVertices.Store(address + i, x);
	}

	//v.position.xyz = mul(pose, float4(v.position.xyz, 1)).xyz;
	//v.normal.xyz = normalize(mul(pose3, float3(v.normal.xyz)));
	//v.tangent = float4(mul(pose3, v.tangent.xyz), v.tangent.w);
}