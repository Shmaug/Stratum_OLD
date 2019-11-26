#include "SplineRenderer.hpp"
#include <Core/CommandBuffer.hpp>

using namespace std;

float3 Bezier(const float3& p0, const float3& p1, const float3& p2, const float3& p3, float t){
    float u = 1 - t;
    float u2 = u*u;
    float t2 = t*t;
    return u2*u*p0 + 3*u2*t*p1 + 3*u*t2*p2 + t*t2*p3;
}
float3 BezierDerivative(const float3& p0, const float3& p1, const float3& p2, const float3& p3, float t){
    float u = 1 - t;
    float u2 = u*u;
    float t2 = t*t;
    return 3*u2*(p1-p0) + 6*u*t*(p2-p1) + 3*t2*(p3-p2);
}

SplineRenderer::SplineRenderer(const string& name) : Object(name), Renderer(), mCurveResolution(1024), mShader(nullptr) { mVisible = true; }
SplineRenderer::~SplineRenderer() {
    for (auto d : mPointBuffers){
        for (uint32_t i = 0; i < d.first->MaxFramesInFlight(); i++)
            safe_delete(d.second[i].second);
        safe_delete_array(d.second);
    }
}

float3 SplineRenderer::Evaluate(float t) {
    if (t < 0) t += (uint32_t)fabsf(t) + 1;
    if (t >= 1) t -= (uint32_t)t;

    uint32_t curveCount = (uint32_t)mSpline.size() / 2;
    uint32_t curveIndex = (uint32_t)(t*curveCount);

    uint32_t n = (uint32_t)mSpline.size();

    if (curveIndex == 0)
        return Bezier(mSpline[0], mSpline[1], mSpline[2], mSpline[3], t*curveCount - curveIndex);
    else if (curveIndex == curveCount-1) {
        return Bezier(mSpline[n-1], 2*mSpline[n-1] - mSpline[n-2], 2*mSpline[0] - mSpline[1], mSpline[0], t*curveCount - curveIndex);
    } else
        return Bezier(mSpline[2*curveIndex+1], 2*mSpline[2*curveIndex+1] - mSpline[2*curveIndex], mSpline[2*curveIndex+2], mSpline[2*curveIndex+3], t*curveCount - curveIndex);
    
}
float3 SplineRenderer::Derivative(float t){
    if (t < 0) t += (uint32_t)fabsf(t) + 1;
    if (t > 1) t -= (uint32_t)t;

    uint32_t curveCount = (uint32_t)mSpline.size() / 2 - 1;
    uint32_t curveIndex = (uint32_t)(t*curveCount);

    float3 p0,p1,p2,p3;
    if (curveIndex == 0) {
        p0 = mSpline[0];
        p1 = mSpline[1];
        p2 = mSpline[2];
        p3 = mSpline[3];
    } else if (curveIndex == curveCount-1) {
        p0 = mSpline[mSpline.size()-1];
        p1 = 2*mSpline[mSpline.size()-1] - mSpline[mSpline.size()-2];
        p2 = 2*mSpline[0] - mSpline[1];
        p3 = mSpline[0];
    } else {
        p0 = mSpline[curveIndex*2 + 1];
        p1 = 2*mSpline[curveIndex*2 + 1] - mSpline[2*curveIndex];
        p2 = mSpline[curveIndex*2 + 2];
        p3 = mSpline[curveIndex*2 + 3];
    }
    
    return BezierDerivative(p0, p1, p2, p3, t*curveCount - curveIndex);
}

void SplineRenderer::Points(const vector<float3>& pts){
    mSpline = pts;

    float3 mn = pts[0];
    float3 mx = pts[0];
    for (uint32_t i = 0; i < pts.size(); i++){
        mn = min(pts[i], mn);
        mx = max(pts[i], mx);
    }
    mPointAABB.mCenter = (mn + mx) * .5f;
    mPointAABB.mExtents = (mx - mn) * .5f;
    Dirty();

    for (auto d : mPointBuffers)
        for (uint32_t i = 0; i < d.first->MaxFramesInFlight(); i++)
            d.second[i].first = true;
}

bool SplineRenderer::UpdateTransform(){
    if (!Object::UpdateTransform()) return false;
    mAABB = mPointAABB * ObjectToWorld();
    return true;
}

void SplineRenderer::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
    for (float t = 0; t <= 1.f; t += 1.f / 1024.f)
        Scene()->Gizmos()->DrawLine(Evaluate(t - 1.f / 1024.f), Evaluate(t), float4(1));

	MouseKeyboardInput* input = Scene()->InputManager()->GetFirst<MouseKeyboardInput>();
	const Ray& ray = input->GetPointer(0)->mWorldRay;
    for (uint32_t i = 0; i < mSpline.size(); i++) {
        float3 p = mSpline[i];
        if (Scene()->Gizmos()->PositionHandle(input->GetPointer(0), camera->WorldRotation(), p, .1f, float4(1,.1f,1,1)))
            mSpline[i] = p;
    }

    for (uint32_t i = 0; i < mSpline.size() / 2; i++){
        uint32_t curveCount = (uint32_t)mSpline.size() / 2;
        uint32_t n = (uint32_t)mSpline.size();

        float3 p0,p1,p2,p3;
        if (i == 0){
            p0 = mSpline[0];
            p1 = mSpline[1];
            p2 = mSpline[2];
            p3 = mSpline[3];
        }else if (i == curveCount-1) {
            p0 = mSpline[n-1];
            p1 = (2.f*mSpline[n-1]) - mSpline[n-2];
            p2 = (2.f*mSpline[0]) - mSpline[1];
            p3 = mSpline[0];
        } else{
            p0 = mSpline[2*i + 1];
            p1 = (2.f*mSpline[2*i + 1]) - mSpline[2*i];
            p2 = mSpline[2*i + 2];
            p3 = mSpline[2*i + 3];
        }

        Scene()->Gizmos()->DrawLine(p0, p1, float4(1, 0, 1, 1));
        Scene()->Gizmos()->DrawLine(p2, p3, float4(1, 0, 1, 1));
    }
}