#pragma once

#include <Util/Util.hpp>

class InputDevice;

// Represents a device capable of "pointing" into the world
// i.e. a mouse or Vive controller
// an InputDevice might have multiple pointers (i.e. multiple fingers)
class InputPointer {
public:
	InputDevice* mDevice;
	Ray mWorldRay;
	Ray mLastWorldRay;
	float mGuiHitT;
	float mLastGuiHitT;
	std::unordered_map<uint32_t, float> mAxis;
	std::unordered_map<uint32_t, float> mLastAxis;
};

class InputDevice {
public:
	inline virtual uint32_t PointerCount() { return 0; };
	// Get info about a pointer
	inline virtual const InputPointer* GetPointer(uint32_t index) { return nullptr; };

	inline virtual void NextFrame() {}
};