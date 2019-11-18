#include "TerrainRenderer.hpp"
#include <Core/CommandBuffer.hpp>

#include "TriangleFan.hpp"

using namespace std;

TerrainRenderer::QuadNode::QuadNode(TerrainRenderer* terrain, QuadNode* parent, uint32_t siblingIndex, uint32_t lod, const float2& pos, float size)
 : mTerrain(terrain), mParent(parent), mSiblingIndex(siblingIndex), mSize(size), mLod(lod), mChildren(nullptr), mPosition(pos) {
	mVertexResolution = Resolution / size;
	mTriangleMask = 0;
}
TerrainRenderer::QuadNode::~QuadNode() {
	safe_delete(mChildren);
}

void TerrainRenderer::QuadNode::Split() {
	if (mChildren) return;

	//  | 0 | 1 |
	//  | 2 | 3 |
	float2 o[4]{
		float2(-mSize,  mSize) / 4,
		float2( mSize,  mSize) / 4,
		float2(-mSize, -mSize) / 4,
		float2( mSize, -mSize) / 4,
	};

	mChildren = new QuadNode[4];
	for (uint32_t i = 0; i < 4; i++) {
		mChildren[i].mTerrain = mTerrain;
		mChildren[i].mParent = this;
		mChildren[i].mChildren = nullptr;
		mChildren[i].mSiblingIndex = i;
		mChildren[i].mLod = mLod + 1;
		mChildren[i].mPosition = mPosition + o[i];
		mChildren[i].mSize = mSize / 2;
		mChildren[i].mVertexResolution = 2 * mVertexResolution;
		mChildren[i].mTriangleMask = 0;
	}

	for (uint32_t i = 0; i < 4; i++)
		mChildren[i].ComputeTriangleFanMask();
	UpdateNeighbors();

}
void TerrainRenderer::QuadNode::Join() {
	if (!mChildren) return;
	safe_delete(mChildren);
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

	uint32_t mask = 0;
	if (l && l->mLod < mLod) mask |= 1;
	if (u && u->mLod < mLod) mask |= 2;
	if (r && r->mLod < mLod) mask |= 4;
	if (d && d->mLod < mLod) mask |= 8;
	mTriangleMask = mask;
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
TerrainRenderer::QuadNode* TerrainRenderer::QuadNode::ForwardNeighbor() {
	if (!mParent) return nullptr;
	QuadNode* n = nullptr;
	switch (mSiblingIndex) {
	case 0:
		n = mParent->ForwardNeighbor();
		if (!n) return nullptr;
		return n->mChildren ? &n->mChildren[2] : n;
	case 1:
		n = mParent->ForwardNeighbor();
		if (!n) return nullptr;
		return n->mChildren ? &n->mChildren[3] : n;
	case 2: return &mParent->mChildren[0];
	case 3: return &mParent->mChildren[1];
	}
	return nullptr;
}
TerrainRenderer::QuadNode* TerrainRenderer::QuadNode::BackNeighbor() {
	if (!mParent) return nullptr;
	QuadNode* n = nullptr;
	switch (mSiblingIndex) {
	case 2:
		n = mParent->BackNeighbor();
		if (!n) return nullptr;
		return n->mChildren ? &n->mChildren[0] : n;
	case 3:
		n = mParent->BackNeighbor();
		if (!n) return nullptr;
		return n->mChildren ? &n->mChildren[1] : n;
	case 0: return &mParent->mChildren[2];
	case 1: return &mParent->mChildren[3];
	}
	return nullptr;
}


TerrainRenderer::TerrainRenderer(const string& name, float size, float height) : Object(name), Renderer(), mRootNode(nullptr), mSize(size), mHeight(height) {
	mVisible = true;
	mRootNode = new QuadNode(this, nullptr, 0, 0, 0, mSize);
	mIndexOffsets.resize(16);
	mIndexCounts.resize(16);
	mLeafNodes.push_back(mRootNode);
}
TerrainRenderer::~TerrainRenderer() {
    safe_delete(mRootNode);
}

bool TerrainRenderer::UpdateTransform(){
    if (!Object::UpdateTransform()) return false;
	mAABB = AABB(float3(0, mHeight / 2, 0), float3(mSize, mHeight, mSize) / 2) * ObjectToWorld();
    return true;
}

void TerrainRenderer::Draw(CommandBuffer* commandBuffer, Camera* camera, Scene::PassType pass) {
	if (!mMaterial) return;

	switch (pass) {
	case Scene::PassType::Main:
		mMaterial->DisableKeyword("DEPTH_PASS");
		break;
	case Scene::PassType::Depth:
		mMaterial->EnableKeyword("DEPTH_PASS");
		break;
	}

	VkPipelineLayout layout = commandBuffer->BindMaterial(mMaterial.get(), nullptr, camera, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	if (!layout) return;

    GraphicsShader* shader = mMaterial->GetShader(commandBuffer->Device());

	vector<QuadNode*> nodes = mLeafNodes;
	sort(nodes.begin(), nodes.end(), [](QuadNode* a, QuadNode* b) {
		return a->mTriangleMask < b->mTriangleMask;
	});
	Buffer* nodeBuffer = commandBuffer->Device()->GetTempBuffer(mName + " Nodes", nodes.size() * sizeof(float4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	float4* bn = (float4*)nodeBuffer->MappedData();
	for (uint32_t i = 0; i < nodes.size(); i++)
		bn[i] = float4(nodes[i]->mPosition.x, 0, nodes[i]->mPosition.y, nodes[i]->mSize);

	DescriptorSet* objds = commandBuffer->Device()->GetTempDescriptorSet(mName, shader->mDescriptorSetLayouts[PER_OBJECT]);
	objds->CreateStorageBufferDescriptor(nodeBuffer, 0, nodeBuffer->Size(), OBJECT_BUFFER_BINDING);
	if (shader->mDescriptorBindings.count("Lights")) {
		Buffer* b = Scene()->LightBuffer(commandBuffer->Device());
		objds->CreateStorageBufferDescriptor(b, 0, b->Size(), LIGHT_BUFFER_BINDING);
	}
	if (shader->mDescriptorBindings.count("Shadows")) {
		Buffer* b = Scene()->ShadowBuffer(commandBuffer->Device());
		objds->CreateStorageBufferDescriptor(b, 0, b->Size(), SHADOW_BUFFER_BINDING);
	}
	if (shader->mDescriptorBindings.count("ShadowAtlas"))
		objds->CreateSampledTextureDescriptor(Scene()->ShadowAtlas(commandBuffer->Device()), SHADOW_ATLAS_BINDING);
	VkDescriptorSet ds = *objds;
	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &ds, 0, nullptr);

	if (shader->mPushConstants.count("LightCount")) {
		VkPushConstantRange lightCountRange = shader->mPushConstants.at("LightCount");
		uint32_t lc = (uint32_t)Scene()->ActiveLights().size();
		vkCmdPushConstants(*commandBuffer, layout, lightCountRange.stageFlags, lightCountRange.offset, lightCountRange.size, &lc);
	}
	if (shader->mPushConstants.count("ShadowTexelSize")) {
		VkPushConstantRange strange = shader->mPushConstants.at("ShadowTexelSize");
		float2 s = Scene()->ShadowTexelSize();
		vkCmdPushConstants(*commandBuffer, layout, strange.stageFlags, strange.offset, strange.size, &s);
	}
	if (shader->mPushConstants.count("WorldToObject")) {
		VkPushConstantRange o2w = shader->mPushConstants.at("ObjectToWorld");
		float4x4 mt = ObjectToWorld();
		vkCmdPushConstants(*commandBuffer, layout, o2w.stageFlags, o2w.offset, o2w.size, &mt);
	}
	if (shader->mPushConstants.count("WorldToObject")) {
		VkPushConstantRange w2o = shader->mPushConstants.at("WorldToObject");
		float4x4 mt = WorldToObject();
		vkCmdPushConstants(*commandBuffer, layout, w2o.stageFlags, w2o.offset, w2o.size, &mt);
	}

	if (mIndexBuffers.count(commandBuffer->Device())) {
		vector<uint16_t> indices;
		for (uint8_t i = 0; i < 16; i++) {
			mIndexOffsets[i] = (uint32_t)indices.size();
			GenerateTriangles(i, QuadNode::Resolution, indices);
			mIndexCounts[i] = (uint32_t)indices.size() - mIndexOffsets[i];
		}
		Buffer* b = new Buffer(mName + " Indices", commandBuffer->Device(), indices.data(), indices.size() * sizeof(uint16_t), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}

	vkCmdBindIndexBuffer(*commandBuffer, *mIndexBuffers.at(commandBuffer->Device()), 0, VK_INDEX_TYPE_UINT16);

	uint32_t si = 0;
	for (uint32_t i = 0; i < nodes.size(); i++) {
		if (nodes[i]->mTriangleMask != nodes[si]->mTriangleMask) {
			vkCmdDrawIndexed(*commandBuffer, mIndexCounts[nodes[si]->mTriangleMask], (uint32_t)nodes.size(), mIndexOffsets[nodes[si]->mTriangleMask], 0, i - si);
			si = i;
		}
	}
	if (nodes.size() > si + 1)
		vkCmdDrawIndexed(*commandBuffer, mIndexCounts[nodes[si]->mTriangleMask], (uint32_t)nodes.size(), mIndexOffsets[nodes[si]->mTriangleMask], 0, nodes.size() - si);
}

void TerrainRenderer::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
   
}