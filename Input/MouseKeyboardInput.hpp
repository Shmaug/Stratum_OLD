#pragma once

#include <unordered_map>
#include <Input/InputDevice.hpp>

class Window;

enum KeyCode {
	KEY_NONE 	= 0x00,
	MOUSE_LEFT 	= 0x01,
	MOUSE_RIGHT = 0x02,
	KEY_CANCEL 	= 0x03,
	MOUSE_MIDDLE = 0x04,
	MOUSE_X1 = 0x05,
	MOUSE_X2 = 0x06,
	// 0x07 is undefined
	KEY_BACKSPACE = 0x08,
	KEY_TAB = 0x09,
	// 0x0A-0B are reserved
	KEY_CLEAR = 0x0c,
	KEY_ENTER = 0x0d,
	// 0x0E-0F are undefined
	KEY_SHIFT 		= 0x10,
	KEY_CONTROL 	= 0x11,
	KEY_ALT 		= 0x12,
	KEY_PAUSE 		= 0x13,
	KEY_LOCK_CAPS 	= 0x14,
	/*
	KanaMode = 0x15,
	HangulMode = 0x15,
	// 0x16 is undefined
	JunjaMode = 0x17,
	FinalMode = 0x18,
	HanjaMode = 0x19,
	KanjiMode = 0x19,
	*/
	// 0x1A is undefined
	KEY_ESCAPE = 0x1b,

	KEY_IME_CONVERT = 0x1c,
	KEY_IMI_NOCONVERT = 0x1d,
	KEY_IME_ACCEPT = 0x1e,
	KEY_IMI_MODECHANGE = 0x1f,

	KEY_SPACE 		= 0x20,
	KEY_PRIOR 		= 0x21,
	KEY_PAGEUP 		= 0x21,
	KEY_NEXT 		= 0x22,
	KEY_PAGEDOWN 	= 0x22,
	KEY_END 		= 0x23,
	KEY_HOME 		= 0x24,
	KEY_LEFT 		= 0x25,
	KEY_UP 			= 0x26,
	KEY_RIGHT 		= 0x27,
	KEY_DOWN 		= 0x28,
	KEY_SELECT 		= 0x29,
	KEY_PRINT 		= 0x2A,
	KEY_EXECUTE 	= 0x2B,
	KEY_PRINTSCREEN = 0x2c,
	KEY_INSERT 		= 0X2d,
	KEY_DELETE 		= 0X2e,
	KEY_HELP 		= 0X2F,
	
	KEY_D0 = 0x30,
	KEY_D1 = 0x31,
	KEY_D2 = 0x32,
	KEY_D3 = 0x33,
	KEY_D4 = 0x34,
	KEY_D5 = 0x35,
	KEY_D6 = 0x36,
	KEY_D7 = 0x37,
	KEY_D8 = 0x38,
	KEY_D9 = 0x39,
	// 0x3A - 40 are undefined
	KEY_A = 0x41,
	KEY_B = 0x42,
	KEY_C = 0x43,
	KEY_D = 0x44,
	KEY_E = 0x45,
	KEY_F = 0x46,
	KEY_G = 0x47,
	KEY_H = 0x48,
	KEY_I = 0x49,
	KEY_J = 0x4a,
	KEY_K = 0x4b,
	KEY_L = 0x4c,
	KEY_M = 0x4d,
	KEY_N = 0x4e,
	KEY_O = 0x4f,
	KEY_P = 0x50,
	KEY_Q = 0x51,
	KEY_R = 0x52,
	KEY_S = 0x53,
	KEY_T = 0x54,
	KEY_U = 0x55,
	KEY_V = 0x56,
	KEY_W = 0x57,
	KEY_X = 0x58,
	KEY_Y = 0x59,
	KEY_Z = 0x5a,
	KEY_LWIN = 0x5b,
	KEY_RWIN = 0x5c,
	KEY_APPS = 0x5d,
	// 0x5E is reserved
	KEY_SLEEP= 0x5f,
	KEY_NUMPAD0 = 0x60,
	KEY_NUMPAD1 = 0x61,
	KEY_NUMPAD2 = 0x62,
	KEY_NUMPAD3 = 0x63,
	KEY_NUMPAD4 = 0x64,
	KEY_NUMPAD5 = 0x65,
	KEY_NUMPAD6 = 0x66,
	KEY_NUMPAD7 = 0x67,
	KEY_NUMPAD8 = 0x68,
	KEY_NUMPAD9 = 0x69,
	KEY_MULTIPLY = 0x6a,
	KEY_ADD = 0x6b,
	KEY_SEPARATOR = 0x6c,
	KEY_SUBTRACT = 0x6d,
	KEY_DECIMAL = 0x6e,
	KEY_DIVIDE = 0x6f,
	KEY_F1 = 0x70,
	KEY_F2 = 0x71,
	KEY_F3 = 0x72,
	KEY_F4 = 0x73,
	KEY_F5 = 0x74,
	KEY_F6 = 0x75,
	KEY_F7 = 0x76,
	KEY_F8 = 0x77,
	KEY_F9 = 0x78,
	KEY_F10 = 0x79,
	KEY_F11 = 0x7a,
	KEY_F12 = 0x7b,
	KEY_F13 = 0x7c,
	KEY_F14 = 0x7d,
	KEY_F15 = 0x7e,
	KEY_F16 = 0x7f,
	KEY_F17 = 0x80,
	KEY_F18 = 0x81,
	KEY_F19 = 0x82,
	KEY_F20 = 0x83,
	KEY_F21 = 0x84,
	KEY_F22 = 0x85,
	KEY_F23 = 0x86,
	KEY_F24 = 0x87,
	// 0x88 - 0x8f are unassigned
	KEY_NUM_LOCK = 0x90,
	KEY_SCROLL_LOCK = 0x91,
	// 0x92 - 96 are OEM specific
	// 0x97 - 9f are unassigned
	KEY_LSHIFT = 0xa0,
	KEY_RSHIFT = 0xa1,
	KEY_LCONTROL = 0xa2,
	KEY_RCONTROL = 0xa3,
	KEY_LALT = 0xa4,
	KEY_RALT = 0xa5,
	KEY_BROWSER_BACK = 0xa6,
	KEY_BROWSER_FORWARD = 0xa7,
	KEY_BROWSER_REFRESH = 0xa8,
	KEY_BROWSER_STOP = 0xa9,
	KEY_BROWSER_SEARCH = 0xaa,
	KEY_BROWSER_FAVORITES = 0xab,
	KEY_BROWSER_HOME = 0xac,
	KEY_VOLUME_MUTE = 0xad,
	KEY_VOLUME_DOWN = 0xae,
	KEY_VOLUME_UP = 0xaf,

	KEY_MEDIA_NEXT = 0xb0,
	KEY_MEDIA_PREVIOUS = 0xb1,
	KEY_MEDIA_STOP = 0xb2,
	KEY_MEDIA_PLAY_PAUSE = 0xb3,

	KEY_LAUNCH_MAIL = 0xb4,
	KEY_SELECT_MEDIA = 0xb5,
	KEY_LAUNCH_APPLICATION_0 = 0xb6,
	KEY_LAUNCH_APPLICATION_1 = 0xb7,
	// 0xB8 - B9 are reserved
	KEY_OEM_SEMICOLON = 0xba, // Used for miscellaneous characters; it can vary by keyboard.  For the US standard keyboard, the ';:' key
	KEY_OEM_PLUS = 0xbb, // For any country/region, the '+' key
	KEY_OEM_COMMA = 0xbc, // For any country/region, the ',' key
	KEY_OEM_MINUS = 0xbd, // For any country/region, the '-' key
	KEY_OEM_PERIOD = 0xbe, // For any country/region, the '.' key
	KEY_OEM_QUESTION = 0xbf, // Used for miscellaneous characters; it can vary by keyboard. For the US standard keyboard, the '/?' key
	KEY_OEM_TILDE = 0xc0, // Used for miscellaneous characters; it can vary by keyboard. For the US standard keyboard, the '`~' key
	// 0xC1 - D7 are reserved
	// 0xD8 - DA are unassigned
	KEY_OEM_BRACKET_OPEN = 0xdb, // Used for miscellaneous characters; it can vary by keyboard. For the US standard keyboard, the '[{' key
	KEY_OEM_PIPE = 0xdc, // Used for miscellaneous characters; it can vary by keyboard. For the US standard keyboard, the '\|' key
	KEY_OEM_BRACKET_CLOSE = 0xdd, // Used for miscellaneous characters; it can vary by keyboard. For the US standard keyboard, the ']}' key
	KEY_OEM_QUOTE = 0xde, // Used for miscellaneous characters; it can vary by keyboard. For the US standard keyboard, the 'single-quote/double-quote' key
	KEY_OEM8 = 0xdf, // Used for miscellaneous characters; it can vary by keyboard.
	// 0xE0 is reserved
	// 0xE1 is OEM specific
	KEY_BACKSLASH = 0xe2, // Either the angle bracket key or the backslash key on the RT 102-key keyboard
	KEY_OEM102 = 0xe2, // Either the angle bracket key or the backslash key on the RT 102-key keyboard
	// 0xE3 - E4 OEM specific
	KEY_PROCESS = 0xe5,
	// 0xE6 is OEM specific
	KEY_UTF_PACKET = 0xe7, // Used to pass Unicode characters as if they were keystrokes. The Packet key value is the low word of a 32-bit virtual-key value used for non-keyboard input methods.
	// 0xE8 is unassigned
	// 0xE9 - F5 OEM specific
	KEY_ATTN = 0xf6,
	KEY_CRSEL = 0xf7,
	KEY_EXSEL = 0xf8,
	KEY_ERASE_EOF = 0xf9,
	KEY_PLAY = 0xfa, 
	KEY_ZOOM = 0xfb,
	// 0xfc is reserved
	KEY_PA1 = 0xfd,
	//KEY_CLEAR = 0xfe,
};

class MouseKeyboardInput : public InputDevice {
public:
	ENGINE_EXPORT MouseKeyboardInput();

	ENGINE_EXPORT void LockMouse(bool l);
	inline bool LockMouse() const { return mLockMouse; }

	inline bool KeyDownFirst(KeyCode key) { return mCurrent.mKeys[key] && !mLast.mKeys[key]; }
	inline bool KeyUpFirst(KeyCode key) { return mLast.mKeys[key] && !mCurrent.mKeys[key]; }
	inline bool KeyDown(KeyCode key) { return mCurrent.mKeys[key]; }
	inline bool KeyUp(KeyCode key) { return !mCurrent.mKeys[key]; }

	inline float2 ScrollDelta() const { return mCurrent.mScrollDelta; }
	inline float2 CursorPos() const { return mCurrent.mCursorPos; }
	inline float2 CursorDelta() const { return mCurrent.mCursorDelta; }

	inline uint32_t PointerCount() override { return 1; }
	inline const InputPointer* GetPointer(uint32_t index) override { return &mMousePointer; }
	ENGINE_EXPORT void NextFrame() override;

private:
	friend class Window;
	struct State {
		float2 mCursorPos;
		float2 mCursorDelta;
		float2 mScrollDelta;
		std::unordered_map<KeyCode, bool> mKeys;
	};
	Window* mLastWindow;
	InputPointer mMousePointer;
	State mCurrent;
	State mLast;
	bool mLockMouse;
};