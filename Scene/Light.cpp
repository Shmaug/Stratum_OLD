#include <Scene/Light.hpp>
#include <Scene/Scene.hpp>

using namespace std;

Light::Light(const string& name)
	: Object(name), mColor(float3(1)), mIntensity(1), mType(Point), mRange(1), mRadius(.025f), mInnerSpotAngle(.34f), mOuterSpotAngle(.25f) {}
Light::~Light() {}

void Light::DrawGizmos(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, ::Material* materialOverride) {
	Scene()->Gizmos()->DrawSphere(commandBuffer, WorldPosition(), Radius(), float4(Color(), 1));
}