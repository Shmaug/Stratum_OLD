#pragma once

//#define PROFILER_ENABLE

#ifdef PROFILER_ENABLE
#define PROFILER_BEGIN(label) Profiler::BeginSample(label, false)
#define PROFILER_BEGIN_RESUME(label, resume) Profiler::BeginSample(label, true)
#define PROFILER_END Profiler::EndSample()
#else
#define PROFILER_BEGIN(label) 
#define PROFILER_BEGIN_RESUME(label, resume)
#define PROFILER_END
#endif

#define PROFILER_FRAME_COUNT 256

#include <Util/Util.hpp>
#include <chrono>

struct ProfilerSample {
	std::string mLabel;
	ProfilerSample* mParent;
	std::chrono::time_point<std::chrono::steady_clock> mStartTime;
	std::chrono::nanoseconds mTime;
	std::vector<ProfilerSample> mChildren;
};

class Profiler {
public:
	ENGINE_EXPORT static void BeginSample(const std::string& label, bool resume);
	ENGINE_EXPORT static void EndSample();

	ENGINE_EXPORT static void FrameStart();
	ENGINE_EXPORT static void FrameEnd();

	ENGINE_EXPORT static void PrintLastFrame(std::ostream& stream);

private:
	static const std::chrono::high_resolution_clock mTimer;
	static ProfilerSample mFrames[PROFILER_FRAME_COUNT];
	static ProfilerSample* mCurrentSample;
	static uint64_t mCurrentFrame;
};