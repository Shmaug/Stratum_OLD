#pragma once

#include <thread>

#include <Core/EnginePlugin.hpp>

#include "TelnetClient.hpp"
#include <Scene/TextRenderer.hpp>

class Starmap : public EnginePlugin {
public:
	PLUGIN_EXPORT Starmap();
	PLUGIN_EXPORT ~Starmap();

	PLUGIN_EXPORT bool Init(Scene* scene, DeviceManager* deviceManager) override;

private:
	std::vector<std::shared_ptr<Object>> mObjects;
	Scene* mScene;
	bool mRunning;
	std::thread mLoadThread;
	TelnetClient* mTelnetClient;

	PLUGIN_EXPORT void RunTelnet();
};