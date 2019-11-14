#pragma vertex vsmain
#pragma fragment fsmain

#pragma render_queue 5000

#include <shadercompat.h>
#include "brdf.hlsli"

//#define SHOW_CASCADE_SPLITS

// per-object
[[vk::binding(OBJECT_BUFFER_BINDING, PER_OBJECT)]] StructuredBuffer<float3> Spline : register(t0);
// per-camera
[[vk::binding(CAMERA_BUFFER_BINDING, PER_CAMERA)]] ConstantBuffer<CameraBuffer> Camera : register(b1);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	uint CurveCount;
	uint CurveResolution;
	float4 Color;
};


float3 SplineRenderer::Evaluate(float t) {
    if (t < 0) t += (int)abs(t) + 1;
    if (t > 1) t -= floor(t);

    uint curveIndex = t*CurveCount;

    float3 p0,p1,p2,p3;
    if (curveIndex == 0) {
        p0 = mSpline[0];
        p1 = mSpline[1];
        p2 = mSpline[2];
        p3 = mSpline[3];
    } else if (curveIndex == CurveCount-1) {
        p0 = Spline[Spline.size()-1];
        p1 = 2*Spline[Spline.size()-1] - Spline[mSpline.size()-2];
        p2 = 2*Spline[0] - mSpline[1];
        p3 = Spline[0];
    } else {
        p0 = Spline[curveIndex*2 + 1];
        p1 = 2*Spline[curveIndex*2 + 1] - Spline[2*curveIndex];
        p2 = Spline[curveIndex*2 + 2];
        p3 = Spline[curveIndex*2 + 3];
    }
    
    return Bezier(p0, p1, p2, p3, t*n - curveIndex);
}

v2f vsmain(uint instance : SV_InstanceID) {
	v2f o;
	float4 worldPos = mul(Points[instance].ObjectToWorld, float4(vertex, 1.0));
	o.position = mul(Camera.ViewProjection, worldPos);
	o.screenPos = o.position;
	return o;
}

void fsmain(v2f i,
	out float4 color : SV_Target0,
	out float4 depthNormal : SV_Target1) {
		
	float depth;
	if (Camera.ProjParams.w)
		depth = i.screenPos.z * (Camera.Viewport.w - Camera.Viewport.z) + Camera.Viewport.z;
	else
		view = normalize(Camera.Position - i.worldPos);
		depth = i.screenPos.w;
	}
	depth /= Camera.Viewport.w;

	color = Color;
	depthNormal = float4(0, 0, 0, depth);
}