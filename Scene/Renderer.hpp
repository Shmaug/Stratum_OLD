#pragma once 

#include <Scene/Object.hpp>
#include <Scene/Scene.hpp>
#include <Util/Util.hpp>

class Renderer : public virtual Object {
public:
	inline virtual uint32_t RenderQueue() { return 1000; }
	virtual bool Visible() = 0;
	virtual bool CastShadows() { return false; };
	inline virtual void PreRender(CommandBuffer* commandBuffer, Camera* camera, Scene::PassType pass) {};
	virtual void Draw(CommandBuffer* commandBuffer, Camera* camera, Scene::PassType pass) = 0;
};