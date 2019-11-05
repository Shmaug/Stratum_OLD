#include <Core/DescriptorSet.hpp>
#include <Scene/MeshRenderer.hpp>
#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>

#include <Shaders/shadercompat.h>

using namespace std;

MeshRenderer::MeshRenderer(const string& name)
	: Object(name), mVisible(true), mMesh(nullptr), mNeedsObjectData(2), mNeedsLightData(2), mLightCountRange({}), mCollisionMask(0x01) {}
MeshRenderer::~MeshRenderer() {}

bool MeshRenderer::UpdateTransform() {
	if (!Object::UpdateTransform()) return false;
	AABB mb = Mesh()->Bounds();
	mOBB = OBB((ObjectToWorld() * float4(mb.mCenter, 1)).xyz, mb.mExtents * WorldScale(), WorldRotation());
	mAABB = mOBB;
	return true;
}

void MeshRenderer::Material(shared_ptr<::Material> m) {
	mMaterial = m;
	mNeedsLightData = 2;
	mNeedsObjectData = 2;
}

bool MeshRenderer::Batchable(Device* device) {
	auto shader = mMaterial->GetShader(device);
	if (mNeedsLightData == 2) {
		mNeedsLightData = shader->mDescriptorBindings.count("Lights");
		if (mNeedsLightData == 1) mLightCountRange = shader->mPushConstants.at("LightCount");
	}
	if (mNeedsObjectData == 2) mNeedsObjectData = shader->mDescriptorBindings.count("Objects");
	return mNeedsLightData == 1 && mNeedsObjectData == 1;
}

void MeshRenderer::DrawInstanced(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, uint32_t instanceCount, VkDescriptorSet instanceDS, ::Material* materialOverride) {
	::Material* material = materialOverride ? materialOverride : mMaterial.get();

	::Mesh* m = Mesh();
	VkPipelineLayout layout = commandBuffer->BindMaterial(material, backBufferIndex, m->VertexInput(), m->Topology());
	if (!layout) return;
	auto shader = material->GetShader(commandBuffer->Device());

	if (mNeedsLightData == 1) {
		uint32_t lc = (uint32_t)Scene()->ActiveLights().size();
		vkCmdPushConstants(*commandBuffer, layout, mLightCountRange.stageFlags, mLightCountRange.offset, mLightCountRange.size, &lc);
	}
	if (mNeedsLightData == 1 && mNeedsObjectData == 1 && instanceDS != VK_NULL_HANDLE)
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &instanceDS, 0, nullptr);

	VkDeviceSize vboffset = 0;
	VkBuffer vb = *m->VertexBuffer(commandBuffer->Device());
	vkCmdBindVertexBuffers(*commandBuffer, 0, 1, &vb, &vboffset);
	vkCmdBindIndexBuffer(*commandBuffer, *m->IndexBuffer(commandBuffer->Device()), 0, m->IndexType());
	vkCmdDrawIndexed(*commandBuffer, m->IndexCount(), instanceCount, 0, 0, 0);
	commandBuffer->mTriangleCount += instanceCount * (m->IndexCount() / 3);
}

void MeshRenderer::Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, ::Material* materialOverride) {
	DrawInstanced(frameTime, camera, commandBuffer, backBufferIndex, 1, nullptr, materialOverride);
}

void MeshRenderer::DrawGizmos(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex) {
	Scene()->Gizmos()->DrawWireCube(Bounds().mCenter, Bounds().mExtents, quaternion(), float4(1));
};