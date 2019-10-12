#pragma once

#ifdef WINDOWS
#include <Windows.h>
#undef near
#undef far
#endif

#include <Core/EnginePlugin.hpp>
#include <vector>

class VkCAVE;
class Scene;
class EnginePlugin;

class PluginManager {
public:
	ENGINE_EXPORT ~PluginManager();

	const std::vector<EnginePlugin*>& Plugins() const { return mPlugins; }

	template<class T>
	inline T* GetPlugin() {
		for (EnginePlugin* p : mPlugins)
			if (T * t = dynamic_cast<T*>(p))
				return t;
		return nullptr;
	}
	
private:
	friend class VkCAVE;

	#ifdef WINDOWS
	std::vector<HMODULE> mPluginModules;
	#else
	std::vector<void*> mPluginModules;
	#endif

	std::vector<EnginePlugin*> mPlugins;

	ENGINE_EXPORT void LoadPlugins(Scene* scene);
	ENGINE_EXPORT void UnloadPlugins();
};