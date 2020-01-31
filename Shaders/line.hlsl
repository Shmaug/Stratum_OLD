#pragma vertex vsmain
#pragma fragment fsmain

#pragma render_queue 5000
#pragma cull false
#pragma zwrite false
#pragma blend alpha

#pragma multi_compile SCREEN_SPACE

#include "include/shadercompat.h"

[[vk::binding(INSTANCE_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<float2> Vertices : register(t0);
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b0);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	STRATUM_PUSH_CONSTANTS

	float4x4 ObjectToWorld;
	float4 Color;
	float4 ScaleTranslate;
	float4 Bounds;
	float2 ScreenSize;
}

#include "include/util.hlsli"

struct v2f {
	float4 position : SV_Position;
#ifndef SCREEN_SPACE
	float4 worldPos : TEXCOORD0;
#endif
	float2 canvasPos;
};

v2f vsmain(uint index : SV_VertexID) {
	float2 p = Vertices[index] * ScaleTranslate.xy + ScaleTranslate.zw;
	v2f o;
#ifdef SCREEN_SPACE
	o.position = float4((p / ScreenSize) * 2 - 1, .01, 1);
	o.position.y = -o.position.y;
#else
	float4x4 ct = float4x4(1, 0, 0, -Camera.Position.x, 0, 1, 0, -Camera.Position.y, 0, 0, 1, -Camera.Position.z, 0, 0, 0, 1);
	float4 worldPos = mul(mul(ct, ObjectToWorld), float4(p, 0, 1.0));
	o.position = mul(STRATUM_MATRIX_VP, worldPos);
	StratumOffsetClipPosStereo(o.position);
	o.worldPos = float4(worldPos.xyz, LinearDepth01(o.position.z));
#endif
	o.canvasPos = abs((p - Bounds.xy) / Bounds.zw) - 1;

	return o;
}

void fsmain(v2f i,
	out float4 color : SV_Target0,
	out float4 depthNormal : SV_Target1) {
#ifdef SCREEN_SPACE
	depthNormal = 0;
#else
	depthNormal = float4(cross(ddx(i.worldPos.xyz), ddy(i.worldPos.xyz)), i.worldPos.w);
#endif
	color = Color;
	color.a *= !any(i.canvasPos > 0);
}