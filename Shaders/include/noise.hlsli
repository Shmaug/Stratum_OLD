float hash12(float2 p) {
	float3 p3 = frac(p.xyx * .1013f);
	p3 += dot(p3, p3.yzx + 19.19f);
	return frac((p3.x + p3.y) * p3.z);
}
float noise(float2 xz) {
	float2 f = frac(xz);
	xz = floor(xz);
	float2 u = f * f * (3 - 2 * f);
	return
		lerp(lerp(hash12(xz), hash12(xz + float2(1,0)), u.x),
		     lerp(hash12(xz + float2(0,1)), hash12(xz + float2(1,1)), u.x),
			 u.y);
}
float3 noised(float2 x) {
	float2 p = floor(x);
	float2 f = frac(x);

	float2 u = f * f * (3 - 2*f);

	float a = hash12(p);
	float b = hash12(p + float2(1,0));
	float c = hash12(p + float2(0,1));
	float d = hash12(p + float2(1,1));
	return float3(
		a + (b - a) * u.x + (c - a) * u.y + (a - b - c + d) * u.x * u.y,
		6 * f * (f - 1) * (float2(b - a, c - a) + (a - b - c + d) * u.yx));
}

float2 hash(float2 x, uint seed) {
	const float2 k = float2(0.3183099f, 0.3678794f);
	x = x * k + k.yx;
	return -1 + 2 * frac(16 * k * frac(x.x * x.y * (x.x + x.y)));
}
float noise(float2 p, uint seed) {
	float2 i = floor(p);
	float2 f = frac(p);

	float2 u = f * f * (3 - 2 * f);

	float i0 = dot(hash(i, seed), f);
	float i1 = dot(hash(i + float2(1, 0), seed), f - float2(1, 0));
	float i2 = dot(hash(i + float2(0, 1), seed), f - float2(0, 1));
	float i3 = dot(hash(i + float2(1, 1), seed), f - float2(1, 1));

	return lerp(lerp(i0, i1, u.x), lerp(i2, i3, u.x), u.y);
}

float fbm6(float2 p, float lac, uint seed) {
	float value = 0;
	float amp = .5f;
	float d = 0;
	for (uint i = 0; i < 6; i++) {
		d += amp;
		value += noise(p, seed + i * 300) * amp;
		p *= lac;
		amp *= .5f;
	}

	return clamp(value / d, -1.f, 1.f);
}

float ridged4(float2 p, float lac, uint seed) {
	float signal = 0;
	float value = 0;
	float amp = .5f;
	float dmax = 0;
	float dmin = 0;
	for (uint i = 0; i < 4; ++i) {
		dmin += amp;
		dmax += 4 * amp;
		signal = 2 - abs(noise(p, seed + i * 300));
		value += signal * signal * amp;
		p *= lac;
		amp *= .5f;
	}
	value = (value - .5f * (dmax + dmin)) / (dmax - dmin);
	return clamp(value, -1.f, 1.f);
}

float multi8(float2 p, float lac, uint seed) {
	float value = 1;
	float amp = .5f;

	float dmax = 1.0;
	float dmin = 1.0;

	for (uint i = 0; i < 8; i++) {
		dmax *= 1 + amp;
		dmin *= 1 - amp;
		value *= 1 + noise(p, seed + i * 300) * amp;
		p *= lac;
		amp *= .5f;
	}

	value = (value - .5f * (dmax + dmin)) / (dmax - dmin);
	return clamp(value, -1.f, 1.f);
}

float billow4(float2 p, float lac, uint seed) {
	float value = 0;
	float amp = .5f;
	float dmax = 1;
	float dmin = 1;
	for (uint i = 0; i < 4; ++i) {
		dmax += amp;
		dmin += -amp;
		value += (2 * abs(noise(p, seed + i * 300)) - 1) * amp;
		p *= lac;
		amp *= .5f;
	}
	value = (value - .5f * (dmax + dmin)) / (dmax - dmin);
	return clamp(value, -1.f, 1.f);
}