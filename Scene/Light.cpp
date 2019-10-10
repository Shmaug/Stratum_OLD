#include <Scene/Light.hpp>

using namespace std;

Light::Light(const string& name)
	: Object(name), mColor(float3(1)), mIntensity(1), mType(Point), mRange(1), mRadius(.025f), mInnerSpotAngle(.34f), mOuterSpotAngle(.25f) {}
Light::~Light() {}