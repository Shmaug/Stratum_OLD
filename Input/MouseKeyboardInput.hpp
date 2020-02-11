#pragma once

#include <vector>
#include <Input/InputDevice.hpp>

class Window;

#ifdef WINDOWS
enum KeyCode {
	KEY_NONE 	= 0x00,

	MOUSE_LEFT 	= 0x01,
	MOUSE_RIGHT = 0x02,
	KEY_CANCEL 	= 0x03,
	MOUSE_MIDDLE = 0x04,
	MOUSE_X1 = 0x05,
	MOUSE_X2 = 0x06,

	KEY_BACKSPACE = 0x08,
	KEY_TAB = 0x09,
	KEY_ENTER = 0x0d,
	KEY_LOCK_CAPS 	= 0x14,
	KEY_ESCAPE = 0x1b,

	KEY_SPACE 		= 0x20,
	KEY_PAGEUP 		= 0x21,
	KEY_PAGEDOWN 	= 0x22,
	KEY_END 		= 0x23,
	KEY_HOME 		= 0x24,
	KEY_LEFT 		= 0x25,
	KEY_UP 			= 0x26,
	KEY_RIGHT 		= 0x27,
	KEY_DOWN 		= 0x28,
	KEY_PRINTSCREEN = 0x2c,
	KEY_INSERT 		= 0X2d,
	KEY_DELETE 		= 0X2e,
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
	KEY_NUM_LOCK = 0x90,
	KEY_SCROLL_LOCK = 0x91,
	KEY_LSHIFT = 0xa0,
	KEY_RSHIFT = 0xa1,
	KEY_LCONTROL = 0xa2,
	KEY_RCONTROL = 0xa3,
	KEY_LALT = 0xa4,
	KEY_RALT = 0xa5,

	KEY_SEMICOLON = 0xba, // Used for miscellaneous characters; it can vary by keyboard.  For the US standard keyboard, the ';:' key
	KEY_EQUAL = 0xbb, // For any country/region, the '+' key
	KEY_COMMA = 0xbc, // For any country/region, the ',' key
	KEY_MINUS = 0xbd, // For any country/region, the '-' key
	KEY_PERIOD = 0xbe, // For any country/region, the '.' key
	KEY_SLASH = 0xbf, // Used for miscellaneous characters; it can vary by keyboard. For the US standard keyboard, the '/?' key
	KEY_TILDE = 0xc0, // Used for miscellaneous characters; it can vary by keyboard. For the US standard keyboard, the '`~' key
	KEY_BRACKET_L = 0xdb, // Used for miscellaneous characters; it can vary by keyboard. For the US standard keyboard, the '[{' key
	KEY_BACKSLASH = 0xdc, // Used for miscellaneous characters; it can vary by keyboard. For the US standard keyboard, the '\|' key
	KEY_BRACKET_R = 0xdd, // Used for miscellaneous characters; it can vary by keyboard. For the US standard keyboard, the ']}' key
	KEY_QUOTE = 0xde, // Used for miscellaneous characters; it can vary by keyboard. For the US standard keyboard, the 'single-quote/double-quote' key
};
#elif defined(__linux)
#include <X11/keysym.h>
enum KeyCode {
	KEY_NONE 			= 0x0000,

	MOUSE_LEFT 			= 0x001,
	MOUSE_RIGHT 		= 0x002,
	KEY_CANCEL 			= 0x003,
	MOUSE_MIDDLE 		= 0x004,
	MOUSE_X1 			= 0x005,
	MOUSE_X2 			= 0x006,

	KEY_BACKSPACE 		= XK_BackSpace,
	KEY_TAB 			= XK_Tab,
	KEY_ENTER 			= XK_Return,
	KEY_LOCK_CAPS 		= XK_Caps_Lock,
	KEY_ESCAPE 			= XK_Escape,

	KEY_SPACE 			= XK_space,
	KEY_PAGEUP 			= XK_Page_Up,
	KEY_PAGEDOWN 		= XK_Page_Down,
	KEY_END 			= XK_End,
	KEY_HOME 			= XK_Home,
	KEY_LEFT 			= XK_Left,
	KEY_UP 				= XK_Up,
	KEY_RIGHT 			= XK_Right,
	KEY_DOWN 			= XK_Down,
	KEY_INSERT 			= XK_Insert,
	KEY_DELETE 			= XK_Delete,
	KEY_D0 				= XK_0,
	KEY_D1 				= XK_1,
	KEY_D2 				= XK_2,
	KEY_D3 				= XK_3,
	KEY_D4 				= XK_4,
	KEY_D5 				= XK_5,
	KEY_D6 				= XK_6,
	KEY_D7 				= XK_7,
	KEY_D8 				= XK_8,
	KEY_D9 				= XK_9,
	KEY_A  				= XK_a,
	KEY_B  				= XK_b,
	KEY_C  				= XK_c,
	KEY_D  				= XK_d,
	KEY_E  				= XK_e,
	KEY_F  				= XK_f,
	KEY_G  				= XK_g,
	KEY_H  				= XK_h,
	KEY_I  				= XK_i,
	KEY_J  				= XK_j,
	KEY_K  				= XK_k,
	KEY_L  				= XK_l,
	KEY_M  				= XK_m,
	KEY_N  				= XK_n,
	KEY_O  				= XK_o,
	KEY_P  				= XK_p,
	KEY_Q  				= XK_q,
	KEY_R  				= XK_r,
	KEY_S  				= XK_s,
	KEY_T  				= XK_t,
	KEY_U  				= XK_u,
	KEY_V  				= XK_v,
	KEY_W  				= XK_w,
	KEY_X  				= XK_x,
	KEY_Y  				= XK_y,
	KEY_Z  				= XK_z,
	KEY_NUMPAD0 		= XK_KP_Insert,
	KEY_NUMPAD1 		= XK_KP_End,
	KEY_NUMPAD2 		= XK_KP_Down,
	KEY_NUMPAD3 		= XK_KP_Page_Down,
	KEY_NUMPAD4 		= XK_KP_Left,
	KEY_NUMPAD5 		= XK_KP_Begin,
	KEY_NUMPAD6 		= XK_KP_Right,
	KEY_NUMPAD7 		= XK_KP_Home,
	KEY_NUMPAD8 		= XK_KP_Up,
	KEY_NUMPAD9 		= XK_KP_Page_Up,
	KEY_MULTIPLY 		= XK_KP_Multiply,
	KEY_ADD 			= XK_KP_Add,
	KEY_NUMPAD_ENTER 	= XK_KP_Enter,
	KEY_SUBTRACT 		= XK_KP_Subtract,
	KEY_DECIMAL 		= XK_KP_Delete,
	KEY_DIVIDE 			= XK_KP_Divide,
	KEY_F1  			= XK_F1,
	KEY_F2  			= XK_F2,
	KEY_F3  			= XK_F3,
	KEY_F4  			= XK_F4,
	KEY_F5  			= XK_F5,
	KEY_F6  			= XK_F6,
	KEY_F7  			= XK_F7,
	KEY_F8  			= XK_F8,
	KEY_F9  			= XK_F9,
	KEY_F10 			= XK_F10,
	KEY_F11 			= XK_F11,
	KEY_F12 			= XK_F12,
	KEY_NUM_LOCK 		= XK_Num_Lock,
	KEY_SCROLL_LOCK 	= XK_Scroll_Lock,
	KEY_LSHIFT 			= XK_Shift_L,
	KEY_RSHIFT 			= XK_Shift_R,
	KEY_LCONTROL 		= XK_Control_L,
	KEY_RCONTROL 		= XK_Control_R,
	KEY_LALT 			= XK_Alt_L,
	KEY_RALT 			= XK_Alt_R,

	KEY_SEMICOLON 		= XK_semicolon,
	KEY_EQUAL 			= XK_equal,
	KEY_COMMA 			= XK_comma,
	KEY_MINUS 			= XK_minus,
	KEY_PERIOD 			= XK_period,
	KEY_SLASH			= XK_slash,
	KEY_TILDE 			= XK_grave,
	KEY_BRACKET_L 		= XK_bracketleft,
	KEY_BRACKET_R 		= XK_bracketright,
	KEY_QUOTE 			= XK_quotedbl,
	KEY_BACKSLASH 		= XK_backslash,
};
#endif

class MouseKeyboardInput : public InputDevice {
public:
	ENGINE_EXPORT MouseKeyboardInput();

	ENGINE_EXPORT void LockMouse(bool l);
	inline bool LockMouse() const { return mLockMouse; }

	inline uint32_t WindowWidth() const { return mWindowWidth; }
	inline uint32_t WindowHeight() const { return mWindowHeight; }

	inline bool KeyDownFirst(KeyCode key) { return mCurrent.mKeys[key] && !mLast.mKeys[key]; }
	inline bool KeyUpFirst(KeyCode key) { return mLast.mKeys[key] && !mCurrent.mKeys[key]; }
	inline bool KeyDown(KeyCode key) { return mCurrent.mKeys[key]; }
	inline bool KeyUp(KeyCode key) { return !mCurrent.mKeys[key]; }

	inline float ScrollDelta() const { return mCurrent.mScrollDelta; }
	inline float2 CursorPos() const { return mCurrent.mCursorPos; }
	inline float2 CursorDelta() const { return mCurrent.mCursorDelta; }

	inline uint32_t PointerCount() override { return 1; }
	inline const InputPointer* GetPointer(uint32_t index) override { return &mMousePointer; }
	ENGINE_EXPORT void NextFrame() override;

private:
	friend class GUI;
	friend class Window;
	friend class Instance;
	struct State {
		float2 mCursorPos;
		float2 mCursorDelta;
		float mScrollDelta;
		std::unordered_map<KeyCode, bool> mKeys;
	};
	InputPointer mMousePointer;
	State mCurrent;
	State mLast;
	bool mLockMouse;

	uint32_t mWindowWidth;
	uint32_t mWindowHeight;
};