#include "CameraControl.hpp"
#include <Interface/UICanvas.hpp>
#include <Scene/Scene.hpp>
#include <Util/Profiler.hpp>

using namespace std;

ENGINE_PLUGIN(CameraControl)

CameraControl::CameraControl()
	: mScene(nullptr), mCameraPivot(nullptr), mFpsText(nullptr), mInput(nullptr), mCameraDistance(1.5f), mCameraEuler(float3(0)),
	mFps(0), mFrameTimeAccum(0), mFrameCount(0), mTriangleCount(0), mPrintPerformance(false) {
	mEnabled = true;
}
CameraControl::~CameraControl() {
	for (Camera* c : mCameras)
		mScene->RemoveObject(c);
	mScene->RemoveObject(mCameraPivot);
	mScene->RemoveObject(mFpsText);
}

bool CameraControl::Init(Scene* scene) {
	mScene = scene;
	mInput = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

	shared_ptr<Object> cameraPivot = make_shared<Object>("CameraPivot");
	mScene->AddObject(cameraPivot);
	mCameraPivot = cameraPivot.get();
	mCameraPivot->LocalPosition(0, .5f, 0);

	shared_ptr<Camera> camera = make_shared<Camera>("Camera", mScene->Instance()->GetWindow(0));
	mScene->AddObject(camera);
	camera->Near(.01f);
	camera->Far(2048.f);
	camera->FieldOfView(radians(65.f));
	camera->LocalPosition(0, 0, -mCameraDistance);
	mCameras.push_back(camera.get());
	mCameraPivot->AddChild(camera.get());

	shared_ptr<TextRenderer> fpsText = make_shared<TextRenderer>("Fps Text");
	mScene->AddObject(fpsText);
	fpsText->Font(mScene->AssetManager()->LoadFont("Assets/Fonts/OpenSans-Regular.ttf", 36));
	fpsText->Text("");
	fpsText->VerticalAnchor(Maximum);
	fpsText->HorizontalAnchor(Minimum);
	mFpsText = fpsText.get();

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

	// print performance
	Camera* mainCamera = mCameras[0];
	if (mainCamera->Orthographic()) {
		mFpsText->TextScale(.025f * mainCamera->OrthographicSize());
		mainCamera->OrthographicSize(mainCamera->OrthographicSize() * (1 - mInput->ScrollDelta() * .06f));
	} else
		mFpsText->TextScale(.0004f * tanf(mainCamera->FieldOfView() / 2));
	mFpsText->LocalRotation(mainCamera->WorldRotation());
	mFpsText->LocalPosition(mainCamera->ClipToWorld(float3(-.99f, -.96f, 0.005f)));

	// count fps
	mFrameTimeAccum += mScene->Instance()->DeltaTime();
	mFrameCount++;
	if (mFrameTimeAccum > 1.f) {
		mFps = mFrameCount / mFrameTimeAccum;
		mFrameTimeAccum -= 1.f;
		mFrameCount = 0;
		char buf[8192];
		memset(buf, 0, 8192);
		size_t sz = sprintf(buf, "%.2f fps | %llu tris\n", mFps, mTriangleCount);
		#ifdef PROFILER_ENABLE
		if (mPrintPerformance)
			Profiler::PrintLastFrame(buf + sz);
		#endif
		mFpsText->Text(buf);
	}
}

void CameraControl::PostRender(CommandBuffer* commandBuffer, Camera* camera) {
	mTriangleCount = commandBuffer->mTriangleCount;
}