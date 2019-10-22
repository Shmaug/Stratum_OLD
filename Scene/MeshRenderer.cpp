#include <Core/DescriptorSet.hpp>
#include <Scene/MeshRenderer.hpp>
#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>

#include <Shaders/shadercompat.h>

using namespace std;

MeshRenderer::MeshRenderer(const string& name)
	: Object(name), mVisible(true), mMesh(nullptr), mNeedsLightData(2), mLightCountRange({}), mNeedsObjectData(true), mCollisionMask(0x01) {}
MeshRenderer::~MeshRenderer() {
	for (auto& d : mDeviceData) {
		for (uint32_t i = 0; i < d.first->MaxFramesInFlight(); i++) {
			safe_delete(d.second.mObjectBuffers[i]);
			safe_delete(d.second.mDescriptorSets[i]);
		}
		safe_delete_array(d.second.mObjectBuffers);
		safe_delete_array(d.second.mDescriptorSets);
		safe_delete_array(d.second.mUniformDirty);
		safe_delete_array(d.second.mBoundLightBuffers);
	}
}

bool MeshRenderer::UpdateTransform() {
	if (!Object::UpdateTransform()) return false;
	AABB mb = Mesh()->Bounds();
	mOBB = OBB(mb.mCenter, mb.mExtents * LocalScale(), WorldRotation());
	mAABB = mb * ObjectToWorld();
	for (auto& d : mDeviceData)
		memset(d.second.mUniformDirty, true, sizeof(bool) * d.first->MaxFramesInFlight());
	return true;
}

void MeshRenderer::Material(shared_ptr<::Material> m) {
	mMaterial = m;
	mNeedsLightData = 2;
	mNeedsObjectData = 2;
}

void MeshRenderer::Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, ::Material* materialOverride) {
	::Material* material = materialOverride ? materialOverride : mMaterial.get();
	if (!material) return;

	::Mesh* m = Mesh();
	VkPipelineLayout layout = commandBuffer->BindMaterial(material, backBufferIndex, m->VertexInput(), m->Topology());
	if (!layout) return;
	auto shader = material->GetShader(commandBuffer->Device());

	if (!mDeviceData.count(commandBuffer->Device())) {
		DeviceData& d = mDeviceData[commandBuffer->Device()];
		d.mObjectBuffers = new Buffer*[commandBuffer->Device()->MaxFramesInFlight()];
		d.mDescriptorSets = new DescriptorSet*[commandBuffer->Device()->MaxFramesInFlight()];
		d.mUniformDirty = new bool[commandBuffer->Device()->MaxFramesInFlight()];
		d.mBoundLightBuffers = new Buffer*[commandBuffer->Device()->MaxFramesInFlight()];
		memset(d.mObjectBuffers, 0, sizeof(Buffer*) * commandBuffer->Device()->MaxFramesInFlight());
		memset(d.mDescriptorSets, 0, sizeof(DescriptorSet*) * commandBuffer->Device()->MaxFramesInFlight());
		memset(d.mUniformDirty, true, sizeof(bool) * commandBuffer->Device()->MaxFramesInFlight());
		memset(d.mBoundLightBuffers, 0, sizeof(Buffer*) * commandBuffer->Device()->MaxFramesInFlight());
	}
	DeviceData& data = mDeviceData.at(commandBuffer->Device());

	if (mNeedsObjectData == 2)
		mNeedsObjectData = (uint8_t)shader->mDescriptorBindings.count("Object");
	if (mNeedsObjectData) {
		if (!data.mObjectBuffers[backBufferIndex]) {
			data.mObjectBuffers[backBufferIndex] = new Buffer(mName + " ObjectBuffer", commandBuffer->Device(), sizeof(ObjectBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			data.mObjectBuffers[backBufferIndex]->Map();
			data.mUniformDirty[backBufferIndex] = true;
		}
		if (!data.mDescriptorSets[backBufferIndex]) {
			data.mDescriptorSets[backBufferIndex] = new DescriptorSet(mName + " PerObject DescriptorSet", commandBuffer->Device()->DescriptorPool(), shader->mDescriptorSetLayouts[PER_OBJECT]);
			data.mDescriptorSets[backBufferIndex]->CreateUniformBufferDescriptor(data.mObjectBuffers[backBufferIndex], OBJECT_BUFFER_BINDING);
		}

		UpdateTransform();
		if (data.mUniformDirty[backBufferIndex]) {
			ObjectBuffer* objbuffer = (ObjectBuffer*)data.mObjectBuffers[backBufferIndex]->MappedData();
			objbuffer->ObjectToWorld = ObjectToWorld();
			objbuffer->WorldToObject = WorldToObject();
			data.mUniformDirty[backBufferIndex] = false;
		}

		VkDescriptorSet objds = *data.mDescriptorSets[backBufferIndex];
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &objds, 0, nullptr);
	}

	if (mNeedsLightData == 2) {
		mNeedsLightData = (uint8_t)shader->mDescriptorBindings.count("Lights");
		if (mNeedsLightData)
			mLightCountRange = shader->mPushConstants.at("LightCount");
	}
	if (mNeedsLightData == 1) {
		Buffer* lights = Scene()->LightBuffer(commandBuffer->Device(), backBufferIndex);
		if (data.mBoundLightBuffers[backBufferIndex] != lights) {
			data.mDescriptorSets[backBufferIndex]->CreateStorageBufferDescriptor(lights, shader->mDescriptorBindings.at("Lights").second.binding);
			data.mBoundLightBuffers[backBufferIndex] = lights;
		}

		uint32_t lc = (uint32_t)Scene()->Lights().size();
		vkCmdPushConstants(*commandBuffer, layout, mLightCountRange.stageFlags, mLightCountRange.offset, mLightCountRange.size, &lc);
	}

	VkDeviceSize vboffset = 0;
	VkBuffer vb = *m->VertexBuffer(commandBuffer->Device());
	vkCmdBindVertexBuffers(*commandBuffer, 0, 1, &vb, &vboffset);
	vkCmdBindIndexBuffer(*commandBuffer, *m->IndexBuffer(commandBuffer->Device()), 0, m->IndexType());
	vkCmdDrawIndexed(*commandBuffer, m->IndexCount(), 1, 0, 0, 0);
}

void MeshRenderer::DrawGizmos(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, ::Material* materialOverride) {
	Scene()->Gizmos()->DrawWireCube(commandBuffer, backBufferIndex, Bounds().mCenter, Bounds().mExtents, quaternion(0, 0, 0, 1), float4(1, 1, 1, 1));
	Scene()->Gizmos()->DrawWireCube(commandBuffer, backBufferIndex, ColliderBounds().mCenter, ColliderBounds().mExtents, ColliderBounds().mOrientation, float4(.4f, 1, .4f, 1));
}