#include <Content/Animation.hpp>
#include <Content/Mesh.hpp>

#include <algorithm>

using namespace std;

BoneTransform BoneTransform::operator*(const BoneTransform& rhs) const {
	BoneTransform t = {};
	t.mPosition = mPosition + (mRotation * rhs.mPosition) * mScale;
	t.mRotation = mRotation * rhs.mRotation;
	t.mScale = rhs.mScale * mScale;
	return t;
}
float4x4 BoneTransform::ToMatrix() const {
	return float4x4::Translate(mPosition) * float4x4(mRotation) * float4x4::Scale(mScale);
}

BoneTransform BoneTransform::Inverse() const {
    quaternion ir = inverse(mRotation);
	BoneTransform t = {};
    t.mPosition = (ir * -mPosition) / mScale;
	t.mRotation = ir;
	t.mScale = 1.f / mScale;
	return t;
}

void BoneTransform::FromMatrix(const float4x4& mat, float scale) {
	mPosition = mat[3].xyz * scale;
	mScale.x = length(mat[0].xyz);
	mScale.y = length(mat[1].xyz);
	mScale.z = length(mat[2].xyz);
	mRotation.x = mat[2].y - mat[1].z;
	mRotation.y = mat[0].z - mat[2].x;
	mRotation.z = mat[1].x - mat[0].y;
	mRotation.w = sqrtf(1.f + mat[0].x + mat[1].y + mat[2].z) * .5f;
	mRotation.xyz /= 4.f * mRotation.w;
}

Animation::Animation(const aiAnimation* anim, const unordered_map<string, uint32_t>& bonesByName, float scale)
	: mLength((float)(anim->mDuration / anim->mTicksPerSecond)) {

	for (unsigned int c = 0; c < anim->mNumChannels; c++) {
		const aiNodeAnim* channel = anim->mChannels[c];
		if (bonesByName.count(channel->mNodeName.C_Str()) == 0) continue;

		AnimationChannel& a = mChannels[bonesByName.at(channel->mNodeName.C_Str())];

		for (unsigned int i = 0; i < channel->mNumPositionKeys; i++)
			a.mTranslationKeys.push_back(TranslationKey{ {
				(float)channel->mPositionKeys[i].mValue.x * scale,
				(float)channel->mPositionKeys[i].mValue.y * scale,
				(float)channel->mPositionKeys[i].mValue.z * scale,
			},
			(float)(channel->mPositionKeys[i].mTime / anim->mTicksPerSecond) });

		for (unsigned int i = 0; i < channel->mNumRotationKeys; i++)
			a.mRotationKeys.push_back(RotationKey{ {
				(float)channel->mRotationKeys[i].mValue.x,
				(float)channel->mRotationKeys[i].mValue.y,
				(float)channel->mRotationKeys[i].mValue.z,
				(float)channel->mRotationKeys[i].mValue.w,
			},
			(float)(channel->mRotationKeys[i].mTime / anim->mTicksPerSecond) });

		for (unsigned int i = 0; i < channel->mNumScalingKeys; i++)
			a.mScaleKeys.push_back(ScaleKey{ {
				(float)channel->mScalingKeys[i].mValue.x,
				(float)channel->mScalingKeys[i].mValue.y,
				(float)channel->mScalingKeys[i].mValue.z,
			},
			(float)(channel->mScalingKeys[i].mTime / anim->mTicksPerSecond) });
	}
}

void Animation::AnimationChannel::EvaluateTranslation(float t, float length, float3& out) const {
	if (mTranslationKeys.size() == 0) return;

	if (mTranslationKeys.size() == 1)
		out = mTranslationKeys[0].position;
	else {
		const TranslationKey* k0 = &mTranslationKeys[mTranslationKeys.size() - 1];
		const TranslationKey* k1 = &mTranslationKeys[0];

		for (uint32_t j = 1; j < (uint32_t)mTranslationKeys.size(); j++) {
			if (mTranslationKeys[j].time > t) {
				k0 = &mTranslationKeys[j - 1];
				k1 = &mTranslationKeys[j];
				break;
			}
		}

		float kt = 0.f;
		float tt = k1->time - k0->time;
		if (tt < 0.0) {
			tt = length - k0->time + k1->time;
			if (t < k0->time) {
				kt = clamp((t + length - k0->time) / tt, 0.f, 1.f);
			} else {
				kt = clamp((t - k0->time) / tt, 0.f, 1.f);
			}
		} else
			kt = clamp((t - k0->time) / tt, 0.f, 1.f);
		out = lerp(k0->position, k1->position, kt);
	}
}
void Animation::AnimationChannel::EvaluateRotation(float t, float length, quaternion& out) const {
	if (mRotationKeys.size() == 0) return;

	if (mRotationKeys.size() == 1)
		out = mRotationKeys[0].rotation;
	else {
		const RotationKey* k0 = &mRotationKeys[mRotationKeys.size() - 1];
		const RotationKey* k1 = &mRotationKeys[0];

		for (uint32_t j = 1; j < (uint32_t)mRotationKeys.size(); j++) {
			if (mRotationKeys[j].time > t) {
				k0 = &mRotationKeys[j - 1];
				k1 = &mRotationKeys[j];
				break;
			}
		}

		float kt = 0.f;
		float tt = k1->time - k0->time;
		if (tt < 0.0) {
			tt = length - k0->time + k1->time;
			if (t < k0->time) {
				kt = clamp((t + length - k0->time) / tt, 0.f, 1.f);
			}else{
				kt = clamp((t - k0->time) / tt, 0.f, 1.f);
			}
		} else
			kt = clamp((t - k0->time) / tt, 0.f, 1.f);
		out.xyzw = normalize(lerp(k0->rotation.xyzw, k1->rotation.xyzw, kt));
	}
}
void Animation::AnimationChannel::EvaluateScale(float t, float length, float3& out) const {
	if (mScaleKeys.size() == 0) return;

	if (mScaleKeys.size() == 1 || t <= mScaleKeys[0].time)
		out = mScaleKeys[0].scale;
	else if (t >= mScaleKeys[mScaleKeys.size() - 1].time)
		out = mScaleKeys[mScaleKeys.size() - 1].scale;
	else {
		const ScaleKey* k0 = &mScaleKeys[mScaleKeys.size() - 1];
		const ScaleKey* k1 = &mScaleKeys[0];

		for (uint32_t j = 1; j < (uint32_t)mScaleKeys.size(); j++) {
			if (mScaleKeys[j].time > t) {
				k0 = &mScaleKeys[j - 1];
				k1 = &mScaleKeys[j];
				break;
			}
		}

		float kt = 0.f;
		float tt = k1->time - k0->time;
		if (tt < 0.0) {
			tt = length - k0->time + k1->time;
			if (t < k0->time) {
				kt = clamp((t + length - k0->time) / tt, 0.f, 1.f);
			} else {
				kt = clamp((t - k0->time) / tt, 0.f, 1.f);
			}
		} else
			kt = clamp((t - k0->time) / tt, 0.f, 1.f);
		out = lerp(k0->scale, k1->scale, kt);
	}
}

void Animation::GetBone(AnimationRig& rig, unsigned int boneIndex, BoneTransform& bone) {
	bone.FromMatrix(rig[boneIndex]->ObjectToParent(), 1.f);
}
void Animation::GetPose(AnimationRig& rig, Pose& dest) {
	dest.resize(rig.size());
	for (unsigned int i = 0; i < rig.size(); i++)
		GetBone(rig, i, dest[i]);
}
void Animation::SetPose(AnimationRig& rig, Pose& pose) {
	for (unsigned int i = 0; i < rig.size(); i++) {
		rig[i]->LocalPosition(pose[i].mPosition);
		rig[i]->LocalRotation(pose[i].mRotation);
		rig[i]->LocalScale(pose[i].mScale);

		if (Bone* parent = dynamic_cast<Bone*>(rig[i]->Parent()))
			pose[i] = pose[parent->mBoneIndex] * pose[i];
	}
}

void Animation::Evaluate(float time, AnimationRig& rig, uint32_t boneIndex, BoneTransform& bone) const {
	bone.mPosition = rig[boneIndex]->LocalPosition();
	bone.mRotation = rig[boneIndex]->LocalRotation();
	bone.mScale = rig[boneIndex]->LocalScale();

	if (mChannels.count(boneIndex)) {
		auto& channel = mChannels.at(boneIndex);
		channel.EvaluateTranslation(time, mLength, bone.mPosition);
		channel.EvaluateRotation(time, mLength, bone.mRotation);
		channel.EvaluateScale(time, mLength, bone.mScale);
	}
}
void Animation::Evaluate(float time, AnimationRig& rig) const {
	for (uint32_t i = 0; i < rig.size(); i++) {
		if (mChannels.count(i)) {
			auto& channel = mChannels.at(i);

			float3 t = rig[i]->LocalPosition();
			quaternion r = rig[i]->LocalRotation();
			float3 s = rig[i]->LocalScale();

			channel.EvaluateTranslation(time, mLength, t);
			channel.EvaluateRotation(time, mLength, r);
			channel.EvaluateScale(time, mLength, s);

			rig[i]->LocalPosition(t);
			rig[i]->LocalRotation(r);
			rig[i]->LocalScale(s);
		}
	}
}
void Animation::Evaluate(float time, AnimationRig& rig, Pose& pose) const {
	for (uint32_t i = 0; i < rig.size(); i++)
		Evaluate(time, rig, i, pose[i]);
}