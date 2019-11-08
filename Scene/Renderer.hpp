#pragma once 

#include <Scene/Object.hpp>
#include <Util/Util.hpp>

class Renderer : public virtual Object {
public:
	inline virtual uint32_t RenderQueue() { return 1000; }
	virtual bool Visible() = 0;
	virtual bool CastShadows() { return false; };
	inline virtual void PreRender(CommandBuffer* commandBuffer, uint32_t backBufferIndex, Camera* camera, Material* materialOverride) {};
	virtual void Draw(CommandBuffer* commandBuffer, uint32_t backBufferIndex, Camera* camera, Material* materialOverride) = 0;
};