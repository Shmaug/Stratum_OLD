#include <Content/Animation.hpp>
#include <Content/Mesh.hpp>

#include <algorithm>

using namespace std;

AnimationChannel::AnimationChannel(const vector<AnimationKeyframe>& keyframes, AnimationExtrapolate in, AnimationExtrapolate out)
	: mKeyframes(keyframes), mExtrapolateIn(in), mExtrapolateOut(out) {
	
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

	// compute curve
	mCoefficients.resize(mKeyframes.size());
	memset(mCoefficients.data(), 0, sizeof(float4) * mCoefficients.size());
	for (uint32_t i = 0; i < mKeyframes.size(); i++) {
		mCoefficients[i].x = mKeyframes[i].mValue;
		if (mKeyframes[i].mTangentModeOut == ANIMATION_TANGENT_STEP) continue;

		mCoefficients[i].y = mKeyframes[i].mTangentOut;

		if (i < mKeyframes.size() - 1) {
			mCoefficients[i].z = mKeyframes[i + 1].mTangentIn + mKeyframes[i].mTangentOut + 3 * (mKeyframes[i + 1].mValue + mCoefficients[i].x + mCoefficients[i].y);
			mCoefficients[i].w = mKeyframes[i + 1].mValue - mCoefficients[i].x - mCoefficients[i].y - mCoefficients[i].z;
		}
	}
}

float AnimationChannel::Sample(float t) const {
	if (mKeyframes.size() == 0) return 0;
	if (mKeyframes.size() == 1) return mKeyframes[0].mTime;

	float length = mKeyframes.back().mTime - mKeyframes[0].mTime;
	float ts = mKeyframes[0].mTime - t;
	float tl = t - length;
	float offset = 0;
	if (tl > 0) {
		switch (mExtrapolateIn) {
		case EXTRAPOLATE_CONSTANT:
			return mKeyframes.back().mValue;
		case EXTRAPOLATE_LINEAR:
			return mKeyframes.back().mValue + mKeyframes.back().mTangentIn * tl;
		case EXTRAPOLATE_CYCLE:
			t = fmodf(tl, length);
			break;
		case EXTRAPOLATE_CYCLE_OFFSET:
			offset += mKeyframes.back().mValue * (floorf(tl / length) + 1);
			t = fmodf(tl, length);
			break;
		case EXTRAPOLATE_BOUNCE:
			t = fmodf(tl, 2 * length);
			if (t > length) t = 2 * length - t;
			break;
		}
		t += mKeyframes[0].mTime; // looping anims loop back to key 0
	}
	if (ts > 0) {
		switch (mExtrapolateIn) {
		case EXTRAPOLATE_CONSTANT:
			return mKeyframes[0].mValue;
		case EXTRAPOLATE_LINEAR:
			return mKeyframes[0].mValue - mKeyframes[0].mTangentIn * ts;
		case EXTRAPOLATE_CYCLE:
			t = fmodf(ts, length);
			break;
		case EXTRAPOLATE_CYCLE_OFFSET:
			offset += mKeyframes[0].mValue * (floorf(ts / length) + 1);
			t = fmodf(ts, length);
			break;
		case EXTRAPOLATE_BOUNCE:
			t = fmodf(ts, 2 * length);
			if (t > length) t = 2 * length - t;
			break;
		}
		t = mKeyframes.back().mTime - t; // looping anims loop back to last key
	}

	// find the first key after t
	uint32_t i = 0;
	for (uint32_t j = 1; j < (uint32_t)mKeyframes.size(); j++)
		if (mKeyframes[j].mTime > t) {
			i = j - 1;
			break;
		}

	float4 c = mCoefficients[i];
	float u = (t - mKeyframes[i].mTime) / (mKeyframes[i + 1].mTime - mKeyframes[i].mTime);
	return c.x + u * (c.y + u * (c.z + u * c.w));
}

Animation::Animation(const unordered_map<uint32_t, AnimationChannel>& channels, float start, float end)
	: mChannels(channels), mTimeStart(start), mTimeEnd(end) {}

void Animation::Sample(float t, AnimationRig& rig) const {
	rig[0]->LocalPosition(mChannels.at(0).Sample(t), mChannels.at(1).Sample(t), mChannels.at(2).Sample(t));
	for (uint32_t i = 3; i < rig.size(); i+=3) {
		quaternion r(float3(mChannels.at(i).Sample(t), mChannels.at(i+1).Sample(t), -mChannels.at(i+2).Sample(t)));
		r.x = -r.x;
		r.y = -r.y;
		rig[i/3]->LocalRotation(r);
	}
}