#include "CameraControl.hpp"
#include <Scene/Scene.hpp>
#include <Scene/Interface.hpp>
#include <Util/Profiler.hpp>

using namespace std;

ENGINE_PLUGIN(CameraControl)

CameraControl::CameraControl()
	: mScene(nullptr), mCameraPivot(nullptr), mInput(nullptr), mCameraDistance(1.5f), mCameraEuler(float3(0)),
	mFps(0), mFrameTimeAccum(0), mFrameCount(0), mShowPerformance(false) {
	mEnabled = true;
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
	camera->Near(.01f);
	camera->Far(1024.f);
	camera->FieldOfView(radians(65.f));
	camera->LocalPosition(0, 0, -mCameraDistance);
	mCameras.push_back(camera.get());
	mCameraPivot->AddChild(camera.get());

	return true;
}

void CameraControl::Update() {
	if (mInput->KeyDownFirst(KEY_F1))
		mScene->DrawGizmos(!mScene->DrawGizmos());
	if (mInput->KeyDownFirst(KEY_TILDE))
		mShowPerformance = !mShowPerformance;

	#pragma region Camera control
	if (mInput->KeyDown(MOUSE_MIDDLE)) {
		float3 md = float3(mInput->CursorDelta(), 0);
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

	// count fps
	mFrameTimeAccum += mScene->Instance()->DeltaTime();
	mFrameCount++;
	if (mFrameTimeAccum > 1.f) {
		mFps = mFrameCount / mFrameTimeAccum;
		mFrameTimeAccum -= 1.f;
		mFrameCount = 0;
	}
}

void CameraControl::PostRenderScene(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {
	if (pass != PASS_MAIN || camera != mScene->Cameras()[0]) return;
	if (mShowPerformance) {
		char tmpText[32];

		Font* reg = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Regular.ttf", 18);
		Font* bld = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Bold.ttf", 16);

		float2 s(camera->FramebufferWidth(), camera->FramebufferHeight());

		#ifdef PROFILER_ENABLE
		const ProfilerSample* frames = Profiler::Frames();
		const uint32_t pointCount = PROFILER_FRAME_COUNT - 1;
		float2 points[pointCount];
		float m = 0;
		for (uint32_t i = 0; i < pointCount; i++) {
			points[i].x = (float)i / (pointCount - 1.f);
			points[i].y = frames[(i + Profiler::CurrentFrameIndex() + 2) % PROFILER_FRAME_COUNT].mDuration.count() * 1e-6;
			m = fmaxf(points[i].y, m);
		}
		m = fmaxf(m, 5.f) + 3.f;
		for (uint32_t i = 0; i < pointCount; i++)
			points[i].y /= m;

		DrawScreenRect(commandBuffer, camera, float2(0, 0), float2(s.x, 100), float4(.1f, .1f, .1f, 1));

		// Draw performance graph
		{
			GraphicsShader* shader = camera->Scene()->AssetManager()->LoadShader("Shaders/line.stm")->GetGraphics(PASS_MAIN, { "SCREEN_SPACE" });
			if (!shader) return;
			VkPipelineLayout layout = commandBuffer->BindShader(shader, PASS_MAIN, nullptr, nullptr, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP);
			if (!layout) return;

			float4 color(.2f, 1.f, .2f, 1.f);
			float4 st(s.x, 100, 0, 0);
			float4 sz(0, 0, s);

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

		snprintf(tmpText, 32, "%.1fms (%.1ffps)", m, 1000.f / m);
		bld->DrawScreenString(commandBuffer, camera, tmpText, 1.f, float2(2, 100), 16.f, Minimum, Maximum);
		#endif

		snprintf(tmpText, 32, "%.2f fps | %llu tris\n", mFps, commandBuffer->mTriangleCount);
		bld->DrawScreenString(commandBuffer, camera, tmpText, 1.f, float2(5, camera->FramebufferHeight() - 18), 18.f);
	}
}