#pragma kernel Raytrace

#pragma static_sampler Sampler maxAnisotropy=0 maxLod=0
#pragma multi_compile ACCUMULATE

#define PASS_RAYTRACE (1u << 23)
#define EPSILON 0.001
#define MAX_RADIANCE 3

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

#include <include/disney.hlsli>

[[vk::binding(0, 0)]] RWTexture2D<float4> OutputPrimary		: register(u0);
[[vk::binding(1, 0)]] RWTexture2D<float4> OutputMeta		: register(u1);
[[vk::binding(2, 0)]] Texture2D<float4> PreviousPrimary		: register(t0);
[[vk::binding(3, 0)]] Texture2D<float4> PreviousMeta		: register(t1);
[[vk::binding(4, 0)]] StructuredBuffer<BvhNode> SceneBvh	: register(t2);
[[vk::binding(5, 0)]] StructuredBuffer<LeafNode> LeafNodes	: register(t3);
[[vk::binding(6, 0)]] ByteAddressBuffer Vertices			: register(t4);
[[vk::binding(7, 0)]] ByteAddressBuffer Triangles			: register(t5);
[[vk::binding(8, 0)]] StructuredBuffer<uint4> Lights		: register(t6);
[[vk::binding(9, 0)]] StructuredBuffer<DisneyMaterial> Materials : register(t7);
[[vk::binding(10, 0)]] Texture2D<float4> NoiseTex			: register(t8);
[[vk::binding(11, 0)]] SamplerState Sampler					: register(s0);

[[vk::push_constant]] cbuffer PushConstants : register(b2) {
	float4x4 LastViewProjection;
	float4x4 InvViewProj;
	float3 CameraPosition;
	float3 LastCameraPosition;
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

float3 AreaLight_Sample(uint light, float2 sample, float3 p, float3 n, out float3 wo, out float pdf) {
	uint4 li = Lights[light];
	uint3 addr = VertexStride * Triangles.Load3(3 * IndexStride * li.x);
	float3 v0 = mul(LeafNodes[li.z].NodeToWorld, float4(asfloat(Vertices.Load3(addr.x)), 1)).xyz;
	float3 v1 = mul(LeafNodes[li.z].NodeToWorld, float4(asfloat(Vertices.Load3(addr.y)), 1)).xyz;
	float3 v2 = mul(LeafNodes[li.z].NodeToWorld, float4(asfloat(Vertices.Load3(addr.z)), 1)).xyz;

	float2 bary = float2(1 - sample.y, sample.y) * sqrt(sample.x);
	float3 lp = v0 + (v1 - v0) * bary.x + (v2 - v0) * bary.y;

	wo = lp - p;
	float nv = dot(n, normalize(wo));
	if (nv <= 0) { pdf = 0; return 0; }

	float3 ke = Materials[li.y].Emission;

	float d2 = dot(wo, wo);
	float d = nv * .5 * cross(v1 - v0, v2 - v0);
	pdf = d > 0 ? d2 / d : 0;
	return d2 > 0 ? ke * nv / d2 : 0;
}

void LoadVertex(uint prim, float2 bary, out float3 normal, out float4 tangent, out float2 uv) {
	uint3 addr = VertexStride * Triangles.Load3(3 * IndexStride * prim);
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

	normal  = n0 + (n1 - n0) * bary.x + (n2 - n0) * bary.y;
	tangent = t0 + (t1 - t0) * bary.x + (t2 - t0) * bary.y;
	uv      = uv0 + (uv1 - uv0) * bary.x + (uv2 - uv0) * bary.y;
}

float3 ShadeSurface(inout Ray ray, inout RandomSampler rng, inout float3 throughput, inout float pdf, out float t, out float3 normal) {
	float2 bary;
	uint prim, object;
	if (IntersectScene(ray, false, PASS_RAYTRACE, t, bary, prim, object)) {
		float4 tangent;
		float2 uv;
		float area;
		LoadVertex(prim, bary, normal, tangent, uv);

		float3 worldPos = ray.Origin + ray.Direction * t * (1 - EPSILON);

		LeafNode leaf = LeafNodes[object];
		normal = normalize(mul(float4(normal, 0), leaf.WorldToNode).xyz);
		float3 tan = mul(tangent, leaf.WorldToNode).xyz;
		DisneyMaterial material = Materials[leaf.MaterialIndex];
		
		tan = normalize(tan - normal * dot(normal, tan));
		if (dot(tan, tan) < .001) tan = GetOrthoVector(normal);
		float3 bitan = cross(normal, tan.xyz);
		float3x3 tangentToWorld = float3x3(
			tan.x, normal.x, bitan.x,
			tan.y, normal.y, bitan.y,
			tan.z, normal.z, bitan.z);

		float3 wi = -ray.Direction;

		// Sample BRDF
		float3 wi_t = mul(wi, tangentToWorld);
		float3 wo_t;
		float brdfpdf;
		float3 brdf = Disney_Sample(material, wi_t, SampleRNG(rng), wo_t, brdfpdf);

		if (any(material.Emission)) {
			uint3 addr = VertexStride * Triangles.Load3(3 * IndexStride * prim);
			float3 v0 = mul(leaf.NodeToWorld, float4(asfloat(Vertices.Load3(addr.x)), 1)).xyz;
			float3 v1 = mul(leaf.NodeToWorld, float4(asfloat(Vertices.Load3(addr.y)), 1)).xyz;
			float3 v2 = mul(leaf.NodeToWorld, float4(asfloat(Vertices.Load3(addr.z)), 1)).xyz;

			float denom = abs(dot(normal, wi)) * .5 * cross(v1 - v0, v2 - v0);
			float bxdflightpdf = denom > 0 ? (t*t / (denom * LightCount)) : 0.f;
			float weight = BalanceHeuristic(1, pdf, 1, bxdflightpdf);
			
			float3 radiance = throughput * material.Emission * weight;
			throughput = 0;
			return radiance;
		}

		float3 radiance = 0;

		// Sample light
		if (LightCount) {
			uint lightIndex = min((uint)(SampleRNG(rng).x * LightCount), LightCount - 1);
			float lightpdf;
			float3 lwo;
			float3 le = AreaLight_Sample(lightIndex, SampleRNG(rng), worldPos, normal, lwo, lightpdf);

			Ray lightRay;
			lightRay.Origin = worldPos;
			lightRay.Direction = lwo;
			lightRay.InvDirection = 1.0 / lightRay.Direction;
			lightRay.TMin = EPSILON;
			lightRay.TMax = 1 - EPSILON;
			float3 ltb;
			uint2 lid;
			if (!IntersectScene(lightRay, true, PASS_RAYTRACE, ltb.x, ltb.yz, lid.x, lid.y)) {
				float weight = BalanceHeuristic(1, pdf, 1, lightpdf);
				lwo = normalize(lwo);
				float nwo = abs(dot(lwo, normal));
				radiance = le * nwo * Disney_Evaluate(material, wi, lwo) * throughput * weight;
			}
		}
		
		// Next bounce
		ray.Origin = worldPos;
		ray.Direction = normalize(mul(tangentToWorld, wo_t));
		ray.InvDirection = 1 / ray.Direction;

		throughput *= abs(dot(normal, ray.Direction)) * brdf / brdfpdf;
		pdf = brdfpdf;

		return clamp(radiance, 0, MAX_RADIANCE);
	}
	throughput = 0;
	return 0;
}

[numthreads(8, 8, 1)]
void Raytrace(uint3 index : SV_DispatchThreadID) {
	float4 unprojected = mul(InvViewProj, float4(index.xy * 2 / Resolution - 1, 0, 1));
	Ray ray;
	ray.Origin = CameraPosition;
	ray.Direction = normalize(unprojected.xyz / unprojected.w);
	ray.InvDirection = 1 / ray.Direction;
	ray.TMin = Near;
	ray.TMax = Far;

	float3 rd = ray.Direction;

	uint rnd = asuint(NoiseTex.Load(uint3(index.xy % 256, 0)).r);
	RandomSampler rng;
	rng.index = FrameIndex % (CMJ_DIM * CMJ_DIM);
	rng.dimension = 1;
	rng.scramble = rnd * 0x1fe3434f * ((FrameIndex + 133 * rnd) / (CMJ_DIM * CMJ_DIM));

	float4 nt;
	float3 throughput = 1;
	float pdf = 1;
	float pdfaccum = 0;
	float4 primary = float4(ShadeSurface(ray, rng, throughput, pdf, nt.w, nt.xyz), 1);

	#ifdef ACCUMULATE
	float3 worldPos = CameraPosition - LastCameraPosition + rd * nt.w;
	float4 lc = mul(LastViewProjection, float4(worldPos, 1));
	float2 lastUV = .5 + .5 * lc.xy / lc.w + .5 / Resolution;
	float4 lastMeta = PreviousMeta.SampleLevel(Sampler, lastUV, 0);
	if (lastUV.x > 0 && lastUV.y > 0 && lastUV.x < 1 && lastUV.y < 1 && abs(length(worldPos) - lastMeta.w) < .0001 && dot(lastMeta.xyz, nt.xyz) > .999)
		primary = lerp(primary, PreviousPrimary.SampleLevel(Sampler, lastUV, 0), .95);
	#endif

	OutputPrimary[index.xy] = primary;
	OutputMeta[index.xy] = nt;
}
