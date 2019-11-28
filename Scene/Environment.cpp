#include <Scene/Environment.hpp>

#include <Scene/Scene.hpp>
#include <Scene/MeshRenderer.hpp>

using namespace std;

Environment::Environment(Scene* scene) : 
	mScene(scene), mSkybox(nullptr),
	mIncomingLight(float4(4)),
	mRayleighScatterCoef(3),
	mRayleighExtinctionCoef(1),
	mMieScatterCoef(2),
	mMieExtinctionCoef(1),
	mMieG(0.76f),
	mDistanceScale(20),
	mLightColorIntensity(1),
	mAmbientColorIntensity(1),
	mSunIntensity(1),
	mAtmosphereHeight(80000.0f),
	mPlanetRadius(6371000.0f),
	mDensityScale(float4(7994, 1200, 0, 0)),
	mRayleighSct(float4(5.8f, 13.5f, 33.1f, 0) * 0.000001f),
	mMieSct(float4(2, 2, 2, 0) * 0.00001f),
	mMoonSize(.04f) {

	shared_ptr<Material> skyboxMat = make_shared<Material>("Skybox", mScene->AssetManager()->LoadShader("Shaders/skybox.shader"));
	shared_ptr<MeshRenderer> skybox = make_shared<MeshRenderer>("SkyCube");
	skybox->LocalScale(1e23f);
	skybox->Mesh(shared_ptr<Mesh>(Mesh::CreateCube("Cube", mScene->Instance())));
	skybox->Material(skyboxMat);
	mSkybox = skybox.get();
	mScene->AddObject(skybox);

	mSkyboxMaterial = skyboxMat.get();

	mShader = mScene->AssetManager()->LoadShader("Shaders/scatter.shader");
	mMoonTexture = mScene->AssetManager()->LoadTexture("Assets/moon.png");
	mStarTexture = mScene->AssetManager()->LoadCubemap("Assets/stars/posx.png", "Assets/stars/negx.png", "Assets/stars/posy.png", "Assets/stars/negy.png", "Assets/stars/posz.png", "Assets/stars/negz.png", false);

	uint8_t r[256 * 4];
	for (uint32_t i = 0; i < 256; i++) {
		r[4 * i + 0] = rand() % 0xFF;
		r[4 * i + 1] = rand() % 0xFF;
		r[4 * i + 2] = rand() % 0xFF;
		r[4 * i + 3] = 0;
	}
	Texture* randTex = new Texture("Random Vectors", mScene->Instance(), r, 256 * 4, 16, 16, 1, VK_FORMAT_R8G8B8A8_SNORM, 1);

	for (uint32_t i = 0; i < mScene->Instance()->DeviceCount(); i++) {
		Device* device = mScene->Instance()->GetDevice(i);
		auto commandBuffer = device->GetCommandBuffer();

		DevLUT dlut = {};

		dlut.mParticleDensityLUT = new Texture("Particle Density LUT", device, 1024, 1024, 1, VK_FORMAT_R32G32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
		dlut.mSkyboxLUT = new Texture("Skybox LUT", commandBuffer->Device(), 32, 128, 32, VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
		dlut.mSkyboxLUT2 = new Texture("Skybox LUT", commandBuffer->Device(), 32, 128, 32, VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

		// compute particle density LUT
		dlut.mParticleDensityLUT->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, commandBuffer.get());
		dlut.mSkyboxLUT->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, commandBuffer.get());
		dlut.mSkyboxLUT2->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, commandBuffer.get());

		ComputeShader* particleDensity = mShader->GetCompute(commandBuffer->Device(), "ParticleDensityLUT", {});
		vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, particleDensity->mPipeline);

		DescriptorSet* ds = new DescriptorSet("Particle Density", device, particleDensity->mDescriptorSetLayouts[0]);
		ds->CreateStorageTextureDescriptor(dlut.mParticleDensityLUT, particleDensity->mDescriptorBindings.at("_RWParticleDensityLUT").second.binding);
		VkDescriptorSet vds = *ds;
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, particleDensity->mPipelineLayout, 0, 1, &vds, 0, nullptr);
		commandBuffer->PushConstant(particleDensity, "_AtmosphereHeight", &mAtmosphereHeight);
		commandBuffer->PushConstant(particleDensity, "_PlanetRadius", &mPlanetRadius);
		commandBuffer->PushConstant(particleDensity, "_DensityScaleHeight", &mDensityScale);
		vkCmdDispatch(*commandBuffer, 1024, 1024, 1);

		dlut.mParticleDensityLUT->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer.get());


		// compute skybox LUT
		ComputeShader* skybox = mShader->GetCompute(commandBuffer->Device(), "SkyboxLUT", {});
		vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, skybox->mPipeline);

		DescriptorSet* ds2 = new DescriptorSet("Scatter LUT", device, skybox->mDescriptorSetLayouts[0]);
		ds2->CreateStorageTextureDescriptor(dlut.mSkyboxLUT, skybox->mDescriptorBindings.at("_SkyboxLUT").second.binding);
		ds2->CreateStorageTextureDescriptor(dlut.mSkyboxLUT2, skybox->mDescriptorBindings.at("_SkyboxLUT2").second.binding);
		ds2->CreateSampledTextureDescriptor(dlut.mParticleDensityLUT, skybox->mDescriptorBindings.at("_ParticleDensityLUT").second.binding);
		vds = *ds2;
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, skybox->mPipelineLayout, 0, 1, &vds, 0, nullptr);

		float4 scatterR = mRayleighSct * mRayleighScatterCoef;
		float4 scatterM = mMieSct * mMieScatterCoef;
		float4 extinctR = mRayleighSct * mRayleighExtinctionCoef;
		float4 extinctM = mMieSct * mMieExtinctionCoef;

		commandBuffer->PushConstant(skybox, "_AtmosphereHeight", &mAtmosphereHeight);
		commandBuffer->PushConstant(skybox, "_PlanetRadius", &mPlanetRadius);
		commandBuffer->PushConstant(skybox, "_DensityScaleHeight", &mDensityScale);
		commandBuffer->PushConstant(skybox, "_ScatteringR", &scatterR);
		commandBuffer->PushConstant(skybox, "_ScatteringM", &scatterM);
		commandBuffer->PushConstant(skybox, "_ExtinctionR", &extinctR);
		commandBuffer->PushConstant(skybox, "_ExtinctionM", &extinctM);
		commandBuffer->PushConstant(skybox, "_IncomingLight", &mIncomingLight);
		commandBuffer->PushConstant(skybox, "_MieG", &mMieG);
		commandBuffer->PushConstant(skybox, "_SunIntensity", &mSunIntensity);
		vkCmdDispatch(*commandBuffer, 32, 128, 32);

		mDeviceLUTs.emplace(device, dlut);

		dlut.mSkyboxLUT->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer.get());
		dlut.mSkyboxLUT2->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer.get());

		device->Execute(commandBuffer, false)->Wait();

		delete ds;
		delete ds2;
	}

	delete randTex;
}
Environment::~Environment() {
	for (auto t : mCameraLUTs) {
		for (uint32_t i = 0; i < t.first->Device()->MaxFramesInFlight(); i++) {
			safe_delete(t.second[i].mInscatterLUT);
			safe_delete(t.second[i].mOutscatterLUT);
		}
		safe_delete_array(t.second);
	}
	for (auto t : mDeviceLUTs) {
		safe_delete(t.second.mParticleDensityLUT);
		safe_delete(t.second.mSkyboxLUT);
		safe_delete(t.second.mSkyboxLUT2);
	}
	mScene->RemoveObject(mSkybox);
}

void Environment::SetEnvironment(Camera* camera, Material* mat) {
	CamLUT* l = mCameraLUTs.at(camera) + camera->Device()->FrameContextIndex();
	mat->SetParameter("InscatteringLUT", l->mInscatterLUT);
	mat->SetParameter("ExtinctionLUT",   l->mOutscatterLUT);
}

void Environment::PreRender(CommandBuffer* commandBuffer, Camera* camera) {
	if (mCameraLUTs.count(camera) == 0) {
		CamLUT* t = new CamLUT[commandBuffer->Device()->MaxFramesInFlight()];
		memset(t, 0, sizeof(CamLUT) * commandBuffer->Device()->MaxFramesInFlight());
		mCameraLUTs.emplace(camera, t);
	}
	CamLUT* l = mCameraLUTs.at(camera) + commandBuffer->Device()->FrameContextIndex();
	DevLUT* dlut = &mDeviceLUTs.at(camera->Device());

	if (!l->mInscatterLUT) {
		l->mInscatterLUT = new Texture("Inscatter LUT", commandBuffer->Device(), 8, 8, 64, VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
		l->mInscatterLUT->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
	}
	if (!l->mOutscatterLUT) {
		l->mOutscatterLUT = new Texture("Outscatter LUT", commandBuffer->Device(), 8, 8, 64, VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
		l->mOutscatterLUT->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
	}

	l->mInscatterLUT->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, commandBuffer);
	l->mOutscatterLUT->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, commandBuffer);

	float3 r0 = camera->ScreenToWorldRay(float2(0, 0)).mDirection * camera->Far();
	float3 r1 = camera->ScreenToWorldRay(float2(0, 1)).mDirection * camera->Far();
	float3 r2 = camera->ScreenToWorldRay(float2(1, 1)).mDirection * camera->Far();
	float3 r3 = camera->ScreenToWorldRay(float2(1, 0)).mDirection * camera->Far();
	float4 scatterR = mRayleighSct * mRayleighScatterCoef;
	float4 scatterM = mMieSct * mMieScatterCoef;
	float4 extinctR = mRayleighSct * mRayleighExtinctionCoef;
	float4 extinctM = mMieSct * mMieExtinctionCoef;
	float3 cp = camera->WorldPosition();
	float3 lightdir = -mScene->ActiveLights()[0]->WorldRotation().forward();

	#pragma region Precompute scattering
	ComputeShader* scatter = mShader->GetCompute(commandBuffer->Device(), "InscatteringLUT", {});

	vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, scatter->mPipeline);
	DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet("Scatter LUT", scatter->mDescriptorSetLayouts[0]);
	ds->CreateStorageTextureDescriptor(l->mInscatterLUT, scatter->mDescriptorBindings.at("_InscatteringLUT").second.binding);
	ds->CreateStorageTextureDescriptor(l->mOutscatterLUT, scatter->mDescriptorBindings.at("_ExtinctionLUT").second.binding);
	ds->CreateSampledTextureDescriptor(dlut->mParticleDensityLUT, scatter->mDescriptorBindings.at("_ParticleDensityLUT").second.binding);

	VkDescriptorSet vds = *ds;
	vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, scatter->mPipelineLayout, 0, 1, &vds, 0, nullptr);

	commandBuffer->PushConstant(scatter, "_BottomLeftCorner", &r0);
	commandBuffer->PushConstant(scatter, "_TopLeftCorner", &r1);
	commandBuffer->PushConstant(scatter, "_TopRightCorner", &r2);
	commandBuffer->PushConstant(scatter, "_BottomRightCorner", &r3);

	commandBuffer->PushConstant(scatter, "_AtmosphereHeight", &mAtmosphereHeight);
	commandBuffer->PushConstant(scatter, "_PlanetRadius", &mPlanetRadius);
	commandBuffer->PushConstant(scatter, "_LightDir", &lightdir);
	commandBuffer->PushConstant(scatter, "_CameraPos", &cp);
	commandBuffer->PushConstant(scatter, "_DensityScaleHeight", &mDensityScale);
	commandBuffer->PushConstant(scatter, "_ScatteringR", &scatterR);
	commandBuffer->PushConstant(scatter, "_ScatteringM", &scatterM);
	commandBuffer->PushConstant(scatter, "_ExtinctionR", &extinctR);
	commandBuffer->PushConstant(scatter, "_ExtinctionM", &extinctM);
	commandBuffer->PushConstant(scatter, "_IncomingLight", &mIncomingLight);
	commandBuffer->PushConstant(scatter, "_MieG", &mMieG);
	commandBuffer->PushConstant(scatter, "_DistanceScale", &mDistanceScale);
	commandBuffer->PushConstant(scatter, "_SunIntensity", &mSunIntensity);
	vkCmdDispatch(*commandBuffer, 8, 8, 1);
	#pragma endregion


	l->mInscatterLUT->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
	l->mOutscatterLUT->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);


	mSkyboxMaterial->SetParameter("_SkyboxLUT", dlut->mSkyboxLUT);
	mSkyboxMaterial->SetParameter("_SkyboxLUT2", dlut->mSkyboxLUT2);
	mSkyboxMaterial->SetParameter("_MoonTex", mMoonTexture);
	mSkyboxMaterial->SetParameter("_StarCube", mStarTexture);
	mSkyboxMaterial->SetParameter("_MoonDir", float3(0,1,0));
	mSkyboxMaterial->SetParameter("_MoonRight", float3(1,0,0));
	mSkyboxMaterial->SetParameter("_MoonSize", mMoonSize);
	mSkyboxMaterial->SetParameter("_IncomingLight", mIncomingLight.xyz);
	mSkyboxMaterial->SetParameter("_SunDir", lightdir);
	mSkyboxMaterial->SetParameter("_PlanetRadius", mPlanetRadius);
	mSkyboxMaterial->SetParameter("_AtmosphereHeight", mAtmosphereHeight);
	mSkyboxMaterial->SetParameter("_SunIntensity", mSunIntensity);
	mSkyboxMaterial->SetParameter("_MieG", mMieG);
	mSkyboxMaterial->SetParameter("_ScatteringR", scatterR.xyz);
	mSkyboxMaterial->SetParameter("_ScatteringM", scatterM.xyz);
	mSkyboxMaterial->SetParameter("_StarRotation", float4(0, 0, 0, 1));
	mSkyboxMaterial->SetParameter("_StarFade", clamp(lightdir.y*10.f, 0.f, 1.f));
}