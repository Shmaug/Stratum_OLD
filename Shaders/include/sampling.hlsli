struct RandomSampler {
	uint index;
	uint dimension;
	uint scramble;
	uint pad;
};

#define CMJ_DIM 16

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

float2 SampleRNG(inout RandomSampler rng) {
	int idx = permute(rng.index, CMJ_DIM * CMJ_DIM, 0xa399d265 * rng.dimension * rng.scramble);
	float2 s = cmj(idx, CMJ_DIM, rng.dimension * rng.scramble);
	rng.dimension++;
	return s;
}

float BalanceHeuristic(int nf, float fpdf, int ng, float gpdf) {
	float f = nf * fpdf;
	float g = ng * gpdf;
	return f / (f + g);
}

float3 GetOrthoVector(float3 n) {
	float3 p;
	if (abs(n.z) > 0) {
		float k = sqrt(n.y * n.y + n.z * n.z);
		p.x = 0; p.y = -n.z / k; p.z = n.y / k;
	}
	else {
		float k = sqrt(n.x * n.x + n.y * n.y);
		p.x = n.y / k; p.y = -n.x / k; p.z = 0;
	}
	return normalize(p);
}

float3 Sample_MapToHemisphere(float2 sample, float3 n, float e) {
	// Construct basis
	float3 u = GetOrthoVector(n);
	float3 v = cross(u, n);
	u = cross(n, v);

	// Calculate 2D sample
	float r1 = sample.x;
	float r2 = sample.y;

	// Transform to spherical coordinates
	float sinpsi = sin(2 * PI * r1);
	float cospsi = cos(2 * PI * r1);
	float costheta = pow(1.f - r2, 1.f / (e + 1.f));
	float sintheta = sqrt(1.f - costheta * costheta);

	return normalize(u * sintheta * cospsi + v * sintheta * sinpsi + n * costheta);
}
float2 Sample_MapToDisk(float2 sample) {
	float r = sqrt(sample.x);
	float theta = 2 * PI * sample.y;
	return float2(r * cos(theta), r * sin(theta));
}
float2 Sample_MapToDiskConcentric(float2 sample) {
	float2 offset = 2 * sample - 1;

	if (offset.x == 0 && offset.y == 0) return 0;

	float theta, r;

	if (abs(offset.x) > abs(offset.y)) {
		r = offset.x;
		theta = PI / 4 * (offset.y / offset.x);
	}
	else {
		r = offset.y;
		theta = PI / 2 * (1 - 0.5 * (offset.x / offset.y));
	}

	return float2(r * cos(theta), r * sin(theta));
}
float3 Sample_MapToSphere(float2 sample) {
	float z = 1 - 2 * sample.x;
	float r = sqrt(max(0, 1 - z * z));
	float phi = 2 * PI * sample.y;
	float x = cos(phi);
	float y = sin(phi);
	return float3(x, y, z);
}
float2 Sample_MapToPolygon(int n, float2 sample, float sample1) {
	float theta = 2 * PI / n;
	int edge = clamp((int)(sample1 * n), 0, n - 1);
	float t = sqrt(sample.x);
	float u = 1 - t;
	float v = t * sample.y;
	float2 v1 = float2(cos(theta * edge), sin(theta * edge));
	float2 v2 = float2(cos(theta * (edge + 1)), sin(theta * (edge + 1)));
	return u * v1 + v * v2;;
}