#include <Scene/SkinnedMeshRenderer.hpp>
#include <Scene/Scene.hpp>

using namespace std;

SkinnedMeshRenderer::SkinnedMeshRenderer(const string& name) : MeshRenderer(name), Object(name) {}
SkinnedMeshRenderer::~SkinnedMeshRenderer() {
    for (Bone* b : mRig)
        safe_delete(b);

	for (auto& d : mDeviceData) {
		for (uint32_t i = 0; i < d.first->MaxFramesInFlight(); i++) {
			safe_delete(d.second.mDescriptorSets[i]);
			safe_delete(d.second.mPoseBuffers[i]);
			safe_delete(d.second.mVertices[i]);
		}
		safe_delete_array(d.second.mPoseBuffers);
		safe_delete_array(d.second.mVertices);
		safe_delete_array(d.second.mDescriptorSets);
		safe_delete_array(d.second.mBoundLightBuffers);
	}
}

Bone* SkinnedMeshRenderer::GetBone(const string& boneName) const {
	if (mCopyRig)
		return mCopyRig->GetBone(boneName);
	else
		return mBoneMap.at(boneName);
}

void SkinnedMeshRenderer::Mesh(::Mesh* mesh, Object* rigRoot) {
	mMesh = mesh;

	if ((mCopyRig = dynamic_cast<SkinnedMeshRenderer*>(rigRoot)) != nullptr && rigRoot != this) {
        for (Bone* b : mRig)
            safe_delete(b);
		mRig.clear();
		mBoneMap.clear();
        mRigRoot = nullptr;
	} else {
		mRig.clear();
		mBoneMap.clear();
		mCopyRig = nullptr;
        mRigRoot = rigRoot;

        if (mesh) {
            AnimationRig& meshRig = *mesh->Rig();

            mRig.resize(meshRig.size());
            for (uint32_t i = 0; i < meshRig.size(); i++) {
				auto bone = make_shared<Bone>(meshRig[i]->mName, i);
				Scene()->AddObject(bone);
                mRig[i] = bone.get();
                mRig[i]->LocalPosition(meshRig[i]->LocalPosition());
                mRig[i]->LocalRotation(meshRig[i]->LocalRotation());
                mRig[i]->LocalScale(meshRig[i]->LocalScale());
                mRig[i]->mBindOffset = meshRig[i]->mBindOffset;
                if (Bone* parent = dynamic_cast<Bone*>(meshRig[i]->Parent()))
                    mRig[parent->mBoneIndex]->AddChild(mRig[i]);
                else
                    rigRoot->AddChild(mRig[i]);
                mBoneMap.emplace(mRig[i]->mName, mRig[i]);
            }
        }
    }
    Dirty();
}
void SkinnedMeshRenderer::Mesh(std::shared_ptr<::Mesh> mesh, Object* rigRoot) {
    Mesh(mesh.get(), rigRoot);
	mMesh = mesh;
}

void SkinnedMeshRenderer::PreRender(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, ::Material* materialOverride) {
	if (!mDeviceData.count(commandBuffer->Device())) {
		DeviceData& d = mDeviceData[commandBuffer->Device()];
		d.mDescriptorSets = new DescriptorSet*[commandBuffer->Device()->MaxFramesInFlight()];
		d.mBoundLightBuffers = new Buffer*[commandBuffer->Device()->MaxFramesInFlight()];
		d.mPoseBuffers = new Buffer*[commandBuffer->Device()->MaxFramesInFlight()];
		d.mVertices = new Buffer*[commandBuffer->Device()->MaxFramesInFlight()];
        d.mMesh = nullptr;
		memset(d.mDescriptorSets, 0, sizeof(DescriptorSet*) * commandBuffer->Device()->MaxFramesInFlight());
		memset(d.mBoundLightBuffers, 0, sizeof(Buffer*) * commandBuffer->Device()->MaxFramesInFlight());
		memset(d.mPoseBuffers, 0, sizeof(Buffer*) * commandBuffer->Device()->MaxFramesInFlight());
		memset(d.mVertices, 0, sizeof(Buffer*) * commandBuffer->Device()->MaxFramesInFlight());
	}
	DeviceData& data = mDeviceData.at(commandBuffer->Device());

	::Mesh* m = Mesh();
    if (data.mMesh != m) {
        data.mMesh = m;
        safe_delete(data.mVertices[backBufferIndex]);
    }

    if (!data.mPoseBuffers[backBufferIndex]){
        data.mPoseBuffers[backBufferIndex] = new Buffer(mName + " PoseBuffer", commandBuffer->Device(), mRig.size() * sizeof(float4x4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        data.mPoseBuffers[backBufferIndex]->Map();
    }
    if (!data.mVertices[backBufferIndex]){
        data.mVertices[backBufferIndex] = new Buffer(mName + " VertexBuffer", commandBuffer->Device(), m->VertexBuffer(commandBuffer->Device())->Size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VkBufferCopy region = {};
        region.size = m->VertexBuffer(commandBuffer->Device())->Size();
        vkCmdCopyBuffer(*commandBuffer, *m->VertexBuffer(commandBuffer->Device()), *data.mVertices[backBufferIndex], 1, &region);
    }

    if (!mCopyRig) {
		float4x4 rigOffset(1.f);
		if (mRigRoot) rigOffset = mRigRoot->WorldToObject();

		// pose space -> bone space
		float4x4* skin = (float4x4*)data.mPoseBuffers[backBufferIndex]->MappedData();
		for (uint32_t i = 0; i < mRig.size(); i++)
			skin[i] = mRig[i]->mBindOffset * mRig[i]->ObjectToWorld() * rigOffset;
    }
}

void SkinnedMeshRenderer::Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, ::Material* materialOverride) {
	::Material* material = materialOverride ? materialOverride : mMaterial.get();
	if (!material) return;

	::Mesh* m = Mesh();
	VkPipelineLayout layout = commandBuffer->BindMaterial(material, backBufferIndex, m->VertexInput(), m->Topology());
	if (!layout) return;
	auto shader = material->GetShader(commandBuffer->Device());

	DeviceData& data = mDeviceData.at(commandBuffer->Device());

	if (mNeedsObjectData == 2)
        mNeedsObjectData = (uint8_t)shader->mDescriptorBindings.count("Object");
	if (mNeedsObjectData) {
		VkPushConstantRange o2w = shader->mPushConstants.at("ObjectToWorld");
		VkPushConstantRange w2o = shader->mPushConstants.at("WorldToObject");
		float4x4 mt = ObjectToWorld();
		vkCmdPushConstants(*commandBuffer, layout, o2w.stageFlags, o2w.offset, o2w.size, &mt);
		mt = WorldToObject();
		vkCmdPushConstants(*commandBuffer, layout, w2o.stageFlags, w2o.offset, w2o.size, &mt);
	}

	if (mNeedsLightData == 2) {
		mNeedsLightData = (uint8_t)shader->mDescriptorBindings.count("Lights");
		if (mNeedsLightData)
			mLightCountRange = shader->mPushConstants.at("LightCount");
	}
	if (mNeedsLightData == 1) {
		if (!data.mDescriptorSets[backBufferIndex])
			data.mDescriptorSets[backBufferIndex] = new DescriptorSet(mName + " PerObject DescriptorSet", commandBuffer->Device()->DescriptorPool(), shader->mDescriptorSetLayouts[PER_OBJECT]);

		Buffer* lights = Scene()->LightBuffer(commandBuffer->Device(), backBufferIndex);
		if (data.mBoundLightBuffers[backBufferIndex] != lights) {
			data.mDescriptorSets[backBufferIndex]->CreateStorageBufferDescriptor(lights, shader->mDescriptorBindings.at("Lights").second.binding);
			data.mBoundLightBuffers[backBufferIndex] = lights;
		}

		uint32_t lc = (uint32_t)Scene()->ActiveLights().size();
		vkCmdPushConstants(*commandBuffer, layout, mLightCountRange.stageFlags, mLightCountRange.offset, mLightCountRange.size, &lc);
		VkDescriptorSet objds = *data.mDescriptorSets[backBufferIndex];
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &objds, 0, nullptr);
	}

	VkDeviceSize vboffset = 0;
	VkBuffer vb = *data.mVertices[backBufferIndex];
	vkCmdBindVertexBuffers(*commandBuffer, 0, 1, &vb, &vboffset);
	vkCmdBindIndexBuffer(*commandBuffer, *m->IndexBuffer(commandBuffer->Device()), 0, m->IndexType());
	vkCmdDrawIndexed(*commandBuffer, m->IndexCount(), 1, 0, 0, 0);

	commandBuffer->mTriangleCount += m->IndexCount() / 3;
}

void SkinnedMeshRenderer::DrawGizmos(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex) {
	if (mRig.size()){
		for (auto b : mRig) {
			Scene()->Gizmos()->DrawWireSphere(commandBuffer, backBufferIndex, b->WorldPosition(), .01f, float4(0.25f, 1.f, 0.25f, 1.f));
			if (Bone* parent = dynamic_cast<Bone*>(b->Parent()))
				Scene()->Gizmos()->DrawLine(commandBuffer, backBufferIndex, b->WorldPosition(), parent->WorldPosition(), float4(0.25f, 1.f, 0.25f, 1.f));
		}
	}
}