#include <Core/PluginManager.hpp>
#include <filesystem>

using namespace std;

#ifdef WINDOWS
typedef EnginePlugin* (__cdecl* CreatePluginProc)(void);
#else
typedef EnginePlugin* (CreatePluginProc)(void);
#endif

PluginManager::~PluginManager() {
	UnloadPlugins();
}

void PluginManager::LoadPlugins(Scene* scene) {
	UnloadPlugins();

	for (const auto& p : filesystem::directory_iterator("Plugins")) {
		#ifdef WINDOWS
		// load plugin DLLs
		if (wcscmp(p.path().extension().c_str(), L".dll") == 0) {
			printf("Loading %S ... ", p.path().c_str());
			HMODULE m = LoadLibraryW(p.path().c_str());
			if (m == NULL) {
				fprintf(stderr, "Failed to load library!\n");
				continue;
			}
			CreatePluginProc proc = (CreatePluginProc)GetProcAddress(m, "CreatePlugin");
			if (proc == NULL) {
				fprintf(stderr, "Failed to find CreatePlugin!\n");
				if (!FreeLibrary(m)) fprintf(stderr, "Failed to unload %S\n", p.path().c_str());
				continue;
			}
			EnginePlugin* plugin = proc();
			if (plugin == nullptr) {
				fprintf(stderr, "Failed to call CreatePlugin!\n");
				if (!FreeLibrary(m)) fprintf(stderr, "Failed to unload %S\n", p.path().c_str());
				continue;
			}
			mPlugins.push_back(plugin);
			mPluginModules.push_back(m);
			printf("Done\n");
		}
		#else
		if (wcscmp(p.path().extension().c_str(), L".so") == 0) {
			void* handle = dlopen(p.path().c_str(), RTLD_LAZY);
			if(handle == nullptr) {
				fprintf(stderr, "Failed to load library!\n");
				continue;
			}

			CreatePluginProc proc = (CreatePluginProc)dlsym(handle, "CreatePlugin");
			if (proc == nullptr) {
				fprintf(stderr, "Failed to find CreatePlugin!\n");
				if (dlclose(handle) != 0) fprintf(stderr, "Failed to free library!\n");
				continue;
			}
			EnginePlugin* plugin = proc();
			if (plugin == nullptr) {
				fprintf(stderr, "Failed to call CreatePlugin!\n");
				if (dlclose(handle) != 0) fprintf(stderr, "Failed to free library!\n");
				continue;
			}
			mPlugins.push_back(plugin);
			mPluginModules.push_back(m);
			printf("Done\n");
		}
		#endif
	}

	sort(mPlugins.begin(), mPlugins.end(), [](const auto& a, const auto& b) {
		return a->Priority() < b->Priority();
	});

	for (const auto& p : mPlugins)
		p->Init(scene);
}
void PluginManager::UnloadPlugins() {
	for (auto& p : mPlugins)
		safe_delete(p);
	mPlugins.clear();

	#ifdef WINDOWS
	for (const auto& m : mPluginModules)
		if (!FreeLibrary(m))
			fprintf("Failed to unload plugin module\n");
	#else
	for (const auto& m : mPluginModules)
		if (dlclose(m) != 0)
			fprintf("Failed to unload plugin library\n");
	#endif
}
