#pragma once

#include <unordered_map>
#include <Input/InputDevice.hpp>
#include <GLFW/glfw3.h>

class Window;

class MouseKeyboardInput : public InputDevice {
public:
	ENGINE_EXPORT MouseKeyboardInput();

	inline bool MouseButtonDownFirst(int key) { return mCurrent.mMousePointer.mAxis[key] == GLFW_PRESS && mLast.mMousePointer.mAxis[key] == GLFW_RELEASE; }
	inline bool MouseButtonUpFirst(int key) { return mLast.mMousePointer.mAxis[key] == GLFW_PRESS && mCurrent.mMousePointer.mAxis[key] == GLFW_RELEASE; }
	inline bool MouseButtonDown(int key) { return mCurrent.mMousePointer.mAxis[key] == GLFW_PRESS; }
	inline bool MouseButtonUp(int key) { return mCurrent.mMousePointer.mAxis[key] == GLFW_RELEASE; }

	inline bool KeyDownFirst(int key) { return mCurrent.mKeys[key] == GLFW_PRESS && mLast.mKeys[key] == GLFW_RELEASE; }
	inline bool KeyUpFirst(int key) { return mLast.mKeys[key] == GLFW_PRESS && mCurrent.mKeys[key] == GLFW_RELEASE; }
	inline bool KeyDown(int key) { return mCurrent.mKeys[key] == GLFW_PRESS; }
	inline bool KeyUp(int key) { return mCurrent.mKeys[key] == GLFW_RELEASE; }

	inline float2 ScrollDelta() const { return mCurrent.mScrollDelta; }
	inline float2 CursorPos() const { return mCurrent.mCursorPos; }
	inline float2 CursorDelta() const { return mCurrent.mCursorPos - mLast.mCursorPos; }

	inline uint32_t PointerCount() override { return 1; }
	inline const InputPointer* GetPointer(uint32_t index) override { return &mCurrent.mMousePointer; }
	inline const InputPointer* GetLastPointer(uint32_t index) override { return &mLast.mMousePointer; }
	ENGINE_EXPORT void NextFrame() override;

private:
	friend class Window;
	struct State {
		InputPointer mMousePointer;
		float2 mCursorPos;
		float2 mScrollDelta;
		std::unordered_map<int, int> mKeys;
	};
	State mCurrent;
	State mLast;
};