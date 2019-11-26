float2 hash(float2 x) {
    const float2 k = float2(0.3183099f, 0.3678794f);
    x = x*k + k.yx;
    return -1 + 2*frac(16 * k*frac( x.x*x.y*(x.x+x.y)));
}
float noise(float2 p) {
    float2 i = floor(p);
    float2 f = frac(p);
    // quintic interpolation
    float2 u = f*f*f*(f*(f*6-15)+10);
    
    float2 ga = hash(i + float2(0,0));
    float2 gb = hash(i + float2(1,0));
    float2 gc = hash(i + float2(0,1));
    float2 gd = hash(i + float2(1,1));
    
    float va = dot(ga, f - float2(0,0));
    float vb = dot(gb, f - float2(1,0));
    float vc = dot(gc, f - float2(0,1));
    float vd = dot(gd, f - float2(1,1));

    return va + u.x*(vb-va) + u.y*(vc-va) + u.x*u.y*(va-vb-vc+vd);
}
float3 noised(float2 p) {
    float2 i = floor(p);
    float2 f = frac(p);
    // quintic interpolation
    float2 u = f*f*f*(f*(f*6-15)+10);
    float2 du = 30.0*f*f*(f*(f-2)+1);
    
    float2 ga = hash(i + float2(0,0));
    float2 gb = hash(i + float2(1,0));
    float2 gc = hash(i + float2(0,1));
    float2 gd = hash(i + float2(1,1));
    
    float va = dot(ga, f - float2(0,0));
    float vb = dot(gb, f - float2(1,0));
    float vc = dot(gc, f - float2(0,1));
    float vd = dot(gd, f - float2(1,1));

    return float3(va + u.x*(vb-va) + u.y*(vc-va) + u.x*u.y*(va-vb-vc+vd),   // value
                  ga + u.x*(gb-ga) + u.y*(gc-ga) + u.x*u.y*(ga-gb-gc+gd) +  // derivatives
                  du * (u.yx*(va-vb-vc+vd) + float2(vb,vc) - va));
}

float billow2(float2 p) {
    float value = 0;
    float amplitude = 1;
    for(uint i = 0; i < 2; i++) {
		float signal = abs(noise(p))*2-1;
		value += amplitude * signal;
		amplitude *= .5;
        p *= 2;
    }
	value /= 1.5;
	return value * .5 + .5;
}

float ridged8(float2 p) {
    float value = 0;
    float amplitude = 1;
    for(uint i = 0; i < 8; i++) {
        float n = noise(p);
		float signal = exp(-10*n*n);
		value += amplitude * signal;
		amplitude *= .5;
        p *= 2;
    }
	value /= 1.9922;
	return value;
}
float ridged3(float2 p) {
	float value = 0;
	float amplitude = 1;
	for (uint i = 0; i < 3; i++) {
		float n = noise(p);
		float signal = exp(-10*n*n);
		value += amplitude * signal;
		amplitude *= .5;
		p *= 2;
	}
	value /= 1.75;
	return value;
}

float fbm3(float2 p) {
    float value = 0;
	float2 tot = 0;
    float amplitude = 1;
    for(uint i = 0; i < 3; i++) {
		float3 signal = noised(p);
		tot += signal.yz;
		value += amplitude * signal.x / (1 + dot(tot, tot));;
		amplitude *= .5;
        p *= 2;
    }
	value /= 1.75;
	return value * .5 + .5;
}
float fbm6(float2 p) {
    float value = 0;
	float2 tot = 0;
    float amplitude = 1;
    for(uint i = 0; i < 6; i++) {
		float3 signal = noised(p);
		tot += signal.yz;
		value += amplitude * signal.x / (1 + dot(tot, tot));
		amplitude *= .5;
        p *= 2;
    }
	value /= 1.9844;
	return value * .5 + .5;
}

float SampleTerrain(float2 p, out float mountain, out float lake) {
	lake = tanh(120 * (ridged3(p * .00025) - .98)) * .5 + .5;
	mountain = tanh(70 * (billow2(p * float2(.00075, .0005)) - .2)) * .5 + .5;

	lake = max(lake - mountain, 0);

	float n = 0;
	n += .10 * fbm3(p * .02);
	n += .50 * fbm3(p * .004) * (1 - lake);
	n += .40 * ridged8(p * .0015) * mountain;

	return n;
}