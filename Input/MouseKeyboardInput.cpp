#include <Input/MouseKeyboardInput.hpp>

MouseKeyboardInput::MouseKeyboardInput(){
	mMousePointer.mDevice = this;
}

void MouseKeyboardInput::NextFrame() {
	mMousePointer.mLastWorldRay = mMousePointer.mWorldRay;
	mMousePointer.mLastAxis = mMousePointer.mAxis;
	mLast = mCurrent;
	mCurrent.mScrollDelta.x = mCurrent.mScrollDelta.y = 0;
}