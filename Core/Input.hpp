#pragma once

#include <unordered_map>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <Util/Util.hpp>

using namespace glm;

class Window;

class Input {
public:
	ENGINE_EXPORT static bool KeyDown(int key);
	ENGINE_EXPORT static bool KeyDownFirst(int key);
	ENGINE_EXPORT static bool MouseButtonDown(int button);
	ENGINE_EXPORT static bool MouseButtonDownFirst(int button);
	ENGINE_EXPORT static vec2 CursorPos();
	ENGINE_EXPORT static vec2 CursorPos(Window* relativeWindow);
	ENGINE_EXPORT static vec2 CursorDelta();
	ENGINE_EXPORT static vec2 ScrollDelta();

private:
	friend class Window;
	friend class VkCAVE;

	struct InputState {
		vec2 mCursorPos;
		vec2 mScrollDelta;
		std::unordered_map<int, int> mMouseButtons;
		std::unordered_map<int, int> mKeys;
	};

	static InputState* mCurrentState;
	static InputState* mLastState;

	ENGINE_EXPORT static void Initialize();
	ENGINE_EXPORT static void Destroy();
	ENGINE_EXPORT static void NextFrame();

	ENGINE_EXPORT static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	ENGINE_EXPORT static void CursorPosCallback(GLFWwindow* window, double x, double y);
	ENGINE_EXPORT static void ScrollCallback(GLFWwindow* window, double x, double y);
	ENGINE_EXPORT static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
};