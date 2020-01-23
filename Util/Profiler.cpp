#include <Util/Profiler.hpp>

#include <sstream>

using namespace std;

ProfilerSample  Profiler::mFrames[PROFILER_FRAME_COUNT];
ProfilerSample* Profiler::mCurrentSample = nullptr;
uint64_t Profiler::mCurrentFrame = 0;
const std::chrono::high_resolution_clock Profiler::mTimer;

void Profiler::BeginSample(const string& label) {
	mCurrentSample->mChildren.push_back({});
	ProfilerSample* s = &mCurrentSample->mChildren.back();
	memset(s, 0, sizeof(ProfilerSample));
	
	strncpy(s->mLabel, label.c_str(), PROFILER_LABEL_SIZE);
	s->mParent = mCurrentSample;
	s->mStartTime = mTimer.now();
	s->mChildren = {};

	mCurrentSample =  s;
}
void Profiler::EndSample() {
	if (!mCurrentSample->mParent) {
		fprintf_color(COLOR_RED, stderr, "Error: Attempt to end nonexistant Profiler sample!");
		throw;
	}
	mCurrentSample->mDuration += mTimer.now() - mCurrentSample->mStartTime;
	mCurrentSample = mCurrentSample->mParent;
}

void Profiler::FrameStart() {
	int i = mCurrentFrame % PROFILER_FRAME_COUNT;
	sprintf(mFrames[i].mLabel, "Frame  %llu", mCurrentFrame);
	mFrames[i].mParent = nullptr;
	mFrames[i].mStartTime = mTimer.now();
	mFrames[i].mDuration = chrono::nanoseconds::zero();
	mFrames[i].mChildren.clear();
	mCurrentSample = &mFrames[i];
}
void Profiler::FrameEnd() {
	int i = mCurrentFrame % PROFILER_FRAME_COUNT;
	mFrames[i].mDuration = mTimer.now() - mFrames[i].mStartTime;
	mCurrentFrame++;
	mCurrentSample = nullptr;
}