#include "OpenVR.hpp"
#include <Scene/Scene.hpp>
#include <Scene/GUI.hpp>
#include <Content/Font.hpp>
#include <Scene/MeshRenderer.hpp>
#include <assimp/pbrmaterial.h>

using namespace std;

ENGINE_PLUGIN(OpenVR)

OpenVR::OpenVR() : mScene(nullptr), mCamera(nullptr) {
	mEnabled = true;
	mVRDevice = new OpenVRDevice();
	
}
OpenVR::~OpenVR() {
	mScene->RemoveObject(mCamera);
	mScene->RemoveObject(mCameraRight);
	mScene->RemoveObject(mBodyBase);
	mScene->RemoveObject(mHead);
	for (Object* obj : mObjects)
		mScene->RemoveObject(obj);
	//delete mLeftEye;
	//delete mRightEye;
	delete mMirror;
	delete mVRDevice;
}

void OpenVR::PreInstanceInit(Instance* instance)
{
	std::vector< std::string > requiredInstanceExtensions;
	mVRDevice->GetVulkanInstanceExtensionsRequired(requiredInstanceExtensions);
	for (std::string ex : requiredInstanceExtensions)
	{
		instance->RequestInstanceExtension(ex);
	}

	//instance->RequestInstanceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	instance->RequestInstanceExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
}

void OpenVR::PreDeviceInit(Instance* instance, VkPhysicalDevice device)
{
	std::vector< std::string > requiredDeviceExtensions;
	mVRDevice->GetVulkanDeviceExtensionsRequired(device, requiredDeviceExtensions);
	for (std::string ex : requiredDeviceExtensions)
	{
		instance->RequestDeviceExtension(ex);
	}

	instance->RequestDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	//instance->RequestDeviceExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
}

bool OpenVR::Init(Scene* scene) {
	mScene = scene;

	VkInstance i = *scene->Instance();
	uint64_t device;
	
#pragma region load glTF
	string folder = "Assets/Models/";
	string file = "cornellbox.gltf";

	shared_ptr<Material> opaque = make_shared<Material>("PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
	opaque->EnableKeyword("TEXTURED");
	opaque->SetParameter("TextureST", float4(1, 1, 0, 0));

	shared_ptr<Material> alphaClip = make_shared<Material>("Cutout PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
	alphaClip->RenderQueue(5000);
	alphaClip->BlendMode(BLEND_MODE_ALPHA);
	alphaClip->CullMode(VK_CULL_MODE_NONE);
	alphaClip->EnableKeyword("TEXTURED");
	alphaClip->EnableKeyword("ALPHA_CLIP");
	alphaClip->EnableKeyword("TWO_SIDED");
	alphaClip->SetParameter("TextureST", float4(1, 1, 0, 0));

	shared_ptr<Material> alphaBlend = make_shared<Material>("Transparent PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
	alphaBlend->RenderQueue(5000);
	alphaBlend->BlendMode(BLEND_MODE_ALPHA);
	alphaBlend->CullMode(VK_CULL_MODE_NONE);
	alphaBlend->EnableKeyword("TEXTURED");
	alphaBlend->EnableKeyword("TWO_SIDED");
	alphaBlend->SetParameter("TextureST", float4(1, 1, 0, 0));

	shared_ptr<Material> curOpaque = nullptr;
	shared_ptr<Material> curClip = nullptr;
	shared_ptr<Material> curBlend = nullptr;

	uint32_t arraySize =
		mScene->AssetManager()->LoadShader("Shaders/pbr.stm")->GetGraphics(PASS_MAIN, { "TEXTURED" })->mDescriptorBindings.at("MainTextures").second.descriptorCount;

	uint32_t opaque_i = 0;
	uint32_t clip_i = 0;
	uint32_t blend_i = 0;

	auto matfunc = [&](Scene* scene, aiMaterial* aimaterial) {
		aiString alphaMode;
		if (aimaterial->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS) {
			if (alphaMode == aiString("MASK")) return alphaClip;
			if (alphaMode == aiString("BLEND")) return alphaBlend;
		}
		return opaque;
	};
	auto objfunc = [&](Scene* scene, Object* object, aiMaterial* aimaterial) {
		MeshRenderer* renderer = dynamic_cast<MeshRenderer*>(object);
		if (!renderer) return;

		Material* mat = renderer->Material();
		uint32_t i;

		if (mat == opaque.get()) {
			i = opaque_i;
			opaque_i++;
			if (opaque_i >= arraySize) curOpaque.reset();
			if (!curOpaque) {
				opaque_i = opaque_i % arraySize;
				curOpaque = make_shared<Material>("PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
				curOpaque->EnableKeyword("TEXTURED");
				curOpaque->SetParameter("TextureST", float4(1, 1, 0, 0));
			}
			renderer->Material(curOpaque);
			mat = curOpaque.get();

		}
		else if (mat == alphaClip.get()) {
			i = clip_i;
			clip_i++;
			if (clip_i >= arraySize) curClip.reset();
			if (!curClip) {
				clip_i = clip_i % arraySize;
				curClip = make_shared<Material>("Cutout PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
				curClip->RenderQueue(5000);
				curClip->BlendMode(BLEND_MODE_ALPHA);
				curClip->CullMode(VK_CULL_MODE_NONE);
				curClip->EnableKeyword("TEXTURED");
				curClip->EnableKeyword("ALPHA_CLIP");
				curClip->EnableKeyword("TWO_SIDED");
				curClip->SetParameter("TextureST", float4(1, 1, 0, 0));
			}
			renderer->Material(curClip);
			mat = curClip.get();

		}
		else if (mat == alphaBlend.get()) {
			i = blend_i;
			blend_i++;
			if (blend_i >= 64) curBlend.reset();
			if (!curBlend) {
				blend_i = blend_i % arraySize;
				curBlend = make_shared<Material>("Transparent PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
				curBlend->RenderQueue(5000);
				curBlend->BlendMode(BLEND_MODE_ALPHA);
				curBlend->CullMode(VK_CULL_MODE_NONE);
				curBlend->EnableKeyword("TEXTURED");
				curBlend->EnableKeyword("TWO_SIDED");
				curBlend->SetParameter("TextureST", float4(1, 1, 0, 0));
			}
			renderer->Material(curBlend);
			mat = curBlend.get();

		}
		else return;

		aiColor3D emissiveColor(0);
		aiColor4D baseColor(1);
		float metallic = 1.f;
		float roughness = 1.f;
		aiString baseColorTexture, metalRoughTexture, normalTexture, emissiveTexture;

		if (aimaterial->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_TEXTURE, &baseColorTexture) == AI_SUCCESS && baseColorTexture.length) {
			mat->SetParameter("MainTextures", i, scene->AssetManager()->LoadTexture(folder + baseColorTexture.C_Str()));
			baseColor = aiColor4D(1);
		}
		else
			mat->SetParameter("MainTextures", i, scene->AssetManager()->LoadTexture("Assets/Textures/white.png"));

		if (aimaterial->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &metalRoughTexture) == AI_SUCCESS && metalRoughTexture.length)
			mat->SetParameter("MaskTextures", i, scene->AssetManager()->LoadTexture(folder + metalRoughTexture.C_Str(), false));
		else
			mat->SetParameter("MaskTextures", i, scene->AssetManager()->LoadTexture("Assets/Textures/mask.png", false));

		if (aimaterial->GetTexture(aiTextureType_NORMALS, 0, &normalTexture) == AI_SUCCESS && normalTexture.length)
			mat->SetParameter("NormalTextures", i, scene->AssetManager()->LoadTexture(folder + normalTexture.C_Str(), false));
		else
			mat->SetParameter("NormalTextures", i, scene->AssetManager()->LoadTexture("Assets/Textures/bump.png", false));

		aimaterial->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_FACTOR, baseColor);
		aimaterial->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, metallic);
		aimaterial->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, roughness);
		aimaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor);

		renderer->PushConstant("TextureIndex", i);
		renderer->PushConstant("Color", float4(baseColor.r, baseColor.g, baseColor.b, baseColor.a));
		renderer->PushConstant("Roughness", roughness);
		renderer->PushConstant("Metallic", metallic);
		renderer->PushConstant("Emission", float3(emissiveColor.r, emissiveColor.g, emissiveColor.b));
	};

	Object* root = mScene->LoadModelScene(folder + file, matfunc, objfunc, .6f, 1.f, .05f, .0015f);

	root->LocalRotation(quaternion(float3(0, PI / 2, 0)));
	//root->LocalScale(float3(10, 10, 10));
	queue<Object*> nodes;
	nodes.push(root);
	while (nodes.size()) {
		Object* o = nodes.front();
		nodes.pop();
		for (uint32_t i = 0; i < o->ChildCount(); i++)
			nodes.push(o->Child(i));

		mObjects.push_back(o);
		if (Light* l = dynamic_cast<Light*>(o)) {
			if (l->Type() == LIGHT_TYPE_SUN) {
				l->CascadeCount(1);
				l->ShadowDistance(30);
			}
		}
	}
#pragma endregion

	mScene->Environment()->EnableCelestials(false);
	mScene->Environment()->EnableScattering(false);
	mScene->Environment()->AmbientLight(.6f);

	
#pragma region Camera setup
	shared_ptr<Object> bodyBase = make_shared<Object>("Openvr body");
	mScene->AddObject(bodyBase);
	mBodyBase = bodyBase.get();
	mBodyBase->LocalPosition(0, -.5f, 0);

	shared_ptr<Object> head = make_shared<Object>("Openvr head");
	mScene->AddObject(head);
	mHead = head.get();
	mBodyBase->AddChild(mHead);
	


	uint32_t renderWidth = 0;
	uint32_t renderHeight = 0;
	mVRDevice->System()->GetRecommendedRenderTargetSize(&renderWidth, &renderHeight);
	fprintf_color(COLOR_GREEN, stderr, "Created stereo camera of size %dx%d\n", renderWidth, renderHeight);

	mVRDevice->CalculateEyeAdjustment();
	mVRDevice->CalculateProjectionMatrices();

	//vector<VkFormat> colorFormats{ VK_FORMAT_R8G8B8A8_UNORM };
	//Framebuffer* f = new Framebuffer("Openvr Camera", scene->Instance()->Device(), renderWidth, renderHeight, colorFormats, VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, {}, VK_ATTACHMENT_LOAD_OP_CLEAR);
	shared_ptr<Camera> camera = make_shared<Camera>("Left Camera", scene->Instance()->Device());
	mScene->AddObject(camera);
	camera->Near(.01f);
	camera->Far(1024.f);
	camera->FramebufferWidth(renderWidth);
	camera->FramebufferHeight(renderHeight);
	camera->ViewportWidth(renderWidth);
	camera->ViewportHeight(renderHeight);

	float3 pos;
	quaternion rot;
	float3 scale;

	float4x4 eye = mVRDevice->LeftEyeMatrix();
	eye.Decompose(&pos, &rot, &scale);
	camera->LocalPosition(pos);
	camera->LocalRotation(rot);
	camera->LocalScale(scale);

	camera->Projection(mVRDevice->LeftProjection());



	mCamera = camera.get();
	mHead->AddChild(mCamera);




	shared_ptr<Camera> camera2 = make_shared<Camera>("Right Camera", scene->Instance()->Device());
	mScene->AddObject(camera2);
	camera2->Near(.01f);
	camera2->Far(1024.f);
	camera2->FramebufferWidth(renderWidth);
	camera2->FramebufferHeight(renderHeight);
	camera2->ViewportWidth(renderWidth);
	camera2->ViewportHeight(renderHeight);


	eye = mVRDevice->RightEyeMatrix();
	eye.Decompose(&pos, &rot, &scale);
	camera2->LocalPosition(pos);
	camera2->LocalRotation(rot);
	camera2->LocalScale(scale);

	camera2->Projection(mVRDevice->RightProjection());



	mCameraRight = camera2.get();
	mHead->AddChild(mCameraRight);




	/*
	camera->StereoMode(STEREO_SBS_HORIZONTAL);


	mVRDevice->CalculateEyeAdjustment();
	float4x4 eye = mVRDevice->LeftEyeMatrix();
	camera->HeadToEye(eye, EYE_LEFT);
	eye = mVRDevice->RightEyeMatrix();
	camera->HeadToEye(eye, EYE_RIGHT);

	mVRDevice->CalculateProjectionMatrices();
	camera->Projection(mVRDevice->LeftProjection() , EYE_LEFT);
	camera->Projection(, EYE_RIGHT);
	*/
#pragma endregion

	VkImageUsageFlags flags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	/*
	mLeftEye = new Texture("Left Eye Texture", 
		scene->Instance()->Device(), 
		mCamera->FramebufferWidth() / 2, mCamera->FramebufferHeight(), 1, 
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, 
		flags);
	mRightEye = new Texture("Right Eye Texture",
		scene->Instance()->Device(),
		mCamera->FramebufferWidth() / 2, mCamera->FramebufferHeight(), 1,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
		flags);
	*/
	mMirror = new Texture("Mirror Texture",
		scene->Instance()->Device(),
		renderWidth * 2, renderHeight, 1,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
		flags);
	return true;
}

void OpenVR::Update(CommandBuffer* commandBuffer) {
	float3 pos;
	quaternion rot;
	float3 scale;

	//Update head position/rotation
	mVRDevice->Update();
	pos = mVRDevice->Position();
	pos.z = -pos.z;
	mHead->LocalPosition(pos);

	rot = mVRDevice->Rotation();
	rot.z = -rot.z;
	mHead->LocalRotation(normalize(rot));

}

void OpenVR::PreRender(CommandBuffer* commandBuffer, Camera* camera, PassType pass)
{
	//if (camera != mCamera)
	//	return;
	//camera->LocalPosition(mVRDevice->Position());
	//camera->LocalRotation(mVRDevice->Rotation());
}

void OpenVR::PostProcess(CommandBuffer* commandBuffer, Camera* camera) {
	if (camera == mCamera)
	{
		//printf_color(COLOR_GREEN, "scale: <%f, %f, %f>\n", camera->LocalScale().x, camera->LocalScale().y, camera->LocalScale().z);
		//printf_color(COLOR_GREEN, "rot: <%f, %f, %f, %f>\n", camera->WorldRotation().x, camera->WorldRotation().y, camera->WorldRotation().z, camera->WorldRotation().w);

		VkPipelineStageFlags srcStage, dstStage, srcStage2, dstStage2;
		VkImageMemoryBarrier barrier[3] = {};
		barrier[0] = mCamera->ResolveBuffer()->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, srcStage, dstStage);
		barrier[1] = mMirror->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, srcStage2, dstStage2);
		srcStage = srcStage | srcStage2;
		dstStage = dstStage | dstStage2;
		vkCmdPipelineBarrier(*commandBuffer,
			srcStage, dstStage,
			0,
			0, nullptr,
			0, nullptr,
			2, barrier);



		VkImageSubresourceLayers srcLayers = {};
		srcLayers.baseArrayLayer = 0;
		srcLayers.layerCount = 1;
		srcLayers.mipLevel = 0;
		srcLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		VkImageSubresourceLayers dstLayers = {};
		dstLayers.baseArrayLayer = 0;
		dstLayers.layerCount = 1;
		dstLayers.mipLevel = 0;
		dstLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		VkExtent3D extent = {};
		extent.width = mMirror->Width() / 2;
		extent.height = mMirror->Height();
		extent.depth = mMirror->Depth();

		//VkOffset3D rightOffset = {};
		//rightOffset.x = mLeftEye->Width();
		//rightOffset.y = 0;
		//rightOffset.z = 0;

		VkImageCopy copy = {};
		copy.srcSubresource = srcLayers;
		copy.dstSubresource = dstLayers;
		copy.extent = extent;

		vkCmdCopyImage(*commandBuffer, mCamera->ResolveBuffer()->Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mMirror->Image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

		VkImageMemoryBarrier barrier2[3] = {};
		barrier2[0] = mCamera->ResolveBuffer()->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, srcStage, dstStage);
		barrier2[1] = mMirror->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, srcStage2, dstStage2);

		srcStage = srcStage | srcStage2;
		dstStage = dstStage | dstStage2;
		vkCmdPipelineBarrier(*commandBuffer,
			srcStage, dstStage,
			0,
			0, nullptr,
			0, nullptr,
			2, barrier2);
	}
	else if (camera == mCameraRight)
	{
		//Change image layouts for transfer
		VkPipelineStageFlags srcStage, dstStage, srcStage2, dstStage2;
		VkImageMemoryBarrier barrier[3] = {};
		barrier[0] = mCameraRight->ResolveBuffer()->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, srcStage, dstStage);
		barrier[1] = mMirror->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, srcStage2, dstStage2);
		srcStage = srcStage | srcStage2;
		dstStage = dstStage | dstStage2;
		vkCmdPipelineBarrier(*commandBuffer,
			srcStage, dstStage,
			0,
			0, nullptr,
			0, nullptr,
			2, barrier);


		//Copy image
		VkImageSubresourceLayers srcLayers = {};
		srcLayers.baseArrayLayer = 0;
		srcLayers.layerCount = 1;
		srcLayers.mipLevel = 0;
		srcLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		VkImageSubresourceLayers dstLayers = {};
		dstLayers.baseArrayLayer = 0;
		dstLayers.layerCount = 1;
		dstLayers.mipLevel = 0;
		dstLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		VkExtent3D extent = {};
		extent.width = mMirror->Width() / 2;
		extent.height = mMirror->Height();
		extent.depth = mMirror->Depth();

		VkOffset3D rightOffset = {};
		rightOffset.x = mMirror->Width() / 2;
		rightOffset.y = 0;
		rightOffset.z = 0;

		VkImageCopy copy = {};
		copy.srcSubresource = srcLayers;
		copy.dstSubresource = dstLayers;
		copy.extent = extent;
		copy.dstOffset = rightOffset;

		vkCmdCopyImage(*commandBuffer, mCameraRight->ResolveBuffer()->Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mMirror->Image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);


		//Change image layouts back to what they were before
		VkImageMemoryBarrier barrier2[3] = {};
		barrier2[0] = mCameraRight->ResolveBuffer()->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, srcStage, dstStage);
		barrier2[1] = mMirror->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, srcStage2, dstStage2);

		srcStage = srcStage | srcStage2;
		dstStage = dstStage | dstStage2;
		vkCmdPipelineBarrier(*commandBuffer,
			srcStage, dstStage,
			0,
			0, nullptr,
			0, nullptr,
			2, barrier2);


		return;
		#pragma region BlitToBackbuffer
		Texture::TransitionImageLayout(mScene->Instance()->Window()->BackBuffer(), mScene->Instance()->Window()->Format().format, 1, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandBuffer);

		VkOffset3D srcoffsets[2];
		srcoffsets[0] = {};
		srcoffsets[1] = {};
		srcoffsets[1].x = mMirror->Width();
		srcoffsets[1].y = mMirror->Height();
		srcoffsets[1].z = 1;
		VkOffset3D dstoffsets[2];
		dstoffsets[0] = {};
		dstoffsets[1] = {};
		dstoffsets[1].x = mScene->Instance()->Window()->BackBufferSize().width;
		dstoffsets[1].y = mScene->Instance()->Window()->BackBufferSize().height;
		dstoffsets[1].z = 1;

		VkImageBlit regions = {};
		regions.srcSubresource = srcLayers;
		regions.dstSubresource = dstLayers;
		regions.srcOffsets[0] = srcoffsets[0];
		regions.srcOffsets[1] = srcoffsets[1];
		regions.dstOffsets[0] = dstoffsets[0];
		regions.dstOffsets[1] = dstoffsets[1];

		vkCmdBlitImage(*commandBuffer, mMirror->Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mScene->Instance()->Window()->BackBuffer(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &regions, VK_FILTER_LINEAR);
	
		Texture::TransitionImageLayout(mScene->Instance()->Window()->BackBuffer(), mScene->Instance()->Window()->Format().format, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, commandBuffer);
		#pragma endregion
		
	}	
}

void OpenVR::PrePresent()
{

	
	// Submit to SteamVR
	vr::VRTextureBounds_t leftBounds;
	leftBounds.uMin = 0.0f;
	leftBounds.uMax = 0.5f;
	leftBounds.vMin = 0.0f;
	leftBounds.vMax = 1.0f;

	vr::VRTextureBounds_t rightBounds;
	rightBounds.uMin = 0.5f;
	rightBounds.uMax = 1.0f;
	rightBounds.vMin = 0.0f;
	rightBounds.vMax = 1.0f;
	
	//tex->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, commandBuffer);

	vr::VRVulkanTextureData_t vulkanDataLeft;
	vulkanDataLeft.m_nImage = (uint64_t)(mMirror->Image());
	vulkanDataLeft.m_pDevice = *mScene->Instance()->Device();
	vulkanDataLeft.m_pPhysicalDevice = mScene->Instance()->Device()->PhysicalDevice();
	vulkanDataLeft.m_pInstance = *mScene->Instance()->Device()->Instance();
	vulkanDataLeft.m_pQueue = mScene->Instance()->Device()->GraphicsQueue();
	vulkanDataLeft.m_nQueueFamilyIndex = mScene->Instance()->Device()->GraphicsQueueFamily();

	vulkanDataLeft.m_nHeight = mMirror->Height();
	vulkanDataLeft.m_nWidth = mMirror->Width();
	vulkanDataLeft.m_nFormat = mMirror->Format();
	vulkanDataLeft.m_nSampleCount = mMirror->SampleCount();

	vr::Texture_t textureLeft = { &vulkanDataLeft, vr::TextureType_Vulkan, vr::ColorSpace_Auto };

	vr::VRVulkanTextureData_t vulkanDataRight;
	vulkanDataRight.m_nImage = (uint64_t)(mMirror->Image());
	vulkanDataRight.m_pDevice = *mScene->Instance()->Device();
	vulkanDataRight.m_pPhysicalDevice = mScene->Instance()->Device()->PhysicalDevice();
	vulkanDataRight.m_pInstance = *mScene->Instance()->Device()->Instance();
	vulkanDataRight.m_pQueue = mScene->Instance()->Device()->GraphicsQueue();
	vulkanDataRight.m_nQueueFamilyIndex = mScene->Instance()->Device()->GraphicsQueueFamily();

	vulkanDataRight.m_nHeight = mMirror->Height();
	vulkanDataRight.m_nWidth = mMirror->Width();
	vulkanDataRight.m_nFormat = mMirror->Format();
	vulkanDataRight.m_nSampleCount = mMirror->SampleCount();

	vr::Texture_t textureRight = { &vulkanDataRight, vr::TextureType_Vulkan, vr::ColorSpace_Auto };



	vr::EVRCompositorError error;
	error = vr::VRCompositor()->Submit(vr::Eye_Left, &textureLeft, &leftBounds);
	if (error != vr::VRCompositorError_None)
	{
		printf_color(COLOR_RED, "Compositor error on left eye submission: %d\n", error);
	}
	vr::VRCompositor()->Submit(vr::Eye_Right, &textureLeft, &rightBounds);
	if (error != vr::VRCompositorError_None)
	{
		//printf_color(COLOR_RED, "Compositor error on right eye submission: %d\n", error);
	}
}