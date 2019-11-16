#include "TerrainRenderer.hpp"
#include <Core/CommandBuffer.hpp>

using namespace std;

TerrainRenderer::TerrainRenderer(const string& name) : Object(name), Renderer(), mShader(nullptr), mRootNode(nullptr) { mVisible = true; }
TerrainRenderer::~TerrainRenderer() {
    safe_delete(mRootNode);
}

bool TerrainRenderer::UpdateTransform(){
    if (!Object::UpdateTransform()) return false;
   // mAABB = mPointAABB * ObjectToWorld();
    return true;
}

void TerrainRenderer::Draw(CommandBuffer* commandBuffer, Camera* camera, ::Material* materialOverride) {
    if (materialOverride) return;
    if (!mShader) mShader = Scene()->AssetManager()->LoadShader("Shaders/terrani.shader");
    GraphicsShader* shader = mShader->GetGraphics(commandBuffer->Device(), {});
	VkPipelineLayout layout = commandBuffer->BindShader(shader, nullptr, camera, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP);
	if (!layout) return;


}

void TerrainRenderer::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
   
}