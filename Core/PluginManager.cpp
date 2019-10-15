#include <Core/PluginManager.hpp>

#ifdef __GNUC__
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif

using namespace std;

#ifdef WINDOWS
typedef EnginePlugin* (__cdecl* CreatePluginProc)(void);
#else
#include <dlfcn.h>
#endif

PluginManager::~PluginManager() {
	UnloadPlugins();
}

void PluginManager::LoadPlugins(Scene* scene) {
	UnloadPlugins();

	for (const auto& p : fs::directory_iterator("Plugins")) {
		#ifdef WINDOWS
		// load plugin DLLs
		if (wcscmp(p.path().extension().c_str(), L".dll") == 0) {
			printf("Loading %S ... ", p.path().c_str());
			HMODULE m = LoadLibraryW(p.path().c_str());
			if (m == NULL) {
				fprintf(stderr, "Failed to load library!\n");
				continue;
			}
			void* proc = (CreatePluginProc)GetProcAddress(m, "CreatePlugin");
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
		if (strcmp(p.path().extension().c_str(), ".so") == 0) {
			printf("Loading %s ... ", p.path().c_str());
			void* handle = dlopen(p.path().c_str(), RTLD_NOW);
			if (handle == nullptr) {
				char* err = dlerror();
				printf("Failed to load: %s\n", err);
				continue;
			}
			void* func = dlsym(handle, "CreatePlugin");
			if (func == nullptr) {
				char* err = dlerror();
				printf("Failed to find CreatePlugin: %s\n", err);
				if (dlclose(handle) != 0) cerr << "Failed to free library! " << dlerror() << endl;
				continue;
			}
			EnginePlugin* (*fptr)(void);
			*(void**)(&fptr) = func;
			EnginePlugin* plugin = (*fptr)();
			if (plugin == nullptr) {
				printf("Failed to create plugin\n");
				if (dlclose(handle) != 0) cerr << "Failed to free library! " << dlerror() << endl;
				continue;
			}
			mPlugins.push_back(plugin);
			mPluginModules.push_back(handle);
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
			fprintf(stderr, "Failed to unload plugin module\n");
	#else
	for (const auto& m : mPluginModules)
		if (dlclose(m) != 0)
			fprintf(stderr, "Failed to unload plugin library\n");
	#endif
}
