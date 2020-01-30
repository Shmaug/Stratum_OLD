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
	// bind space -> object space
	vector<float4x4> skin(mRig.size());
	for (uint32_t i = 0; i < mRig.size(); i++)
		skin[i] = (WorldToObject() * mRig[i]->ObjectToWorld()) * mRig[i]->mInverseBind; // * vertex;
	
	::Mesh* m = MeshRenderer::Mesh();
	
	mVertexBuffer = commandBuffer->Device()->GetTempBuffer(mName + " VertexBuffer", m->VertexBuffer()->Size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	StdVertex* vertices = (StdVertex*)mVertexBuffer->MappedData();
	memcpy(vertices, mVertices.data(), m->VertexBuffer()->Size());

	for (uint32_t i = 0; i < mVertices.size(); i++) {
		float4x4 transform = 0;
		transform += skin[mWeights[i].Indices[0]] * mWeights[i].Weights[0];
		transform += skin[mWeights[i].Indices[1]] * mWeights[i].Weights[1];
		transform += skin[mWeights[i].Indices[2]] * mWeights[i].Weights[2];
		transform += skin[mWeights[i].Indices[3]] * mWeights[i].Weights[3];

		vertices[i].position = (transform * float4(mVertices[i].position, 1)).xyz;
		vertices[i].normal = (transpose(inverse(transform)) * float4(mVertices[i].normal, 1)).xyz;
	}

	/*
	ComputeShader* s = Scene()->AssetManager()->LoadShader("Shaders/skinner.stm")->GetCompute("skin", {});
	vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, s->mPipeline);

	DescriptorSet* ds = new DescriptorSet("Skinning", commandBuffer->Device(), s->mDescriptorSetLayouts[0]);
	ds->CreateStorageBufferDescriptor(m->VertexBuffer().get(), 0, m->VertexBuffer()->Size(), s->mDescriptorBindings.at("InputVertices").second.binding);
	ds->CreateStorageBufferDescriptor(mVertexBuffer,		   0, mVertexBuffer->Size(),     s->mDescriptorBindings.at("OutputVertices").second.binding);
	//ds->CreateStorageBufferDescriptor(m->WeightBuffer().get(), 0, m->WeightBuffer()->Size(), s->mDescriptorBindings.at("Weights").second.binding);
	//ds->CreateStorageBufferDescriptor(poseBuffer, 0, poseBuffer->Size(), s->mDescriptorBindings.at("Pose").second.binding);
	ds->FlushWrites();
	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, s->mPipelineLayout, 0, 1, *ds, 0, nullptr);

	uint32_t vc = m->VertexCount();
	uint32_t vs = sizeof(StdVertex);
	commandBuffer->PushConstant(s, "VertexCount", &vc);
	commandBuffer->PushConstant(s, "VertexStride", &vs);

	vkCmdDispatch(*commandBuffer, (vc + 63) / 64, 1, 1);
	*/
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