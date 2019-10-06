#pragma once

#include <unordered_map>
#include <Input/InputDevice.hpp>
#include <glfw/glfw3.h>

class Window;

class MouseKeyboardInput : public InputDevice {
public:
	inline bool MouseButtonDownFirst(int key) { return mCurrent.mMouseButtons[key] == GLFW_PRESS && mLast.mMouseButtons[key] == GLFW_RELEASE; }
	inline bool MouseButtonUpFirst(int key) { return mLast.mMouseButtons[key] == GLFW_PRESS && mCurrent.mMouseButtons[key] == GLFW_RELEASE; }
	inline bool MouseButtonDown(int key) { return mCurrent.mMouseButtons[key] == GLFW_PRESS; }
	inline bool MouseButtonUp(int key) { return mCurrent.mMouseButtons[key] == GLFW_RELEASE; }

	inline bool KeyDownFirst(int key) { return mCurrent.mKeys[key] == GLFW_PRESS && mLast.mKeys[key] == GLFW_RELEASE; }
	inline bool KeyUpFirst(int key) { return mLast.mKeys[key] == GLFW_PRESS && mCurrent.mKeys[key] == GLFW_RELEASE; }
	inline bool KeyDown(int key) { return mCurrent.mKeys[key] == GLFW_PRESS; }
	inline bool KeyUp(int key) { return mCurrent.mKeys[key] == GLFW_RELEASE; }

	inline vec2 ScrollDelta() const { return mCurrent.mScrollDelta; }
	inline vec2 CursorPos() const { return mCurrent.mCursorPos; }
	inline vec2 CursorDelta() const { return mCurrent.mCursorPos - mLast.mCursorPos; }

	ENGINE_EXPORT void NextFrame() override;

private:
	friend class Window;
	struct State {
		vec2 mCursorPos;
		vec2 mScrollDelta;
		std::unordered_map<int, int> mMouseButtons;
		std::unordered_map<int, int> mKeys;
	};
	State mCurrent;
	State mLast;
};