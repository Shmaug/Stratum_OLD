#include <Input/InputManager.hpp>

InputManager::~InputManager() {
	for (InputDevice* d : mInputDevices)
		safe_delete(d);
}

void InputManager::RegisterInputDevice(InputDevice* device) {
	for (InputDevice* d : mInputDevices)
		if (d == device) return;
	mInputDevices.push_back(device);
}

void InputManager::UnregisterInputDevice(InputDevice* device) {
	for (auto d = mInputDevices.begin(); d != mInputDevices.end();)
		if (*d == device)
			d = mInputDevices.erase(d);
		else
			d++;
}