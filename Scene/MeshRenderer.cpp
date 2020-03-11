#include <Core/DescriptorSet.hpp>
#include <Scene/MeshRenderer.hpp>
#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Environment.hpp>
#include <Util/Profiler.hpp>

#include <Shaders/include/shadercompat.h>

using namespace std;

MeshRenderer::MeshRenderer(const string& name)
	: Object(name), mVisible(true), mMesh(nullptr), mRayMask(0) {}
MeshRenderer::~MeshRenderer() {}

bool MeshRenderer::UpdateTransform() {
	if (!Object::UpdateTransform()) return false;
	mAABB = Mesh()->Bounds() * ObjectToWorld();
	return true;
}

void MeshRenderer::PreRender(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {
	if (pass == PASS_MAIN) Scene()->Environment()->SetEnvironment(camera, mMaterial.get());
}

void MeshRenderer::DrawInstanced(CommandBuffer* commandBuffer, Camera* camera, uint32_t instanceCount, VkDescriptorSet instanceDS, PassType pass) {
	::Mesh* mesh = Mesh();

	VkCullModeFlags cull = (pass == PASS_DEPTH) ? VK_CULL_MODE_NONE : VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
	VkPipelineLayout layout = commandBuffer->BindMaterial(mMaterial.get(), pass, mesh->VertexInput(), camera, mesh->Topology(), cull);
	if (!layout) return;
	auto shader = mMaterial->GetShader(pass);

	uint32_t lc = (uint32_t)Scene()->ActiveLights().size();
	float2 s = Scene()->ShadowTexelSize();
	float t = Scene()->TotalTime();
	commandBuffer->PushConstant(shader, "Time", &t);
	commandBuffer->PushConstant(shader, "LightCount", &lc);
	commandBuffer->PushConstant(shader, "ShadowTexelSize", &s);
	for (const auto& kp : mPushConstants)
		commandBuffer->PushConstant(shader, kp.first, &kp.second);
	
	if (instanceDS != VK_NULL_HANDLE)
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &instanceDS, 0, nullptr);

	commandBuffer->BindVertexBuffer(mesh->VertexBuffer().get(), 0, 0);
	commandBuffer->BindIndexBuffer(mesh->IndexBuffer().get(), 0, mesh->IndexType());
	camera->SetStereo(commandBuffer, shader, EYE_LEFT);
	vkCmdDrawIndexed(*commandBuffer, mesh->IndexCount(), instanceCount, mesh->BaseIndex(), mesh->BaseVertex(), 0);
	commandBuffer->mTriangleCount += instanceCount * (mesh->IndexCount() / 3);
	
	if (camera->StereoMode() != STEREO_NONE) {
		camera->SetStereo(commandBuffer, shader, EYE_RIGHT);
		vkCmdDrawIndexed(*commandBuffer, mesh->IndexCount(), instanceCount, mesh->BaseIndex(), mesh->BaseVertex(), 0);
		commandBuffer->mTriangleCount += instanceCount * (mesh->IndexCount() / 3);
	}
}

void MeshRenderer::Draw(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {
	DrawInstanced(commandBuffer, camera, 1, VK_NULL_HANDLE, pass);
}

bool MeshRenderer::Intersect(const Ray& ray, float* t, bool any) {
	::Mesh* m = Mesh();
	if (!m) return false;
	Ray r;
	r.mOrigin = (WorldToObject() * float4(ray.mOrigin, 1)).xyz;
	r.mDirection = (WorldToObject() * float4(ray.mDirection, 0)).xyz;
	return m->Intersect(r, t, any);
}