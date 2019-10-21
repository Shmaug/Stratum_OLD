#pragma once

#include <Util/Util.hpp>

// Represents a device capable of "pointing" into the world
// i.e. a mouse or Vive controller
// an InputDevice might have multiple pointers (i.e. multiple fingers)
class InputPointer {
public:
	float3 mWorldPosition;
	quaternion mWorldRotation;
	std::unordered_map<uint32_t, float> mAxis;
};

class InputDevice {
public:
	inline virtual uint32_t PointerCount() { return 0; };
	inline virtual const InputPointer* GetPointer(uint32_t index) { return nullptr; };

	inline virtual void NextFrame() {}
};