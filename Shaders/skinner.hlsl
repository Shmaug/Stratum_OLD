#pragma kernel skin
#pragma kernel blend

#include <include/shadercompat.h>

[[vk::binding(0, 0)]] RWByteAddressBuffer Vertices				: register(u0);
[[vk::binding(1, 0)]] RWByteAddressBuffer BlendTarget0			: register(u1);
[[vk::binding(2, 0)]] RWByteAddressBuffer BlendTarget1			: register(u2);
[[vk::binding(3, 0)]] RWByteAddressBuffer BlendTarget2			: register(u3);
[[vk::binding(4, 0)]] RWByteAddressBuffer BlendTarget3			: register(u4);
[[vk::binding(5, 0)]] RWStructuredBuffer<VertexWeight> Weights	: register(u5);
[[vk::binding(6, 0)]] RWStructuredBuffer<float4x4> Pose			: register(u6);

[[vk::push_constant]] cbuffer PushConstants : register(b0) {
	uint VertexCount;
	uint VertexStride;
	uint NormalOffset;
	uint TangentOffset;

	float4 BlendFactors;
}

[numthreads(64, 1, 1)]
void skin(uint3 index : SV_DispatchThreadID) {
	if (index.x >= VertexCount) return;
	
	VertexWeight w = Weights[index.x];

	float4x4 transform = 0;
	transform += Pose[w.Indices[0]] * w.Weights[0];
	transform += Pose[w.Indices[1]] * w.Weights[1];
	transform += Pose[w.Indices[2]] * w.Weights[2];
	transform += Pose[w.Indices[3]] * w.Weights[3];

	uint address = index.x * VertexStride;
	float3 vertex = asfloat(Vertices.Load3(address));
	float3 normal = asfloat(Vertices.Load3(address + NormalOffset));
	float3 tangent = asfloat(Vertices.Load3(address + TangentOffset));

	vertex = mul(transform, float4(vertex, 1)).xyz;
	normal = mul((float3x3)transform, normal);
	tangent = mul((float3x3)transform, tangent);

	Vertices.Store3(address, asuint(vertex));
	Vertices.Store3(address + NormalOffset, asuint(normal));
	Vertices.Store3(address + TangentOffset, asuint(tangent));
}

[numthreads(64, 1, 1)]
void blend(uint3 index : SV_DispatchThreadID) {
	if (index.x >= VertexCount) return;
	
	uint address = index.x * VertexStride;

	float sum = dot(1, abs(BlendFactors));
	float isum = max(0, 1 - sum);

	float3 vertex = isum * asfloat(Vertices.Load3(address));
	float3 normal = isum * asfloat(Vertices.Load3(address + NormalOffset));
	float3 tangent = isum * asfloat(Vertices.Load3(address + TangentOffset));

	vertex += BlendFactors[0] * asfloat(BlendTarget0.Load3(address));
	normal += BlendFactors[0] * asfloat(BlendTarget0.Load3(address + NormalOffset));
	tangent += BlendFactors[0] * asfloat(BlendTarget0.Load3(address + TangentOffset));

	vertex += BlendFactors[1] * asfloat(BlendTarget1.Load3(address));
	normal += BlendFactors[1] * asfloat(BlendTarget1.Load3(address + NormalOffset));
	tangent += BlendFactors[1] * asfloat(BlendTarget1.Load3(address + TangentOffset));

	vertex += BlendFactors[2] * asfloat(BlendTarget2.Load3(address));
	normal += BlendFactors[2] * asfloat(BlendTarget2.Load3(address + NormalOffset));
	tangent += BlendFactors[2] * asfloat(BlendTarget2.Load3(address + TangentOffset));

	vertex += BlendFactors[3] * asfloat(BlendTarget3.Load3(address));
	normal += BlendFactors[3] * asfloat(BlendTarget3.Load3(address + NormalOffset));
	tangent += BlendFactors[3] * asfloat(BlendTarget3.Load3(address + TangentOffset));

	normal = normalize(normal);

	Vertices.Store3(address, asuint(vertex));
	Vertices.Store3(address + NormalOffset, asuint(normal));
	Vertices.Store3(address + TangentOffset, asuint(tangent));
}