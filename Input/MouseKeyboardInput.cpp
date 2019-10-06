#include <Input/MouseKeyboardInput.hpp>

void MouseKeyboardInput::NextFrame() {
	mLast = mCurrent;
	mCurrent.mScrollDelta.x = mCurrent.mScrollDelta.y = 0;
}