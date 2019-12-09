#include "TerrainRenderer.hpp"
#include <Core/CommandBuffer.hpp>
#include <Scene/Environment.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <ThirdParty/stb_image.h>

#include "TriangleFan.hpp"

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

	int x, y, channels;
	mHeights = stbi_loadf("Assets/heightmap.png", &x, &y, &channels, 1);

	mHeightmap = new Texture("Heightmap", Scene()->Instance(), mHeights, x*y*sizeof(float), x, y, 1,
		VK_FORMAT_R32_SFLOAT, 1);

	/*
	uint32_t Resolution = 8192;
	Shader* shader = Scene()->AssetManager()->LoadShader("Shaders/terraincompute.shader");
	float scale = mSize / Resolution;
	float offset = -mSize * .5f;

	for (uint32_t i = 0; i < Scene()->Instance()->DeviceCount(); i++) {
		Device* device = Scene()->Instance()->GetDevice(i);
		ComputeShader* gen = shader->GetCompute(device, "GenHeight", {});

		auto commandBuffer = device->GetCommandBuffer();
		mHeightmap->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, commandBuffer.get());

		vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, gen->mPipeline);
		DescriptorSet* ds = new DescriptorSet("Terrain DS", device, gen->mDescriptorSetLayouts[0]);
		ds->CreateStorageTextureDescriptor(mHeightmap, gen->mDescriptorBindings.at("Heightmap").second.binding);
		
		VkDescriptorSet vds = *ds;
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, gen->mPipelineLayout, 0, 1, &vds, 0, nullptr);
		
		commandBuffer->PushConstant(gen, "Height", &mHeight);
		commandBuffer->PushConstant(gen, "Scale", &scale);
		commandBuffer->PushConstant(gen, "Offset", &offset);
		vkCmdDispatch(*commandBuffer, (Resolution + 7) / 8, (Resolution + 7) / 8, 1);

		Buffer* dst = nullptr;
		if (i == 0) {
			mHeightmap->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, commandBuffer.get());

			dst = new Buffer("Heightmap", device, sizeof(float)*x*y, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
				
			VkBufferImageCopy rgn = {};
			rgn.imageExtent = { x, y, 1 };
			rgn.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			rgn.imageSubresource.layerCount = 1;
			vkCmdCopyImageToBuffer(*commandBuffer, mHeightmap->Image(device), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *dst, 1, &rgn);

			mHeightmap->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer.get());
		}else
			mHeightmap->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer.get());
		device->Execute(commandBuffer, false)->Wait();

		if (dst) {
			dst->Map();
			memcpy(mHeights, dst->MappedData(), sizeof(float) * Resolution * Resolution);
			delete dst;
		}

		delete ds;
	}
	*/

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
	nodes.push(mRootNode);
	while (nodes.size()) {
		QuadNode* n = nodes.front();
		nodes.pop();
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
	float y0 = mHeights[pi.y * mHeightmap->Width() + pi.x];
	float y1 = mHeights[pi.y * mHeightmap->Width() + pi.x+1];
	float y2 = mHeights[(pi.y+1) * mHeightmap->Width() + pi.x];
	float y3 = mHeights[(pi.y+1) * mHeightmap->Width() + pi.x+1];
	return mHeight * lerp(lerp(y0, y1, p.x - pi.x), lerp(y2, y3, p.x - pi.x), p.y - pi.y);
}

bool TerrainRenderer::UpdateTransform(){
    if (!Object::UpdateTransform()) return false;
	mAABB = AABB(float3(0, mHeight * .5f, 0), float3(mSize, mHeight, mSize) * .5f) * ObjectToWorld();
    return true;
}

void TerrainRenderer::Draw(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {
	if (!mMaterial) return;

	VkCullModeFlags cull = VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
	if (pass & Main) Scene()->Environment()->SetEnvironment(camera, mMaterial.get());
	if (pass & Depth) mMaterial->EnableKeyword("DEPTH_PASS");
	else mMaterial->DisableKeyword("DEPTH_PASS");
	if (pass & Shadow) cull = VK_CULL_MODE_NONE;

	mMaterial->SetParameter("Heightmap", mHeightmap);

	VkPipelineLayout layout = commandBuffer->BindMaterial(mMaterial.get(), nullptr, camera, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, cull);
	if (!layout) return;

    GraphicsShader* shader = mMaterial->GetShader(commandBuffer->Device());

	vector<QuadNode*> leafNodes;
	for (QuadNode* n : mLeafNodes)
		if (camera->IntersectFrustum(AABB(float3(n->mPosition.x, mHeight * .5f, n->mPosition.z), float3(n->mSize, mHeight, n->mSize) * .5f)))
			leafNodes.push_back(n);

	if (!leafNodes.size()) return;

	Buffer* nodeBuffer = commandBuffer->Device()->GetTempBuffer(mName + " Nodes", leafNodes.size() * sizeof(float4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	float4* bn = (float4*)nodeBuffer->MappedData();
	for (QuadNode* n : leafNodes) {
		*bn = float4(n->mPosition.x, 0, n->mPosition.z, n->mSize);
		bn++;
	}


	#pragma region Populate descriptor set/push constants
	DescriptorSet* objds = commandBuffer->Device()->GetTempDescriptorSet(mName + to_string(commandBuffer->Device()->FrameContextIndex()), shader->mDescriptorSetLayouts[PER_OBJECT]);
	objds->CreateStorageBufferDescriptor(nodeBuffer, OBJECT_BUFFER_BINDING);
	if (shader->mDescriptorBindings.count("Lights"))
		objds->CreateStorageBufferDescriptor(Scene()->LightBuffer(commandBuffer->Device()), LIGHT_BUFFER_BINDING);
	if (shader->mDescriptorBindings.count("Shadows"))
		objds->CreateStorageBufferDescriptor(Scene()->ShadowBuffer(commandBuffer->Device()), SHADOW_BUFFER_BINDING);
	if (shader->mDescriptorBindings.count("ShadowAtlas"))
		objds->CreateSampledTextureDescriptor(Scene()->ShadowAtlas(commandBuffer->Device()), SHADOW_ATLAS_BINDING);
	
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

void TerrainRenderer::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {}