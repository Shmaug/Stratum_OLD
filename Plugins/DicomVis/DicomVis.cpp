#include <Scene/MeshRenderer.hpp>
#include <Content/Font.hpp>
#include <Scene/GUI.hpp>
#include <Util/Profiler.hpp>

#include <Core/EnginePlugin.hpp>
#include <assimp/pbrmaterial.h>

#include "ImageLoader.hpp"

using namespace std;

class DicomVis : public EnginePlugin {
private:
	Scene* mScene;
	vector<Object*> mObjects;
	Object* mSelected;

	uint32_t mFrameIndex;

	float3 mVolumePosition;
	quaternion mVolumeRotation;
	float3 mVolumeScale;
	float3 mDirectLight;

	bool mPhysicalShading;
	bool mLighting;
	bool mColorize;
	float mStepSize;
	float mLightStep;
	float mTransferMin;
	float mTransferMax;
	float mDensity;
	float mRemapMin;
	float mRemapMax;
	float mVolumeScatter;
	float mVolumeExtinction;
	float mRoughness;

	Texture* mRawVolume;
	Texture* mRawMask;
	Texture* mGradientAlpha;
	Texture* mTransferLUT;
	bool mRawVolumeNew;

	bool mGradientAlphaDirty;
	bool mTransferLUTDirty;
	
	Camera* mMainCamera;

	MouseKeyboardInput* mInput;

	float mZoom;

	bool mShowPerformance;
	bool mSnapshotPerformance;
	ProfilerSample mProfilerFrames[PROFILER_FRAME_COUNT - 1];
	uint32_t mSelectedFrame;

	std::thread mScanThread;
	bool mScanDone;

	std::unordered_map<std::string, ImageStackType> mDataFolders;

	PLUGIN_EXPORT void ScanFolders() {
		string path = "/Data";
		for (uint32_t i = 0; i < mScene->Instance()->CommandLineArguments().size(); i++)
			if (mScene->Instance()->CommandLineArguments()[i] == "--datapath") {
				i++;
				if (i < mScene->Instance()->CommandLineArguments().size())
					path = mScene->Instance()->CommandLineArguments()[i];
			}
		if (!fs::exists(path)) path = "/Data";
		if (!fs::exists(path)) path = "/data";
		if (!fs::exists(path)) path = "~/Data";
		if (!fs::exists(path)) path = "~/data";
		if (!fs::exists(path)) path = "C:/Data";
		if (!fs::exists(path)) path = "D:/Data";
		if (!fs::exists(path)) path = "E:/Data";
		if (!fs::exists(path)) path = "F:/Data";
		if (!fs::exists(path)) path = "G:/Data";
		if (!fs::exists(path)) {
			fprintf_color(COLOR_RED, stderr, "DicomVis: Could not locate datapath. Please specify with --datapath <path>\n");
			return;
		}

		for (const auto& p : fs::recursive_directory_iterator(path)) {
			if (!p.is_directory()) continue;
			ImageStackType type = ImageLoader::FolderStackType(p.path());
			if (type == IMAGE_STACK_NONE) continue;
			mDataFolders[p.path().string()] = type;
		}

		mScanDone = true;
	}

public:
	PLUGIN_EXPORT DicomVis(): mScene(nullptr), mSelected(nullptr), mShowPerformance(false), mSnapshotPerformance(false),
		mFrameIndex(0), mRawVolume(nullptr), mRawMask(nullptr), mGradientAlpha(nullptr), mTransferLUT(nullptr), mRawVolumeNew(false), mGradientAlphaDirty(false), mTransferLUTDirty(false),
		mColorize(false), mPhysicalShading(false), mLighting(false),
		mVolumePosition(float3(0,0,0)), mVolumeRotation(quaternion(0,0,0,1)), mDirectLight(1.f),
		mDensity(500.f), mRemapMin(.125f), mRemapMax(1.f), mStepSize(.001f), mLightStep(.01f), mTransferMin(.01f), mTransferMax(.5f),
		mVolumeScatter(.2f), mVolumeExtinction(.3f), mRoughness(.8f) {
		mEnabled = true;
	}
	PLUGIN_EXPORT ~DicomVis() {
		if (mScanThread.joinable()) mScanThread.join();
		safe_delete(mRawVolume);
		safe_delete(mRawMask);
		safe_delete(mGradientAlpha);
		safe_delete(mTransferLUT);
		for (Object* obj : mObjects)
			mScene->RemoveObject(obj);
	}

	PLUGIN_EXPORT bool Init(Scene* scene) override {
		mScene = scene;
		mInput = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

		mZoom = 3.f;

		shared_ptr<Camera> camera = make_shared<Camera>("Camera", mScene->Instance()->Window());
		mScene->AddObject(camera);
		camera->Near(.01f);
		camera->Far(800.f);
		camera->FieldOfView(radians(65.f));
		camera->LocalPosition(0, 1.6f, -mZoom);
		mMainCamera = camera.get();
		mObjects.push_back(mMainCamera);

		mScene->Environment()->EnableCelestials(false);
		mScene->Environment()->EnableScattering(false);
		mScene->Environment()->EnvironmentTexture(mScene->AssetManager()->LoadTexture("Assets/Textures/photo_studio_01_2k.hdr"));
		mScene->Environment()->AmbientLight(.9f);

		mScanDone = false;
		mScanThread = thread(&DicomVis::ScanFolders, this);

		return true;
	}
	PLUGIN_EXPORT void Update(CommandBuffer* commandBuffer) override {
		if (mInput->KeyDownFirst(KEY_F1))
			mScene->DrawGizmos(!mScene->DrawGizmos());
		if (mInput->KeyDownFirst(KEY_TILDE))
			mShowPerformance = !mShowPerformance;

		// Snapshot profiler frames
		if (mInput->KeyDownFirst(KEY_F3)) {
			mSnapshotPerformance = !mSnapshotPerformance;
			if (mSnapshotPerformance) {
				mSelectedFrame = PROFILER_FRAME_COUNT;
				queue<pair<ProfilerSample*, const ProfilerSample*>> samples;
				for (uint32_t i = 0; i < PROFILER_FRAME_COUNT - 1; i++) {
					mProfilerFrames[i].mParent = nullptr;
					samples.push(make_pair(mProfilerFrames + i, Profiler::Frames() + ((i + Profiler::CurrentFrameIndex() + 2) % PROFILER_FRAME_COUNT)));
					while (samples.size()) {
						auto p = samples.front();
						samples.pop();

						p.first->mStartTime = p.second->mStartTime;
						p.first->mDuration = p.second->mDuration;
						strncpy(p.first->mLabel, p.second->mLabel, PROFILER_LABEL_SIZE);
						p.first->mChildren.resize(p.second->mChildren.size());

						auto it2 = p.second->mChildren.begin();
						for (auto it = p.first->mChildren.begin(); it != p.first->mChildren.end(); it++, it2++) {
							it->mParent = p.first;
							samples.push(make_pair(&*it, &*it2));
						}
					}
				}
			}
		}

		if (mInput->GetPointer(0)->mLastGuiHitT < 0) {
			if (mInput->ScrollDelta() != 0){
				mZoom = clamp(mZoom - mInput->ScrollDelta() * .025f, -1.f, 5.f);
				mMainCamera->LocalPosition(0, 1.6f, -mZoom);

				mFrameIndex = 0;
			}
			if (mInput->KeyDown(MOUSE_LEFT)) {
				float3 axis = mMainCamera->WorldRotation() * float3(0, 1, 0) * mInput->CursorDelta().x + mMainCamera->WorldRotation() * float3(1, 0, 0) * mInput->CursorDelta().y;
				if (dot(axis, axis) > .001f){
					mVolumeRotation = quaternion(length(axis) * .003f, -normalize(axis)) * mVolumeRotation;
					mFrameIndex = 0;
				}
			}
		}
	}

	PLUGIN_EXPORT void PreRender(CommandBuffer* commandBuffer, Camera* camera, PassType pass) override {
		if (pass != PASS_MAIN || camera != mScene->Cameras()[0]) return;

		Font* reg14 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Regular.ttf", 14);
		Font* sem11 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-SemiBold.ttf", 11);
		Font* sem16 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-SemiBold.ttf", 16);
		Font* bld24 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Bold.ttf", 24);

		float2 s(camera->FramebufferWidth(), camera->FramebufferHeight());
		float2 c = mInput->CursorPos();
		c.y = s.y - c.y;
		
		if (mShowPerformance) {
			char tmpText[64];
			snprintf(tmpText, 64, "%.2f fps\n", mScene->FPS());
			GUI::DrawString(sem16, tmpText, 1.f, float2(5, camera->FramebufferHeight() - 18), 18.f, TEXT_ANCHOR_MIN, TEXT_ANCHOR_MAX);

			#ifdef PROFILER_ENABLE
			const uint32_t pointCount = PROFILER_FRAME_COUNT - 1;
			
			float graphHeight = 100;

			float2 points[pointCount];
			float m = 0;
			for (uint32_t i = 0; i < pointCount; i++) {
				points[i].y = (mSnapshotPerformance ? mProfilerFrames[i] : Profiler::Frames()[(i + Profiler::CurrentFrameIndex() + 2) % PROFILER_FRAME_COUNT]).mDuration.count() * 1e-6f;
				points[i].x = (float)i / (pointCount - 1.f);
				m = fmaxf(points[i].y, m);
			}
			m = fmaxf(m, 5.f) + 3.f;
			for (uint32_t i = 0; i < pointCount; i++)
				points[i].y /= m;

			GUI::Rect(fRect2D(0, 0, s.x, graphHeight), float4(.1f, .1f, .1f, 1));
			GUI::Rect(fRect2D(0, graphHeight - 1, s.x, 2), float4(.2f, .2f, .2f, 1));

			snprintf(tmpText, 64, "%.1fms", m);
			GUI::DrawString(sem11, tmpText, float4(.6f, .6f, .6f, 1.f), float2(2, graphHeight - 10), 11.f);

			for (float i = 1; i < 3; i++) {
				float x = m * i / 3.f;
				snprintf(tmpText, 32, "%.1fms", x);
				GUI::Rect(fRect2D(0, graphHeight * (i / 3.f) - 1, s.x, 1), float4(.2f, .2f, .2f, 1));
				GUI::DrawString(sem11, tmpText, float4(.6f, .6f, .6f, 1.f), float2(2, graphHeight * (i / 3.f) + 2), 11.f);
			}

			GUI::DrawScreenLine(points, pointCount, 1.5f, 0, float2(s.x, graphHeight), float4(.2f, 1.f, .2f, 1.f));

			if (mSnapshotPerformance) {
				if (c.y < 100) {
					uint32_t hvr = (uint32_t)((c.x / s.x) * (PROFILER_FRAME_COUNT - 2) + .5f);
					GUI::Rect(fRect2D(s.x * hvr / (PROFILER_FRAME_COUNT - 2), 0, 1, graphHeight), float4(1, 1, 1, .15f));
					if (mInput->KeyDown(MOUSE_LEFT))
						mSelectedFrame = hvr;
				}

				if (mSelectedFrame < PROFILER_FRAME_COUNT - 1) {
					ProfilerSample* selected = nullptr;
					float sampleHeight = 20;

					// selection line
					GUI::Rect(fRect2D(s.x * mSelectedFrame / (PROFILER_FRAME_COUNT - 2), 0, 1, graphHeight), 1);

					float id = 1.f / (float)mProfilerFrames[mSelectedFrame].mDuration.count();

					queue<pair<ProfilerSample*, uint32_t>> samples;
					samples.push(make_pair(mProfilerFrames + mSelectedFrame, 0));
					while (samples.size()) {
						auto p = samples.front();
						samples.pop();

						float2 pos(s.x * (p.first->mStartTime - mProfilerFrames[mSelectedFrame].mStartTime).count() * id, graphHeight + 20 + sampleHeight * p.second);
						float2 size(s.x * (float)p.first->mDuration.count() * id, sampleHeight);
						float4 col(0, 0, 0, 1);

						if (c.x > pos.x&& c.y > pos.y && c.x < pos.x + size.x && c.y < pos.y + size.y) {
							selected = p.first;
							col.rgb = 1;
						}

						GUI::Rect(fRect2D(pos, size), col);
						GUI::Rect(fRect2D(pos + 1, size - 2), float4(.3f, .9f, .3f, 1));

						for (auto it = p.first->mChildren.begin(); it != p.first->mChildren.end(); it++)
							samples.push(make_pair(&*it, p.second + 1));
					}

					if (selected) {
						snprintf(tmpText, 64, "%s: %.2fms\n", selected->mLabel, selected->mDuration.count() * 1e-6f);
						GUI::Rect(fRect2D(0, graphHeight, s.x, 20), float4(0,0,0,.8f));
						GUI::DrawString(reg14, tmpText, 1, float2(s.x * .5f, graphHeight + 8), 14.f, TEXT_ANCHOR_MID, TEXT_ANCHOR_MID);
					}
				}

			}
			#endif
		}

		if (!mScanDone) {
			GUI::BeginScreenLayout(LAYOUT_VERTICAL, fRect2D(10, s.y * .5f - 30, 300, 60), float4(.3f, .3f, .3f, 1), 10);
			GUI::LayoutLabel(bld24, "Scanning...", 24, 38, 0, 1);
			GUI::EndLayout();
			return;
		}
		if (mScanThread.joinable()) mScanThread.join();

		GUI::BeginScreenLayout(LAYOUT_VERTICAL, fRect2D(10, s.y * .5f - 425, 300, 850), float4(.3f, .3f, .3f, 1), 10);

		GUI::LayoutLabel(bld24, "Load Data Set", 24, 38, 0, 1);
		GUI::LayoutSeparator(.5f, 1);

		GUI::BeginScrollSubLayout(175, mDataFolders.size() * 24, float4(.2f, .2f, .2f, 1), 5);
		for (const auto& p : mDataFolders)
			if (GUI::LayoutButton(sem16, fs::path(p.first).stem().string(), 16, 24, float4(.2f, .2f, .2f, 1), 1, 2, TEXT_ANCHOR_MID))
				LoadVolume(commandBuffer, p.first, p.second);
		GUI::EndLayout();

		float sliderHeight = 12;

		if (GUI::LayoutButton(sem16, "Colorize", 16, 24, mColorize ? float4(.5f, .5f, .5f, 1) : float4(.25f, .25f, .25f, 1), 1)) {
			mColorize = !mColorize;
			mTransferLUTDirty = true;
			mFrameIndex = 0;
		}
		if (GUI::LayoutButton(sem16, "Physical Shading", 16, 24, mPhysicalShading ? float4(.5f, .5f, .5f, 1) : float4(.25f, .25f, .25f, 1), 1)) {
			mPhysicalShading = !mPhysicalShading;
			mFrameIndex = 0;
		}
		if (!mPhysicalShading && GUI::LayoutButton(sem16, "Lighting", 16, 24, mLighting ? float4(.5f, .5f, .5f, 1) : float4(.25f, .25f, .25f, 1), 1)) {
			mLighting = !mLighting;
			mFrameIndex = 0;
		}
		GUI::LayoutSeparator(.5f, 1, 3);

		GUI::LayoutLabel(bld24, "Render Settings", 18, 24, 0, 1);
		GUI::LayoutSpace(8);

		GUI::LayoutLabel(sem16, "Step Size: " + to_string(mStepSize), 14, 14, 0, 1, 0, TEXT_ANCHOR_MIN);
		if (GUI::LayoutSlider(mStepSize, .0001f, .01f, sliderHeight, float4(.5f, .5f, .5f, 1), 4)) mFrameIndex = 0;
		GUI::LayoutLabel(sem16, "Density: " + to_string(mDensity), 14, 14, 0, 1, 0, TEXT_ANCHOR_MIN);
		if (GUI::LayoutSlider(mDensity, 10, 50000.f, sliderHeight, float4(.5f, .5f, .5f, 1), 4)) mFrameIndex = 0;
		GUI::LayoutSpace(20);

		GUI::LayoutLabel(sem16, "Remap Min: " + to_string(mRemapMin), 14, 14, 0, 1, 0, TEXT_ANCHOR_MIN);
		if (GUI::LayoutSlider(mRemapMin, 0, 1, sliderHeight, float4(.5f, .5f, .5f, 1), 4)) {
			mGradientAlphaDirty = true;
			mTransferLUTDirty = true;
			mFrameIndex = 0;
		}
		GUI::LayoutLabel(sem16, "Remap Max: " + to_string(mRemapMax), 14, 14, 0, 1, 0, TEXT_ANCHOR_MIN);
		if (GUI::LayoutSlider(mRemapMax, 0, 1, sliderHeight, float4(.5f, .5f, .5f, 1), 4)) {
			mGradientAlphaDirty = true;
			mTransferLUTDirty = true;
			mFrameIndex = 0;
		}

		if (mColorize) {
			GUI::LayoutSpace(20);

			GUI::LayoutLabel(sem16, "Transfer Min: " + to_string(mTransferMin), 14, 14, 0, 1, 0, TEXT_ANCHOR_MIN);
			if (GUI::LayoutSlider(mTransferMin, 0, 1, sliderHeight, float4(.5f, .5f, .5f, 1), 4)) {
				mTransferLUTDirty = true;
				mFrameIndex = 0;
			}
			GUI::LayoutLabel(sem16, "Transfer Max: " + to_string(mTransferMax), 14, 14, 0, 1, 0, TEXT_ANCHOR_MIN);
			if (GUI::LayoutSlider(mTransferMax, 0, 1, sliderHeight, float4(.5f, .5f, .5f, 1), 4)) {
				mTransferLUTDirty = true;
				mFrameIndex = 0;
			}
		}
		if (mPhysicalShading || mLighting) {
			GUI::LayoutSpace(20);

			float a = mScene->Environment()->AmbientLight().x;
			float d = mDirectLight.x;

			GUI::LayoutLabel(sem16, "Roughness: " + to_string(mRoughness), 14, 14, 0, 1, 0, TEXT_ANCHOR_MIN);
			if (GUI::LayoutSlider(mRoughness, 0, 1, sliderHeight, float4(.5f, .5f, .5f, 1), 4)) {
				mFrameIndex = 0;
			}

			GUI::LayoutLabel(sem16, "Ambient Light: " + to_string(a), 14, 14, 0, 1, 0, TEXT_ANCHOR_MIN);
			if (GUI::LayoutSlider(a, 0, 3, sliderHeight, float4(.5f, .5f, .5f, 1), 4)) {
				mScene->Environment()->AmbientLight(a);
				mFrameIndex = 0;
			}
			GUI::LayoutLabel(sem16, "Direct Light: " + to_string(d), 14, 14, 0, 1, 0, TEXT_ANCHOR_MIN);
			if (GUI::LayoutSlider(d, 0, 3, sliderHeight, float4(.5f, .5f, .5f, 1), 4)) {
				mDirectLight = d;
				mFrameIndex = 0;
			}

			if (mPhysicalShading) {
				GUI::LayoutLabel(sem16, "Light Step: " + to_string(mLightStep), 14, 14, 0, 1, 0, TEXT_ANCHOR_MIN);
				if (GUI::LayoutSlider(mLightStep, 0.0005f, .05f, sliderHeight, float4(.5f, .5f, .5f, 1), 4)) mFrameIndex = 0;
				GUI::LayoutLabel(sem16, "Extinction: " + to_string(mVolumeExtinction), 14, 14, 0, 1, 0, TEXT_ANCHOR_MIN);
				if (GUI::LayoutSlider(mVolumeExtinction, 0, 1, sliderHeight, float4(.5f, .5f, .5f, 1), 4)) mFrameIndex = 0;
				GUI::LayoutLabel(sem16, "Scattering: " + to_string(mVolumeScatter), 14, 14, 0, 1, 0, TEXT_ANCHOR_MIN);
				if (GUI::LayoutSlider(mVolumeScatter, 0, 3, sliderHeight, float4(.5f, .5f, .5f, 1), 4)) mFrameIndex = 0;
			}
		}

		GUI::EndLayout();
	}

	PLUGIN_EXPORT void PostProcess(CommandBuffer* commandBuffer, Camera* camera) override {
		if (!mRawVolume) return;
		
		if (mRawVolumeNew) {
			mRawVolume->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, commandBuffer);
			if (mGradientAlpha) mGradientAlpha->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, commandBuffer);
			if (mRawMask) mRawMask->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, commandBuffer);
			mRawVolumeNew = false;
		}
		
		float2 res(camera->FramebufferWidth(), camera->FramebufferHeight());
		float4x4 ivp[2];
		ivp[0] = camera->InverseViewProjection(EYE_LEFT);
		ivp[1] = camera->InverseViewProjection(EYE_RIGHT);
		float3 cp[2];
		cp[0] = camera->InverseView(EYE_LEFT)[3].xyz;
		cp[1] = camera->InverseView(EYE_RIGHT)[3].xyz;
		float4 ivr = inverse(mVolumeRotation).xyzw;
		float3 ivs = 1.f / mVolumeScale;
		float near = camera->Near();
		float far = camera->Far();
		
		float remapRange = 1.f / (mRemapMax - mRemapMin);

		float3 lightCol = 2;
		float3 lightDir = normalize(float3(.1f, .5f, -1));
		
		if (mGradientAlpha && mGradientAlphaDirty) {
			set<string> kw;
			if (mRawMask) kw.emplace("READ_MASK");
			ComputeShader* shader = mScene->AssetManager()->LoadShader("Shaders/precompute.stm")->GetCompute("ComputeGradient", kw);
			vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, shader->mPipeline);
			
			DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet("CopyRaw", shader->mDescriptorSetLayouts[0]);
			ds->CreateStorageTextureDescriptor(mRawVolume, shader->mDescriptorBindings.at("RawVolume").second.binding, VK_IMAGE_LAYOUT_GENERAL);
			if (mRawMask) ds->CreateStorageTextureDescriptor(mRawMask, shader->mDescriptorBindings.at("RawMask").second.binding, VK_IMAGE_LAYOUT_GENERAL);
			ds->CreateStorageTextureDescriptor(mGradientAlpha, shader->mDescriptorBindings.at("GradientAlpha").second.binding, VK_IMAGE_LAYOUT_GENERAL);
			ds->FlushWrites();
			vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, shader->mPipelineLayout, 0, 1, *ds, 0, nullptr);

			uint3 r(mGradientAlpha->Width(), mGradientAlpha->Height(), mGradientAlpha->Depth());
			commandBuffer->PushConstant(shader, "RemapMin", &mRemapMin);
			commandBuffer->PushConstant(shader, "InvRemapRange", &remapRange);
			commandBuffer->PushConstant(shader, "Resolution", &r);

			vkCmdDispatch(*commandBuffer, (mRawVolume->Width() + 3) / 4, (mRawVolume->Height() + 3) / 4, (mRawVolume->Depth() + 3) / 4);

			mGradientAlpha->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, commandBuffer);

			mGradientAlphaDirty = false;
		}

		if (mTransferLUTDirty) {
			if (!mTransferLUT) {
				mTransferLUT = new Texture("Transfer LUT", mScene->Instance()->Device(), 256, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
				mTransferLUT->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, commandBuffer);
			} else
				mTransferLUT->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, commandBuffer);
			
			set<string> kw;
			if (mColorize) kw.emplace("COLORIZE");
			ComputeShader* shader = mScene->AssetManager()->LoadShader("Shaders/precompute.stm")->GetCompute("ComputeLUT", kw);
			vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, shader->mPipeline);

			DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet("ComputeTransferLUT", shader->mDescriptorSetLayouts[0]);
			ds->CreateStorageTextureDescriptor(mTransferLUT, shader->mDescriptorBindings.at("TransferLUT").second.binding);
			ds->FlushWrites();
			vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, shader->mPipelineLayout, 0, 1, *ds, 0, nullptr);

			uint3 r(mTransferLUT->Width(), 1, 1);
			commandBuffer->PushConstant(shader, "RemapMin", &mRemapMin);
			commandBuffer->PushConstant(shader, "InvRemapRange", &remapRange);
			commandBuffer->PushConstant(shader, "TransferMin", &mTransferMin);
			commandBuffer->PushConstant(shader, "TransferMax", &mTransferMax);
			commandBuffer->PushConstant(shader, "Resolution", &r);

			vkCmdDispatch(*commandBuffer, (mTransferLUT->Width() + shader->mWorkgroupSize.x - 1) / shader->mWorkgroupSize.x, 1, 1);

			mTransferLUT->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);

			mTransferLUTDirty = false;
		}
		
		{
			float3 vres(mRawVolume->Width(), mRawVolume->Height(), mRawVolume->Depth());
			set<string> kw;
			if (mPhysicalShading) kw.emplace("PHYSICAL_SHADING");
			else if (mLighting) kw.emplace("LIGHTING");
			if (mGradientAlpha) kw.emplace("PRECOMPUTED_GRADIENT");
			ComputeShader* shader = mScene->AssetManager()->LoadShader("Shaders/volume.stm")->GetCompute("Draw", kw);
			vkCmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, shader->mPipeline);
			
			DescriptorSet* ds = commandBuffer->Device()->GetTempDescriptorSet("Draw Volume", shader->mDescriptorSetLayouts[0]);
			if (mGradientAlpha) ds->CreateSampledTextureDescriptor(mGradientAlpha, shader->mDescriptorBindings.at("GradientAlpha").second.binding, VK_IMAGE_LAYOUT_GENERAL);
			else ds->CreateSampledTextureDescriptor(mRawVolume, shader->mDescriptorBindings.at("RawVolume").second.binding, VK_IMAGE_LAYOUT_GENERAL);
			ds->CreateSampledTextureDescriptor(mTransferLUT, shader->mDescriptorBindings.at("TransferLUT").second.binding);
			if (mPhysicalShading || mLighting)
				ds->CreateSampledTextureDescriptor(mScene->Environment()->EnvironmentTexture(), shader->mDescriptorBindings.at("EnvironmentTexture").second.binding);
			ds->CreateStorageTextureDescriptor(camera->ResolveBuffer(0), shader->mDescriptorBindings.at("RenderTarget").second.binding, VK_IMAGE_LAYOUT_GENERAL);
			ds->CreateStorageTextureDescriptor(camera->ResolveBuffer(1), shader->mDescriptorBindings.at("DepthNormal").second.binding, VK_IMAGE_LAYOUT_GENERAL);
			ds->CreateSampledTextureDescriptor(mScene->AssetManager()->LoadTexture("Assets/Textures/rgbanoise.png", false), shader->mDescriptorBindings.at("NoiseTex").second.binding);
			ds->FlushWrites();
			vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, shader->mPipelineLayout, 0, 1, *ds, 0, nullptr);

			uint2 wo(0);
			float3 vp = mVolumePosition - camera->WorldPosition();
			float3 ambient = mScene->Environment()->AmbientLight();

			commandBuffer->PushConstant(shader, "VolumePosition", &vp);
			commandBuffer->PushConstant(shader, "VolumeRotation", &mVolumeRotation.xyzw);
			commandBuffer->PushConstant(shader, "VolumeScale", &mVolumeScale);
			commandBuffer->PushConstant(shader, "InvVolumeRotation", &ivr);
			commandBuffer->PushConstant(shader, "InvVolumeScale", &ivs);
			commandBuffer->PushConstant(shader, "VolumeResolution", &vres);

			commandBuffer->PushConstant(shader, "DirectLight", &mDirectLight.x);
			commandBuffer->PushConstant(shader, "AmbientLight", &ambient.x);
			commandBuffer->PushConstant(shader, "Density", &mDensity);
			commandBuffer->PushConstant(shader, "Extinction", &mVolumeExtinction);
			commandBuffer->PushConstant(shader, "Scattering", &mVolumeScatter);
			commandBuffer->PushConstant(shader, "Roughness", &mRoughness);

			commandBuffer->PushConstant(shader, "StepSize", &mStepSize);
			commandBuffer->PushConstant(shader, "LightStep", &mLightStep);
			commandBuffer->PushConstant(shader, "FrameIndex", &mFrameIndex);

			switch (camera->StereoMode()) {
			case STEREO_NONE:
				commandBuffer->PushConstant(shader, "InvViewProj", &ivp[0]);
				commandBuffer->PushConstant(shader, "CameraPosition", &cp[0]);
				commandBuffer->PushConstant(shader, "WriteOffset", &wo);
				commandBuffer->PushConstant(shader, "ScreenResolution", &res);
				vkCmdDispatch(*commandBuffer, (camera->FramebufferWidth() + 7) / 8, (camera->FramebufferHeight() + 7) / 8, 1);
				break;
			case STEREO_SBS_HORIZONTAL:
				res.x *= .5f;
				commandBuffer->PushConstant(shader, "InvViewProj", &ivp[0]);
				commandBuffer->PushConstant(shader, "CameraPosition", &cp[0]);
				commandBuffer->PushConstant(shader, "WriteOffset", &wo);
				commandBuffer->PushConstant(shader, "ScreenResolution", &res);
				vkCmdDispatch(*commandBuffer, (camera->FramebufferWidth() / 2 + 7) / 8, (camera->FramebufferHeight() + 7) / 8, 1);
				wo.x = camera->FramebufferWidth() / 2;
				commandBuffer->PushConstant(shader, "InvViewProj", &ivp[1]);
				commandBuffer->PushConstant(shader, "CameraPosition", &cp[1]);
				commandBuffer->PushConstant(shader, "WriteOffset", &wo);
				vkCmdDispatch(*commandBuffer, (camera->FramebufferWidth()/2 + 7) / 8, (camera->FramebufferHeight() + 7) / 8, 1);
				break;
			case STEREO_SBS_VERTICAL:
				res.y *= .5f;
				commandBuffer->PushConstant(shader, "InvViewProj", &ivp[0]);
				commandBuffer->PushConstant(shader, "CameraPosition", &cp[0]);
				commandBuffer->PushConstant(shader, "WriteOffset", &wo);
				commandBuffer->PushConstant(shader, "ScreenResolution", &res);
				vkCmdDispatch(*commandBuffer, (camera->FramebufferWidth() + 7) / 8, (camera->FramebufferHeight() / 2 + 7) / 8, 1);
				wo.y = camera->FramebufferWidth() / 2;
				commandBuffer->PushConstant(shader, "InvViewProj", &ivp[1]);
				commandBuffer->PushConstant(shader, "CameraPosition", &cp[1]);
				commandBuffer->PushConstant(shader, "WriteOffset", &wo);
				vkCmdDispatch(*commandBuffer, (camera->FramebufferWidth() + 7) / 8, (camera->FramebufferHeight() / 2 + 7) / 8, 1);
				break;
			}

			camera->ResolveBuffer(0)->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, commandBuffer);
		}

		mFrameIndex++;
	}
	
	void LoadVolume(CommandBuffer* commandBuffer, const fs::path& folder, ImageStackType type) {
		safe_delete(mRawVolume);
		safe_delete(mRawMask);
		safe_delete(mGradientAlpha);

		Texture* vol = nullptr;
		switch (type) {
		case IMAGE_STACK_STANDARD:
			vol = ImageLoader::LoadStandardStack(folder, mScene->Instance()->Device(), &mVolumeScale);
			break;
		case IMAGE_STACK_DICOM:
			vol = ImageLoader::LoadDicomStack(folder, mScene->Instance()->Device(), &mVolumeScale);
			break;
		case IMAGE_STACK_RAW:
			vol = ImageLoader::LoadRawStack(folder, mScene->Instance()->Device(), &mVolumeScale);
			break;
		}
		
		if (!vol) {
			fprintf_color(COLOR_RED, stderr, "Failed to load volume!\n");
			return;
		}

		mVolumeRotation = quaternion(0,0,0,1);
		mVolumePosition = float3(0, 1.6f, 0);

		mRawVolume = vol;
		mRawMask = ImageLoader::LoadStandardStack(folder.string() + "/_mask", mScene->Instance()->Device(), nullptr);
		//mGradientAlpha = new Texture("Gradient Alpha", mScene->Instance()->Device(), nullptr, 0, mRawVolume->Width(), mRawVolume->Height(), mRawVolume->Depth(), VK_FORMAT_R8G8B8A8_UNORM, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

		mRawVolumeNew = true;
		mTransferLUTDirty = true;
		mGradientAlphaDirty = true;
		mFrameIndex = 0;
	}
};

ENGINE_PLUGIN(DicomVis)