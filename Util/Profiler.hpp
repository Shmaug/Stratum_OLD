#pragma once

#define PROFILER_ENABLE

#ifdef PROFILER_ENABLE
#define PROFILER_BEGIN(label) Profiler::BeginSample(label)
#define PROFILER_END Profiler::EndSample()
#else
#define PROFILER_BEGIN(label) 
#define PROFILER_END
#endif

#define PROFILER_FRAME_COUNT 512
#define PROFILER_LABEL_SIZE 64

#include <Util/Util.hpp>
#include <chrono>
#include <list>

struct ProfilerSample {
	char mLabel[PROFILER_LABEL_SIZE];
	ProfilerSample* mParent;
	std::chrono::high_resolution_clock::time_point mStartTime;
	std::chrono::nanoseconds mDuration;
	std::vector<ProfilerSample> mChildren;
};

class Profiler {
public:
	ENGINE_EXPORT static void BeginSample(const std::string& label);
	ENGINE_EXPORT static void EndSample();

	ENGINE_EXPORT static void FrameStart();
	ENGINE_EXPORT static void FrameEnd();

	inline static const uint64_t CurrentFrameIndex() { return (mCurrentFrame + PROFILER_FRAME_COUNT - 1) % PROFILER_FRAME_COUNT; }
	inline static const ProfilerSample* Frames() { return mFrames; }
	inline static const ProfilerSample* LastFrame() { return &mFrames[CurrentFrameIndex()]; }

private:
	ENGINE_EXPORT static const std::chrono::high_resolution_clock mTimer;
	ENGINE_EXPORT static ProfilerSample mFrames[PROFILER_FRAME_COUNT];
	ENGINE_EXPORT static ProfilerSample* mCurrentSample;
	ENGINE_EXPORT static uint64_t mCurrentFrame;
};