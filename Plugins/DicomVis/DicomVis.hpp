#pragma once

#include <Core/EnginePlugin.hpp>

class DicomVis : public EnginePlugin {
public:
	PLUGIN_EXPORT DicomVis();
	PLUGIN_EXPORT ~DicomVis();

	PLUGIN_EXPORT bool Init(Scene* scene, DeviceManager* deviceManager) override;
	PLUGIN_EXPORT void Update(const FrameTime& frameTime) override;

private:
	Scene* mScene;
};