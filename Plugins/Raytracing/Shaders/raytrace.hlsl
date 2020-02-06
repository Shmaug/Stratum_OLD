#pragma kernel Raytrace

#pragma multi_compile ACCUMULATE

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
	float4x4 NodeToWorld;
	float4x4 WorldToNode;
	uint RootIndex;
	uint MaterialIndex;
	uint pad[2];
};

struct Ray {
	float3 Origin;
	float TMin;
	float3 Direction;
	float TMax;
	float3 InvDirection;
};

#include "disney.hlsli"

#define CMJ_DIM 16

[[vk::binding(0, 0)]] RWTexture2D<float4> OutputTexture : register(u0);
[[vk::binding(1, 0)]] RWTexture2D<float4> PreviousTexture : register(u1);
[[vk::binding(2, 0)]] StructuredBuffer<BvhNode> SceneBvh : register(t0);
[[vk::binding(3, 0)]] StructuredBuffer<LeafNode> LeafNodes : register(t1);
[[vk::binding(4, 0)]] ByteAddressBuffer Vertices : register(t2);
[[vk::binding(5, 0)]] ByteAddressBuffer Triangles : register(t3);
[[vk::binding(6, 0)]] StructuredBuffer<uint4> Lights : register(t4);
[[vk::binding(7, 0)]] StructuredBuffer<DisneyMaterial> Materials : register(t5);
[[vk::binding(8, 0)]] Texture2D<float4> NoiseTex : register(t6);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4x4 InvViewProj;
	float3 CameraPosition;
	float2 Resolution;
	float Near;
	float Far;
	uint VertexStride;
	uint IndexStride;
	uint BvhRoot;
	uint LightCount;
	uint FrameIndex;
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
	lray.Direction = mul(float4(ray.Direction, 0), leaf.NodeToWorld).xyz;
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

void LoadVertex(uint prim, float2 bary, out float3 vertex, out float3 normal, out float4 tangent, out float2 uv) {
	uint3 addr = VertexStride * Triangles.Load3(3 * IndexStride * prim);
	float3 v0 = asfloat(Vertices.Load3(addr.x));
	float3 v1 = asfloat(Vertices.Load3(addr.y));
	float3 v2 = asfloat(Vertices.Load3(addr.z));
	addr += 12;
	float3 n0 = asfloat(Vertices.Load3(addr.x));
	float3 n1 = asfloat(Vertices.Load3(addr.y));
	float3 n2 = asfloat(Vertices.Load3(addr.z));
	addr += 12;
	float4 t0 = asfloat(Vertices.Load4(addr.x));
	float4 t1 = asfloat(Vertices.Load4(addr.y));
	float4 t2 = asfloat(Vertices.Load4(addr.z));
	addr += 16;
	float2 uv0 = asfloat(Vertices.Load2(addr.x));
	float2 uv1 = asfloat(Vertices.Load2(addr.y));
	float2 uv2 = asfloat(Vertices.Load2(addr.z));

	vertex  = v0 + (v1 - v0) * bary.x + (v2 - v0) * bary.y;
	normal  = n0 + (n1 - n0) * bary.x + (n2 - n0) * bary.y;
	tangent = t0 + (t1 - t0) * bary.x + (t2 - t0) * bary.y;
	uv      = uv0 + (uv1 - uv0) * bary.x + (uv2 - uv0) * bary.y;
}
float3 LoadVertex(uint prim, float2 bary) {
	uint3 addr = VertexStride * Triangles.Load3(3 * IndexStride * prim);
	float3 v0 = asfloat(Vertices.Load3(addr.x));
	float3 v1 = asfloat(Vertices.Load3(addr.y));
	float3 v2 = asfloat(Vertices.Load3(addr.z));
	return v0 + (v1 - v0) * bary.x + (v2 - v0) * bary.y;
}

float3 BRDF(float2 sample, float3 worldPos, float3 normal, DisneyMaterial material, inout Ray ray) {
	float4 noise = NoiseTex.Load(uint3(asuint(sample), 0));

	float3 bary = noise.yzw / dot(noise.yzw, 1);

	uint4 lightIndex = Lights[(uint)(noise.r * (LightCount - 1) + .5)];
	float3 lightPoint = LoadVertex(lightIndex.x, bary.xy);
	lightPoint = mul(LeafNodes[lightIndex.z].NodeToWorld, float4(lightPoint, 1)).xyz;
	float3 light = Materials[lightIndex.y].Emission;
	
	Ray lightRay;
	lightRay.Origin = worldPos;
	lightRay.Direction = lightPoint - worldPos;
	float dist2 = dot(lightRay.Direction, lightRay.Direction);
	float dist = sqrt(dist2);
	lightRay.Direction /= dist;
	lightRay.InvDirection = 1.0 / lightRay.Direction;
	lightRay.TMin = .002;
	lightRay.TMax = dist - .002;

	float2 hb;
	float ht;
	uint hp, ho;
	light *= 1 - IntersectScene(lightRay, true, PASS_RAYTRACE, ht, hb, hp, ho);

	light *= saturate(dot(lightRay.Direction, normal)) / (1 + dist2);
	
	float pdf;
	ray.Origin = worldPos + normal * .001;
	return material.Emission + light * Disney_Sample(material, -ray.Direction, sample, ray.Direction, pdf);
}

float3 SampleBRDF(inout Ray ray, float2 sample, out float ndotl) {
	float t;
	float2 bary;
	uint prim, object;
	if (IntersectScene(ray, false, PASS_RAYTRACE, t, bary, prim, object)) {
		float3 vertex;
		float3 normal;
		float4 tangent;
		float2 uv;
		LoadVertex(prim, bary, vertex, normal, tangent, uv);

		LeafNode leaf = LeafNodes[object];
		vertex = mul(leaf.NodeToWorld, float4(vertex, 1)).xyz;
		normal = normalize(mul(float4(normal, 0), leaf.WorldToNode).xyz);
		tangent = mul(tangent, leaf.WorldToNode);
		DisneyMaterial material = Materials[leaf.MaterialIndex];

		float3 brdf = BRDF(sample, vertex, normal, material, ray);
		ndotl = saturate(dot(normal, ray.Direction));
		return brdf;
	}
	ndotl = 0;
	return 0;
}


uint permute(uint i, uint l, uint p) {
	uint w = l - 1;
	w |= w >> 1;
	w |= w >> 2;
	w |= w >> 4;
	w |= w >> 8;
	w |= w >> 16;

	do {
		i ^= p;
		i *= 0xe170893d;
		i ^= p >> 16;
		i ^= (i & w) >> 4;
		i ^= p >> 8;
		i *= 0x0929eb3f;
		i ^= p >> 23;
		i ^= (i & w) >> 1;
		i *= 1 | p >> 27;
		i *= 0x6935fa69;
		i ^= (i & w) >> 11;
		i *= 0x74dcb303;
		i ^= (i & w) >> 2;
		i *= 0x9e501cc3;
		i ^= (i & w) >> 2;
		i *= 0xc860a3df;
		i &= w;
		i ^= i >> 5;
	} while (i >= l);
	return (i + p) % l;
}
float randfloat(uint i, uint p) {
	i ^= p;
	i ^= i >> 17;
	i ^= i >> 10;
	i *= 0xb36534e5;
	i ^= i >> 12;
	i ^= i >> 21;
	i *= 0x93fc4795;
	i ^= 0xdf6e307f;
	i ^= i >> 17;
	i *= 1 | p >> 18;
	return i * (1.0 / 4294967808.0f);
}
float2 cmj(int s, int n, int p) {
	int sx = permute(s % n, n, p * 0xa511e9b3);
	int sy = permute(s / n, n, p * 0x63d83595);
	float jx = randfloat(s, p * 0xa399d265);
	float jy = randfloat(s, p * 0x711ad6a5);

	return float2((s % n + (sy + jx) / n) / n, (s / n + (sx + jy) / n) / n);
}

[numthreads(8, 8, 1)]
void Raytrace(uint3 index : SV_DispatchThreadID) {
	float4 unprojected = mul(InvViewProj, float4(index.xy * 2 / Resolution - 1, 0, 1));
	Ray ray;
	ray.Origin = CameraPosition;
	ray.Direction = normalize(unprojected.xyz / unprojected.w);
	ray.InvDirection = 1.0 / ray.Direction;
	ray.TMin = Near;
	ray.TMax = Far;

	uint dimension = 1;
	uint rnd = NoiseTex.Load(uint3(index.xy, 0));
	uint scramble = rnd * 0x1fe3434f * ((FrameIndex + 133 * rnd) / (CMJ_DIM * CMJ_DIM));
	int idx = permute(FrameIndex % (CMJ_DIM % CMJ_DIM), CMJ_DIM * CMJ_DIM, 0xa399d265 * dimension * scramble);
	float2 sample = cmj(idx, 16, dimension * scramble);

	float ndotl;
	float3 brdf1 = SampleBRDF(ray, sample, ndotl);

	float3 eval = 2 * PI * brdf1;

	#ifdef ACCUMULATE
	float3 last = PreviousTexture[index.xy].rgb;
	eval = FrameIndex == 0 ? eval : last + (eval - last) * 4 / (FrameIndex + 1);
	#endif
	OutputTexture[index.xy] = float4(eval, 1);
}
