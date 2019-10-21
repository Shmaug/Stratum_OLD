#include <Scene/Light.hpp>
#include <Scene/Scene.hpp>

using namespace std;

Light::Light(const string& name)
	: Object(name), mColor(float3(1)), mIntensity(1), mType(Point), mRange(1), mRadius(.025f), mInnerSpotAngle(.34f), mOuterSpotAngle(.25f) {}
Light::~Light() {}

void Light::DrawGizmos(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, ::Material* materialOverride) {
	switch (mType) {
	case LightType::Point:
		Scene()->Gizmos()->DrawWireSphere(commandBuffer, backBufferIndex, WorldPosition(), Radius(), float4(Color(), 1));
		Scene()->Gizmos()->DrawWireSphere(commandBuffer, backBufferIndex, WorldPosition(), mRange, float4(Color(), .5f));
		break;

	case LightType::Spot:
		Scene()->Gizmos()->DrawWireCircle(commandBuffer, backBufferIndex, WorldPosition(), Radius(), WorldRotation(), float4(Color(), 1));
		Scene()->Gizmos()->DrawWireCircle(commandBuffer, backBufferIndex, WorldPosition() + WorldRotation() * float3(0,0,mRange), mRange * tanf(mInnerSpotAngle * .5f), WorldRotation(), float4(Color(), 1));
		Scene()->Gizmos()->DrawWireCircle(commandBuffer, backBufferIndex, WorldPosition() + WorldRotation() * float3(0,0,mRange), mRange * tanf(mOuterSpotAngle * .5f), WorldRotation(), float4(Color(), 1));
		break;
	}
}