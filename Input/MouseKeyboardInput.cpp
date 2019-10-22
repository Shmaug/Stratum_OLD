#include <Input/MouseKeyboardInput.hpp>

MouseKeyboardInput::MouseKeyboardInput(){
	mCurrent.mMousePointer.mDevice = this;
	mLast.mMousePointer.mDevice = this;
}

void MouseKeyboardInput::NextFrame() {
	mLast = mCurrent;
	mCurrent.mScrollDelta.x = mCurrent.mScrollDelta.y = 0;
}