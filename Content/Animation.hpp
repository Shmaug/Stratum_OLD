#pragma once

#include <unordered_map>

#include <assimp/scene.h>
#include <assimp/anim.h>

#include <Util/Util.hpp>

inline float4x4 ConvertMatrix(const aiMatrix4x4& m) {
	return float4x4(
		m.a1, m.a2, m.a3, m.a4,
		m.b1, m.b2, m.b3, m.b4,
		m.c1, m.c2, m.c3, m.c4,
		m.d1, m.d2, m.d3, m.d4
	);
}
inline float4x4 GetGlobalTransform(aiNode* node) {
	float4x4 t(1.f);
	while (node && node->mParent) {
		t = (t * ConvertMatrix(node->mTransformation));
		node = node->mParent;
	}
	return t;
}

struct BoneTransform {
	float3 mPosition;
	quaternion mRotation;
	float3 mScale;

	float4x4 ToMatrix() const;
	BoneTransform Inverse() const;
	void FromMatrix(const float4x4& mat, float scale);
	BoneTransform operator*(const BoneTransform& rhs) const;
};

class Bone;
typedef std::vector<Bone*> AnimationRig;
typedef std::vector<BoneTransform> Pose;

inline void lerp(BoneTransform& dest, const BoneTransform& p0, const BoneTransform& p1, float t) {
	dest.mPosition = lerp(p0.mPosition, p1.mPosition, t);
	dest.mRotation = slerp(p0.mRotation, p1.mRotation, t);
	dest.mScale = lerp(p0.mScale, p1.mScale, t);
}
inline void lerp(Pose& dest, const Pose& p0, const Pose& p1, float t) {
	for (uint32_t i = 0; i < dest.size(); i++)
		lerp(dest[i], p0[i], p1[i], t);
}

class Animation {
public:
	ENGINE_EXPORT Animation(const aiAnimation* anim, const std::unordered_map<std::string, uint32_t>& bonesByName, float scale);

	const float Length() const { return mLength; }

	ENGINE_EXPORT void Evaluate(float t, AnimationRig& rig) const;
	ENGINE_EXPORT void Evaluate(float t, AnimationRig& rig, Pose& pose) const;
	ENGINE_EXPORT void Evaluate(float t, AnimationRig& rig, uint32_t boneIndex, BoneTransform& bone) const;

	ENGINE_EXPORT static void GetPose(AnimationRig& rig, Pose& dest);
	ENGINE_EXPORT static void SetPose(AnimationRig& rig, Pose& pose);
	ENGINE_EXPORT static void Interpolate(Pose& dest, const Pose& p0, const Pose& p1, float t);

	ENGINE_EXPORT static void GetBone(AnimationRig& rig, uint32_t boneIndex, BoneTransform& bone);
	ENGINE_EXPORT static void Interpolate(BoneTransform& dest, const BoneTransform& p0, const BoneTransform& p1, float t);

private:
	struct TranslationKey {
		float3 position;
		float time;
	};
	struct RotationKey {
		quaternion rotation;
		float time;
	};
	struct ScaleKey {
	    float3 scale;
		float time;
	};
	struct AnimationChannel {
		std::vector<TranslationKey> mTranslationKeys;
		std::vector<RotationKey> mRotationKeys;
		std::vector<ScaleKey> mScaleKeys;

		void EvaluateTranslation(float t, float length, float3& out) const;
		void EvaluateRotation(float t, float length, quaternion& out) const;
		void EvaluateScale(float t, float length, float3& out) const;
	};

	float mLength;
	std::unordered_map<uint32_t, AnimationChannel> mChannels;
};