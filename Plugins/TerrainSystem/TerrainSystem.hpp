#pragma once

#include <Core/EnginePlugin.hpp>

#include "TerrainRenderer.hpp"

class TerrainSystem : public EnginePlugin {
public:
	PLUGIN_EXPORT TerrainSystem();
	PLUGIN_EXPORT ~TerrainSystem();

	inline TerrainRenderer* Terrain() { return mTerrain; }

	PLUGIN_EXPORT bool Init(Scene* scene) override;
	PLUGIN_EXPORT void Update() override;
	PLUGIN_EXPORT void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) override;

	inline int Priority() override { return 1000; }

private:
	Scene* mScene;
	TerrainRenderer* mTerrain;
	std::vector<Object*> mObjects;
	Object* mSelected;
	MouseKeyboardInput* mInput;
};
