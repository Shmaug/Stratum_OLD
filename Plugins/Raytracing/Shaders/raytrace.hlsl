#pragma kernel Raytrace

struct BvhNode {
	float3 Min;
	uint StartIndex;
	float3 Max;
	uint PrimitiveCount;
	uint RightOffset; // 1st child is at node[index + 1], 2nd child is at node[index + mRightOffset]
	uint Mask;
};
struct LeafNode {
	float4x4 WorldToNode;
	uint RootIndex;
};
struct Ray {
	float3 Origin;
	float3 Direction;
	float TMin;
	float TMax;
};

[[vk::binding(0, 0)]] RWTexture2D<float4> OutputTexture : register(u0);
[[vk::binding(1, 0)]] RWStructuredBuffer<BvhNode> SceneBvh : register(u1);
[[vk::binding(2, 0)]] RWStructuredBuffer<LeafNode> LeafNodes : register(u2);
[[vk::binding(3, 0)]] RWByteAddressBuffer Vertices : register(u3);
[[vk::binding(4, 0)]] RWStructuredBuffer<uint3> Triangles : register(u4);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float2 Resolution;
	float4x4 InvVP;
	float Near;
	float Far;
	float3 CameraPosition;
	uint VertexStride;
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

	return bary.x >= 0 && bary.y >= 0 && (bary.x + bary.y) <= 1;
}
bool RayBox(Ray ray, float3 mn, float3 mx, out float t) {
	float3 m = 1.f / ray.Direction;
	float3 n = m * (ray.Origin - (mx + mn) * .5f);
	float3 k = abs(m) * (mx - mn) * .5f;
	float3 t1 = -n - k;
	float3 t2 = -n + k;
	t = float2(max(max(t1.x, t1.y), t1.z), min(min(t2.x, t2.y), t2.z));
	return t2 > t1;
}

int IntersectSceneLeaf(Ray ray, bool any, uint nodeIndex, out float t, out float2 bary) {
	LeafNode node = LeafNodes[nodeIndex];

	ray.Origin = mul(node.WorldToNode, float4(ray.Origin, 1)).xyz;
	ray.Direction = mul((float3x3)node.WorldToNode, ray.Direction).xyz;

	t = 1.#INF;
	bary = 0;
	int hitIndex = -1;

	uint todo[1024];
	int stackptr = 0;

	todo[stackptr] = nodeIndex;

	while (stackptr >= 0) {
		uint ni = todo[stackptr];
		stackptr--;
		BvhNode node = SceneBvh[ni];

		if (node.RightOffset == 0) { // leaf node
			for (uint o = 0; o < node.PrimitiveCount; ++o) {
				uint ti = node.StartIndex + o;
				uint i0 = Indices[(ti + 0) * 4];
				uint i1 = Indices[(ti + 1) * 4];
				uint i2 = Indices[(ti + 2) * 4];
				float3 v0 = Vertices.Load3(VertexStride * i0);
				float3 v1 = Vertices.Load3(VertexStride * i1);
				float3 v2 = Vertices.Load3(VertexStride * i2);

				float ct;
				float2 cb;
				bool h = RayTriangle(ray, v0, v1, v2, ct, cb);

				if (!h || ct < ray.TMin || ct > ray.TMax) continue;

				if (ct < t) {
					t = ct;
					bary = cb;
					hitIndex = ti;
					if (any) return hitIndex;
				}
			}
		} else {
			uint n0 = ni + 1;
			uint n1 = ni + node.RightOffset;

			float2 t0;
			float2 t1;
			bool h0 = RayBox(ray, SceneBvh[n0].Min, SceneBvh[n0].Max, t0);
			bool h1 = RayBox(ray, SceneBvh[n1].Min, SceneBvh[n1].Max, t1);

			h0 = h0 && t0.y > ray.TMin&& t0.x < ray.TMax;
			h1 = h1 && t1.y > ray.TMin&& t1.x < ray.TMax;

			if (h0 && h1) {
				todo[++stackptr] = t0.x < t1.x ? n0 : n1;
			} else if (h0) {
				todo[++stackptr] = ni + 1;
			} else if (h1) {
				todo[++stackptr] = ni + node.RightOffset;
			}
		}
	}
	return hitIndex;
}
int IntersectScene(Ray ray, bool any, uint mask, out float t, out float2 bary) {
	t = 1.#INF;
	bary = 0;
	int hitIndex = -1;

	uint todo[1024];
	int stackptr = 0;

	todo[stackptr] = BvhRoot;

	while (stackptr >= 0) {
		uint ni = todo[stackptr];
		stackptr--;
		BvhNode node = SceneBvh[ni];

		if ((node.Mask & mask) == 0) continue;

		if (node.RightOffset == 0) { // leaf node
			for (uint o = 0; o < node.PrimitiveCount; ++o) {
				float ct;
				float2 cb;
				int ci = IntersectSceneLeaf(ray, any, node.StartIndex + o, ct, cb);

				if (ci < 0 || ct < ray.TMin || ct > ray.TMax) continue;

				if (ct < t) {
					t = ct;
					bary = cb;
					hitIndex = ci;
					if (any) return hitIndex;
				}
			}
		} else {
			uint n0 = ni + 1;
			uint n1 = ni + node.RightOffset;

			float2 t0;
			float2 t1;
			bool h0 = RayBox(ray, SceneBvh[n0].Min, SceneBvh[n0].Max, t0);
			bool h1 = RayBox(ray, SceneBvh[n1].Min, SceneBvh[n1].Max, t1);

			h0 = h0 && t0.y > ray.TMin&& t0.x < ray.TMax;
			h1 = h1 && t1.y > ray.TMin&& t1.x < ray.TMax;

			if (h0 && h1) {
				todo[++stackptr] = t0.x < t1.x ? n0 : n1;
			} else if (h0) {
				todo[++stackptr] = ni + 1;
			} else if (h1) {
				todo[++stackptr] = ni + node.RightOffset;
			}
		}
	}
	return hitIndex;
}

float3 Unproject(float2 uv) {
	float4 unprojected = mul(InvVP, float4(uv * 2 - 1, 0, 1));
	return normalize(unprojected.xyz / unprojected.w);
}

[numthreads(8, 8, 1)]
void Raytrace(uint3 index : SV_DispatchThreadID) {

	float3 color = OutputTexture[index.xy].rgb;
	
	float t;
	float2 bary;

	Ray ray;
	ray.Origin = CameraPosition;
	ray.Direction = Unproject(index.xy / Resolution).xyz;
	ray.TMin = Near;
	ray.TMin = Far;
	int prim = IntersectScene(ray, false, ~0, t, bary);

	if (prim > 0) {
		color = float3(bary, t / Far);
	}

	OutputTexture[index.xy] = float4(color, 1);
}
