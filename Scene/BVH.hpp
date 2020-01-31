#pragma once

#include <Util/Util.hpp>


class BVH2 {
public:
	struct Node {
		float3 mMin;
		float3 mMax;
		BVHNode* mChildren[2];
	};

	ENGINE_EXPORT BVH2();

private:
	Node* mRootNode;
};