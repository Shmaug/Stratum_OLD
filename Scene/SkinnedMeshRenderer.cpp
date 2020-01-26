#include <Scene/SkinnedMeshRenderer.hpp>
#include <Scene/Scene.hpp>
#include <Util/Profiler.hpp>

using namespace std;

SkinnedMeshRenderer::SkinnedMeshRenderer(const string& name) : MeshRenderer(name), Object(name) {}
SkinnedMeshRenderer::~SkinnedMeshRenderer() {}

void SkinnedMeshRenderer::Rig(const AnimationRig& rig) {
	mBoneMap.clear();
	mRig = rig;
	for (auto b : mRig) mBoneMap.emplace(b->mName, b);
}

Bone* SkinnedMeshRenderer::GetBone(const string& boneName) const {
	return mBoneMap.count(boneName) ? mBoneMap.at(boneName) : nullptr;
}

void SkinnedMeshRenderer::PreFrame(CommandBuffer* commandBuffer) {
	uint32_t frameContextIndex = commandBuffer->Device()->FrameContextIndex();

	::Mesh* m = MeshRenderer::Mesh();

	mVertexBuffer = commandBuffer->Device()->GetTempBuffer(mName + " VertexBuffer", m->VertexBuffer()->Size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VkBufferCopy region = {};
	region.size = m->VertexBuffer()->Size();
	vkCmdCopyBuffer(*commandBuffer, *m->VertexBuffer(), *mVertexBuffer, 1, &region);

	Buffer* poseBuffer = commandBuffer->Device()->GetTempBuffer(mName + " PoseBuffer", mRig.size() * sizeof(float4x4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	
	// pose space -> bone space
	float4x4* skin = (float4x4*)poseBuffer->MappedData();
	for (uint32_t i = 0; i < mRig.size(); i++)
		skin[i] = mRig[i]->mBindOffset * mRig[i]->ObjectToWorld() * WorldToObject();
}

void SkinnedMeshRenderer::DrawInstanced(CommandBuffer* commandBuffer, Camera* camera, uint32_t instanceCount, VkDescriptorSet instanceDS, PassType pass) {
	::Mesh* mesh = MeshRenderer::Mesh();

	VkCullModeFlags cull = (pass == PASS_DEPTH) ? VK_CULL_MODE_NONE : VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
	VkPipelineLayout layout = commandBuffer->BindMaterial(mMaterial.get(), pass, mesh->VertexInput(), camera, mesh->Topology(), cull);
	if (!layout) return;
	auto shader = mMaterial->GetShader(pass);

	for (const auto& kp : mPushConstants)
		commandBuffer->PushConstant(shader, kp.first, &kp.second);

	uint32_t lc = (uint32_t)Scene()->ActiveLights().size();
	float2 s = Scene()->ShadowTexelSize();
	float t = Scene()->Instance()->TotalTime();
	commandBuffer->PushConstant(shader, "Time", &t);
	commandBuffer->PushConstant(shader, "LightCount", &lc);
	commandBuffer->PushConstant(shader, "ShadowTexelSize", &s);
	for (const auto& kp : mPushConstants)
		commandBuffer->PushConstant(shader, kp.first, &kp.second);
	
	if (instanceDS != VK_NULL_HANDLE)
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &instanceDS, 0, nullptr);

	commandBuffer->BindVertexBuffer(mVertexBuffer, 0, 0);
	commandBuffer->BindIndexBuffer(mesh->IndexBuffer().get(), 0, mesh->IndexType());
	vkCmdDrawIndexed(*commandBuffer, mesh->IndexCount(), instanceCount, mesh->BaseIndex(), mesh->BaseVertex(), 0);
	commandBuffer->mTriangleCount += instanceCount * (mesh->IndexCount() / 3);
}

void SkinnedMeshRenderer::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
	if (mRig.size()){
		for (auto b : mRig) {
			Scene()->Gizmos()->DrawWireSphere(b->WorldPosition(), .01f, float4(0.25f, 1.f, 0.25f, 1.f));
			if (Bone* parent = dynamic_cast<Bone*>(b->Parent()))
				Scene()->Gizmos()->DrawLine(b->WorldPosition(), parent->WorldPosition(), float4(0.25f, 1.f, 0.25f, 1.f));
		}
	}
}