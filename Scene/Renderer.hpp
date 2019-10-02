#pragma once 

#include <Scene/Object.hpp>
#include <Util/Util.hpp>

class Renderer : public Object {
public:
	inline Renderer(const std::string& name) : Object(name) {}

	inline virtual uint32_t RenderQueue() { return 1000; }
	virtual bool Visible() = 0;
	virtual void Draw(const FrameTime& frameTime, Camera* camera, CommandBuffer* commandBuffer, uint32_t backBufferIndex, Material* materialOverride) = 0;
};