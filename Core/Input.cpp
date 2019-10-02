#include <Core/Input.hpp>
#include <Core/Window.hpp>

using namespace glm;

Input::InputState* Input::mCurrentState = nullptr;
Input::InputState* Input::mLastState = nullptr;

void Input::Initialize() {
	mCurrentState = new InputState();
	mLastState = new InputState();
}
void Input::Destroy() {
	safe_delete(mCurrentState);
	safe_delete(mLastState);
}
void Input::NextFrame() {
	mCurrentState->mScrollDelta = vec2(0, 0);
	mLastState->mCursorPos = mCurrentState->mCursorPos;
	mLastState->mKeys = mCurrentState->mKeys;
	mLastState->mMouseButtons = mCurrentState->mMouseButtons;
}

bool Input::KeyDown(int key) {
	return mCurrentState->mKeys[key] == GLFW_PRESS;
}
bool Input::KeyDownFirst(int key) {
	return mCurrentState->mKeys[key] == GLFW_PRESS && mLastState->mKeys[key] == GLFW_RELEASE;
}
bool Input::MouseButtonDown(int btn) {
	return mCurrentState->mMouseButtons[btn] == GLFW_PRESS;
}
bool Input::MouseButtonDownFirst(int btn) {
	return mCurrentState->mMouseButtons[btn] == GLFW_PRESS && mLastState->mMouseButtons[btn] == GLFW_RELEASE;
}
vec2 Input::CursorPos() {
	return mCurrentState->mCursorPos;
}
vec2 Input::CursorPos(Window* relativeWindow) {
	return mCurrentState->mCursorPos + vec2(relativeWindow->ClientRect().offset.x, relativeWindow->ClientRect().offset.y);
}
vec2 Input::CursorDelta() {
	return mCurrentState->mCursorPos - mLastState->mCursorPos;
}
vec2 Input::ScrollDelta() {
	return mCurrentState->mScrollDelta;
}

void Input::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (action == GLFW_REPEAT) return;
	mCurrentState->mKeys[key] = action;

	Window* win = (Window*)glfwGetWindowUserPointer(window);
	if (mCurrentState->mKeys[GLFW_KEY_LEFT_ALT] == GLFW_PRESS && mCurrentState->mKeys[GLFW_KEY_ENTER] == GLFW_PRESS)
		win->Fullscreen(!win->Fullscreen());
}
void Input::CursorPosCallback(GLFWwindow* window, double x, double y) {
	Window* win = (Window*)glfwGetWindowUserPointer(window);
	mCurrentState->mCursorPos.x = (float)x + win->ClientRect().offset.x;
	mCurrentState->mCursorPos.y = (float)y + win->ClientRect().offset.y;
}
void Input::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	mCurrentState->mMouseButtons[button] = action;
}
void Input::ScrollCallback(GLFWwindow* window, double x, double y) {
	mCurrentState->mScrollDelta.x += (float)x;
	mCurrentState->mScrollDelta.y += (float)y;
}