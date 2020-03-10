#include "CameraControl.hpp"
#include <Scene/Scene.hpp>
#include <Scene/GUI.hpp>
#include <Content/Font.hpp>

using namespace std;

ENGINE_PLUGIN(CameraControl)

CameraControl::CameraControl()
	: mScene(nullptr), mCameraPivot(nullptr), mInput(nullptr), mCameraDistance(1.5f), mCameraEuler(float3(0)),
	mSnapshotPerformance(false), mShowPerformance(false), mSelectedFrame(PROFILER_FRAME_COUNT) {
	mEnabled = true;
	memset(mProfilerFrames, 0, sizeof(ProfilerSample) * (PROFILER_FRAME_COUNT - 1));
}
CameraControl::~CameraControl() {
	for (Camera* c : mCameras)
		mScene->RemoveObject(c);
	mScene->RemoveObject(mCameraPivot);
}

bool CameraControl::Init(Scene* scene) {
	mScene = scene;
	mInput = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

	shared_ptr<Object> cameraPivot = make_shared<Object>("CameraPivot");
	mScene->AddObject(cameraPivot);
	mCameraPivot = cameraPivot.get();
	mCameraPivot->LocalPosition(0, .5f, 0);

	shared_ptr<Camera> camera = make_shared<Camera>("Camera", mScene->Instance()->Window());
	mScene->AddObject(camera);
	camera->Near(.1f);
	camera->Far(1024.f);
	camera->FieldOfView(radians(65.f));
	camera->LocalPosition(0, 0, -mCameraDistance);
	mCameras.push_back(camera.get());
	mCameraPivot->AddChild(camera.get());

	return true;
}

void CameraControl::Update(CommandBuffer* commandBuffer) {
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

	if (mInput->GetPointer(0)->mLastGuiHitT < 0){
		#pragma region Camera control
		if (mInput->KeyDown(MOUSE_MIDDLE) || (mInput->KeyDown(MOUSE_LEFT) && mInput->KeyDown(KEY_LALT))) {
			float3 md = mInput->CursorDelta();
			if (mInput->KeyDown(KEY_LSHIFT)) {
				md.x = -md.x;
				md = md * .0005f * mCameraDistance;
			} else
				md = float3(md.y, md.x, 0) * .005f;

			if (mInput->KeyDown(KEY_LSHIFT))
				// translate camera
				mCameraPivot->LocalPosition(mCameraPivot->LocalPosition() + mCameraPivot->LocalRotation() * md);
			else {
				mCameraEuler += md;
				mCameraEuler.x = clamp(mCameraEuler.x, -PI * .5f, PI * .5f);
				// rotate camera
			}
			mCameraPivot->LocalRotation(quaternion(mCameraEuler));
		}
		mCameraDistance *= 1.0f - .2f * mInput->ScrollDelta();
		mCameraDistance = max(.01f, mCameraDistance);

		for (uint32_t i = 0; i < mCameras.size(); i++)
			mCameras[i]->LocalPosition(0, 0, -mCameraDistance);
		#pragma endregion
	}
}

void CameraControl::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
}

void CameraControl::PreRenderScene(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {
	if (pass != PASS_MAIN || camera != mScene->Cameras()[0]) return;
	if (mShowPerformance) {
		Font* sem11 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-SemiBold.ttf", 11);
		Font* sem16 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-SemiBold.ttf", 16);
		Font* reg14 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Regular.ttf", 14);
		Font* bld16 = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Bold.ttf", 16);

		char tmpText[64];

		float2 s(camera->FramebufferWidth(), camera->FramebufferHeight());

		float graphHeight = 100;

		#ifdef PROFILER_ENABLE
		const uint32_t pointCount = PROFILER_FRAME_COUNT - 1;

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
			float2 c = mInput->CursorPos();
			c.y = s.y - c.y;

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

					float2 pos(s.x * (p.first->mStartTime - mProfilerFrames[mSelectedFrame].mStartTime).count() * id, graphHeight + sampleHeight * p.second);
					float2 size(s.x * (float)p.first->mDuration.count() * id, sampleHeight);
					float4 col(0, 0, 0, 1);

					if (c.x > pos.x && c.y > pos.y && c.x < pos.x + size.x && c.y < pos.y + size.y) {
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
					GUI::DrawString(reg14, tmpText, 1, c + 8, 14.f, TEXT_ANCHOR_MAX, TEXT_ANCHOR_MAX);
				}
			}

		}
		#endif

		snprintf(tmpText, 64, "%.2fms\n%d DescriptorSets", mScene->FPS(), commandBuffer->Device()->DescriptorSetCount());
		GUI::DrawString(sem16, tmpText, 1.f, float2(5, camera->FramebufferHeight() - 30), 18.f);
	}
}