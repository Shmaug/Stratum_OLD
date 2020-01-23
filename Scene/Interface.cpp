#include <Scene/Interface.hpp>
#include <Scene/Scene.hpp>

using namespace std;

void DrawScreenRect(CommandBuffer* commandBuffer, Camera* camera, const float2& screenPos, const float2& scale, const float4& color) {
	GraphicsShader* shader = camera->Scene()->AssetManager()->LoadShader("Shaders/ui.stm")->GetGraphics(PASS_MAIN, { "SCREEN_SPACE" });
	if (!shader) return;

	VkPipelineLayout layout = commandBuffer->BindShader(shader, PASS_MAIN, nullptr);
	if (!layout) return;

	float2 s((float)camera->FramebufferWidth(), (float)camera->FramebufferHeight());
	float4 b(0, 0, s);
	float4 st(scale, screenPos);

	commandBuffer->PushConstant(shader, "Color", &color);
	commandBuffer->PushConstant(shader, "Bounds", &b);
	commandBuffer->PushConstant(shader, "ScreenSize", &s);
	commandBuffer->PushConstant(shader, "ScaleTranslate", &st);
	vkCmdDraw(*commandBuffer, 6, 1, 0, 0);
}

void DrawWorldRect(CommandBuffer * commandBuffer, Camera * camera, const float4x4& objectToWorld, const float2& offset, const float2& scale, const float4& color) {
	GraphicsShader* shader = camera->Scene()->AssetManager()->LoadShader("Shaders/ui.stm")->GetGraphics(PASS_MAIN, {});
	if (!shader) return;

	VkPipelineLayout layout = commandBuffer->BindShader(shader, PASS_MAIN, nullptr, camera);
	if (!layout) return;

	float4 b(-1e20f, -1e20f, 1e20f, 1e20f);
	float4 st(scale, offset);

	commandBuffer->PushConstant(shader, "ObjectToWorld", &objectToWorld);
	commandBuffer->PushConstant(shader, "Color", &color);
	commandBuffer->PushConstant(shader, "Bounds", &b);
	commandBuffer->PushConstant(shader, "ScaleTranslate", &st);
	vkCmdDraw(*commandBuffer, 6, 1, 0, 0);
}