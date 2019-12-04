#pragma once

#include <Input/InputDevice.hpp>

class InputManager {
public:
	ENGINE_EXPORT ~InputManager();
	ENGINE_EXPORT void RegisterInputDevice(InputDevice* device);
	ENGINE_EXPORT void UnregisterInputDevice(InputDevice* device);

	template<class T>
	inline T* GetFirst() {
		for (InputDevice* p : mInputDevices)
			if (T* t = dynamic_cast<T*>(p))
				return t;
		return nullptr;
	}

	template<class T>
	inline void GetDevices(std::vector<T*>& devices) {
		for (InputDevice* p : mInputDevices)
			if (T* t = dynamic_cast<T*>(p))
				devices.push_back(t);
	}

private:
	friend class Stratum;
	std::vector<InputDevice*> mInputDevices;
};