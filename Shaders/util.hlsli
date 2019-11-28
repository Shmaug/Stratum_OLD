float4 ComputeScreenPos(float4 pos) {
	return float4((pos.xy * float2(1, sign(Camera.Projection[1][1])) + pos.w) * .5, pos.zw);
}

void ComputeDepth(float3 worldPos, float4 screenPos, out float3 view, out float depth) {
	if (Camera.ProjParams.w) {
		view = float3(screenPos.xy / screenPos.w * 2 - 1, Camera.Viewport.z);
		view.x *= Camera.ProjParams.x; // aspect
		view.xy *= Camera.ProjParams.y; // ortho size
		view = mul(float4(view, 1), Camera.View).xyz;
		view = -view;
		depth = screenPos.z * (Camera.Viewport.w - Camera.Viewport.z) + Camera.Viewport.z;
	} else {
		view = normalize(-worldPos.xyz);
		depth = screenPos.w;
	}
	depth /= Camera.Viewport.w;
}