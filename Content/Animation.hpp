#pragma once

#include <unordered_map>

#include <assimp/scene.h>
#include <assimp/anim.h>

#include <Util/Util.hpp>

class Bone;
typedef std::vector<Bone*> AnimationRig;

struct BoneTransform {
	float3 mPosition;
	quaternion mRotation;
	float3 mScale;

	inline BoneTransform operator*(const BoneTransform& rhs) const {
		BoneTransform t = {};
		t.mPosition = mPosition + (mRotation * rhs.mPosition) * mScale;
		t.mRotation = mRotation * rhs.mRotation;
		t.mScale = rhs.mScale * mScale;
		return t;
	}
};

typedef std::vector<BoneTransform> Pose;

inline BoneTransform inverse(const BoneTransform& bt) {
	BoneTransform t = {};
	t.mRotation = inverse(bt.mRotation);
	t.mPosition = (t.mRotation * -bt.mPosition) / bt.mScale;
	t.mScale = 1.f / bt.mScale;
	return t;
}

inline BoneTransform lerp(const BoneTransform& p0, const BoneTransform& p1, float t) {
	BoneTransform dest;
	dest.mPosition = lerp(p0.mPosition, p1.mPosition, t);
	dest.mRotation = slerp(p0.mRotation, p1.mRotation, t);
	dest.mScale = lerp(p0.mScale, p1.mScale, t);
	return dest;
}
inline void lerp(Pose& dest, const Pose& p0, const Pose& p1, float t) {
	for (uint32_t i = 0; i < dest.size(); i++)
		dest[i] = lerp(p0[i], p1[i], t);
}

enum AnimationExtrapolate {
	EXTRAPOLATE_CONSTANT,
	EXTRAPOLATE_LINEAR,
	EXTRAPOLATE_CYCLE,
	EXTRAPOLATE_CYCLE_OFFSET,
	EXTRAPOLATE_BOUNCE,
};
enum AnimationTangent {
	ANIMATION_TANGENT_MANUAL,
	ANIMATION_TANGENT_FLAT,
	ANIMATION_TANGENT_LINEAR,
	ANIMATION_TANGENT_SMOOTH,
	ANIMATION_TANGENT_STEP,
};

struct AnimationKeyframe {
	float mValue;
	float mTime;
	float mTangentIn;
	float mTangentOut;
	AnimationTangent mTangentModeIn;
	AnimationTangent mTangentModeOut;
};
class AnimationChannel {
public:
	inline AnimationChannel() : mExtrapolateIn(EXTRAPOLATE_CONSTANT), mExtrapolateOut(EXTRAPOLATE_CONSTANT) {};
	ENGINE_EXPORT AnimationChannel(const std::vector<AnimationKeyframe>& keyframes, AnimationExtrapolate in, AnimationExtrapolate out);
	ENGINE_EXPORT float Sample(float t) const;

	inline AnimationExtrapolate ExtrapolateIn() const { return mExtrapolateIn; }
	inline AnimationExtrapolate ExtrapolateOut() const { return mExtrapolateOut; }
	inline uint32_t KeyframeCount() const { return mKeyframes.size(); }
	inline AnimationKeyframe Keyframe(uint32_t index) const { return mKeyframes[index]; }
	inline float4 CurveCoefficient(uint32_t index) const { return mCoefficients[index]; }

private:
	AnimationExtrapolate mExtrapolateIn;
	AnimationExtrapolate mExtrapolateOut;
	std::vector<float4> mCoefficients;
	std::vector<AnimationKeyframe> mKeyframes;
};

class Animation {
public:
	ENGINE_EXPORT Animation(const std::unordered_map<uint32_t, AnimationChannel>& channels, float start, float end);

	inline const float TimeStart() const { return mTimeStart; }
	inline const float TimeEnd() const { return mTimeEnd; }
	inline const std::unordered_map<uint32_t, AnimationChannel>& Channels() const { return mChannels; }

	ENGINE_EXPORT void Sample(float t, AnimationRig& rig) const;

private:
	std::unordered_map<uint32_t, AnimationChannel> mChannels;
	float mTimeStart;
	float mTimeEnd;
};