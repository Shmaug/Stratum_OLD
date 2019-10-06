#pragma vertex vsmain
#pragma fragment fsmain

#pragma render_queue 1000
#pragma cull false

#pragma static_sampler Sampler

#include <shadercompat.h>

struct Point {
	float4 Position;
	float4 Color;
};

// per-object
[[vk::binding(OBJECT_BUFFER_BINDING, PER_OBJECT)]] ConstantBuffer<ObjectBuffer> Object : register(b0);
[[vk::binding(BINDING_START, PER_OBJECT)]] StructuredBuffer<Point> Points : register(t0);
// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);
// per-material
[[vk::binding(BINDING_START + 1, PER_MATERIAL)]] Texture2D<float4> Noise : register(t1);
[[vk::binding(BINDING_START + 2, PER_MATERIAL)]] SamplerState Sampler : register(s0);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float Time;
	float PointSize;
	float3 Extents;
}

struct v2f {
	float4 position : SV_Position;
	float4 color : Color0;
	float3 rd : TEXCOORD0;
	float3 center : TEXCOORD1;
};
struct fs_out {
	float4 color : SV_Target0;
	float4 depthNormal : SV_Target1;
	uint coverage : SV_Coverage;
	float depth : SV_Depth;
};

v2f vsmain(uint id : SV_VertexId) {
	static const float3 offsets[6] = {
		float3(-1,-1, 0),
		float3( 1,-1, 0),
		float3(-1, 1, 0),
		float3( 1,-1, 0),
		float3( 1, 1, 0),
		float3(-1, 1, 0)
	};

	uint idx = id / 6;
	Point pt = Points[idx];
	float3 offset = offsets[id % 6];
	
	offset = offset.x * Camera.Right + offset.y * Camera.Up;

	float3 noise = Noise.SampleLevel(Sampler, float2(idx / 255, idx % 255) / 255, 0).xyz * 2 - 1;
	float t = saturate(Time);
	float3 p = lerp(noise * Extents, pt.Position, t * t * (3 - 2 * t));

	float4 wp = mul(Object.ObjectToWorld, float4(p + PointSize * offset, 1.0));
	v2f o;
	o.position = mul(Camera.ViewProjection, wp);
	o.rd = wp.xyz - Camera.Position;
	o.center = mul(Object.ObjectToWorld, float4(p, 1)).xyz;
	o.color = pt.Color;
	return o;
}

fs_out fsmain(v2f i) {
	float3 qp = Camera.Position - i.center;
	float a = dot(i.rd, i.rd);
	float b = dot(qp, i.rd);
	float c = dot(qp, qp) - PointSize * PointSize;

	float det = b * b - a * c;
	clip(det);

	float t = (-b - sqrt(det)) / a;

	float3 p = Camera.Position + i.rd * t;
	float3 normal = normalize(p - i.center);

	fs_out o;
	o.color = i.color * dot(normal, -normalize(i.rd));
	o.depthNormal = float4(normal * .5 + .5, length(i.rd) / Camera.Viewport.w);
	return o;
}