#include <Input/MouseKeyboardInput.hpp>
#include <Core/Window.hpp>

MouseKeyboardInput::MouseKeyboardInput(){
	mLastWindow = nullptr;
	mMousePointer.mDevice = this;
	mLockMouse = false;
}

void MouseKeyboardInput::LockMouse(bool l) {
	mLockMouse = l;

	if (mLockMouse)
		glfwSetInputMode(*mLastWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	else
		glfwSetInputMode(*mLastWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void MouseKeyboardInput::NextFrame() {
	mMousePointer.mLastWorldRay = mMousePointer.mWorldRay;
	mMousePointer.mLastAxis = mMousePointer.mAxis;
	mLast = mCurrent;
	mCurrent.mScrollDelta = 0;
	mCurrent.mCursorDelta = 0;
}