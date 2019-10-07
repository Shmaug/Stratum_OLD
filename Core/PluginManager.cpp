#include <Core/PluginManager.hpp>
#include <filesystem>

using namespace std;

#ifdef WINDOWS
typedef EnginePlugin* (__cdecl* CreatePluginProc)(void);
#else
static_assert(false, "Not implemented!");
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
				fprintf(stderr, "Failed to find create function!\n");
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
		static_assert(false, "Not implemented!");
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
			cerr << "Failed to unload plugin module" << endl;
	#else
	static_assert(false, "Not implemented!");
	#endif
}
