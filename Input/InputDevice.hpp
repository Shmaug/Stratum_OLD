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
	std::unordered_map<uint32_t, float> mAxis;
};

class InputDevice {
public:
	inline virtual uint32_t PointerCount() { return 0; };
	// Get info about a pointer
	inline virtual const InputPointer* GetPointer(uint32_t index) { return nullptr; };
	// Get info about a pointer from 1 frame ago
	inline virtual const InputPointer* GetLastPointer(uint32_t index) { return nullptr; };

	inline virtual void NextFrame() {}
};