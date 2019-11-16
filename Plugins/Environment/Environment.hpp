#pragma once

#include <Content/Texture.hpp>
#include <Core/EnginePlugin.hpp>
#include <Scene/Scene.hpp>

class Environment : public EnginePlugin {
public:
	PLUGIN_EXPORT Environment();
	PLUGIN_EXPORT ~Environment();

	PLUGIN_EXPORT bool Init(Scene* scene) override;

	inline int Priority() override { return 1000; }

	inline float ReflectionMapStrength() { return mEnvironmentStrength; }
	inline Texture* ReflectionMap() { return mEnvironmentTexture; }
	
private:
	float mEnvironmentStrength;
	Texture* mEnvironmentTexture;

    Scene* mScene;
	Object* mSkybox;
};