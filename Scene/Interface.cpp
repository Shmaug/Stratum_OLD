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

void DrawScreenLine(CommandBuffer* commandBuffer, Camera* camera, const float2* points, size_t pointCount, const float2& pos, const float2& size, const float4& color) {
	GraphicsShader* shader = camera->Scene()->AssetManager()->LoadShader("Shaders/line.stm")->GetGraphics(PASS_MAIN, { "SCREEN_SPACE" });
	if (!shader) return;
	VkPipelineLayout layout = commandBuffer->BindShader(shader, PASS_MAIN, nullptr, nullptr, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP);
	if (!layout) return;

	float4 st(size, pos);
	float4 sz(0, 0, camera->FramebufferWidth(), camera->FramebufferHeight());

	Buffer* b = commandBuffer->Device()->GetTempBuffer("Perf Graph Pts", sizeof(float2) * pointCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	memcpy(b->MappedData(), points, sizeof(float2) * pointCount);
	DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet("Perf Graph DS", shader->mDescriptorSetLayouts[PER_OBJECT]);
	ds->CreateStorageBufferDescriptor(b, 0, sizeof(float2) * pointCount, INSTANCE_BUFFER_BINDING);
	ds->FlushWrites();

	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, PER_OBJECT, 1, *ds, 0, nullptr);

	commandBuffer->PushConstant(shader, "Color", &color);
	commandBuffer->PushConstant(shader, "ScaleTranslate", &st);
	commandBuffer->PushConstant(shader, "Bounds", &sz);
	commandBuffer->PushConstant(shader, "ScreenSize", &sz.z);
	vkCmdDraw(*commandBuffer, pointCount, 1, 0, 0);
}