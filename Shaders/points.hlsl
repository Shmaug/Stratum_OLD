#pragma vertex vsmain
#pragma fragment fsmain

#pragma render_queue 1000
#pragma cull false

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

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float Time;
	float PointSize;
	float3 Extents;
}

struct v2f {
	float4 position : SV_Position;
	float3 worldPos : TEXCOORD1;
	float4 color : Color0;
};
struct fs_out {
	float4 color : SV_Target0;
	float4 depthNormal : SV_Target1;
};

v2f vsmain(uint id : SV_VertexId) {
	v2f o;

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

	float3 p = pt.Position + PointSize * offset;
	float n = idx - floor(idx / 289.0) * 289.0 + 223.5245;
	float3 noise = frac(n * n * float3(.00000012, .00000543, .00000423)) * 2 - 1;
	float t = saturate(Time * .25);
	p = lerp(noise * Extents, p, t * t * (3 - 2 * t));

	float4 wp = mul(Object.ObjectToWorld, float4(p, 1.0));
	o.position = mul(Camera.ViewProjection, wp);
	o.worldPos = wp.xyz;
	o.color = pt.Color;

	return o;
}

fs_out fsmain(v2f i) {
	float3 normal = float3(0, 0, 1);

	fs_out o;
	o.color = i.color;
	o.depthNormal = float4(normal * .5 + .5, length(Camera.Position - i.worldPos.xyz) / Camera.Viewport.w);
	return o;
}