#include <Scene/Gizmos.hpp>
#include <Scene/Scene.hpp>

using namespace std;

Gizmos::Gizmos(Scene* scene) {
	mColorMaterial = new Material("Colored", scene->AssetManager()->LoadShader("Shaders/color.shader"));
}
Gizmos::~Gizmos() {
	safe_delete(mColorMaterial);
	safe_delete(mCubeMesh);
	safe_delete(mSphereMesh);
}

void Gizmos::DrawCube(CommandBuffer* commandBuffer, const float3& center, const float3& extents, const quaternion& rotation, const float4& color) {

}
void Gizmos::DrawSphere(CommandBuffer* commandBuffer, const float3& center, const float radius, const float4& color) {

}