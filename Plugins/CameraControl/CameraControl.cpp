#include "CameraControl.hpp"
#include <Interface/UICanvas.hpp>
#include <Scene/Scene.hpp>
#include <Util/Profiler.hpp>

using namespace std;

ENGINE_PLUGIN(CameraControl)

CameraControl::CameraControl()
	: mScene(nullptr), mCameraPivot(nullptr), mInput(nullptr), mCameraDistance(1.5f), mCameraEuler(float3(0)),
	mFps(0), mFrameTimeAccum(0), mFrameCount(0), mPrintPerformance(false) {
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
	if (mInput->KeyDownFirst(KEY_F3))
		mPrintPerformance = !mPrintPerformance;

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

	char perfText[8192];
	snprintf(perfText, 8192, "%.2f fps | %llu tris\n", mFps, commandBuffer->mTriangleCount);

	Font* reg = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Regular.ttf", 18);
	Font* bld = mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Bold.ttf", 16);

	bld->Draw(commandBuffer, camera, perfText, 1.f, float2(5, camera->FramebufferHeight() - 18), 18.f);

	#ifdef PROFILER_ENABLE
	if (mPrintPerformance) {
		Profiler::PrintLastFrame(perfText);
		reg->Draw(commandBuffer, camera, perfText, 1.f, float2(5, camera->FramebufferHeight() - 32), 16.f);
	}
	#endif
}