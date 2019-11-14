#include <Scene/SplineRenderer.hpp>
#include <Core/CommandBuffer.hpp>

using namespace std;

float3 Bezier(const float3& p0, const float3& p1, const float3& p2, const float3& p3, float t){
    float u = 1 - t;
    float u2 = u*u;
    float t2 = t*t;
    return u2*u * p0 + 3 * u2*t+p1 + 3*u*t2*p2 + t*t2*p3;
}
float3 BezierDerivative(const float3& p0, const float3& p1, const float3& p2, const float3& p3, float t){
    float u = 1 - t;
    float u2 = u*u;
    float t2 = t*t;
    return 3*u2*(p1-p0) + 6*u*t*(p2-p1) + 3*t2*(p3-p2);
}

SplineRenderer::SplineRenderer(const string& name) : Object(name), Renderer(), mCurveResolution(1024) {}

float3 SplineRenderer::Evaluate(float t) {
    if (t < 0) t += (uint32_t)fabsf(t) + 1;
    if (t > 1) t -= (uint32_t)t;

    uint32_t curveCount = (uint32_t)mSpline.size() / 2;
    uint32_t curveIndex = t*curveCount;

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
    
    return Bezier(p0, p1, p2, p3, t*n - curveIndex);
}
float3 SplineRenderer::Derivative(float t){
    if (t < 0) t += (uint32_t)fabsf(t) + 1;
    if (t > 1) t -= (uint32_t)t;

    uint32_t n = (uint32_t)mSpline.size() / 2 - 1;
    uint32_t curveIndex = t*n;

    float3 p0,p1,p2,p3;
    if (curveIndex == 0) {
        p0 = mSpline[0];
        p1 = mSpline[1];
        p2 = mSpline[2];
        p3 = mSpline[3];
    } else if (curveIndex == n-1) {
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
    
    return BezierDerivative(p0, p1, p2, p3, t*n - curveIndex);
}

void SplineRenderer::Points(const vector<float3>& pts){
    mSpline = pts;
    for (auto d : mPointBuffers)
        for (uint32_t i = 0; i < d.first->MaxFramesInFlight(); i++)
            d.second[i].first = true;
}

void SplineRenderer::Draw(CommandBuffer* commandBuffer, Camera* camera, ::Material* materialOverride) {
    if (materialOverride) return;
    if (!mShader) mShader = Scene()->AssetManager()->LoadShader("Shaders/bezier.shader");
    GraphicsShader* shader = mShader->GetGraphics(commandBuffer->Device(), {});
	VkPipelineLayout layout = commandBuffer->BindShader(shader, nullptr, camera, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
	if (!layout) return;

    if (mPointBuffers.count(commandBuffer->Device())) {
        pair<bool, Buffer*>* d = new pair<bool, Buffer*>[commandBuffer->Device()->MaxFramesInFlight()];
        mPointBuffers.emplace(commandBuffer->Device(), d);
        for (uint32_t i = 0; i < commandBuffer->Device()->MaxFramesInFlight(); i++){
            d[i].first = true;
            d[i].second = nullptr;
        }
    }
    auto& p = mPointBuffers.at(commandBuffer->Device())[commandBuffer->Device()->FrameContextIndex()];
    if (p.first || !p.second){
        safe_delete(p.second);
        p.second = new Buffer(mName, commandBuffer->Device(), mSpline.size() * sizeof(float3), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        p.second->Map();
        memcpy(p.second->MappedData(), mSpline.data(), mSpline.size() * sizeof(float3));
        p.first = false;
    }

    VkPushConstantRange rrange = shader->mPushConstants.at("CurveResolution");

    DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet(mName, shader->mDescriptorSetLayouts[PER_OBJECT]);
    ds->CreateStorageBufferDescriptor(p.second, 0, p.second->Size(), );

    uint32_t n = (uint32_t)mSpline.size() / 2;
    float4 color = 1;

    VkPushConstantRange ccrange = shader->mPushConstants.at("CurveCount");
    VkPushConstantRange crange = shader->mPushConstants.at("Color");
    vkCmdPushConstants(*commandBuffer, layout, ccrange.stageFlags, ccrange.offset, ccrange.size, &n);
    vkCmdPushConstants(*commandBuffer, layout, rrange.stageFlags, rrange.offset, rrange.size, &mCurveResolution);
    vkCmdPushConstants(*commandBuffer, layout, crange.stageFlags, crange.offset, crange.size, &color);
    vkCmdDraw(*commandBuffer, 100 * (mSpline.size() / 2), 1, 0, 0);
}