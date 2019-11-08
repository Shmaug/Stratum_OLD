#include <Core/DescriptorSet.hpp>
#include <Scene/MeshRenderer.hpp>
#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>

#include <Shaders/shadercompat.h>

using namespace std;

MeshRenderer::MeshRenderer(const string& name)
	: Object(name), mVisible(true), mMesh(nullptr), mCollisionMask(0x01), mCastShadows(true) {}
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
}

void MeshRenderer::DrawInstanced(CommandBuffer* commandBuffer, uint32_t backBufferIndex, Camera* camera, uint32_t instanceCount, VkDescriptorSet instanceDS, ::Material* materialOverride) {
	::Material* material = materialOverride ? materialOverride : mMaterial.get();

	::Mesh* m = Mesh();
	VkPipelineLayout layout = commandBuffer->BindMaterial(material, backBufferIndex, m->VertexInput(), camera, m->Topology());
	if (!layout) return;
	auto shader = material->GetShader(commandBuffer->Device());

	if (shader->mPushConstants.count("LightCount")) {
		VkPushConstantRange lightCountRange = shader->mPushConstants.at("LightCount");
		uint32_t lc = (uint32_t)Scene()->ActiveLights().size();
		vkCmdPushConstants(*commandBuffer, layout, lightCountRange.stageFlags, lightCountRange.offset, lightCountRange.size, &lc);
	}
	if (instanceDS != VK_NULL_HANDLE)
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &instanceDS, 0, nullptr);

	VkDeviceSize vboffset = 0;
	VkBuffer vb = *m->VertexBuffer(commandBuffer->Device());
	vkCmdBindVertexBuffers(*commandBuffer, 0, 1, &vb, &vboffset);
	vkCmdBindIndexBuffer(*commandBuffer, *m->IndexBuffer(commandBuffer->Device()), 0, m->IndexType());
	vkCmdDrawIndexed(*commandBuffer, m->IndexCount(), instanceCount, 0, 0, 0);
	commandBuffer->mTriangleCount += instanceCount * (m->IndexCount() / 3);
}

void MeshRenderer::Draw(CommandBuffer* commandBuffer, uint32_t backBufferIndex, Camera* camera, ::Material* materialOverride) {
	DrawInstanced(commandBuffer, backBufferIndex, camera, 1, VK_NULL_HANDLE, materialOverride);
}

void MeshRenderer::DrawGizmos(CommandBuffer* commandBuffer, uint32_t backBufferIndex, Camera* camera) {
	Scene()->Gizmos()->DrawWireCube(Bounds().mCenter, Bounds().mExtents, quaternion(), float4(1));
};