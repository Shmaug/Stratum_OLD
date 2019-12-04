void ApplyScattering(inout float3 color, float2 uv, float linearDepth) {
	float3 inscattering = InscatteringLUT.SampleLevel(AtmosphereSampler, float3(uv, linearDepth), 0).rgb;
	float3 extinction = ExtinctionLUT.SampleLevel(AtmosphereSampler, float3(uv, linearDepth), 0).rgb;

	float shadow = LightShaftLUT.SampleLevel(AtmosphereSampler, uv, 0);
	float shadow4 = shadow*shadow;
	shadow4 *= shadow4;
	inscattering *= shadow * .2 + shadow4 * .8;

	color *= extinction;
	color += inscattering;
}