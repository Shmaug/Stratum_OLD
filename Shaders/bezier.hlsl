#pragma vertex vsmain
#pragma fragment fsmain

#pragma render_queue 5000

#include <shadercompat.h>
#include "brdf.hlsli"

//#define SHOW_CASCADE_SPLITS

// per-object
[[vk::binding(OBJECT_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<float3> Spline : register(t0);
// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b0);

[[vk::push_constant]] cbuffer PushConstants : register(b1) {
	float4x4 ObjectToWorld;
	uint CurveCount;
	uint CurveResolution;
	float4 Color;
};

float3 Bezier(float3 p0, float3 p1, float3 p2, float3 p3, float t){
    float u = 1 - t;
    float u2 = u*u;
    float t2 = t*t;
    return u2*u * p0 + 3 * u2*t+p1 + 3*u*t2*p2 + t*t2*p3;
}
float3 Evaluate(float t) {
    if (t < 0) t += floor(abs(t)) + 1;
    if (t >= 1) t -= floor(t);

    uint curveIndex = t*CurveCount;
    uint n = 2*CurveCount;

    float3 p0,p1,p2,p3;
    if (curveIndex == 0) {
        p0 = Spline[0];
        p1 = Spline[1];
        p2 = Spline[2];
        p3 = Spline[3];
    } else if (curveIndex == CurveCount-1) {
        p0 = Spline[n-1];
        p1 = 2*Spline[n-1] - Spline[n-2];
        p2 = 2*Spline[0] - Spline[1];
        p3 = Spline[0];
    } else {
        p0 = Spline[curveIndex*2 + 1];
        p1 = 2*Spline[curveIndex*2 + 1] - Spline[2*curveIndex];
        p2 = Spline[curveIndex*2 + 2];
        p3 = Spline[curveIndex*2 + 3];
    }
    
    return Bezier(p0, p1, p2, p3, t*CurveCount - curveIndex);
}

void vsmain(uint v : SV_VertexID, out float4 position : SV_Target0, out float4 screenPos : TEXCOORD0) {
	//float4 worldPos = mul(ObjectToWorld, float4(Evaluate((float)v / (float)CurveResolution), 1.0));
	float4 worldPos = mul(ObjectToWorld, float4(0, 1, (v - CurveResolution*.5f)*.1f, 1.0));
	position = mul(Camera.ViewProjection, worldPos);
	screenPos = position;
}

void fsmain(in float4 screenPos : TEXCOORD0,
	out float4 color : SV_Target0,
	out float4 depthNormal : SV_Target1) {
		
	float depth;
	if (Camera.ProjParams.w)
		depth = screenPos.z * (Camera.Viewport.w - Camera.Viewport.z) + Camera.Viewport.z;
	else
		depth = screenPos.w;
	depth /= Camera.Viewport.w;

	color = Color;
	depthNormal = float4(0, 0, 0, depth);
}