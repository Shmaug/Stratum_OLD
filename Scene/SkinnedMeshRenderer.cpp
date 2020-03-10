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
	Shader* skinner = Scene()->AssetManager()->LoadShader("Shaders/skinner.stm");
	::Mesh* m = MeshRenderer::Mesh();

	uint32_t vc = m->VertexCount();
	uint32_t no = offsetof(StdVertex, normal);
	uint32_t to = offsetof(StdVertex, tangent);
	uint32_t vs = m->VertexSize();

	mVertexBuffer = commandBuffer->Device()->GetTempBuffer(mName + " VertexBuffer", m->VertexBuffer()->Size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	
	VkBufferCopy rgn = {};
	rgn.size = mVertexBuffer->Size();
	vkCmdCopyBuffer(*commandBuffer, *m->VertexBuffer(), *mVertexBuffer, 1, &rgn);

	VkBufferMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.buffer = *mVertexBuffer;
	barrier.size = mVertexBuffer->Size();
	barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	vkCmdPipelineBarrier(*commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, nullptr, 1, &barrier, 0, nullptr);

	// Shape Keys
	if (mShapeKeys.size()) {
		float4 weights = 0;
		Buffer* targets[4] {
			mVertexBuffer, mVertexBuffer, mVertexBuffer, mVertexBuffer
		};

		uint32_t ti = 0;

		for (auto& it : mShapeKeys) {
			if (it.second > -.0001f && it.second < .0001f) continue;
			auto k = m->ShapeKey(it.first);
			if (!k) continue;

			targets[ti] = k.get();
			weights[ti] = it.second;
			ti++;
			if (ti > 3) break;
		}

		ComputeShader* s = skinner->GetCompute("blend", {});
		vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, s->mPipeline);
	
		DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet("Blend", s->mDescriptorSetLayouts[0]);
		ds->CreateStorageBufferDescriptor(mVertexBuffer, 0, mVertexBuffer->Size(), s->mDescriptorBindings.at("Vertices").second.binding);
		ds->CreateStorageBufferDescriptor(targets[0], 0, mVertexBuffer->Size(), s->mDescriptorBindings.at("BlendTarget0").second.binding);
		ds->CreateStorageBufferDescriptor(targets[1], 0, mVertexBuffer->Size(), s->mDescriptorBindings.at("BlendTarget1").second.binding);
		ds->CreateStorageBufferDescriptor(targets[2], 0, mVertexBuffer->Size(), s->mDescriptorBindings.at("BlendTarget2").second.binding);
		ds->CreateStorageBufferDescriptor(targets[3], 0, mVertexBuffer->Size(), s->mDescriptorBindings.at("BlendTarget3").second.binding);
		ds->FlushWrites();
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, s->mPipelineLayout, 0, 1, *ds, 0, nullptr);

		commandBuffer->PushConstant(s, "VertexCount", &vc);
		commandBuffer->PushConstant(s, "VertexStride", &vs);
		commandBuffer->PushConstant(s, "NormalOffset", &no);
		commandBuffer->PushConstant(s, "TangentOffset", &to);
		commandBuffer->PushConstant(s, "BlendFactors", &weights);

		vkCmdDispatch(*commandBuffer, (vc + 63) / 64, 1, 1);

		if (mRig.size()){
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			vkCmdPipelineBarrier(*commandBuffer,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0, 0, nullptr, 1, &barrier, 0, nullptr);

		}
	}

	// Skeleton
	if (mRig.size()) {
		// bind space -> object space
		Buffer* poseBuffer = commandBuffer->Device()->GetTempBuffer(mName + " Pose", mRig.size() * sizeof(float4x4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
		float4x4* skin = (float4x4*)poseBuffer->MappedData();
		for (uint32_t i = 0; i < mRig.size(); i++)
			skin[i] = (WorldToObject() * mRig[i]->ObjectToWorld()) * mRig[i]->mInverseBind; // * vertex;

		ComputeShader* s = skinner->GetCompute("skin", {});
		vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, s->mPipeline);

		DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet("Skinning", s->mDescriptorSetLayouts[0]);
		ds->CreateStorageBufferDescriptor(mVertexBuffer,		   0, mVertexBuffer->Size(),     s->mDescriptorBindings.at("Vertices").second.binding);
		ds->CreateStorageBufferDescriptor(m->WeightBuffer().get(), 0, m->WeightBuffer()->Size(), s->mDescriptorBindings.at("Weights").second.binding);
		ds->CreateStorageBufferDescriptor(poseBuffer, 0, poseBuffer->Size(), s->mDescriptorBindings.at("Pose").second.binding);
		ds->FlushWrites();
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, s->mPipelineLayout, 0, 1, *ds, 0, nullptr);

		commandBuffer->PushConstant(s, "VertexCount", &vc);
		commandBuffer->PushConstant(s, "VertexStride", &vs);
		commandBuffer->PushConstant(s, "NormalOffset", &no);
		commandBuffer->PushConstant(s, "TangentOffset", &to);

		vkCmdDispatch(*commandBuffer, (vc + 63) / 64, 1, 1);
	}

	barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	vkCmdPipelineBarrier(*commandBuffer,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
		0, 0, nullptr, 1, &barrier, 0, nullptr);
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
	float t = Scene()->TotalTime();
	commandBuffer->PushConstant(shader, "Time", &t);
	commandBuffer->PushConstant(shader, "LightCount", &lc);
	commandBuffer->PushConstant(shader, "ShadowTexelSize", &s);
	for (const auto& kp : mPushConstants)
		commandBuffer->PushConstant(shader, kp.first, &kp.second);
	
	if (instanceDS != VK_NULL_HANDLE)
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &instanceDS, 0, nullptr);

	commandBuffer->BindVertexBuffer(mVertexBuffer, 0, 0);
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

bool SkinnedMeshRenderer::Intersect(const Ray& ray, float* t, bool any) {
	return false;
}

void SkinnedMeshRenderer::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
	if (mRig.size()){
		for (auto b : mRig) {
			Gizmos::DrawWireSphere(b->WorldPosition(), .01f, float4(0.25f, 1.f, 0.25f, 1.f));
			if (Bone* parent = dynamic_cast<Bone*>(b->Parent()))
				Gizmos::DrawLine(b->WorldPosition(), parent->WorldPosition(), float4(0.25f, 1.f, 0.25f, 1.f));
		}
	}
}