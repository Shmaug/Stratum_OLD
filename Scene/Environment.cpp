#include <Scene/Environment.hpp>

#include <Core/Buffer.hpp>
#include <Scene/Scene.hpp>
#include <Scene/MeshRenderer.hpp>

using namespace std;

#pragma pack(push)
#pragma pack(0)
struct ScatterInputs {
	float4 DensityScaleHeight;
	float4 ScatteringR;
	float4 ScatteringM;
	float4 ExtinctionR;
	float4 ExtinctionM;

	float4 BottomLeftCorner;
	float4 BottomRightCorner;
	float4 TopLeftCorner;
	float4 TopRightCorner;

	float3 LightDir;
	float SunIntensity;

	float3 CameraPos;
	float MieG;

	float4 IncomingLight;
	
	float DistanceScale;
	float AtmosphereHeight;
	float PlanetRadius;
	float pad;
};
struct SkyboxInputs{
	float4 MoonRotation;
	float MoonSize;

	float3 IncomingLight;

	float3 SunDir;

	float PlanetRadius;
	float AtmosphereHeight;

	float SunIntensity;
	float MieG;

	float3 ScatteringR;
	float3 ScatteringM;

	float4 StarRotation;
	float StarFade;
};
#pragma pack(pop)

Environment::Environment(Scene* scene) : 
	mTimeOfDay(.25f),
	mScene(scene),
	mIncomingLight(2.3f),
	mRayleighScatterCoef(2),
	mRayleighExtinctionCoef(.5f),
	mMieScatterCoef(4),
	mMieExtinctionCoef(2),
	mMieG(0.76f),
	mDistanceScale(100),
	mSunIntensity(.1f),
	mAtmosphereHeight(80000.0f),
	mPlanetRadius(6371000.0f),
	mDensityScale(float4(20000, 8000, 0, 0)),
	mRayleighSct(float4(5.8f, 13.5f, 33.1f, 0) * .000001f),
	mMieSct(float4(2, 2, 2, 0) * .000001f),
	mAmbientLight(0),
	mMoonSize(.04f),
	mEnableCelestials(false),
	mEnableScattering(false),
	mAtmosphereInitialized(false),
	mEnvironmentTexture(nullptr) {

	mSkyboxMaterial = make_shared<Material>("Skybox", mScene->AssetManager()->LoadShader("Shaders/skybox.stm"));
}
Environment::~Environment() {
	if (mAtmosphereInitialized) {
		mScene->RemoveObject(mSun);
		mScene->RemoveObject(mMoon);
	}

	for (auto t : mCameraLUTs) {
		for (uint32_t i = 0; i < t.first->Device()->MaxFramesInFlight(); i++) {
			safe_delete(t.second[i].mInscatterLUT);
			safe_delete(t.second[i].mOutscatterLUT);
			safe_delete(t.second[i].mLightShaftLUT);
		}
		safe_delete_array(t.second);
	}
	for (auto t : mDeviceLUTs) {
		safe_delete(t.second.mParticleDensityLUT);
		safe_delete(t.second.mSkyboxLUTR);
		safe_delete(t.second.mSkyboxLUTM);
	}
}

void Environment::InitializeAtmosphere() {
	shared_ptr<Light> sun = make_shared<Light>("Sun");
	mScene->AddObject(sun);
	sun->CastShadows(true);
	sun->ShadowDistance(4096);
	sun->Color(float3(1, .99f, .95f));
	sun->LocalRotation(quaternion(float3(PI / 4, PI / 4, 0)));
	sun->Type(LIGHT_TYPE_SUN);
	mSun = sun.get();

	shared_ptr<Light> moon = make_shared<Light>("Moon");
	mScene->AddObject(moon);
	moon->CastShadows(false);
	moon->Color(float3(1));
	moon->LocalRotation(quaternion(float3(PI / 4, PI / 4, 0)));
	moon->Type(LIGHT_TYPE_SUN);
	mMoon = moon.get();

	mShader = mScene->AssetManager()->LoadShader("Shaders/scatter.stm");
	mMoonTexture = mScene->AssetManager()->LoadTexture("Assets/Textures/moon.png");
	mStarTexture = mScene->AssetManager()->LoadCubemap(
		"Assets/Textures/stars/posx.png",
		"Assets/Textures/stars/negx.png",
		"Assets/Textures/stars/posy.png",
		"Assets/Textures/stars/negy.png",
		"Assets/Textures/stars/posz.png",
		"Assets/Textures/stars/negz.png", false);

	uint8_t r[256 * 4];
	for (uint32_t i = 0; i < 256; i++) {
		r[4 * i + 0] = rand() % 0xFF;
		r[4 * i + 1] = rand() % 0xFF;
		r[4 * i + 2] = rand() % 0xFF;
		r[4 * i + 3] = 0;
	}
	Texture* randTex = new Texture("Random Vectors", mScene->Instance()->Device(), r, 256 * 4, 16, 16, 1, VK_FORMAT_R8G8B8A8_UNORM, 1,
		VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT);

	printf("Precomputing scattering LUTs... ");


	ScatterInputs inputs = {};
	inputs.AtmosphereHeight = mAtmosphereHeight;
	inputs.PlanetRadius = mPlanetRadius;
	inputs.DensityScaleHeight = mDensityScale;
	inputs.ScatteringR = mRayleighSct * mRayleighScatterCoef;
	inputs.ScatteringM = mMieSct * mMieScatterCoef;
	inputs.ExtinctionR = mRayleighSct * mRayleighExtinctionCoef;
	inputs.ExtinctionM = mMieSct * mMieExtinctionCoef;
	inputs.IncomingLight = mIncomingLight;
	inputs.MieG = mMieG;
	inputs.SunIntensity = mSunIntensity;

	Device* device = mScene->Instance()->Device();
	Buffer* scatterInputBuffer = new Buffer("Scatter Inputs", device, &inputs, sizeof(ScatterInputs), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	{
		auto commandBuffer = device->GetCommandBuffer();

		DevLUT dlut = {};

		dlut.mParticleDensityLUT = new Texture("Particle Density LUT", device, 1024, 1024, 1, VK_FORMAT_R32G32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
		dlut.mSkyboxLUTR = new Texture("Skybox LUT R", commandBuffer->Device(), 64, 256, 64, VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
		dlut.mSkyboxLUTM = new Texture("Skybox LUT M", commandBuffer->Device(), 64, 256, 64, VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

		// compute particle density LUT
		dlut.mParticleDensityLUT->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, commandBuffer.get());
		dlut.mSkyboxLUTR->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, commandBuffer.get());
		dlut.mSkyboxLUTM->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, commandBuffer.get());

		ComputeShader* particleDensity = mShader->GetCompute("ParticleDensityLUT", {});
		vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, particleDensity->mPipeline);

		DescriptorSet* ds = new DescriptorSet("Particle Density", device, particleDensity->mDescriptorSetLayouts[0]);
		ds->CreateStorageTextureDescriptor(dlut.mParticleDensityLUT, particleDensity->mDescriptorBindings.at("RWParticleDensityLUT").second.binding);
		ds->CreateUniformBufferDescriptor(scatterInputBuffer, 0, scatterInputBuffer->Size(), particleDensity->mDescriptorBindings.at("Inputs").second.binding);
		ds->FlushWrites();
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, particleDensity->mPipelineLayout, 0, 1, *ds, 0, nullptr);
		vkCmdDispatch(*commandBuffer, 128, 128, 1);

		dlut.mParticleDensityLUT->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer.get());

		// compute skybox LUT
		ComputeShader* skyboxc = mShader->GetCompute("SkyboxLUT", {});
		vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, skyboxc->mPipeline);

		DescriptorSet* ds2 = new DescriptorSet("Scatter LUT", device, skyboxc->mDescriptorSetLayouts[0]);
		ds2->CreateStorageTextureDescriptor(dlut.mSkyboxLUTR, skyboxc->mDescriptorBindings.at("SkyboxLUTR").second.binding);
		ds2->CreateStorageTextureDescriptor(dlut.mSkyboxLUTM, skyboxc->mDescriptorBindings.at("SkyboxLUTM").second.binding);
		ds2->CreateSampledTextureDescriptor(dlut.mParticleDensityLUT, skyboxc->mDescriptorBindings.at("ParticleDensityLUT").second.binding);
		ds2->CreateUniformBufferDescriptor(scatterInputBuffer, 0, scatterInputBuffer->Size(), skyboxc->mDescriptorBindings.at("Inputs").second.binding);
		ds2->FlushWrites();
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, skyboxc->mPipelineLayout, 0, 1, *ds2, 0, nullptr);

		vkCmdDispatch(*commandBuffer, 16, 64, 16);

		mDeviceLUTs.emplace(device, dlut);

		dlut.mSkyboxLUTR->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer.get());
		dlut.mSkyboxLUTM->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer.get());

		device->Execute(commandBuffer, false)->Wait();

		delete ds;
		delete ds2;
	}
	{

		auto commandBuffer = device->GetCommandBuffer();

		Buffer ambientBuffer("Ambient", device, 128 * sizeof(float4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		Buffer dirBuffer("Direct", device, 128 * sizeof(float4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		ComputeShader* ambient = mShader->GetCompute("AmbientLightLUT", {});
		vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ambient->mPipeline);

		DescriptorSet* ds = new DescriptorSet("Ambient LUT", device, ambient->mDescriptorSetLayouts[0]);
		ds->CreateStorageBufferDescriptor(&ambientBuffer, 0, ambientBuffer.Size(), ambient->mDescriptorBindings.at("RWAmbientLightLUT").second.binding);
		ds->CreateStorageTextureDescriptor(randTex, ambient->mDescriptorBindings.at("RandomVectors").second.binding);
		ds->CreateSampledTextureDescriptor(mDeviceLUTs.at(device).mParticleDensityLUT, ambient->mDescriptorBindings.at("ParticleDensityLUT").second.binding);
		ds->CreateUniformBufferDescriptor(scatterInputBuffer, 0, scatterInputBuffer->Size(), ambient->mDescriptorBindings.at("Inputs").second.binding);
		ds->FlushWrites();
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ambient->mPipelineLayout, 0, 1, *ds, 0, nullptr);

		vkCmdDispatch(*commandBuffer, 2, 1, 1);


		ComputeShader* direct = mShader->GetCompute("DirectLightLUT", {});
		vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, direct->mPipeline);

		DescriptorSet* ds2 = new DescriptorSet("Ambient LUT", device, direct->mDescriptorSetLayouts[0]);
		ds2->CreateStorageBufferDescriptor(&dirBuffer, 0, dirBuffer.Size(), direct->mDescriptorBindings.at("RWDirectLightLUT").second.binding);
		ds2->CreateSampledTextureDescriptor(mDeviceLUTs.at(device).mParticleDensityLUT, direct->mDescriptorBindings.at("ParticleDensityLUT").second.binding);
		ds2->CreateUniformBufferDescriptor(scatterInputBuffer, 0, scatterInputBuffer->Size(), direct->mDescriptorBindings.at("Inputs").second.binding);
		ds2->FlushWrites();
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, direct->mPipelineLayout, 0, 1, *ds2, 0, nullptr);
		
		vkCmdDispatch(*commandBuffer, 2, 1, 1);

		device->Execute(commandBuffer, false)->Wait();

		std::memcpy(mAmbientLUT, ambientBuffer.MappedData(), 128 * sizeof(float4));
		std::memcpy(mDirectionalLUT, dirBuffer.MappedData(), 128 * sizeof(float4));

		delete ds;
		delete ds2;
	}

	printf("Done\n");

	delete randTex;

	mAtmosphereInitialized = true;
}

void Environment::SetEnvironment(Camera* camera, Material* mat) {
	if (mEnableScattering) {
		if (!mAtmosphereInitialized) InitializeAtmosphere();
		CamLUT* l = mCameraLUTs.at(camera) + camera->Device()->FrameContextIndex();
		mat->SetParameter("InscatteringLUT", l->mInscatterLUT);
		mat->SetParameter("ExtinctionLUT", l->mOutscatterLUT);
		mat->SetParameter("LightShaftLUT", l->mLightShaftLUT);
		mat->SetParameter("AmbientLight", mAmbientLight);
		mat->SetUniformBuffer("ScatterParams", 0, sizeof(ScatteringParameters), mScatterParamBuffer);
		mat->EnableKeyword("ENABLE_SCATTERING");
		mat->DisableKeyword("ENVIRONMENT_TEXTURE");
		mat->DisableKeyword("ENVIRONMENT_TEXTURE_HDR");
	} else if (mEnvironmentTexture) {
		if (mat == mSkyboxMaterial.get() && (mEnvironmentTexture->Format() == VK_FORMAT_R32G32B32A32_SFLOAT || mEnvironmentTexture->Format() == VK_FORMAT_R16G16B16A16_SFLOAT)) {
			mat->DisableKeyword("ENVIRONMENT_TEXTURE");
			mat->EnableKeyword("ENVIRONMENT_TEXTURE_HDR");
		} else {
			mat->EnableKeyword("ENVIRONMENT_TEXTURE");
			mat->DisableKeyword("ENVIRONMENT_TEXTURE_HDR");
		}
		mat->DisableKeyword("ENABLE_SCATTERING");
		mat->SetParameter("EnvironmentTexture", mEnvironmentTexture);
	}else{
		mat->DisableKeyword("ENVIRONMENT_TEXTURE");
		mat->DisableKeyword("ENVIRONMENT_TEXTURE_HDR");
		mat->DisableKeyword("ENABLE_SCATTERING");
	}
	mat->SetParameter("AmbientLight", mAmbientLight);
}

void Environment::Update() {
	if (mEnableCelestials) {
		if (!mAtmosphereInitialized) InitializeAtmosphere();
		mSun->LocalRotation(quaternion(float3(0, radians(23.5f), radians(23.5f))) * quaternion(float3(mTimeOfDay * PI * 2, 0, 0)));
		mMoon->LocalRotation(quaternion(float3(0, radians(70.f), radians(70.f))) * quaternion(float3(PI + mTimeOfDay * PI * 2, 0, 0)));

		float f = clamp(10 * (-mMoon->WorldRotation().forward().y + .1f), 0.f, 1.f);
		mMoon->Intensity(.05f * f * f);

		f = clamp(10 * (-mSun->WorldRotation().forward().y + .1f), 0.f, 1.f);
		mSun->Intensity(1 * f * f);

		float cosAngle = dot(float3(0, 1, 0), -mSun->WorldRotation().forward());
		float u = (cosAngle + 0.1f) / 1.1f;
		u = u * 128;
		int index0 = (int)u;
		int index1 = index0 + 1;
		float weight1 = u - index0;
		float weight0 = 1 - weight1;
		index0 = clamp(index0, 0, 128 - 1);
		index1 = clamp(index1, 0, 128 - 1);

		mSun->Color((1.055f * pow((mDirectionalLUT[index0] * weight0 + mDirectionalLUT[index1] * weight1).rgb, 1.f / 2.4f) - .055f));
		mAmbientLight = .1f * length(1.055f * pow(mAmbientLUT[index0] * weight0 + mAmbientLUT[index1] * weight1, 1.f / 2.4f) - .055f);
	} else {
		if (mAtmosphereInitialized) {
			mSun->mEnabled = false;
			mMoon->mEnabled = false;
		}
	}
}

void Environment::PreRender(CommandBuffer* commandBuffer, Camera* camera) {
	if (mEnableScattering) {
		if (!mAtmosphereInitialized) InitializeAtmosphere();
		if (mCameraLUTs.count(camera) == 0) {
			CamLUT* t = new CamLUT[commandBuffer->Device()->MaxFramesInFlight()];
			memset(t, 0, sizeof(CamLUT) * commandBuffer->Device()->MaxFramesInFlight());
			mCameraLUTs.emplace(camera, t);
		}
		CamLUT* l = mCameraLUTs.at(camera) + commandBuffer->Device()->FrameContextIndex();
		DevLUT* dlut = &mDeviceLUTs.at(camera->Device());

		if (l->mLightShaftLUT && (l->mLightShaftLUT->Width() != camera->FramebufferWidth() / 2 || l->mLightShaftLUT->Height() != camera->FramebufferHeight() / 2))
			safe_delete(l->mLightShaftLUT);

		if (!l->mInscatterLUT) {
			l->mInscatterLUT = new Texture("Inscatter LUT", commandBuffer->Device(), 32, 32, 256, VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
			l->mInscatterLUT->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
		}
		if (!l->mOutscatterLUT) {
			l->mOutscatterLUT = new Texture("Outscatter LUT", commandBuffer->Device(), 32, 32, 256, VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
			l->mOutscatterLUT->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
		}
		if (!l->mLightShaftLUT) {
			l->mLightShaftLUT = new Texture("Light Shaft LUT", commandBuffer->Device(), camera->FramebufferWidth() / 2, camera->FramebufferHeight() / 2, 1, VK_FORMAT_R32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
			l->mLightShaftLUT->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
		}

		l->mInscatterLUT->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, commandBuffer);
		l->mOutscatterLUT->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, commandBuffer);
		l->mLightShaftLUT->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, commandBuffer);

		ScatterInputs inputs = {};
		inputs.BottomLeftCorner  = camera->ClipToWorld(float3(-1, 1, 1));
		inputs.TopLeftCorner = camera->ClipToWorld(float3(-1, -1, 1));
		inputs.TopRightCorner = camera->ClipToWorld(float3(1, -1, 1));
		inputs.BottomRightCorner = camera->ClipToWorld(float3(1, 1, 1));
		inputs.AtmosphereHeight = mAtmosphereHeight;
		inputs.PlanetRadius = mPlanetRadius;
		inputs.LightDir = -normalize(mSun->WorldRotation().forward());
		inputs.CameraPos = camera->WorldPosition();
		inputs.DensityScaleHeight = mDensityScale;
		inputs.ScatteringR = mRayleighSct * mRayleighScatterCoef;
		inputs.ScatteringM = mMieSct * mMieScatterCoef;
		inputs.ExtinctionR = mRayleighSct * mRayleighExtinctionCoef;
		inputs.ExtinctionM = mMieSct * mMieExtinctionCoef;
		inputs.IncomingLight = mSun->mEnabled ? mIncomingLight * clamp(length(mSun->Color()) * mSun->Intensity(), 0.f, 1.f) : 0;
		inputs.MieG = mMieG;
		inputs.DistanceScale = mDistanceScale;
		inputs.SunIntensity = mSunIntensity;

		Device* device = mScene->Instance()->Device();
		Buffer* scatterInputBuffer = device->GetTempBuffer("Scatter Inputs", sizeof(ScatterInputs), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		memcpy(scatterInputBuffer->MappedData(), &inputs, sizeof(ScatterInputs));

		#pragma region Precompute scattering
		ComputeShader* scatter = mShader->GetCompute("InscatteringLUT", {});

		vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, scatter->mPipeline);
		DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet("Scatter LUT", scatter->mDescriptorSetLayouts[0]);
		ds->CreateStorageTextureDescriptor(l->mInscatterLUT, scatter->mDescriptorBindings.at("InscatteringLUT").second.binding);
		ds->CreateStorageTextureDescriptor(l->mOutscatterLUT, scatter->mDescriptorBindings.at("ExtinctionLUT").second.binding);
		ds->CreateSampledTextureDescriptor(dlut->mParticleDensityLUT, scatter->mDescriptorBindings.at("ParticleDensityLUT").second.binding);
		ds->CreateUniformBufferDescriptor(scatterInputBuffer, 0, sizeof(ScatterInputs), scatter->mDescriptorBindings.at("Inputs").second.binding);
		ds->FlushWrites();
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, scatter->mPipelineLayout, 0, 1, *ds, 0, nullptr);

		vkCmdDispatch(*commandBuffer, l->mInscatterLUT->Width() / 8, l->mInscatterLUT->Width() / 8, 1);
		#pragma endregion
		/*
		#pragma region Precompute light shafts
		ComputeShader* shaft = mShader->GetCompute(commandBuffer->Device(), "LightShaftLUT", {});

		vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, shaft->mPipeline);
		ds = commandBuffer->Device()->GetTempDescriptorSet("Light Shaft LUT", shaft->mDescriptorSetLayouts[0]);
		ds->CreateStorageTextureDescriptor(l->mLightShaftLUT, shaft->mDescriptorBindings.at("LightShaftLUT").second.binding);
		ds->CreateSampledTextureDescriptor(camera->DepthFramebuffer(), shaft->mDescriptorBindings.at("DepthTexture").second.binding);
		ds->CreateStorageBufferDescriptor(mScene->LightBuffer(commandBuffer->Device()), shaft->mDescriptorBindings.at("Lights").second.binding);
		ds->CreateStorageBufferDescriptor(mScene->ShadowBuffer(commandBuffer->Device()), shaft->mDescriptorBindings.at("Shadows").second.binding);
		ds->CreateSampledTextureDescriptor(mScene->ShadowAtlas(commandBuffer->Device()), shaft->mDescriptorBindings.at("ShadowAtlas").second.binding);
		ds->CreateUniformBufferDescriptor(scatterInputBuffer, 0, sizeof(ScatterInputs), shaft->mDescriptorBindings.at("Inputs").second.binding);
		ds->FlushWrites();
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, shaft->mPipelineLayout, 0, 1, *ds, 0, nullptr);

		commandBuffer->PushConstant(shaft, "_CameraPos", &cp);
		vkCmdDispatch(*commandBuffer, (l->mLightShaftLUT->Width() + 7) / 8, (l->mLightShaftLUT->Height() + 7) / 8, 1);
		#pragma endregion
		*/
		l->mInscatterLUT->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
		l->mOutscatterLUT->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
		l->mLightShaftLUT->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);

		mSkyboxMaterial->SetParameter("SkyboxLUTR", dlut->mSkyboxLUTR);
		mSkyboxMaterial->SetParameter("SkyboxLUTM", dlut->mSkyboxLUTM);
		mSkyboxMaterial->SetParameter("LightShaftLUT", l->mLightShaftLUT);
		mSkyboxMaterial->SetParameter("MoonTexture", mMoonTexture);
		mSkyboxMaterial->SetParameter("StarTexture", mStarTexture);

		ScatteringParameters params = {};
		params.MoonRotation = inverse(mMoon->WorldRotation()).xyzw;
		params.MoonSize = mMoonSize;
		params.IncomingLight = mIncomingLight.xyz;
		params.SunDir = inputs.LightDir;
		params.PlanetRadius = mPlanetRadius;
		params.AtmosphereHeight = mAtmosphereHeight;
		params.SunIntensity = mSunIntensity;
		params.MieG = mMieG;
		params.ScatteringR = inputs.ScatteringR.xyz;
		params.ScatteringM = inputs.ScatteringM.xyz;
		params.StarRotation = quaternion(float3(-mTimeOfDay * PI * 2, 0, 0)).xyzw;
		params.StarFade = 100 * clamp(inputs.LightDir.y, 0.f, 1.f);

		mScatterParamBuffer = device->GetTempBuffer("Scattering", sizeof(ScatteringParameters), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		memcpy(mScatterParamBuffer->MappedData(), &params, sizeof(ScatteringParameters));
	}

	SetEnvironment(camera, mSkyboxMaterial.get());
}