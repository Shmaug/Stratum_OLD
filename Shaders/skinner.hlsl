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
	
	VertexWeight w = Weights[index.x];

	float4x4 pose = 0;
	float sum = dot(w.Weights, 1);

	if (sum < .0001) return;

	for (uint i = 0; i < 4; i++)
		if (w.Weights[i] > 0)
			pose += w.Weights[i] * Pose[w.Indices[i]];
	pose /= sum;

	float3x3 pose3 = (float3x3)pose;

	uint address = VertexStride * index.x;

	float3 vertex;
	vertex.x = asfloat(InputVertices.Load(address + 0));
	vertex.y = asfloat(InputVertices.Load(address + 4));
	vertex.z = asfloat(InputVertices.Load(address + 8));

	vertex = mul(pose, float4(vertex, 1)).xyz;

	vertex.x = OutputVertices.Store(address + 0, asuint(vertex.x));
	vertex.y = OutputVertices.Store(address + 4, asuint(vertex.y));
	vertex.z = OutputVertices.Store(address + 8, asuint(vertex.z));
}