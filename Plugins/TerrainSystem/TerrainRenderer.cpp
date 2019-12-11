#include "TerrainRenderer.hpp"
#include <Core/CommandBuffer.hpp>
#include <Scene/Environment.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <ThirdParty/stb_image.h>

#include "TriangleFan.hpp"

using namespace std;

inline float2 iqhash(float2 p) {
	return frac(sin(float2(dot(p, float2(127.1f, 311.7f)), dot(p, float2(269.5f, 183.3f)))) * 18.5453f);
}
inline float2 voronoi(const float2& x, float u) {
	float2 n = floor(x);
	float2 f = frac(x);

	float3 m = 8;
	for (int j = -1; j <= 1; j++)
		for (int i = -1; i <= 1; i++) {
			float2 g((float)i, (float)j);
			float2 o = g + iqhash(n + g) * u;
			float2 r = o - f;
			float d = dot(r, r);
			if (d < m.z) m = float3(o, d);
		}

	return n + m.xy;
}

TerrainRenderer::QuadNode::QuadNode(TerrainRenderer* terrain, QuadNode* parent, uint32_t siblingIndex, uint32_t lod, const float2& pos, float size)
 : mTerrain(terrain), mParent(parent), mSiblingIndex(siblingIndex), mSize(size), mLod(lod), mChildren(nullptr), mHasDetails(false), mPosition(float3(pos.x, terrain->Height(float3(pos.x, 0, pos.y)), pos.y)) {
	mVertexResolution = Resolution / size;
	mTriangleMask = 0;
	mDetails.resize(mTerrain->mDetails.size());
}
TerrainRenderer::QuadNode::~QuadNode() {
	safe_delete_array(mChildren);
}

void TerrainRenderer::QuadNode::GenerateDetails() {
	mHasDetails = false;
	mDetails.resize(mTerrain->mDetails.size());

	float3 corner = mPosition - mSize * .5f;
	for (uint32_t i = 0; i < mTerrain->mDetails.size(); i++) {
		if (mParent->mVertexResolution >= mTerrain->mDetails[i].mMinVertexResolution || mVertexResolution < mTerrain->mDetails[i].mMinVertexResolution) continue;

		mHasDetails = true;

		uint32_t c = mSize * mTerrain->mDetails[i].mFrequency;
		mDetails[i].resize(c * c);
		for (uint32_t x = 0; x < c; x++)
			for (uint32_t z = 0; z < c; z++) {
				DetailTransform* d = mDetails[i].data() + (x * c + z);
				float2 p = mSize * voronoi(float2((float)x, (float)z), 1) / (float)c;
				d->mPosition = corner + float3(p.x, 0, p.y);
				d->mPosition.y = mTerrain->Height(d->mPosition) + mTerrain->mDetails[i].mOffset;
				d->mScale = 1;
				d->mRotation = quaternion(0, 0, 0, 1);
			}
	}
}

void TerrainRenderer::QuadNode::Split() {
	if (mChildren) return;

	float s4 = mSize / 4;
	float s2 = mSize / 2;

	//  | 0 | 1 |
	//  | 2 | 3 |
	float3 o[4]{
		float3(-s4, 0, -s4),
		float3( s4, 0, -s4),
		float3(-s4, 0,  s4),
		float3( s4, 0,  s4)
	};

	mChildren = new QuadNode[4];
	for (uint32_t i = 0; i < 4; i++) {
		mChildren[i].mTerrain = mTerrain;
		mChildren[i].mParent = this;
		mChildren[i].mChildren = nullptr;
		mChildren[i].mSiblingIndex = i;
		mChildren[i].mLod = mLod + 1;
		mChildren[i].mPosition = mPosition + o[i];
		mChildren[i].mPosition.y = mTerrain->Height(mChildren[i].mPosition);
		mChildren[i].mSize = s2;
		mChildren[i].mVertexResolution = 2 * mVertexResolution;
		mChildren[i].mTriangleMask = 0;
		mChildren[i].GenerateDetails();
	}

	for (uint32_t i = 0; i < 4; i++)
		mChildren[i].ComputeTriangleFanMask();
	UpdateNeighbors();

}
void TerrainRenderer::QuadNode::Join() {
	if (!mChildren) return;
	safe_delete_array(mChildren);
	ComputeTriangleFanMask();
	UpdateNeighbors();
}

void TerrainRenderer::QuadNode::ComputeTriangleFanMask(bool recurse) {
	if (recurse && mChildren) {
		mChildren[0].ComputeTriangleFanMask();
		mChildren[1].ComputeTriangleFanMask();
		mChildren[2].ComputeTriangleFanMask();
		mChildren[3].ComputeTriangleFanMask();
	}

	QuadNode* r = RightNeighbor();
	QuadNode* l = LeftNeighbor();
	QuadNode* d = BackNeighbor();
	QuadNode* u = ForwardNeighbor();

	mTriangleMask = 0;
	if (l && l->mLod < mLod) mTriangleMask |= 1;
	if (u && u->mLod < mLod) mTriangleMask |= 2;
	if (r && r->mLod < mLod) mTriangleMask |= 4;
	if (d && d->mLod < mLod) mTriangleMask |= 8;
}
void TerrainRenderer::QuadNode::UpdateNeighbors() {
	QuadNode* r = RightNeighbor();
	QuadNode* l = LeftNeighbor();
	QuadNode* d = BackNeighbor();
	QuadNode* u = ForwardNeighbor();
	if (r) r->ComputeTriangleFanMask();
	if (l) l->ComputeTriangleFanMask();
	if (d) d->ComputeTriangleFanMask();
	if (u) u->ComputeTriangleFanMask();
}

bool TerrainRenderer::QuadNode::ShouldSplit(const float3& camPos, float tanFov) {
	if (mLod < 3) return true;

	float3 v = abs(mPosition - camPos) - mSize;

	float x = dot(v, v) / (mSize*mSize) * tanFov;
	if (mVertexResolution < 2 && x < 50) return true;

	QuadNode* l = LeftNeighbor();
	if (l && l->mChildren && (l->mChildren[1].mChildren || l->mChildren[3].mChildren)) return true;
	QuadNode* r = RightNeighbor();
	if (r && r->mChildren && (r->mChildren[0].mChildren || r->mChildren[2].mChildren)) return true;
	QuadNode* u = ForwardNeighbor();
	if (u && u->mChildren && (u->mChildren[2].mChildren || u->mChildren[3].mChildren)) return true;
	QuadNode* d = BackNeighbor();
	if (d && d->mChildren && (d->mChildren[0].mChildren || d->mChildren[1].mChildren)) return true;
	
	return false;
}

TerrainRenderer::QuadNode* TerrainRenderer::QuadNode::LeftNeighbor() {
	if (!mParent) return nullptr;
	QuadNode* n = nullptr;
	switch (mSiblingIndex) {
	case 0:
		n = mParent->LeftNeighbor();
		if (!n) return nullptr;
		return n->mChildren ? &n->mChildren[1] : n;
	case 2:
		n = mParent->LeftNeighbor();
		if (!n) return nullptr;
		return n->mChildren ? &n->mChildren[3] : n;
	case 1: return &mParent->mChildren[0];
	case 3: return &mParent->mChildren[2];
	}
	return nullptr;
}
TerrainRenderer::QuadNode* TerrainRenderer::QuadNode::RightNeighbor() {
	if (!mParent) return nullptr;
	QuadNode* n = nullptr;
	switch (mSiblingIndex) {
	case 1:
		n = mParent->RightNeighbor();
		if (!n) return nullptr;
		return n->mChildren ? &n->mChildren[0] : n;
	case 3:
		n = mParent->RightNeighbor();
		if (!n) return nullptr;
		return n->mChildren ? &n->mChildren[2] : n;
	case 0: return &mParent->mChildren[1];
	case 2: return &mParent->mChildren[3];
	}
	return nullptr;
}
TerrainRenderer::QuadNode* TerrainRenderer::QuadNode::BackNeighbor() {
	if (!mParent) return nullptr;
	QuadNode* n = nullptr;
	switch (mSiblingIndex) {
	case 0:
		n = mParent->BackNeighbor();
		if (!n) return nullptr;
		return n->mChildren ? &n->mChildren[2] : n;
	case 1:
		n = mParent->BackNeighbor();
		if (!n) return nullptr;
		return n->mChildren ? &n->mChildren[3] : n;
	case 2: return &mParent->mChildren[0];
	case 3: return &mParent->mChildren[1];
	}
	return nullptr;
}
TerrainRenderer::QuadNode* TerrainRenderer::QuadNode::ForwardNeighbor() {
	if (!mParent) return nullptr;
	QuadNode* n = nullptr;
	switch (mSiblingIndex) {
	case 2:
		n = mParent->ForwardNeighbor();
		if (!n) return nullptr;
		return n->mChildren ? &n->mChildren[0] : n;
	case 3:
		n = mParent->ForwardNeighbor();
		if (!n) return nullptr;
		return n->mChildren ? &n->mChildren[1] : n;
	case 0: return &mParent->mChildren[2];
	case 1: return &mParent->mChildren[3];
	}
	return nullptr;
}

TerrainRenderer::TerrainRenderer(const string& name, float size, float height)
	: Object(name), Renderer(), mSize(size), mHeight(height), mMaxVertexResolution(2.f), mHeights(nullptr) {}
void TerrainRenderer::Initialize() {
	mVisible = true;
	mIndexOffsets.resize(16);
	mIndexCounts.resize(16);

	uint32_t Resolution = 4096;
	mTerrainCompute = Scene()->AssetManager()->LoadShader("Shaders/terraincompute.shader");
	float scale = mSize / Resolution;
	float offset = -mSize * .5f;

	mHeights = new uint16_t[Resolution * Resolution];

	mHeightmap = new Texture(mName + " Heightmap", Scene()->Instance(), Resolution, Resolution, 1, VK_FORMAT_R16_UNORM,
		VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);

	for (uint32_t i = 0; i < Scene()->Instance()->DeviceCount(); i++) {
		Device* device = Scene()->Instance()->GetDevice(i);
		ComputeShader* gen = mTerrainCompute->GetCompute(device, "GenHeight", {});

		auto commandBuffer = device->GetCommandBuffer();
		mHeightmap->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, commandBuffer.get());

		vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, gen->mPipeline);
		DescriptorSet* ds = new DescriptorSet("Terrain DS", device, gen->mDescriptorSetLayouts[0]);
		ds->CreateStorageTextureDescriptor(mHeightmap, gen->mDescriptorBindings.at("Heightmap").second.binding);
		
		VkDescriptorSet vds = *ds;
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, gen->mPipelineLayout, 0, 1, &vds, 0, nullptr);
		
		commandBuffer->PushConstant(gen, "Scale", &scale);
		commandBuffer->PushConstant(gen, "Offset", &offset);
		vkCmdDispatch(*commandBuffer, (Resolution + 7) / 8, (Resolution + 7) / 8, 1);

		Buffer* dst = nullptr;
		if (i == 0) {
			mHeightmap->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, commandBuffer.get());

			dst = new Buffer("Heightmap", device, sizeof(uint16_t)*Resolution*Resolution, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
				
			VkBufferImageCopy rgn = {};
			rgn.imageExtent = { Resolution, Resolution, 1 };
			rgn.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			rgn.imageSubresource.layerCount = 1;
			vkCmdCopyImageToBuffer(*commandBuffer, mHeightmap->Image(device), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *dst, 1, &rgn);

			mHeightmap->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer.get());
		}else
			mHeightmap->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer.get());
		device->Execute(commandBuffer, false)->Wait();

		if (dst) {
			dst->Map();
			memcpy(mHeights, dst->MappedData(), sizeof(uint16_t) * Resolution * Resolution);
			delete dst;
		}

		delete ds;
	}

	mRootNode = new QuadNode(this, nullptr, 0, 0, 0, mSize);
}
TerrainRenderer::~TerrainRenderer() {
	safe_delete_array(mHeights);
	safe_delete(mHeightmap);
	safe_delete(mRootNode);
	for (auto d : mIndexBuffers)
		safe_delete(d.second);
}

void TerrainRenderer::UpdateLOD(Camera* camera) {
	// Create node buffer
	float3 cp = (WorldToObject() * float4(camera->WorldPosition(), 1)).xyz;
	float tanFov = tan(.5f * camera->FieldOfView());
	queue<QuadNode*> nodes;
	nodes.push(mRootNode);
	while (nodes.size()) {
		QuadNode* n = nodes.front();
		nodes.pop();

		if (n->ShouldSplit(cp, tanFov)) {
			n->Split();
			for (uint32_t i = 0; i < 4; i++)
				nodes.push(&n->mChildren[i]);
		} else
			n->Join();
	}

	mLeafNodes.clear();
	mDetailNodes.clear();
	nodes.push(mRootNode);
	while (nodes.size()) {
		QuadNode* n = nodes.front();
		nodes.pop();

		if (n->mHasDetails)
			mDetailNodes.push_back(n);

		if (n->mChildren)
			for (uint32_t i = 0; i < 4; i++)
				nodes.push(&n->mChildren[i]);
		else
			mLeafNodes.push_back(n);
	}

	if (!mLeafNodes.size()) return;
	sort(mLeafNodes.begin(), mLeafNodes.end(), [](QuadNode* a, QuadNode* b) {
		return a->mTriangleMask < b->mTriangleMask;
	});
}

float TerrainRenderer::Height(const float3& lp) {
	float2 p = (float2(lp.x, lp.z) + mSize * .5f) / mSize;
	p *= float2(mHeightmap->Width(), mHeightmap->Height());
	uint2 pi = clamp(uint2((uint32_t)p.x, (uint32_t)p.y), 0, uint2(mHeightmap->Width(), mHeightmap->Height()) - 2);
	float y0 = (float)mHeights[pi.y * mHeightmap->Width() + pi.x];
	float y1 = (float)mHeights[pi.y * mHeightmap->Width() + pi.x+1];
	float y2 = (float)mHeights[(pi.y+1) * mHeightmap->Width() + pi.x];
	float y3 = (float)mHeights[(pi.y+1) * mHeightmap->Width() + pi.x+1];
	return mHeight * lerp(lerp(y0, y1, p.x - pi.x), lerp(y2, y3, p.x - pi.x), p.y - pi.y) / (float)0xFFFF;;
}

bool TerrainRenderer::UpdateTransform(){
    if (!Object::UpdateTransform()) return false;
	mAABB = AABB(float3(0, mHeight * .5f, 0), float3(mSize, mHeight, mSize) * .5f) * ObjectToWorld();
    return true;
}

void TerrainRenderer::AddDetail(Mesh* mesh, std::shared_ptr<::Material> material, float imposterRange, bool surfaceAlign, float frequency, float res, float offset) {
	mDetails.push_back({ nullptr, imposterRange, mesh, material, surfaceAlign, frequency, offset, res });
}

void TerrainRenderer::PreRender(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {
	vector<QuadNode*> detailNodes;
	for (QuadNode* n : mDetailNodes)
		if (camera->IntersectFrustum(AABB(float3(n->mPosition.x, mHeight * .5f, n->mPosition.z), float3(n->mSize, mHeight, n->mSize) * .5f)))
			detailNodes.push_back(n);
	if (!detailNodes.size()) return;

	float3 cp = camera->WorldPosition();
	mIndirectBuffers.resize(mDetails.size());
	mInstanceBuffers.resize(mDetails.size());
	mIndirectBuffers.clear();
	mInstanceBuffers.clear();
	vector<uint32_t> counts(mDetails.size());
	vector<Buffer*> transformBuffers(mDetails.size());
	vector<VkBufferMemoryBarrier> barriers;

	for (uint32_t i = 0; i < mDetails.size(); i++) {
		counts[i] = 0;
		for (QuadNode* n : detailNodes)
			counts[i] += n->mDetails[i].size();
		transformBuffers[i] = commandBuffer->Device()->GetTempBuffer(mName + " Detail Transforms", counts[i] * sizeof(DetailTransform), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		DetailTransform* transforms = (DetailTransform*)transformBuffers[i]->MappedData();
		for (QuadNode* n : detailNodes) {
			if (n->mDetails[i].size()) {
				memcpy(transforms, n->mDetails[i].data(), n->mDetails[i].size() * sizeof(DetailTransform));
				transforms += n->mDetails[i].size();
			}
		}

		mIndirectBuffers[i] = commandBuffer->Device()->GetTempBuffer(mName + " Detail Indirect", 2*sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		memset(mIndirectBuffers[i]->MappedData(), 0, mIndirectBuffers[i]->Size());
		VkDrawIndexedIndirectCommand* indirect = (VkDrawIndexedIndirectCommand*)mIndirectBuffers[i]->MappedData();
		indirect->indexCount = mDetails[i].mMesh->IndexCount();

		VkBufferMemoryBarrier b = {};
		b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.buffer = *mIndirectBuffers[i];
		b.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
		b.size = VK_WHOLE_SIZE;
		barriers.push_back(b);
	}

	vkCmdPipelineBarrier(*commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
		0, nullptr, barriers.size(), barriers.data(), 0, nullptr);
	barriers.clear();

	for (uint32_t i = 0; i < mDetails.size(); i++) {
		mInstanceBuffers[i] = commandBuffer->Device()->GetTempBuffer(mName + " Detail Buffers", counts[i] * sizeof(ObjectBuffer), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		
		ComputeShader* compute = mTerrainCompute->GetCompute(commandBuffer->Device(), "DrawDetails", {});
		vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute->mPipeline);
		commandBuffer->PushConstant(compute, "DetailCount", &counts[i]);
		commandBuffer->PushConstant(compute, "CameraPosition", &cp);
		commandBuffer->PushConstant(compute, "ImposterRange", &mDetails[i].mImposterRange);
		DescriptorSet* compDesc = commandBuffer->Device()->GetTempDescriptorSet(mName + " Detail Compute", compute->mDescriptorSetLayouts[0]);
		compDesc->CreateStorageBufferDescriptor(mInstanceBuffers[i], compute->mDescriptorBindings.at("IndirectCommands").second.binding);
		compDesc->CreateStorageBufferDescriptor(mInstanceBuffers[i], compute->mDescriptorBindings.at("DetailInstances").second.binding);
		compDesc->CreateStorageBufferDescriptor(transformBuffers[i], compute->mDescriptorBindings.at("DetailTransforms").second.binding);
		VkDescriptorSet ds = *compDesc;
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute->mPipelineLayout, 0, 1, &ds, 0, nullptr);
		vkCmdDispatch(*commandBuffer, (counts[i] + 63) / 64, 1, 1);

		VkBufferMemoryBarrier b = {};
		b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.buffer = *mIndirectBuffers[i];
		b.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
		b.size = VK_WHOLE_SIZE;
		barriers.push_back(b);
		b.buffer = *mInstanceBuffers[i];
		b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		b.size = VK_WHOLE_SIZE;
		barriers.push_back(b);
	}

	vkCmdPipelineBarrier(*commandBuffer,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0,
		0, nullptr, barriers.size(), barriers.data(), 0, nullptr);

}

void TerrainRenderer::Draw(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {
	if (!mMaterial) return;

	VkCullModeFlags cull = VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
	if (pass & Main) Scene()->Environment()->SetEnvironment(camera, mMaterial.get());
	if (pass & Depth) mMaterial->EnableKeyword("DEPTH_PASS");
	else mMaterial->DisableKeyword("DEPTH_PASS");
	if (pass & Shadow) cull = VK_CULL_MODE_NONE;

	mMaterial->SetParameter("Heightmap", mHeightmap);

	vector<QuadNode*> leafNodes;
	for (QuadNode* n : mLeafNodes)
		if (camera->IntersectFrustum(AABB(float3(n->mPosition.x, mHeight * .5f, n->mPosition.z), float3(n->mSize, mHeight, n->mSize) * .5f)))
			leafNodes.push_back(n);

	if (!leafNodes.size()) return;

	#pragma region Populate descriptor set/push constants
	Buffer* nodeBuffer = commandBuffer->Device()->GetTempBuffer(mName + " Nodes", leafNodes.size() * sizeof(float4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	float4* bn = (float4*)nodeBuffer->MappedData();
	for (QuadNode* n : leafNodes) {
		*bn = float4(n->mPosition.x, 0, n->mPosition.z, n->mSize);
		bn++;
	}
    
	GraphicsShader* shader = mMaterial->GetShader(commandBuffer->Device());

	DescriptorSet* objds = commandBuffer->Device()->GetTempDescriptorSet(mName + to_string(commandBuffer->Device()->FrameContextIndex()), shader->mDescriptorSetLayouts[PER_OBJECT]);
	objds->CreateStorageBufferDescriptor(nodeBuffer, OBJECT_BUFFER_BINDING);
	if (shader->mDescriptorBindings.count("Lights"))
		objds->CreateStorageBufferDescriptor(Scene()->LightBuffer(commandBuffer->Device()), LIGHT_BUFFER_BINDING);
	if (shader->mDescriptorBindings.count("Shadows"))
		objds->CreateStorageBufferDescriptor(Scene()->ShadowBuffer(commandBuffer->Device()), SHADOW_BUFFER_BINDING);
	if (shader->mDescriptorBindings.count("ShadowAtlas"))
		objds->CreateSampledTextureDescriptor(Scene()->ShadowAtlas(commandBuffer->Device()), SHADOW_ATLAS_BINDING);
	
	VkPipelineLayout layout = commandBuffer->BindMaterial(mMaterial.get(), nullptr, camera, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, cull);
	if (!layout) return;

	VkDescriptorSet ds = *objds;
	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &ds, 0, nullptr);

	float sz = mSize * .5f;

	uint32_t lc = (uint32_t)Scene()->ActiveLights().size();
	float2 s = Scene()->ShadowTexelSize();
	float4x4 o2w = ObjectToWorld();
	float4x4 w2o = WorldToObject();
	commandBuffer->PushConstant(shader, "LightCount", &lc);
	commandBuffer->PushConstant(shader, "ShadowTexelSize", &s);
	commandBuffer->PushConstant(shader, "ObjectToWorld", &o2w);
	commandBuffer->PushConstant(shader, "WorldToObject", &w2o);
	commandBuffer->PushConstant(shader, "TerrainSize", &sz);
	commandBuffer->PushConstant(shader, "TerrainHeight", &mHeight);
	#pragma endregion

	if (mIndexBuffers.count(commandBuffer->Device()) == 0) {
		vector<uint16_t> indices;
		for (uint8_t i = 0; i < 16; i++) {
			mIndexOffsets[i] = (uint32_t)indices.size();
			GenerateTriangles(i, QuadNode::Resolution, indices);
			mIndexCounts[i] = (uint32_t)indices.size() - mIndexOffsets[i];
		}
		mIndexBuffers.emplace(commandBuffer->Device(), new Buffer(mName + " Indices", commandBuffer->Device(), indices.data(), indices.size() * sizeof(uint16_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
	}

	vkCmdBindIndexBuffer(*commandBuffer, *mIndexBuffers.at(commandBuffer->Device()), 0, VK_INDEX_TYPE_UINT16);

	// Render terrain surface
	uint32_t ni = 0;
	uint32_t si = 0;
	QuadNode* sn = leafNodes[0];
	for (QuadNode* n : leafNodes) {
		if (n->mTriangleMask != sn->mTriangleMask) {
			vkCmdDrawIndexed(*commandBuffer, mIndexCounts[sn->mTriangleMask], ni - si, mIndexOffsets[sn->mTriangleMask], 0, si);
			commandBuffer->mTriangleCount += (ni - si) * mIndexCounts[sn->mTriangleMask] / 3;
			sn = n;
			si = ni;
		}
		ni++;
	}
	if (ni > si) {
		vkCmdDrawIndexed(*commandBuffer, mIndexCounts[sn->mTriangleMask], ni - si, mIndexOffsets[sn->mTriangleMask], 0, si);
		commandBuffer->mTriangleCount += (ni - si) * mIndexCounts[sn->mTriangleMask] / 3;
	}

	// Details
	for (uint32_t i = 0; i < mDetails.size(); i++){
		if (pass & Main) Scene()->Environment()->SetEnvironment(camera, mDetails[i].mMaterial.get());
		if (pass & Depth) mDetails[i].mMaterial->EnableKeyword("DEPTH_PASS");
		else mDetails[i].mMaterial->DisableKeyword("DEPTH_PASS");

		layout = commandBuffer->BindMaterial(mDetails[i].mMaterial.get(), mDetails[i].mMesh->VertexInput(), camera, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, cull);
		if (!layout) return;

		shader = mDetails[i].mMaterial->GetShader(commandBuffer->Device());

		objds = commandBuffer->Device()->GetTempDescriptorSet(mName + to_string(commandBuffer->Device()->FrameContextIndex()), shader->mDescriptorSetLayouts[PER_OBJECT]);
		objds->CreateStorageBufferDescriptor(mInstanceBuffers[i], OBJECT_BUFFER_BINDING);
		if (shader->mDescriptorBindings.count("Lights"))
			objds->CreateStorageBufferDescriptor(Scene()->LightBuffer(commandBuffer->Device()), LIGHT_BUFFER_BINDING);
		if (shader->mDescriptorBindings.count("Shadows"))
			objds->CreateStorageBufferDescriptor(Scene()->ShadowBuffer(commandBuffer->Device()), SHADOW_BUFFER_BINDING);
		if (shader->mDescriptorBindings.count("ShadowAtlas"))
			objds->CreateSampledTextureDescriptor(Scene()->ShadowAtlas(commandBuffer->Device()), SHADOW_ATLAS_BINDING);

		ds = *objds;
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &ds, 0, nullptr);

		float t = Scene()->Instance()->TotalTime();
		commandBuffer->PushConstant(shader, "Time", &t);
		commandBuffer->PushConstant(shader, "LightCount", &lc);
		commandBuffer->PushConstant(shader, "ShadowTexelSize", &s);
		commandBuffer->PushConstant(shader, "TerrainSize", &sz);
		commandBuffer->PushConstant(shader, "TerrainHeight", &mHeight);

		VkDeviceSize vboffset = 0;
		VkBuffer vb = *mDetails[i].mMesh->VertexBuffer(commandBuffer->Device());
		vkCmdBindVertexBuffers(*commandBuffer, 0, 1, &vb, &vboffset);
		vkCmdBindIndexBuffer(*commandBuffer, *mDetails[i].mMesh->IndexBuffer(commandBuffer->Device()), 0, mDetails[i].mMesh->IndexType());
		vkCmdDrawIndexedIndirect(*commandBuffer, *mIndirectBuffers[i], 0, 1, 0);
	}
}

void TerrainRenderer::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {}