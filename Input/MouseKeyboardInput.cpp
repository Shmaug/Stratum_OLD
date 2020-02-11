#include <Input/MouseKeyboardInput.hpp>
#include <Core/Window.hpp>

MouseKeyboardInput::MouseKeyboardInput(){
	mMousePointer.mDevice = this;
	mMousePointer.mAxis.emplace(0, 0.f);
	mMousePointer.mAxis.emplace(1, 0.f);
	mMousePointer.mAxis.emplace(2, 0.f);
	mLockMouse = false;
	mCurrent.mCursorPos = mLast.mCursorPos = 0;
	mCurrent.mCursorDelta = mLast.mCursorDelta = 0;
	mCurrent.mCursorDelta = mLast.mScrollDelta = 0;
	mCurrent.mKeys = {};
	mLast.mKeys = {};
}

void MouseKeyboardInput::LockMouse(bool l) {
	#ifdef WINDOWS
	if (mLockMouse && !l)
		ShowCursor(TRUE);
	else if (!mLockMouse && l)
		ShowCursor(FALSE);
	#else
	// TODO: hide cursor on linux
	#endif

	mLockMouse = l;
}

void MouseKeyboardInput::NextFrame() {
	mMousePointer.mLastWorldRay = mMousePointer.mWorldRay;
	mMousePointer.mLastAxis = mMousePointer.mAxis;
	mMousePointer.mLastGuiHitT = mMousePointer.mGuiHitT;
	mLast = mCurrent;
	mCurrent.mScrollDelta = 0;
	mCurrent.mCursorDelta = 0;
	mMousePointer.mGuiHitT = -1.f;
}