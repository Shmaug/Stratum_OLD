float CascadeSplit(float4 cascades, float depth) {
	if (depth < cascades[0]) return depth / cascades[0];
	if (depth < cascades[1]) return 1 + (depth - cascades[0]) / (cascades[1] - cascades[0]);
	if (depth < cascades[2]) return 2 + (depth - cascades[1]) / (cascades[2] - cascades[1]);
	if (depth < cascades[3]) return 3 + (depth - cascades[2]) / (cascades[3] - cascades[2]);
	return -1;
}

float SampleShadowCascade(uint index, float3 cameraPos, float3 worldPos) {
	ShadowData s = Shadows[index];

	float4 shadowPos = mul(s.WorldToShadow, float4(worldPos + (cameraPos - s.CameraPosition), 1));
	float z = shadowPos.z * s.InvProj22;

	float2 shadowUV = saturate(shadowPos.xy / shadowPos.w * .5 + .5);
	shadowUV = shadowUV * s.ShadowST.xy + s.ShadowST.zw;

	return ShadowAtlas.SampleCmpLevelZero(ShadowSampler, shadowUV, z);
}
float SampleShadow(GPULight l, float3 cameraPos, float3 worldPos, float depth) {
	float ct = l.Type == LIGHT_SUN ? CascadeSplit(l.CascadeSplits, depth) : 0;
	return (ct >= 0) ? SampleShadowCascade(l.ShadowIndex + (uint)ct, cameraPos, worldPos) : 1;
}

float SampleShadowCascadePCF(uint index, float3 cameraPos, float3 worldPos) {
	ShadowData s = Shadows[index];

	float4 shadowPos = mul(s.WorldToShadow, float4(worldPos + (cameraPos - s.CameraPosition), 1));
	float z = shadowPos.z * s.InvProj22;

	float2 shadowUV = saturate(shadowPos.xy / shadowPos.w * .5 + .5);
	shadowUV = shadowUV * s.ShadowST.xy + s.ShadowST.zw;

	float attenuation = 0;
	attenuation += ShadowAtlas.SampleCmpLevelZero(ShadowSampler, shadowUV, z, int2( 0,  0));
	attenuation += ShadowAtlas.SampleCmpLevelZero(ShadowSampler, shadowUV, z, int2( 1,  0));
	attenuation += ShadowAtlas.SampleCmpLevelZero(ShadowSampler, shadowUV, z, int2(-1,  0));
	attenuation += ShadowAtlas.SampleCmpLevelZero(ShadowSampler, shadowUV, z, int2( 0,  1));
	attenuation += ShadowAtlas.SampleCmpLevelZero(ShadowSampler, shadowUV, z, int2( 1,  1));
	attenuation += ShadowAtlas.SampleCmpLevelZero(ShadowSampler, shadowUV, z, int2(-1,  1));
	attenuation += ShadowAtlas.SampleCmpLevelZero(ShadowSampler, shadowUV, z, int2( 0, -1));
	attenuation += ShadowAtlas.SampleCmpLevelZero(ShadowSampler, shadowUV, z, int2( 1, -1));
	attenuation += ShadowAtlas.SampleCmpLevelZero(ShadowSampler, shadowUV, z, int2(-1, -1));
	return attenuation / 9;
}
float SampleShadowPCF(GPULight l, float3 cameraPos, float3 worldPos, float depth) {
	float attenuation = 1;
	float ct = l.Type == LIGHT_SUN ? CascadeSplit(l.CascadeSplits, depth) : 0;
	if (ct >= 0) {
		uint ci = (uint)ct;
		attenuation = SampleShadowCascadePCF(l.ShadowIndex + ci, cameraPos, worldPos);
		if (ci < 3) attenuation = lerp(attenuation, SampleShadowCascadePCF(l.ShadowIndex + ci + 1, cameraPos, worldPos), saturate(((ct - ci) - .92) / .08));
	}
	return attenuation;
}

float LightAttenuation(uint li, float3 cameraPos, float3 worldPos, float3 normal, float depth, out float3 L) {
	GPULight l = Lights[li];
	L = l.Direction;
	float attenuation = 1;

	if (l.ShadowIndex >= 0)
		attenuation = SampleShadowPCF(l, cameraPos, worldPos, depth);

	if (l.Type > LIGHT_SUN) {
		float3 lightPos = worldPos + (cameraPos - l.WorldPosition);
		float d2 = dot(lightPos, lightPos);
		L = -lightPos / sqrt(d2);
		attenuation *= 1 / max(d2, .0001);
		float f = d2 * l.InvSqrRange;
		f = saturate(1 - f * f);
		attenuation *= f * f;
		if (l.Type == LIGHT_SPOT) {
			float a = saturate(dot(L, l.Direction) * l.SpotAngleScale + l.SpotAngleOffset);
			attenuation *= a * a;
		}
	}

	return attenuation;
}