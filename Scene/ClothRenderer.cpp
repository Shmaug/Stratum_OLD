#include <Core/DescriptorSet.hpp>
#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Environment.hpp>
#include <Util/Profiler.hpp>

#include <Shaders/include/shadercompat.h>

#include "ClothRenderer.hpp"

using namespace std;

ClothRenderer::ClothRenderer(const string& name)
	:  MeshRenderer(name), Object(name), mMove(0),
	mVertexBuffer(nullptr), mVelocityBuffer(nullptr), mForceBuffer(nullptr), mEdgeBuffer(nullptr), mCopyVertices(false), mPin(true),
	mFriction(5), mDrag(1), mStiffness(1000), mDamping(0.5f), mGravity(float3(0,-9.8f,0)) {}
ClothRenderer::~ClothRenderer() { safe_delete(mVertexBuffer); safe_delete(mVelocityBuffer); safe_delete(mForceBuffer); safe_delete(mEdgeBuffer); }

void ClothRenderer::Mesh(::Mesh* m) {
	mMesh = m;
	Dirty();

	// safe_delete(mVertexBuffer);
	// safe_delete(mVelocityBuffer);
	// safe_delete(mForceBuffer);
	// safe_delete(mEdgeBuffer);
	if (m) {
		uint32_t x = m->VertexCount()-1;

		mVertexBuffer = new Buffer(mName + "Vertices", m->VertexBuffer()->Device(), m->VertexBuffer()->Size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		mVelocityBuffer = new Buffer(mName + "Velocities", m->VertexBuffer()->Device(), m->VertexCount() * sizeof(float4), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		mForceBuffer = new Buffer(mName + "Forces", m->VertexBuffer()->Device(), sizeof(float4) * m->VertexCount(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		mEdgeBuffer = new Buffer(mName + "Edges", m->VertexBuffer()->Device(), (((x+x)*(x+x+1)/2+x)+31)/32, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		mCopyVertices = true;
	}
}
void ClothRenderer::Mesh(std::shared_ptr<::Mesh> m) {
	mMesh = m;
	Dirty();

	// safe_delete(mVertexBuffer);
	// safe_delete(mVelocityBuffer);
	// safe_delete(mForceBuffer);
	// safe_delete(mEdgeBuffer);
	if (m) {
		uint32_t x = m->VertexCount()-1;

		mVertexBuffer = new Buffer(mName + "Vertices", m->VertexBuffer()->Device(), m->VertexBuffer()->Size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		mVelocityBuffer = new Buffer(mName + "Velocities", m->VertexBuffer()->Device(), m->VertexCount() * sizeof(float4), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		mForceBuffer = new Buffer(mName + "Forces", m->VertexBuffer()->Device(), sizeof(float4) * m->VertexCount(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		mEdgeBuffer = new Buffer(mName + "Edges", m->VertexBuffer()->Device(), (((x+x)*(x+x+1)/2+x)+31)/32, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		mCopyVertices = true;
	}
}

bool ClothRenderer::UpdateTransform() {
	if (!Object::UpdateTransform()) return false;
	MeshRenderer::mAABB = MeshRenderer::Mesh()->Bounds();
	float3 e = mAABB.Extents();
	mAABB.mMin -= 10*e;
	mAABB.mMax += 10*e;
	mAABB *= ObjectToWorld();
	return true;
}

void ClothRenderer::FixedUpdate(CommandBuffer* commandBuffer) {
	if (!mVertexBuffer) return;
	::Mesh* m = MeshRenderer::Mesh();

	VkBufferMemoryBarrier b = {};
	b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

	if (mCopyVertices) {
		VkBufferCopy cpy = {};
		cpy.srcOffset = m->VertexSize() * m->BaseVertex();
		cpy.size = mVertexBuffer->Size();
		vkCmdCopyBuffer(*commandBuffer, *m->VertexBuffer(), *mVertexBuffer, 1, &cpy);
		vkCmdFillBuffer(*commandBuffer, *mVelocityBuffer, 0, mVelocityBuffer->Size(), 0);

		b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		b.buffer = *mVertexBuffer;
		b.size = mVertexBuffer->Size();
		vkCmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &b, 0, nullptr);

		b.buffer = *mVelocityBuffer;
		b.size = mVelocityBuffer->Size();
		vkCmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &b, 0, nullptr);

		mCopyVertices = false;
	}

	vkCmdFillBuffer(*commandBuffer, *mForceBuffer, 0, mForceBuffer->Size(), 0);
	vkCmdFillBuffer(*commandBuffer, *mEdgeBuffer, 0, mEdgeBuffer->Size(), 0);
	b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	b.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	b.buffer = *mForceBuffer;
	b.size = mForceBuffer->Size();
	vkCmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &b, 0, nullptr);
	b.buffer = *mEdgeBuffer;
	b.size = mEdgeBuffer->Size();
	vkCmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &b, 0, nullptr);

	Shader* shader = Scene()->AssetManager()->LoadShader("Shaders/cloth.stm");

	Buffer* sphereBuffer = commandBuffer->Device()->GetTempBuffer("Cloth Spheres", sizeof(float4) * max(1u, mSphereColliders.size()), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
	for (uint32_t i = 0; i < mSphereColliders.size(); i++)
		((float4*)sphereBuffer->MappedData())[i] = float4(mSphereColliders[i].first->WorldPosition(), mSphereColliders[i].second);
	
	Buffer* objBuffer = commandBuffer->Device()->GetTempBuffer("Cloth Obj", sizeof(float4x4) * 2, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
	((float4x4*)objBuffer->MappedData())[0] = ObjectToWorld();
	((float4x4*)objBuffer->MappedData())[1] = WorldToObject();

	uint32_t vc = m->VertexCount();
	uint32_t tc = m->IndexCount()/3;
	uint32_t no = offsetof(StdVertex, normal);
	uint32_t to = offsetof(StdVertex, tangent);
	uint32_t tco = offsetof(StdVertex, uv);
	uint32_t vs = sizeof(StdVertex);
	uint32_t sc = mSphereColliders.size();
	
	float dt = Scene()->FixedTimeStep();
	VkDeviceSize vsize = m->VertexSize();

	VkDeviceSize baseVertex = m->BaseVertex() * m->VertexSize();
	VkDeviceSize baseIndex = m->BaseIndex() * (m->IndexType() == VK_INDEX_TYPE_UINT16 ? sizeof(uint16_t) : sizeof(uint32_t));

	ComputeShader* add = m->IndexType() == VK_INDEX_TYPE_UINT16 ? shader->GetCompute("AddForces", {}) : shader->GetCompute("AddForces", {"INDEX_UINT32"});
	DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet("AddForces", add->mDescriptorSetLayouts[0]);
	ds->CreateUniformBufferDescriptor(objBuffer, 0, objBuffer->Size(), add->mDescriptorBindings.at("ObjectBuffer").second.binding);
	ds->CreateStorageBufferDescriptor(m->VertexBuffer().get(), baseVertex, m->VertexBuffer()->Size() - baseVertex, add->mDescriptorBindings.at("SourceVertices").second.binding);
	ds->CreateStorageBufferDescriptor(m->IndexBuffer().get(), baseIndex, m->IndexBuffer()->Size() - baseIndex, add->mDescriptorBindings.at("Triangles").second.binding);
	ds->CreateStorageBufferDescriptor(mVertexBuffer, 0, mVertexBuffer->Size(), add->mDescriptorBindings.at("Vertices").second.binding);
	ds->CreateStorageBufferDescriptor(mVelocityBuffer, 0, mVelocityBuffer->Size(), add->mDescriptorBindings.at("Velocities").second.binding);
	ds->CreateStorageBufferDescriptor(mForceBuffer, 0, mForceBuffer->Size(), add->mDescriptorBindings.at("Forces").second.binding);
	ds->CreateStorageBufferDescriptor(mEdgeBuffer, 0, mEdgeBuffer->Size(), add->mDescriptorBindings.at("Edges").second.binding);
	ds->FlushWrites();
	vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, add->mPipeline);
	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, add->mPipelineLayout, 0, 1, *ds, 0, nullptr);
	commandBuffer->PushConstant(add, "TriangleCount", &tc);
	commandBuffer->PushConstant(add, "VertexCount", &vc);
	commandBuffer->PushConstant(add, "VertexSize", &vsize);
	commandBuffer->PushConstant(add, "NormalLocation", &no);
	commandBuffer->PushConstant(add, "TangentLocation", &to);
	commandBuffer->PushConstant(add, "TexcoordLocation", &tco);
	commandBuffer->PushConstant(add, "Friction", &mFriction);
	commandBuffer->PushConstant(add, "Drag", &mDrag);
	commandBuffer->PushConstant(add, "SpringK", &mStiffness);
	commandBuffer->PushConstant(add, "SpringD", &mDamping);
	commandBuffer->PushConstant(add, "DeltaTime", &dt);
	commandBuffer->PushConstant(add, "SphereCount", &sc);
	commandBuffer->PushConstant(add, "Gravity", &mGravity);
	commandBuffer->PushConstant(add, "Move", &mMove);
	vkCmdDispatch(*commandBuffer, (tc + 63) / 64, 1, 1);


	b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	VkBufferMemoryBarrier bb[2] = { b, b };
	bb[0].buffer = *mForceBuffer;
	bb[0].size = mForceBuffer->Size();
	bb[1].buffer = *mVelocityBuffer;
	bb[1].size = mVelocityBuffer->Size();
	vkCmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 2, bb, 0, nullptr);


	ComputeShader* integrate = mPin ? shader->GetCompute("Integrate", { "PIN" }) : shader->GetCompute("Integrate", {});
	ds = commandBuffer->Device()->GetTempDescriptorSet("Integrate0", integrate->mDescriptorSetLayouts[0]);
	if (mPin) ds->CreateStorageBufferDescriptor(m->VertexBuffer().get(), baseVertex, m->VertexBuffer()->Size() - baseVertex, integrate->mDescriptorBindings.at("SourceVertices").second.binding);
	ds->CreateUniformBufferDescriptor(objBuffer, 0, objBuffer->Size(), integrate->mDescriptorBindings.at("ObjectBuffer").second.binding);
	ds->CreateStorageBufferDescriptor(mVertexBuffer, 0, mVertexBuffer->Size(), integrate->mDescriptorBindings.at("Vertices").second.binding);
	ds->CreateStorageBufferDescriptor(mVelocityBuffer, 0, mVelocityBuffer->Size(), integrate->mDescriptorBindings.at("Velocities").second.binding);
	ds->CreateStorageBufferDescriptor(mForceBuffer, 0, mForceBuffer->Size(), integrate->mDescriptorBindings.at("Forces").second.binding);
	ds->CreateStorageBufferDescriptor(sphereBuffer, 0, sphereBuffer->Size(), integrate->mDescriptorBindings.at("Spheres").second.binding);
	ds->FlushWrites();
	vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, integrate->mPipeline);
	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, integrate->mPipelineLayout, 0, 1, *ds, 0, nullptr);
	vkCmdDispatch(*commandBuffer, (vc + 63) / 64, 1, 1);


	b.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	b.buffer = *mVertexBuffer;
	b.size = mVertexBuffer->Size();
	vkCmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &b, 0, nullptr);


	ComputeShader* normals = m->IndexType() == VK_INDEX_TYPE_UINT16 ? shader->GetCompute("ComputeNormals0", {}) : shader->GetCompute("ComputeNormals0", { "INDEX_UINT32" });
	ds = commandBuffer->Device()->GetTempDescriptorSet("Normals0", normals->mDescriptorSetLayouts[0]);
	ds->CreateStorageBufferDescriptor(mVertexBuffer, 0, mVertexBuffer->Size(), normals->mDescriptorBindings.at("Verticesu").second.binding);
	ds->CreateStorageBufferDescriptor(m->VertexBuffer().get(), baseVertex, m->VertexBuffer()->Size() - baseVertex, normals->mDescriptorBindings.at("SourceVertices").second.binding);
	ds->CreateStorageBufferDescriptor(m->IndexBuffer().get(), baseIndex, m->IndexBuffer()->Size() - baseIndex, normals->mDescriptorBindings.at("Triangles").second.binding);
	ds->FlushWrites();
	vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, normals->mPipeline);
	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, normals->mPipelineLayout, 0, 1, *ds, 0, nullptr);
	vkCmdDispatch(*commandBuffer, (tc + 63) / 64, 1, 1);


	b.buffer = *mVertexBuffer;
	b.size = mVertexBuffer->Size();
	vkCmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &b, 0, nullptr);


	ComputeShader* normals2 = shader->GetCompute("ComputeNormals1", {});
	ds = commandBuffer->Device()->GetTempDescriptorSet("Normals1", normals2->mDescriptorSetLayouts[0]);
	ds->CreateStorageBufferDescriptor(mVertexBuffer, 0, mVertexBuffer->Size(), normals2->mDescriptorBindings.at("Vertices").second.binding);
	ds->FlushWrites();
	vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, normals2->mPipeline);
	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, normals2->mPipelineLayout, 0, 1, *ds, 0, nullptr);
	vkCmdDispatch(*commandBuffer, (vc + 63) / 64, 1, 1);


	b.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	b.buffer = *mVertexBuffer;
	b.size = mVertexBuffer->Size();
	vkCmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &b, 0, nullptr);
}

void ClothRenderer::PreRender(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {
	if (!mVertexBuffer) return;

	VkBufferMemoryBarrier b = {};
	b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	b.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	b.buffer = *mVertexBuffer;
	b.size = mVertexBuffer->Size();
	vkCmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &b, 0, nullptr);
}
void ClothRenderer::DrawInstanced(CommandBuffer* commandBuffer, Camera* camera, uint32_t instanceCount, VkDescriptorSet instanceDS, PassType pass) {
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
	vkCmdDrawIndexed(*commandBuffer, mesh->IndexCount(), instanceCount, mesh->BaseIndex(), 0, 0);
	commandBuffer->mTriangleCount += instanceCount * (mesh->IndexCount() / 3);

	if (camera->StereoMode() != STEREO_NONE) {
		camera->SetStereo(commandBuffer, shader, EYE_RIGHT);
		vkCmdDrawIndexed(*commandBuffer, mesh->IndexCount(), instanceCount, mesh->BaseIndex(), 0, 0);
		commandBuffer->mTriangleCount += instanceCount * (mesh->IndexCount() / 3);
	}
}

bool ClothRenderer::Intersect(const Ray& ray, float* t, bool any) {
	return false;
}