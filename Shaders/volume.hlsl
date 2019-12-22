#pragma vertex vsmain
#pragma fragment fsmain

#pragma multi_compile PLANE
#pragma multi_compile COLOR

struct CBData {
	float4x4 ObjectToView;
	float4x4 WorldToObject;
	float4x4 ViewToObject;
	float4x4 Projection;
};
struct RootBuffer {
	float3 CameraPosition;
	float  Projection43;
	float3 LightPosition;
	float  Projection33;
	float3 PlanePoint;
	float  Density;
	float3 PlaneNormal;
	float  LightDensity;
	float3 LightDirection;
	float  Exposure;
	float3 WorldScale;
	float  LightIntensity;
	float  LightAngle;
	float  LightAmbient;
	float  TresholdValue;
};

ConstantBuffer<RootBuffer> Root : register(b0);
ConstantBuffer<CBData> CB : register(b1);
#ifdef COLOR
Texture3D<float4> Volume : register(t0);
#else
Texture3D<float2> Volume : register(t0);
#endif
Texture2D<float> DepthTexture : register(t1);
sampler Sampler : register(s0);

struct v2f {
	float4 pos : SV_Position;
	float4 ro : TEXCOORD0;
	float4 rd : TEXCOORD1;
	float4 sp : TEXCOORD2;
};

v2f vsmain(float3 vertex : POSITION) {
	v2f o;

	float4 vp = mul(CB.ObjectToView, float4(vertex, 1));
	o.pos = mul(CB.Projection, vp);

	o.ro.xyz = mul(CB.WorldToObject, float4(Root.CameraPosition.xyz, 1)).xyz;
	o.rd.xyz = vertex - o.ro.xyz;
	o.sp.xyz = o.pos.xyw;

	float3 ray = vp.xyz;
	o.ro.w = ray.x;
	o.rd.w = ray.y;
	o.sp.w = ray.z;

	return o;
}

float2 RayCube(float3 ro, float3 rd, float3 extents) {
	float3 aabb[2] = { -extents, extents };
	float3 ird = 1.0 / rd;
	int3 sign = int3(rd.x < 0 ? 1 : 0, rd.y < 0 ? 1 : 0, rd.z < 0 ? 1 : 0);

	float tmin, tmax, tymin, tymax, tzmin, tzmax;
	tmin = (aabb[sign.x].x - ro.x) * ird.x;
	tmax = (aabb[1 - sign.x].x - ro.x) * ird.x;
	tymin = (aabb[sign.y].y - ro.y) * ird.y;
	tymax = (aabb[1 - sign.y].y - ro.y) * ird.y;
	tzmin = (aabb[sign.z].z - ro.z) * ird.z;
	tzmax = (aabb[1 - sign.z].z - ro.z) * ird.z;
	tmin = max(max(tmin, tymin), tzmin);
	tmax = min(min(tmax, tymax), tzmax);

	return float2(tmin, tmax);
}
float RayPlane(float3 ro, float3 rd, float3 planep, float3 planen) {
	float d = dot(planen, rd);
	float t = dot(planep - ro, planen);
	return d > 1e-5 ? (t / d) : (t > 0 ? 1e5 : -1e5);
}

float4 Sample(float3 p) {
	float4 s;

#ifdef COLOR
	s = Volume.SampleLevel(Sampler, p, 0);
#else
	float2 ra = Volume.SampleLevel(Sampler, p, 0).rg;
	s.rgb = ra.r;
	s.a = ra.g;
#endif

#ifdef PLANE
	s.a = (dot((p - .5) - Root.PlanePoint, Root.PlaneNormal) < 0) ? 0 : s.a;
#endif

	return s;
}

float LinearDepth(float d) {
	return Root.Projection43 / (d - Root.Projection33);
}

// depth texture to object-space ray depth
float DepthTextureToObjectDepth(float3 ro, float3 viewRay, float3 screenPos) {
	float2 uv = screenPos.xy / screenPos.z;
	uv.y = -uv.y;
	uv = uv * .5 + .5;

	float3 vp = viewRay / viewRay.z;
	vp *= LinearDepth(DepthTexture.Sample(Sampler, uv).r);

	return length(mul(CB.ViewToObject, float4(vp, 1)).xyz - ro);
}

float4 fsmain(v2f i) : SV_Target{
	float3 ro = i.ro.xyz;
	float3 rd = normalize(i.rd.xyz);

	float2 intersect = RayCube(ro, rd, .5);
	intersect.x = max(0, intersect.x);

	#ifdef PLANE
	intersect.x = max(intersect.x, RayPlane(ro, rd, Root.PlanePoint, Root.PlaneNormal));
	#endif

	// depth buffer intersection
	float z = DepthTextureToObjectDepth(ro, float3(i.ro.w, i.rd.w, i.sp.w), i.sp.xyz);
	intersect.y = min(intersect.y, z);

	clip(intersect.y - intersect.x);

	ro += .5; // cube has a radius of .5, transform to UVW space

	float4 sum = 0;

	#define dt .003
	uint steps = 0;
	float pd = 0;
	for (float t = intersect.x; t < intersect.y;) {
		if (sum.a > .98 || steps > 128) break;

		float3 p = ro + rd * t;
		float4 col = Sample(p);

		if (col.a > .01) {
			if (pd < .01) {
				// first time entering volume, binary subdivide to get closer to entrance point
				float t0 = t - dt * 3;
				float t1 = t;
				float tm;
				#define BINARY_SUBDIV tm = (t0 + t1) * .5; p = ro + rd * tm; if (Sample(p).a) t1 = tm; else t0 = tm;
				BINARY_SUBDIV
				BINARY_SUBDIV
				BINARY_SUBDIV
				#undef BINARY_SUBDIV
				t = tm;
				col = Sample(p);
			}

			col.rgb *= col.a;
			sum += col * (1 - sum.a);

			steps++; // only count steps through the volume
		}

		pd = col.a;
		t += col.a > .01 ? dt : dt * 5; // step farther if not in dense part
	}

	sum.a = saturate(sum.a);
	return sum;
}