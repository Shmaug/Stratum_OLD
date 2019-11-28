#include "TerrainRenderer.hpp"
#include <Core/CommandBuffer.hpp>
#include <Scene/Environment.hpp>

#include "TriangleFan.hpp"

#include <Shaders/noise.hlsli>

using namespace std;

TerrainRenderer::QuadNode::QuadNode(TerrainRenderer* terrain, QuadNode* parent, uint32_t siblingIndex, uint32_t lod, const float2& pos, float size)
 : mTerrain(terrain), mParent(parent), mSiblingIndex(siblingIndex), mSize(size), mLod(lod), mChildren(nullptr), mPosition(float3(pos.x, terrain->Height(float3(pos.x, 0, pos.y)), pos.y)) {
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
	float3 v = abs(mPosition - camPos);

	float verticesPerPixel = mVertexResolution * length(v) * tanFov;
	if (mVertexResolution < 2 && verticesPerPixel < .02f) return true;

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


float TerrainRenderer::Height(const float3& lp) {
	float m, l;
	return SampleTerrain(float2(lp.x, lp.z), m, l) * mHeight;
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
		Scene()->Environment()->SetEnvironment(camera, mMaterial.get());
		break;
	case Depth:
		mMaterial->EnableKeyword("DEPTH_PASS");
		break;
	}

	VkPipelineLayout layout = commandBuffer->BindMaterial(mMaterial.get(), nullptr, camera, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	if (!layout) return;

    GraphicsShader* shader = mMaterial->GetShader(commandBuffer->Device());

	// Create node buffer
	float3 cp = (WorldToObject() * float4(camera->WorldPosition(), 1)).xyz;
	float tanFov = tan(.5f * camera->FieldOfView()) / (.5f * camera->FramebufferHeight());
	QuadNode* root = new QuadNode(this, nullptr, 0, 0, 0, mSize);
	queue<QuadNode*> nodes;
	nodes.push(root);
	while(nodes.size()){
		QuadNode* n = nodes.front();
		nodes.pop();

		if (camera->IntersectFrustum(AABB(float3(n->mPosition.x, mHeight * .5f, n->mPosition.z), float3(n->mSize, mHeight, n->mSize) * .5f)))

		if (n->ShouldSplit(cp, tanFov)) {
			n->Split();
			for (uint32_t i = 0; i < 4; i++)
				nodes.push(&n->mChildren[i]);

			QuadNode* nn[4]{
				n->LeftNeighbor(),
				n->RightNeighbor(),
				n->ForwardNeighbor(),
				n->BackNeighbor(),
			};
			for (uint32_t i = 0; i < 4; i++)
				if (nn[i] && nn[i]->mLod < n->mLod) {
					nn[i]->Split();
					for (uint32_t j = 0; j < 4; j++)
						nodes.push(&nn[i]->mChildren[j]);
				}
		}
	}

	nodes.push(root);
	vector<QuadNode*> leafNodes;
	while (nodes.size()) {
		QuadNode* n = nodes.front();
		nodes.pop();
		if (n->mChildren)
			for (uint32_t i = 0; i < 4; i++)
				nodes.push(&n->mChildren[i]);
		else
			leafNodes.push_back(n);
	}

	if (!leafNodes.size()) return;

	sort(leafNodes.begin(), leafNodes.end(), [](QuadNode* a, QuadNode* b) {
		return a->mTriangleMask < b->mTriangleMask;
	});
	
	Buffer* nodeBuffer = commandBuffer->Device()->GetTempBuffer(mName + " Nodes", leafNodes.size() * sizeof(float4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	float4* bn = (float4*)nodeBuffer->MappedData();
	for (QuadNode* n : leafNodes) {
		*bn = float4(n->mPosition.x, 0, n->mPosition.z, n->mSize);
		bn++;
	}

	#pragma region Populate descriptor set/push constants
	DescriptorSet* objds = commandBuffer->Device()->GetTempDescriptorSet(mName, shader->mDescriptorSetLayouts[PER_OBJECT]);
	objds->CreateStorageBufferDescriptor(nodeBuffer, OBJECT_BUFFER_BINDING);
	if (shader->mDescriptorBindings.count("Lights"))
		objds->CreateStorageBufferDescriptor(Scene()->LightBuffer(commandBuffer->Device()), LIGHT_BUFFER_BINDING);
	if (shader->mDescriptorBindings.count("Shadows"))
		objds->CreateStorageBufferDescriptor(Scene()->ShadowBuffer(commandBuffer->Device()), SHADOW_BUFFER_BINDING);
	if (shader->mDescriptorBindings.count("ShadowAtlas"))
		objds->CreateSampledTextureDescriptor(Scene()->ShadowAtlas(commandBuffer->Device()), SHADOW_ATLAS_BINDING);
	
	VkDescriptorSet ds = *objds;
	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, &ds, 0, nullptr);

	uint32_t lc = (uint32_t)Scene()->ActiveLights().size();
	float2 s = Scene()->ShadowTexelSize();
	float4x4 o2w = ObjectToWorld();
	float4x4 w2o = WorldToObject();
	commandBuffer->PushConstant(shader, "LightCount", &lc);
	commandBuffer->PushConstant(shader, "ShadowTexelSize", &s);
	commandBuffer->PushConstant(shader, "ObjectToWorld", &o2w);
	commandBuffer->PushConstant(shader, "WorldToObject", &w2o);
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

	delete root;
}

void TerrainRenderer::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
   
}