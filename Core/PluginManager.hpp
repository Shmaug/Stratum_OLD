#pragma once

#include <Core/EnginePlugin.hpp>
#include <vector>

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
	friend class Stratum;

	#ifdef WINDOWS
	typedef HMODULE PluginHandle;
	#else
	typedef void* PluginHandle;
	#endif

	std::vector<PluginHandle> mPluginModules;
	std::vector<EnginePlugin*> mPlugins;

	ENGINE_EXPORT EnginePlugin* CreatePlugin(PluginHandle handle);
	ENGINE_EXPORT void UnloadPlugin(PluginHandle handle);
	ENGINE_EXPORT PluginHandle LoadPlugin(const std::string& filename, bool errmsg);
	
	ENGINE_EXPORT void LoadPlugins();
	ENGINE_EXPORT void InitPlugins(Scene* scene);
	ENGINE_EXPORT void UnloadPlugins();
};