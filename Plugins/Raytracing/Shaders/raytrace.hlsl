#pragma kernel Raytrace

#define PASS_RAYTRACE (1u << 23)

struct BvhNode {
	float3 Min;
	uint StartIndex;
	float3 Max;
	uint PrimitiveCount;
	uint RightOffset; // 1st child is at node[index + 1], 2nd child is at node[index + mRightOffset]
	uint pad[3];
};
struct LeafNode {
	float4x4 WorldToNode;
	uint RootIndex;
	uint pad[3];
};
struct Ray {
	float3 Origin;
	float TMin;
	float3 Direction;
	float TMax;
	float3 InvDirection;
};

[[vk::binding(0, 0)]] RWTexture2D<float4> OutputTexture : register(u0);
[[vk::binding(1, 0)]] StructuredBuffer<BvhNode> SceneBvh : register(t0);
[[vk::binding(2, 0)]] StructuredBuffer<LeafNode> LeafNodes : register(t1);
[[vk::binding(3, 0)]] ByteAddressBuffer Vertices : register(t2);
[[vk::binding(4, 0)]] ByteAddressBuffer Triangles : register(t3);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4x4 InvViewProj;
	float3 CameraPosition;
	float2 Resolution;
	float Near;
	float Far;
	uint VertexStride;
	uint IndexStride;
	uint BvhRoot;

	uint StereoEye;
}

bool RayTriangle(Ray ray, float3 v0, float3 v1, float3 v2, out float t, out float2 bary) {
	float3 v1v0 = v1 - v0;
	float3 v2v0 = v2 - v0;
	float3 rov0 = ray.Origin - v0;

	float3 n = cross(v1v0, v2v0);
	float3 q = cross(rov0, ray.Direction);
	float d = 1 / dot(ray.Direction, n);
	bary.x = d * dot(-q, v2v0);
	bary.y = d * dot( q, v1v0);
	t = d * dot(-n, rov0);

	return bary.x >= 0 && bary.y >= 0 && t < ray.TMax && t > ray.TMin && (bary.x + bary.y) <= 1;
}
bool RayBox(Ray ray, float3 mn, float3 mx, out float2 t) {
	float3 t0 = (mn - ray.Origin) * ray.InvDirection;
	float3 t1 = (mx - ray.Origin) * ray.InvDirection;
	float3 tmin = min(t0, t1);
	float3 tmax = max(t0, t1);
	t.x = max(max(tmin.x, tmin.y), tmin.z);
	t.y = min(min(tmax.x, tmax.y), tmax.z);
	return t.x < t.y && t.y >= ray.TMin && t.x <= ray.TMax;
}

int IntersectSceneLeaf(Ray ray, bool any, uint nodeIndex, out float t, out float2 bary) {
	LeafNode leaf = LeafNodes[nodeIndex];

	Ray lray = ray;
	lray.Origin = mul(leaf.WorldToNode, float4(ray.Origin, 1)).xyz;
	lray.Direction = mul((float3x3)leaf.WorldToNode, ray.Direction);
	lray.InvDirection = float3(1.0) / lray.Direction;

	t = 1.#INF;
	bary = 0;
	int hitIndex = -1;

	uint todo[64];
	int stackptr = 0;

	todo[stackptr] = leaf.RootIndex;
	
	while (stackptr >= 0) {
		uint ni = todo[stackptr];
		stackptr--;

		BvhNode node = SceneBvh[ni];

		if (node.RightOffset == 0) {
			/*
			float2 ct;
			bool h = RayBox(lray, node.Min, node.Max, ct);
			if (h && ct.x < t) {
				t = ct.x;
				hitIndex = ni;
			}
			/*/
			for (uint o = 0; o < node.PrimitiveCount; ++o) {
				uint3 addr = VertexStride * Triangles.Load3(3 * IndexStride * (node.StartIndex + o));
				float3 v0 = asfloat(Vertices.Load3(addr.x));
				float3 v1 = asfloat(Vertices.Load3(addr.y));
				float3 v2 = asfloat(Vertices.Load3(addr.z));

				float ct;
				float2 cb;
				bool h = RayTriangle(lray, v0, v1, v2, ct, cb);

				if (h && ct < t) {
					t = ct;
					bary = cb;
					hitIndex = node.StartIndex + o;
					if (any) return hitIndex;
				}
			}
			//*/
		} else {
			uint n0 = ni + 1;
			uint n1 = ni + node.RightOffset;

			float2 t0;
			float2 t1;
			bool h0 = RayBox(lray, SceneBvh[n0].Min, SceneBvh[n0].Max, t0);
			bool h1 = RayBox(lray, SceneBvh[n1].Min, SceneBvh[n1].Max, t1);

			if (h0) todo[++stackptr] = n0;
			if (h1) todo[++stackptr] = n1;
		}
	}

	return hitIndex;
}
bool IntersectScene(Ray ray, bool any, uint mask, out float t, out float2 bary, out uint primitiveId, out uint objectId) {
	t = 1.#INF;
	bool hit = false;

	uint todo[32];
	int stackptr = 0;

	todo[stackptr] = BvhRoot;

	while (stackptr >= 0) {
		uint ni = todo[stackptr];
		stackptr--;

		BvhNode node = SceneBvh[ni];

		if (node.RightOffset == 0) {
			float ct;
			float2 cb;
			int prim = IntersectSceneLeaf(ray, any, node.StartIndex, ct, cb);

			if (prim >= 0 && ct < t) {
				t = ct;
				bary = cb;
				primitiveId = prim;
				objectId = node.StartIndex;
				hit = true;
				if (any) return true;
			}
		} else  {
			uint n0 = ni + 1;
			uint n1 = ni + node.RightOffset;

			float2 t0;
			float2 t1;
			bool h0 = RayBox(ray, SceneBvh[n0].Min, SceneBvh[n0].Max, t0);
			bool h1 = RayBox(ray, SceneBvh[n1].Min, SceneBvh[n1].Max, t1);

			if (h0) todo[++stackptr] = n0;
			if (h1) todo[++stackptr] = n1;
		}
	}
	return hit;
}

float3 Unproject(float2 uv) {
	float4 unprojected = mul(InvViewProj, float4(uv * 2 - 1, 0, 1));
	return normalize(unprojected.xyz / unprojected.w);
}

float3 hash31(float p) {
	float3 p3 = frac(float3(p) * float3(.1031, .1030, .0973));
	p3 += dot(p3, p3.yzx + 33.33);
	return frac((p3.xxy + p3.yzz) * p3.zyx);
}

[numthreads(8, 8, 1)]
void Raytrace(uint3 index : SV_DispatchThreadID) {
	float3 color = .2;
	
	float t;
	float2 bary;

	Ray ray;
	ray.Origin = CameraPosition;
	ray.Direction = Unproject(index.xy / Resolution);
	ray.InvDirection = 1.0 / ray.Direction;
	ray.TMin = Near;
	ray.TMax = Far;

	uint prim, object;
	if (IntersectScene(ray, false, PASS_RAYTRACE, t, bary, prim, object)) {
		color = hash31(prim) * .05 + hash31(object) * .95;
	}

	OutputTexture[index.xy] = float4(color, 1);
}
