#pragma warning(disable : 3568) // unrecognized pragma

#pragma rootsig RootSig
#pragma compute main

#pragma multi_compile LIGHT_DIRECTIONAL LIGHT_SPOT LIGHT_POINT
#pragma multi_compile PLANE
#pragma multi_compile MASK

struct DataBuffer {
	float4x4 indexToWorld;
	float3 paintPos0;
	float paintRadius;
	float3 paintPos1;
	float paintValue;
	float3 texelSize;
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

#ifdef COLOR
RWTexture3D<float4> volume : register(u0);
RWTexture3D<float4> baked : register(u1);
#else
RWTexture3D<float2> volume : register(u0);
RWTexture3D<float2> baked : register(u1);
#endif
ConstantBuffer<DataBuffer> Data : register(b0);
ConstantBuffer<RootBuffer> Root : register(b1);

const static int3 offsets[6] = {
	int3(-1, 0, 0),
	int3(1, 0, 0),
	int3(0, -1, 0),
	int3(0,  1, 0),
	int3(0, 0, -1),
	int3(0, 0,  1)
};

float minimum_distance(float3 v, float3 w, float3 p) {
	float3 wv = w - v;
	float l2 = dot(wv, wv);
	float3 pv = p - v;

	if (l2 < l2 < .00001) pv = v + saturate(dot(pv, wv) / l2) * wv - p;

	return dot(pv, pv);
}

#define Epsilon 1e-10
float3 RGBtoHCV(in float3 RGB) {
	// Based on work by Sam Hocevar and Emil Persson
	float4 P = (RGB.g < RGB.b) ? float4(RGB.bg, -1.0, 2.0 / 3.0) : float4(RGB.gb, 0.0, -1.0 / 3.0);
	float4 Q = (RGB.r < P.x) ? float4(P.xyw, RGB.r) : float4(RGB.r, P.yzx);
	float C = Q.x - min(Q.w, Q.y);
	float H = abs((Q.w - Q.y) / (6 * C + Epsilon) + Q.z);
	return float3(H, C, Q.x);
}

float4 Sample(uint3 p) {
	float4 s = 0;

	// get base color
#ifdef COLOR

	s.rgb = volume[p].rgb;
	float3 hcv = RGBtoHCV(s.rgb);
	s.a = (1 - hcv.x) * (hcv.z > .1); // get rid of blue values
	s.a *= s.a;

#else

	float2 x = volume[p].rg;
	s = x.r;
#ifdef MASK
	s.a *= x.g;
#endif

#endif

	s.rgb *= Root.Exposure;

#ifdef INVERT
	s.a = 1 - s.a;
#endif

	// ISO
	//s.a = (s.a > Root.TresholdValue) * s.a; // cutoff for hard edges
	s.a = max(0, (s.a - Root.TresholdValue) / (1 - Root.TresholdValue)); // subtractive for soft edges

	s.a *= Root.Density;

	return s;
}

void Lighting(float3 p, inout float4 col) {
#define ldt .002
#define ls 5
#define ils2 .04

#if defined(LIGHT_DIRECTIONAL)

	float3 uvwldir = -Root.LightDirection / Root.WorldScale;
	float ld = 0;
	for (uint i = 0; i < ls; i++)
		ld += Sample((uint3)((p + uvwldir * ldt * i) / Data.texelSize)).a * (1 - (float)i * i * ils2);

	ld *= Root.LightDensity * ldt;
	col.rgb *= (exp(-ld) + Root.LightAmbient) * Root.LightIntensity; // extinction = e^(-x)

#elif defined(LIGHT_SPOT) || defined(LIGHT_POINT)

	float3 wp = (p - .5) * Root.WorldScale;
	float3 ldir = Root.LightPosition - wp;
	float dist = length(ldir);
	ldir /= dist;

	float3 uvwldir = ldir / Root.WorldScale;

	// sum density from 2 samples towards the light source
	float ld = 0;
	for (uint i = 0; i < ls; i++)
		ld += Sample((uint3)((p + uvwldir * ldt * i) / Data.texelSize)).a * (1 - (float)i * i * ils2);

	dist += 1;

	ld *= Root.LightDensity * ldt;
	col.rgb *= (exp(-ld) // extinction = e^(-x)
		* 1 / (dist * dist) // LightDistanceAttenuation
#ifdef LIGHT_SPOT
		* saturate(10 * (max(0, dot(Root.LightDirection, -ldir)) - Root.LightAngle)) // LightAngleAttenuation
#endif
		+ Root.LightAmbient) * Root.LightIntensity;
#endif
}
void Precompute(uint3 index) {

	float4 s = Sample(index);
	Lighting((float3)index.xyz * Data.texelSize, s);

#ifdef COLOR
	baked[index.xyz] = s;
#else
	baked[index.xyz] = s.ra;
#endif
}

[numthreads(4, 4, 4)]
void main(uint3 index : SV_DispatchThreadID) {
#ifdef PRECOMPUTE
	Precompute(index);
#else

	float2 rg = volume[index.xyz];
	float m = rg.g;

#if defined(FILL)

	m = Data.paintValue;

#elif defined(PAINT)

	float3 wp = mul(Data.indexToWorld, float4((float3)index.xyz, 1)).xyz;
	float x = minimum_distance(Data.paintPos0, Data.paintPos1, wp);

	x = 1 - x / Data.paintRadius;
	m = lerp(m, Data.paintValue, saturate(3 * x));

#elif defined(GROW)
	for (uint i = 0; i < 6; i++)
		m = max(m, volume[index + offsets[i]].g);
#elif defined(SHRINK)
	for (uint i = 0; i < 6; i++)
		m = min(m, volume[index + offsets[i]].g);
#endif

	rg.g = m;
	volume[index.xyz] = rg;

#endif
}
