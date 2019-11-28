#include <Core/DescriptorSet.hpp>
#include <Scene/MeshRenderer.hpp>
#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Environment.hpp>

#include <Shaders/shadercompat.h>

using namespace std;

MeshRenderer::MeshRenderer(const string& name)
	: Object(name), mVisible(true), mMesh(nullptr), mCollisionMask(0x01), mPassMask(Main) {}
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

void MeshRenderer::DrawInstanced(CommandBuffer* commandBuffer, Camera* camera, uint32_t instanceCount, VkDescriptorSet instanceDS, PassType pass) {
	if (!mMaterial) return;
	::Mesh* m = Mesh();
	if (!m) return;

	switch (pass) {
	case Main:
		mMaterial->DisableKeyword("DEPTH_PASS");
		Scene()->Environment()->SetEnvironment(camera, mMaterial.get());
		break;
	case Depth:
		mMaterial->EnableKeyword("DEPTH_PASS");
		break;
	}

	VkPipelineLayout layout = commandBuffer->BindMaterial(mMaterial.get(), m->VertexInput(), camera, m->Topology());
	if (!layout) return;
	auto shader = mMaterial->GetShader(commandBuffer->Device());

	uint32_t lc = (uint32_t)Scene()->ActiveLights().size();
	float2 s = Scene()->ShadowTexelSize();
	commandBuffer->PushConstant(shader, "LightCount", &lc);
	commandBuffer->PushConstant(shader, "ShadowTexelSize", &s);
	if (instanceDS != VK_NULL_HANDLE)
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &instanceDS, 0, nullptr);

	VkDeviceSize vboffset = 0;
	VkBuffer vb = *m->VertexBuffer(commandBuffer->Device());
	vkCmdBindVertexBuffers(*commandBuffer, 0, 1, &vb, &vboffset);
	vkCmdBindIndexBuffer(*commandBuffer, *m->IndexBuffer(commandBuffer->Device()), 0, m->IndexType());
	vkCmdDrawIndexed(*commandBuffer, m->IndexCount(), instanceCount, 0, 0, 0);
	commandBuffer->mTriangleCount += instanceCount * (m->IndexCount() / 3);
}

void MeshRenderer::Draw(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {
	DrawInstanced(commandBuffer, camera, 1, VK_NULL_HANDLE, pass);
}