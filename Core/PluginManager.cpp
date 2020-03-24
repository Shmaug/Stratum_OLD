#include <Core/PluginManager.hpp>

using namespace std;

#ifdef WINDOWS
typedef EnginePlugin* (__cdecl* CreatePluginProc)(void);
#define NULL_PLUGIN NULL
#else
#include <dlfcn.h>
#define NULL_PLUGIN nullptr
#endif

#include <cstring>

PluginManager::~PluginManager() {
	UnloadPlugins();
}

EnginePlugin* PluginManager::CreatePlugin(PluginHandle handle) {
	EnginePlugin* (*fptr)(void);
	#ifdef WINDOWS
	*(void**)(&fptr) = (void*)GetProcAddress(handle, "CreatePlugin");
	#else
	*(void**)(&fptr) = dlsym(handle, "CreatePlugin");
	#endif

	EnginePlugin* plugin = (*fptr)();
	if (plugin == nullptr) {
		fprintf(stderr, "Failed to call CreatePlugin!\n");
		return nullptr;
	}
	return plugin;
}

PluginManager::PluginHandle PluginManager::LoadPlugin(const string& filename, bool errmsg) {
	#ifdef WINDOWS
	HMODULE m = LoadLibraryA(filename.c_str());
	if (m == NULL) {
		if (errmsg) fprintf_color(COLOR_RED, stderr, "Failed to load library!\n");
		return NULL_PLUGIN;
	}
	EnginePlugin* (*fptr)(void);
	*(void**)(&fptr) = (void*)GetProcAddress(m, "CreatePlugin");
	if (fptr == NULL) {
		if (errmsg) fprintf_color(COLOR_RED, stderr, "Failed to find CreatePlugin!\n");
		UnloadPlugin(m);
		return NULL_PLUGIN;
	}
	return m;
	#else
	void* handle = dlopen(filename.c_str(), RTLD_NOW);
	if (handle == nullptr) {
		char* err = dlerror();
		if (errmsg) fprintf_color(COLOR_RED, stderr, "Failed to load: %s\n", err);
		return NULL_PLUGIN;
	}
	EnginePlugin* (*fptr)(void);
	*(void**)(&fptr) = dlsym(handle, "CreatePlugin");
	if (fptr == nullptr) {
		char* err = dlerror();
		if (errmsg) fprintf_color(COLOR_RED, stderr, "Failed to find CreatePlugin: %s\n", err);
		if (dlclose(handle) != 0) fprintf_color(COLOR_RED, stderr, "Failed to free library! %s\n", dlerror());
		return NULL_PLUGIN;
	}
	return handle;
	#endif
}
void PluginManager::UnloadPlugin(PluginHandle handle){
	#ifdef WINDOWS
	if (!FreeLibrary(handle)) fprintf_color(COLOR_RED, stderr, "Failed to unload plugin module\n");
	#else
	if (dlclose(handle) != 0) fprintf_color(COLOR_RED, stderr, "Failed to unload plugin library\n");
	#endif
}

void PluginManager::LoadPlugins() {
	UnloadPlugins();

	std::vector<string> failed;

	for (const auto& p : fs::directory_iterator("Plugins")) {
#ifdef WINDOWS
		// load plugin DLLs
		if (wcscmp(p.path().extension().c_str(), L".dll") == 0) {
#else
		if (strcmp(p.path().extension().c_str(), ".so") == 0) {
#endif
			PluginHandle handle = LoadPlugin(p.path().string(), false);
			if (handle == NULL_PLUGIN) { failed.push_back(p.path().string()); continue; }
			EnginePlugin* plugin = CreatePlugin(handle);
			if (!plugin) {
				failed.push_back(p.path().string());
				UnloadPlugin(handle);
				continue;
			}
			mPluginModules.push_back(handle);
			mPlugins.push_back(plugin);
			printf_color(COLOR_YELLOW, "Loaded %s\n", p.path().string().c_str());
		}
	}

	// try to resolve any link errors by loading any plugins that failed to load after the rest
	bool c = failed.size();
	while (c) {
		c = false;
		for (auto it = failed.begin(); it != failed.end();) {
			PluginHandle handle = LoadPlugin(*it, false);
			if (handle == NULL_PLUGIN) { it++; continue; }
			EnginePlugin* plugin = CreatePlugin(handle);
			if (!plugin) {
				UnloadPlugin(handle);
				it++;
				continue;
			}
			mPluginModules.push_back(handle);
			mPlugins.push_back(plugin);
			printf_color(COLOR_YELLOW, "Loaded %s\n", it->c_str());
			it = failed.erase(it);
			c = true;
		}
	}
	// load failed plugins AGAIN just to print the error message
	for (string p : failed) {
		PluginHandle handle = LoadPlugin(p, true);
		if (handle == NULL_PLUGIN) continue;
		EnginePlugin* plugin = CreatePlugin(handle);
		if (!plugin) {
			UnloadPlugin(handle);
			continue;
		}
		// if the plugin sucsessfully loads here then WTF
		printf_color(COLOR_YELLOW, "Loaded %s\n", p.c_str());
		mPluginModules.push_back(handle);
		mPlugins.push_back(plugin);
	}

	sort(mPlugins.begin(), mPlugins.end(), [](const auto& a, const auto& b) {
		return a->Priority() > b->Priority();
	});
}

void PluginManager::InitPlugins(Scene* scene) {
	for (auto it = mPlugins.begin(); it != mPlugins.end();) {
		if (!(*it)->Init(scene)) {
			safe_delete(*it);
			it = mPlugins.erase(it);
		} else
			it++;
	}
}
void PluginManager::UnloadPlugins() {
	for (auto& p : mPlugins)
		safe_delete(p);
	mPlugins.clear();
	for (const auto& m : mPluginModules)
		UnloadPlugin(m);
	mPluginModules.clear();
}
