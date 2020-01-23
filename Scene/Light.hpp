#pragma once

#include <Scene/Object.hpp>
#include <Util/Util.hpp>

#include <Shaders/include/shadercompat.h>

enum LightType {
	LIGHT_TYPE_SUN = LIGHT_SUN,
	LIGHT_TYPE_POINT = LIGHT_POINT,
	LIGHT_TYPE_SPOT = LIGHT_SPOT,
};

class Light : public virtual Object {
public:
	ENGINE_EXPORT Light(const std::string& name);
	ENGINE_EXPORT ~Light();

	inline void Color(const float3& c) { mColor = c; }
	inline float3 Color() const { return mColor; }

	inline void Intensity(float i) { mIntensity = i; }
	inline float Intensity() const { return mIntensity; }

	inline void Type(LightType t) { mType = t; }
	inline LightType Type() const { return mType; }

	inline void InnerSpotAngle(float a) { mInnerSpotAngle = a; }
	inline float InnerSpotAngle() const { return mInnerSpotAngle; }

	inline void OuterSpotAngle(float a) { mOuterSpotAngle = a; }
	inline float OuterSpotAngle() const { return mOuterSpotAngle; }

	// Distance light travels
	inline void Range(float r) { mRange = r; }
	// Distance light travels
	inline float Range() const { return mRange; }

	// Physical radius of the point/spot light
	inline void Radius(float r) { mRadius = r; }
	// Physical radius of the point/spot light
	inline float Radius() const { return mRadius; }

	inline void CastShadows(bool c) { mCastShadows = c; }
	inline bool CastShadows() { return mCastShadows; }

	inline void ShadowDistance(float d) { mShadowDistance = d; }
	inline float ShadowDistance() { return mShadowDistance; }

	inline void CascadeCount(uint32_t c) { mCascadeCount = c; }
	inline uint32_t CascadeCount() { return mCascadeCount; }
	
	inline AABB Bounds() override {
		float3 c, e;
		switch (mType) {
		case LIGHT_TYPE_POINT:
			return AABB(WorldPosition() - mRange, WorldPosition() + mRange);
		case LIGHT_TYPE_SPOT:
			e = float3(0, 0, mRange * .5f);
			c = float3(mRange * float2(sinf(mOuterSpotAngle * .5f)), mRange * .5f);
			return AABB(c - e, c + e) * ObjectToWorld();
		case LIGHT_TYPE_SUN:
			return AABB(float3(-1e10f), float3(1e10f));
		}
		fprintf_color(COLOR_RED_BOLD, stderr, "Unknown light type!");
		throw;
	}

private:
	float3 mColor;
	float mIntensity;
	LightType mType;
	// Size of the Physical point/spot light
	float mRadius;
	float mRange;
	float mInnerSpotAngle;
	float mOuterSpotAngle;

	bool mCastShadows;
	float mShadowDistance;
	uint32_t mCascadeCount;
};