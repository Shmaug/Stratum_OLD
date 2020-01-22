#pragma once

#include <Scene/Object.hpp>

class Bvh {
public:
	virtual void Build(Object** objects, uint32_t count, uint32_t mask = ~0) = 0;
	virtual Object* Intersect(const Ray& ray, float* t = nullptr, bool any = false, uint32_t mask = ~0) = 0;
	virtual void FrustumCheck(Camera* camera, std::vector<Object*>& objects, uint32_t mask = ~0) = 0;
	virtual void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera, Scene* scene) = 0;
};