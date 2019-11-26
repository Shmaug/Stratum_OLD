float2 hash(float2 x) {
    const float2 k = float2( 0.3183099, 0.3678794 );
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

	return value * .5 + .5;
}
float ridged8(float2 p) {
    float value = 0;
    float amplitude = 1;
    for(uint i = 0; i < 8; i++) {
        float n = noise(p);
		float signal = exp(-10*n*n);// 1 - abs(n);
		value += amplitude * signal;
		amplitude *= .5;
        p *= 2;
    }

	return value * .5 + .5;
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

	return value * .5 + .5;
}