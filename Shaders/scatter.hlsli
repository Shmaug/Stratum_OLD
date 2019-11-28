void ApplyScattering(inout float3 color, float2 uv, float linearDepth) {
	float3 inscattering = InscatteringLUT.SampleLevel(Sampler, float3(uv, linearDepth), 0).rgb;
	float3 extinction = ExtinctionLUT.SampleLevel(Sampler, float3(uv, linearDepth), 0).rgb;

	//float shadow = tex2D(_LightShaft1, uv).x;
	//shadow = (pow(shadow, 4) + shadow) / 2;
	//shadow = max(0.1, shadow);
	//inscattering *= shadow;

	//if (linearDepth > 0.99999) {
	//	#ifdef LIGHT_SHAFTS
	//	color *= shadow;
	//	#endif
	//	inscattering = 0;
	//	extinction = 1;
	//}

	color *= extinction;
	color += inscattering;
}