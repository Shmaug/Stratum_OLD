#include <Scene/Bvh2.hpp>

#include <Scene/Renderer.hpp>
#include <Scene/Camera.hpp>

using namespace std;

Bvh2::Bvh2(uint32_t leafSize) : mLeafSize(leafSize) {}
Bvh2::~Bvh2() {}

void Bvh2::Build(Object** objects, uint32_t objectCount, uint32_t mask) {
    mPrimitives.clear();
    mNodes.clear();

    for (uint32_t i = 0; i < objectCount; i++)
        if (objects[i]->LayerMask() & mask)
            mPrimitives.push_back({ objects[i], objects[i]->Bounds(), objects[i]->Bounds().Center() });

    struct BuildTask {
        uint32_t mParentOffset;
        uint32_t mStart;
        uint32_t mEnd;
    };

    BuildTask todo[1024];
    uint32_t stackptr = 0;
    const uint32_t untouched = 0xffffffff;
    const uint32_t touchedTwice = 0xfffffffd;

    uint32_t nNodes = 0;
    uint32_t nLeafs = 0;

    Node node;

    todo[stackptr].mStart = 0;
    todo[stackptr].mEnd = mPrimitives.size();
    todo[stackptr].mParentOffset = 0xfffffffc;
    stackptr++;

    while (stackptr > 0) {
        // Pop the next item off of the stack
        BuildTask& bnode(todo[--stackptr]);
        uint32_t start = bnode.mStart;
        uint32_t end = bnode.mEnd;
        uint32_t nPrims = end - start;

        nNodes++;
        node.mStartIndex = start;
        node.mCount = nPrims;
        node.mRightOffset = untouched;

        // Calculate the bounding box for this node
        AABB bb(mPrimitives[start].mBounds);
        AABB bc(mPrimitives[start].mCenter, mPrimitives[start].mCenter);
        for (uint32_t p = start + 1; p < end; ++p) {
            bb.Encapsulate(mPrimitives[p].mBounds);
            bc.Encapsulate(mPrimitives[p].mCenter);
        }
        node.mBounds = bb;

        // If the number of primitives at this point is less than the leaf
        // size, then this will become a leaf. (Signified by rightOffset == 0)
        if (nPrims <= 1) {
            node.mRightOffset = 0;
            nLeafs++;
        }

        mNodes.push_back(node);

        // Child touches parent...
        // Special case: Don't do this for the root.
        if (bnode.mParentOffset != 0xfffffffc) {
            mNodes[bnode.mParentOffset].mRightOffset--;

            // When this is the second touch, this is the right child.
            // The right child sets up the offset for the flat tree.
            if (mNodes[bnode.mParentOffset].mRightOffset == touchedTwice) {
                mNodes[bnode.mParentOffset].mRightOffset = nNodes - 1 - bnode.mParentOffset;
            }
        }

        // If this is a leaf, no need to subdivide.
        if (node.mRightOffset == 0) continue;

        // Set the split dimensions
        uint32_t split_dim = 0;
        float3 ext = bc.Extents();
        if (ext.y > ext.x) {
            split_dim = 1;
            if (ext.z > ext.y) split_dim = 2;
        } else
            if (ext.z > ext.x) split_dim = 2;

        // Split on the center of the longest axis
        float split_coord = .5f * (bc.mMin[split_dim] + bc.mMax[split_dim]);

        // Partition the list of objects on this split
        uint32_t mid = start;
        for (uint32_t i = start; i < end; ++i)
            if (mPrimitives[i].mCenter[split_dim] < split_coord) {
                swap(mPrimitives[i], mPrimitives[mid]);
                mid++;
            }

        // If we get a bad split, just choose the center...
        if (mid == start || mid == end)
            mid = start + (end - start) / 2;

        todo[stackptr].mStart = mid;
        todo[stackptr].mEnd = end;
        todo[stackptr].mParentOffset = nNodes - 1;
        stackptr++;
        todo[stackptr].mStart = start;
        todo[stackptr].mEnd = mid;
        todo[stackptr].mParentOffset = nNodes - 1;
        stackptr++;
    }
}

void Bvh2::FrustumCheck(Camera* camera, vector<Object*>& objects, uint32_t mask) {
    if (mNodes.size() == 0) return;

    bool bbhits[2];
	
    uint32_t todo[1024];
    int32_t stackptr = 0;

    todo[stackptr] = 0;

    while (stackptr >= 0) {
        int ni = todo[stackptr];
        stackptr--;
        const Node& node(mNodes[ni]);

        if (node.mRightOffset == 0) { // leaf node
            for (uint32_t o = 0; o < node.mCount; ++o)
                if ((mPrimitives[node.mStartIndex + o].mObject->LayerMask() & mask) && camera->IntersectFrustum(mPrimitives[node.mStartIndex + o].mBounds))
                    objects.push_back(mPrimitives[node.mStartIndex + o].mObject);
        } else {
            bbhits[0] = camera->IntersectFrustum(mNodes[ni + 1].mBounds);
            bbhits[1] = camera->IntersectFrustum(mNodes[ni + node.mRightOffset].mBounds);
            if (bbhits[0] && bbhits[1]) {
                todo[++stackptr] = ni + 1;
                todo[++stackptr] = ni + node.mRightOffset;
            } else if (bbhits[0]) todo[++stackptr] = ni + 1;
            else if (bbhits[1]) todo[++stackptr] = ni + node.mRightOffset;
        }
    }
}
Object* Bvh2::Intersect(const Ray& ray, float* t, bool any, uint32_t mask) {
    if (mNodes.size() == 0) return nullptr;

    float hitT = 1e20f;
    Object* hitObject = nullptr;

    float bbhits[2];
    int32_t closer, other;

    struct WorkItem {
        uint32_t mIndex;
        float mTmin;
    };
    WorkItem todo[1024];
    int32_t stackptr = 0;

    todo[stackptr].mIndex = 0;
    todo[stackptr].mTmin = -1e20f;

    while (stackptr >= 0) {
        int ni = todo[stackptr].mIndex;
        float near = todo[stackptr].mTmin;
        stackptr--;
        const Node& node(mNodes[ni]);

        if (near > hitT) continue;

        if (node.mRightOffset == 0) { // leaf node
            for (uint32_t o = 0; o < node.mCount; ++o) {
                if ((mPrimitives[node.mStartIndex + o].mObject->LayerMask() & mask) == 0) continue;
                
                float curT;
                if (!mPrimitives[node.mStartIndex + o].mObject->Intersect(ray, &curT, any)) continue;

                if (any) {
                    if (t) *t = curT;
                    return mPrimitives[node.mStartIndex + o].mObject;
                }
                if (curT < hitT) {
                    hitT = curT;
                    hitObject = mPrimitives[node.mStartIndex + o].mObject;
                }
            }
        } else {
            bbhits[0] = ray.Intersect(mNodes[ni + 1].mBounds).x;
            bbhits[1] = ray.Intersect(mNodes[ni + node.mRightOffset].mBounds).x;
            if (bbhits[0] >= 0 && bbhits[1] >= 0) {
                closer = ni + 1;
                other = ni + node.mRightOffset;
                if (bbhits[1] < bbhits[0]) {
                    swap(bbhits[0], bbhits[1]);
                    swap(closer, other);
                }
                todo[++stackptr] = { (uint32_t)other, bbhits[1] };
                todo[++stackptr] = { (uint32_t)closer, bbhits[0] };
            } else if (bbhits[0] >= 0) todo[++stackptr] = { (uint32_t)ni + 1, bbhits[0] };
            else if (bbhits[1] >= 0) todo[++stackptr]   = { (uint32_t)ni + node.mRightOffset, bbhits[1] };
        }
    }

    if (hitObject && t) *t = hitT;
    return hitObject;
}

void Bvh2::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera, Scene* scene) {
    return;
    
    const float3 colors[] {
        float3(1,0,0),
        float3(0,1,0),
        float3(0,0,1),

        float3(1,.5f,.5f),
        float3(.5f,1,.5f),
        float3(.5f,.5f,1),

        float3(.5f,1,1),
        float3(1,.5f,1),
        float3(1,1,.5f),

        float3(1,1,1),
    };
    for (uint32_t i = 0; i < mNodes.size(); i++)
		scene->Gizmos()->DrawWireCube(mNodes[i].mBounds.Center(), mNodes[i].mBounds.Extents(), quaternion(), float4(colors[i % 10], .5f));
}