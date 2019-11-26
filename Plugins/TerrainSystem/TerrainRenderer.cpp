#include "TerrainRenderer.hpp"
#include <Core/CommandBuffer.hpp>

#include "TriangleFan.hpp"

#include <Plugins/Environment/Environment.hpp>

using namespace std;

TerrainRenderer::QuadNode::QuadNode(TerrainRenderer* terrain, QuadNode* parent, uint32_t siblingIndex, uint32_t lod, const float2& pos, float size)
 : mTerrain(terrain), mParent(parent), mSiblingIndex(siblingIndex), mSize(size), mLod(lod), mChildren(nullptr), mPosition(pos) {
	mVertexResolution = Resolution / size;
	mTriangleMask = 0;
}
TerrainRenderer::QuadNode::~QuadNode() {
	safe_delete_array(mChildren);
}

void TerrainRenderer::QuadNode::Split() {
	if (mChildren) return;

	float s4 = mSize / 4;
	float s2 = mSize / 2;

	//  | 0 | 1 |
	//  | 2 | 3 |
	float2 o[4]{
		float2(-s4, -s4),
		float2( s4, -s4),
		float2(-s4,  s4),
		float2( s4,  s4)
	};

	mChildren = new QuadNode[4];
	for (uint32_t i = 0; i < 4; i++) {
		mChildren[i].mTerrain = mTerrain;
		mChildren[i].mParent = this;
		mChildren[i].mChildren = nullptr;
		mChildren[i].mSiblingIndex = i;
		mChildren[i].mLod = mLod + 1;
		mChildren[i].mPosition = mPosition + o[i];
		mChildren[i].mSize = s2;
		mChildren[i].mVertexResolution = 2 * mVertexResolution;
		mChildren[i].mTriangleMask = 0;
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

bool TerrainRenderer::QuadNode::ShouldSplit(const float2& camPos) {
	float2 v = mSize - abs(mPosition - camPos);
	if (mVertexResolution < mTerrain->mMaxVertexResolution && v.x > 0 && v.y > 0) return true;

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
 : Object(name), Renderer(), mSize(size), mHeight(height), mMaxVertexResolution(2.f) {
	mVisible = true;
	mIndexOffsets.resize(16);
	mIndexCounts.resize(16);
}
TerrainRenderer::~TerrainRenderer() {
	for (auto d : mIndexBuffers)
		safe_delete(d.second);
}

bool TerrainRenderer::UpdateTransform(){
    if (!Object::UpdateTransform()) return false;
	mAABB = AABB(float3(0, 0, 0), float3(mSize, mHeight, mSize) / 2) * ObjectToWorld();
    return true;
}

void TerrainRenderer::Draw(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {
	if (!mMaterial) return;

	switch (pass) {
	case Main:
		mMaterial->DisableKeyword("DEPTH_PASS");
		mMaterial->SetParameter("EnvironmentTexture", Scene()->PluginManager()->GetPlugin<Environment>()->EnvironmentTexture(commandBuffer->Device()));
		break;
	case Depth:
		mMaterial->EnableKeyword("DEPTH_PASS");
		break;
	}

	VkPipelineLayout layout = commandBuffer->BindMaterial(mMaterial.get(), nullptr, camera, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	if (!layout) return;

    GraphicsShader* shader = mMaterial->GetShader(commandBuffer->Device());

	// Create node buffer
	float3 lc = (WorldToObject() * float4(camera->WorldPosition(), 1)).xyz;
	float2 cp = float2(lc.x, lc.z);
	QuadNode* root = new QuadNode(this, nullptr, 0, 0, 0, mSize);
	vector<QuadNode*> leafNodes;
	queue<QuadNode*> nodes;
	nodes.push(root);
	while(nodes.size()){
		QuadNode* n = nodes.front();
		nodes.pop();

		bool split = n->ShouldSplit(cp);

		if (!n->mChildren && split)
			n->Split();
		else if (n->mParent && !split)
			n->Join();

		if (n->mChildren)
			for (uint32_t i = 0; i < 4; i++)
				nodes.push(&n->mChildren[i]);
		else
			leafNodes.push_back(n);
	}
	sort(leafNodes.begin(), leafNodes.end(), [](QuadNode* a, QuadNode* b) {
		return a->mTriangleMask < b->mTriangleMask;
	});
	Buffer* nodeBuffer = commandBuffer->Device()->GetTempBuffer(mName + " Nodes", leafNodes.size() * sizeof(float4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	float4* bn = (float4*)nodeBuffer->MappedData();
	for (QuadNode* n : leafNodes) {
		*bn = float4(n->mPosition.x, 0, n->mPosition.y, n->mSize);
		bn++;
	}
	delete root;

	#pragma region Populate descriptor set/push constants
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
	if (shader->mPushConstants.count("ObjectToWorld")) {
		VkPushConstantRange o2w = shader->mPushConstants.at("ObjectToWorld");
		float4x4 mt = ObjectToWorld();
		vkCmdPushConstants(*commandBuffer, layout, o2w.stageFlags, o2w.offset, o2w.size, &mt);
	}
	if (shader->mPushConstants.count("WorldToObject")) {
		VkPushConstantRange w2o = shader->mPushConstants.at("WorldToObject");
		float4x4 mt = WorldToObject();
		vkCmdPushConstants(*commandBuffer, layout, w2o.stageFlags, w2o.offset, w2o.size, &mt);
	}
	if (shader->mPushConstants.count("TerrainHeight")) {
		VkPushConstantRange th = shader->mPushConstants.at("TerrainHeight");
		vkCmdPushConstants(*commandBuffer, layout, th.stageFlags, th.offset, th.size, &mHeight);
	}
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

	uint32_t i = 0;
	uint32_t si = 0;
	QuadNode* sn = leafNodes[0];
	for (QuadNode* n : leafNodes) {
		if (n->mTriangleMask != sn->mTriangleMask) {
			vkCmdDrawIndexed(*commandBuffer, mIndexCounts[sn->mTriangleMask], i - si, mIndexOffsets[sn->mTriangleMask], 0, si);
			commandBuffer->mTriangleCount += (i - si) * mIndexCounts[sn->mTriangleMask] / 3;
			sn = n;
			si = i;
		}
		i++;
	}
	if (i > si){
		vkCmdDrawIndexed(*commandBuffer, mIndexCounts[sn->mTriangleMask], i - si, mIndexOffsets[sn->mTriangleMask], 0, si);
		commandBuffer->mTriangleCount += (i - si) * mIndexCounts[sn->mTriangleMask] / 3;
	}
}

void TerrainRenderer::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
   
}