float4 ComputeScreenPos(float4 pos) {
	return float4((pos.xy * float2(1, sign(STRATUM_MATRIX_P[1][1])) + pos.w) * .5, pos.zw);
}

float3 ComputeView(float3 worldPos, float4 screenPos) {
	if (Camera.ProjParams.w) {
		float2 uv = screenPos.xy / screenPos.w;
		uv = ((uv*2-1) * StereoClipTransform.xy + StereoClipTransform.zw)*.5+.5;
		float3 view = float3(uv * 2 - 1, Camera.Viewport.z);
		view.x *= Camera.ProjParams.x; // aspect
		view.xy *= Camera.ProjParams.y; // ortho size
		return -mul(float4(view, 1), STRATUM_MATRIX_V).xyz;
	} else
		return normalize(-worldPos.xyz);
	
}
float LinearDepth01(float screenPos_z) {
	return screenPos_z / STRATUM_MATRIX_P[2][2] / (Camera.Viewport.w - Camera.Viewport.z);
}