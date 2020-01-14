#include <Input/MouseKeyboardInput.hpp>
#include <Core/Window.hpp>

MouseKeyboardInput::MouseKeyboardInput(){
	mLastWindow = nullptr;
	mMousePointer.mDevice = this;
	mLockMouse = false;
	memset(mCurrent.mKeys, 0, sizeof(bool) * 0xff);
	memset(mLast.mKeys, 0, sizeof(bool) * 0xff);
}

void MouseKeyboardInput::LockMouse(bool l) {
	#ifdef WINDOWS
	if (mLockMouse && !l)
		ShowCursor(TRUE);
	else if (!mLockMouse && l)
		ShowCursor(FALSE);
	#else
	// TODO
	#endif

	mLockMouse = l;
}

void MouseKeyboardInput::NextFrame() {
	mMousePointer.mLastWorldRay = mMousePointer.mWorldRay;
	mMousePointer.mLastAxis = mMousePointer.mAxis;
	mLast = mCurrent;
	mCurrent.mScrollDelta = 0;
	mCurrent.mCursorDelta = 0;
}