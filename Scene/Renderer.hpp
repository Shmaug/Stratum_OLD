#pragma once 

#include <Scene/Object.hpp>
#include <Util/Util.hpp>

class Renderer : public virtual Object {
public:
	inline virtual uint32_t RenderQueue() { return 1000; }
	virtual bool Visible() = 0;
	virtual bool CastShadows() { return false; };
	inline virtual void PreRender(Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, Material* materialOverride) {};
	virtual void Draw(Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, Material* materialOverride) = 0;
};