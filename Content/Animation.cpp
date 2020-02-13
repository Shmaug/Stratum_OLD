#include <Content/Animation.hpp>
#include <Content/Mesh.hpp>

#include <algorithm>

using namespace std;

AnimationChannel::AnimationChannel(const vector<AnimationKeyframe>& keyframes, AnimationExtrapolate in, AnimationExtrapolate out)
	: mKeyframes(keyframes), mExtrapolateIn(in), mExtrapolateOut(out) {
	
	if (!mKeyframes.size()) return;

	// compute tangents
	for (uint32_t i = 0; i < mKeyframes.size(); i++) {
		switch (mKeyframes[i].mTangentModeIn) {
		case ANIMATION_TANGENT_SMOOTH:
			if (i > 0 && i < mKeyframes.size()-1) {
				mKeyframes[i].mTangentIn = (mKeyframes[i + 1].mValue - mKeyframes[i - 1].mValue) / (mKeyframes[i + 1].mTime - mKeyframes[i - 1].mTime);
				break;
			}
		case ANIMATION_TANGENT_LINEAR:
			if (i > 0) mKeyframes[i].mTangentIn = (mKeyframes[i].mValue - mKeyframes[i-1].mValue) / (mKeyframes[i].mTime - mKeyframes[i - 1].mTime);
			break;
		}

		switch (mKeyframes[i].mTangentModeOut) {
		case ANIMATION_TANGENT_SMOOTH:
			if (i > 0 && i < mKeyframes.size() - 1) {
				mKeyframes[i].mTangentOut = (mKeyframes[i + 1].mValue - mKeyframes[i - 1].mValue) / (mKeyframes[i + 1].mTime - mKeyframes[i - 1].mTime);
				break;
			}
		case ANIMATION_TANGENT_LINEAR:
			if (i < mKeyframes.size() - 1) mKeyframes[i].mTangentOut = (mKeyframes[i + 1].mValue - mKeyframes[i].mValue) / (mKeyframes[i + 1].mTime - mKeyframes[i].mTime);
			break;
		}
	}

	mKeyframes[0].mTangentIn = mKeyframes[0].mTangentOut;
	mKeyframes[mKeyframes.size() - 1].mTangentOut = mKeyframes[mKeyframes.size() - 1].mTangentIn;
	
	// compute curve
	mCoefficients.resize(mKeyframes.size());
	memset(mCoefficients.data(), 0, sizeof(float4) * mCoefficients.size());
	for (uint32_t i = 0; i < mKeyframes.size() - 1; i++) {

		float ts =  mKeyframes[i + 1].mTime - mKeyframes[i].mTime;

		float p0y = mKeyframes[i].mValue;
		float p1y = mKeyframes[i + 1].mValue;
		float v0 = mKeyframes[i].mTangentOut * ts;
		float v1 = mKeyframes[i + 1].mTangentIn * ts;

		mCoefficients[i].x = p0y;
		if (mKeyframes[i].mTangentModeOut == ANIMATION_TANGENT_STEP) continue;

		mCoefficients[i].y = v0;
		mCoefficients[i].z = 3 * (p1y - p0y) - 2*v0 - v1;
		mCoefficients[i].w = p1y - p0y - v0 - mCoefficients[i].z;
	}
}

float AnimationChannel::Sample(float t) const {
	if (mKeyframes.size() == 0) return 0;
	if (mKeyframes.size() == 1) return mKeyframes[0].mValue;

	const AnimationKeyframe& first = mKeyframes[0];
	const AnimationKeyframe& last = mKeyframes[mKeyframes.size() - 1];

	float length = last.mTime - first.mTime;
	float ts = first.mTime - t;
	float tl = t - last.mTime;
	float offset = 0;
	
	if (tl > 0) {
		switch (mExtrapolateOut) {
		case EXTRAPOLATE_CONSTANT:
			return last.mValue;
		case EXTRAPOLATE_LINEAR:
			return last.mValue + last.mTangentOut * tl;
		case EXTRAPOLATE_CYCLE_OFFSET:
			offset += (last.mValue - first.mValue) * (floorf(tl / length) + 1);
		case EXTRAPOLATE_CYCLE:
			t = fmodf(tl, length);
			break;
		case EXTRAPOLATE_BOUNCE:
			t = fmodf(tl, 2 * length);
			if (t > length) t = 2 * length - t;
			break;
		}
		t += first.mTime;
	}
	if (ts > 0) {
		switch (mExtrapolateIn) {
		case EXTRAPOLATE_CONSTANT:
			return first.mValue;
		case EXTRAPOLATE_LINEAR:
			return first.mValue - first.mTangentIn * ts;
		case EXTRAPOLATE_CYCLE_OFFSET:
			offset += (first.mValue - last.mValue) * (floorf(ts / length) + 1);
		case EXTRAPOLATE_CYCLE:
			t = fmodf(ts, length);
			break;
		case EXTRAPOLATE_BOUNCE:
			t = fmodf(ts, 2 * length);
			if (t > length) t = 2 * length - t;
			break;
		}
		t = last.mTime - t; // looping anims loop back to last key
	}

	// find the first key after t
	uint32_t i = 0;
	for (uint32_t j = 1; j < (uint32_t)mKeyframes.size(); j++)
		if (mKeyframes[j].mTime > t) {
			i = j - 1;
			break;
		}

	float u = (t - mKeyframes[i].mTime) / (mKeyframes[i + 1].mTime - mKeyframes[i].mTime);
	float4 c = mCoefficients[i];
	return u*u*u*c.w + u*u*c.z + u*c.y + c.x + offset;
}

Animation::Animation(const unordered_map<uint32_t, AnimationChannel>& channels, float start, float end)
	: mChannels(channels), mTimeStart(start), mTimeEnd(end) {}

void Animation::Sample(float t, AnimationRig& rig) const {
	rig[0]->LocalPosition(mChannels.at(0).Sample(t), mChannels.at(1).Sample(t), -mChannels.at(2).Sample(t));
	for (uint32_t i = 0; i < rig.size(); i++) {
		float3 euler(mChannels.at(3 * i + 3).Sample(t), mChannels.at(3 * i + 4).Sample(t), mChannels.at(3 * i + 5).Sample(t));
		quaternion r(euler);
		r.x = -r.x;
		r.y = -r.y;
		rig[i]->LocalRotation(r);
	}
}