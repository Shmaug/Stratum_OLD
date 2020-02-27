#include <Core/DescriptorSet.hpp>
#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Environment.hpp>
#include <Util/Profiler.hpp>

#include <Shaders/include/shadercompat.h>

#include "ClothRenderer.hpp"

using namespace std;

struct ClothVertex {
	float4 Position; // object-space
	float4 Velocity; // world-space
};

ClothRenderer::ClothRenderer(const string& name, float size, uint32_t resolution)
	: Object(name), mVisible(true), mRayMask(0), mMaterial(nullptr), mVertices(nullptr), mResolution(resolution), mDrag(10), mStiffness(1), mSize(size),
	mColor(1.f), mMetallic(0.f), mRoughness(.5f), mBumpStrength(1.f), mEmission(0.f), mTextureST(float4(1,1,0,0)),
	mMainTexture(nullptr), mMaskTexture(nullptr), mNormalTexture(nullptr), mExternalForce(0), mPinPos(0), mPinIndex(0xFFFF) {}
ClothRenderer::~ClothRenderer() { safe_delete(mMaterial); safe_delete(mVertices); }

void ClothRenderer::FixedUpdate(CommandBuffer* commandBuffer) {
	if (!mVertices) return;

	float4x4 o2w = ObjectToWorld();
	float4x4 w2o = WorldToObject();
	float sd = mSize / mResolution;
	float ClothStiffness;
	float dt = Scene()->FixedTimeStep();

	ComputeShader* add = Scene()->AssetManager()->LoadShader("Shaders/cloth.stm")->GetCompute("AddForces", {});
	ComputeShader* integrate = Scene()->AssetManager()->LoadShader("Shaders/cloth.stm")->GetCompute("Integrate", {});

	DescriptorSet* ds = new DescriptorSet("Particle Density", Scene()->Instance()->Device(), integrate->mDescriptorSetLayouts[0]);
	ds->CreateStorageBufferDescriptor(mVertices, 0, mVertices->Size(), integrate->mDescriptorBindings.at("Vertices").second.binding);
	ds->FlushWrites();

	vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, add->mPipeline);
	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, add->mPipelineLayout, 0, 1, *ds, 0, nullptr);
	commandBuffer->PushConstant(add, "ObjectToWorld", &o2w);
	commandBuffer->PushConstant(add, "WorldToObject", &w2o);
	commandBuffer->PushConstant(add, "ExternalForce", &mExternalForce);
	commandBuffer->PushConstant(add, "ClothResolution", &mResolution);
	commandBuffer->PushConstant(add, "ClothSpringDistance", &sd);
	commandBuffer->PushConstant(add, "Drag", &mDrag);
	commandBuffer->PushConstant(integrate, "Stiffness", &mStiffness);
	commandBuffer->PushConstant(add, "DeltaTime", &dt);
	commandBuffer->PushConstant(add, "PinIndex", &mPinIndex);
	commandBuffer->PushConstant(add, "PinPos", &mPinPos);
	vkCmdDispatch(*commandBuffer, ((mResolution + 7) / 8), (mResolution + 7) / 8, 1);

	VkBufferMemoryBarrier b = {};
	b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	b.buffer = *mVertices;
	b.size = mVertices->Size();
	vkCmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &b, 0, nullptr);

	vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, integrate->mPipeline);
	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, integrate->mPipelineLayout, 0, 1, *ds, 0, nullptr);
	commandBuffer->PushConstant(integrate, "ObjectToWorld", &o2w);
	commandBuffer->PushConstant(integrate, "WorldToObject", &w2o);
	commandBuffer->PushConstant(integrate, "ExternalForce", &mExternalForce);
	commandBuffer->PushConstant(integrate, "ClothResolution", &mResolution);
	commandBuffer->PushConstant(integrate, "ClothSpringDistance", &sd);
	commandBuffer->PushConstant(integrate, "Drag", &mDrag);
	commandBuffer->PushConstant(integrate, "Stiffness", &mStiffness);
	commandBuffer->PushConstant(integrate, "DeltaTime", &dt);
	commandBuffer->PushConstant(integrate, "PinIndex", &mPinIndex);
	commandBuffer->PushConstant(integrate, "PinPos", &mPinPos);
	vkCmdDispatch(*commandBuffer, ((mResolution + 7) / 8), (mResolution + 7) / 8, 1);

	vkCmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &b, 0, nullptr);

}

void ClothRenderer::PreRender(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {
	if (!mMaterial) mMaterial = new Material("Cloth", Scene()->AssetManager()->LoadShader("Shaders/cloth_pbr.stm"));
	Scene()->Environment()->SetEnvironment(camera, mMaterial);

	mMaterial->SetParameter("ObjectToWorld", ObjectToWorld());
	mMaterial->SetParameter("WorldToObject", WorldToObject());
	mMaterial->SetParameter("ClothResolution", mResolution);
	mMaterial->SetParameter("Color", mColor);
	mMaterial->SetParameter("Metallic", mMetallic);
	mMaterial->SetParameter("Roughness", mRoughness);
	mMaterial->SetParameter("BumpStrength", mBumpStrength);
	mMaterial->SetParameter("Emission", mEmission);
	mMaterial->SetParameter("TextureST", mTextureST);
}

void ClothRenderer::Draw(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {
	if (!mVertices) {
		mVertices = new Buffer(mName + " Vertices", commandBuffer->Device(), sizeof(ClothVertex) * mResolution * mResolution, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		Buffer upload(mName + " Upload", commandBuffer->Device(), sizeof(ClothVertex) * mResolution * mResolution, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		upload.Map();
		ClothVertex* vertices = (ClothVertex*)upload.MappedData();
		for (uint32_t y = 0; y < mResolution; y++)
			for (uint32_t x = 0; x < mResolution; x++) {
				float3 p((float)x / (mResolution - 1.f), 0, (float)y / (mResolution - 1.f));
				p = p - float3(.5f, 0, .5f);
				p *= mSize;
				float k = 0;// x == 0 ? 0 : 1;
				vertices[y * mResolution + x] = { float4(p, k), float4(p, k) };
			}
		
		mVertices->CopyFrom(upload);
	}

	VkPipelineLayout layout = commandBuffer->BindMaterial(mMaterial, pass, nullptr, camera, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_CULL_MODE_NONE);
	if (!layout) return;
	auto shader = mMaterial->GetShader(pass);

	uint32_t lc = (uint32_t)Scene()->ActiveLights().size();
	float2 s = Scene()->ShadowTexelSize();
	float t = Scene()->TotalTime();
	commandBuffer->PushConstant(shader, "Time", &t);
	commandBuffer->PushConstant(shader, "LightCount", &lc);
	commandBuffer->PushConstant(shader, "ShadowTexelSize", &s);

	DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet("Cloth", shader->mDescriptorSetLayouts[PER_OBJECT]);
	ds->CreateStorageBufferDescriptor(mVertices, 0, mVertices->Size(), INSTANCE_BUFFER_BINDING);
	if (pass == PASS_MAIN) {
		if (shader->mDescriptorBindings.count("Lights"))
			ds->CreateStorageBufferDescriptor(Scene()->LightBuffer(), 0, Scene()->LightBuffer()->Size(), LIGHT_BUFFER_BINDING);
		if (shader->mDescriptorBindings.count("Shadows"))
			ds->CreateStorageBufferDescriptor(Scene()->ShadowBuffer(), 0, Scene()->ShadowBuffer()->Size(), SHADOW_BUFFER_BINDING);
		if (shader->mDescriptorBindings.count("ShadowAtlas"))
			ds->CreateSampledTextureDescriptor(Scene()->ShadowAtlas(), SHADOW_ATLAS_BINDING);
	}
	ds->FlushWrites();
	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, *ds, 0, nullptr);

	uint32_t quadCount = (mResolution - 1) * (mResolution - 1);
	camera->SetStereo(commandBuffer, shader, EYE_LEFT);
	vkCmdDraw(*commandBuffer, quadCount * 6, 1, 0, 0);
	commandBuffer->mTriangleCount += quadCount*6;

	if (camera->StereoMode() != STEREO_NONE) {
		camera->SetStereo(commandBuffer, shader, EYE_RIGHT);
		vkCmdDraw(*commandBuffer, quadCount * 6, 1, 0, 0);
		commandBuffer->mTriangleCount += quadCount*6;
	}
}

void ClothRenderer::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
	if (Gizmos::PositionHandle("Pin", Scene()->InputManager()->GetFirst<MouseKeyboardInput>()->GetPointer(0), camera->WorldRotation(), mPinPos))
		mPinIndex = uint2(0, mResolution / 2);
	else
		mPinIndex = 0xFFFF;
}