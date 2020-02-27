#include <Core/DescriptorSet.hpp>
#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Environment.hpp>
#include <Util/Profiler.hpp>

#include <Shaders/include/shadercompat.h>

#include "ClothRenderer.hpp"

using namespace std;

ClothRenderer::ClothRenderer(const string& name)
	: Object(name), mVisible(true), mRayMask(0), mResolution(128) {}
ClothRenderer::~ClothRenderer() {}

bool ClothRenderer::UpdateTransform() {
	if (!Object::UpdateTransform()) return false;
	mAABB = mClothAABB * ObjectToWorld();
	return true;
}

void ClothRenderer::Material(shared_ptr<::Material> m) {
	mMaterial = m;
}

void ClothRenderer::PreRender(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {
	if (pass == PASS_MAIN) Scene()->Environment()->SetEnvironment(camera, mMaterial.get());
}

void ClothRenderer::Draw(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {

}

void ClothRenderer::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
    
};