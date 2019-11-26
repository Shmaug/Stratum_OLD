#include "CameraControl.hpp"
#include <Interface/UICanvas.hpp>
#include <Scene/Scene.hpp>
#include <Util/Profiler.hpp>

using namespace std;

ENGINE_PLUGIN(CameraControl)

CameraControl::CameraControl()
	: mScene(nullptr), mCameraPivot(nullptr), mFpsText(nullptr), mInput(nullptr), mCameraDistance(1.5f), mCameraEuler(float3(0)), mFps(0), mFrameTimeAccum(0), mFrameCount(0), mTriangleCount(0) {
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

	shared_ptr<Object> cameraPivot = make_shared<Object>("CameraPivot");
	mScene->AddObject(cameraPivot);
	mCameraPivot = cameraPivot.get();
	mCameraPivot->LocalPosition(0, .5f, 0);

	shared_ptr<Camera> camera = make_shared<Camera>("Camera", mScene->Instance()->GetWindow(0));
	mScene->AddObject(camera);
	camera->Near(.01f);
	camera->Far(8192.f);
	camera->FieldOfView(radians(65.f));
	camera->LocalPosition(0, 0, -mCameraDistance);
	mCameras.push_back(camera.get());
	mCameraPivot->AddChild(camera.get());

	Shader* fontshader = mScene->AssetManager()->LoadShader("Shaders/font.shader");
	Font* font = mScene->AssetManager()->LoadFont("Assets/OpenSans-Regular.ttf", 36);

	shared_ptr<TextRenderer> fpsText = make_shared<TextRenderer>("Fps Text");
	mScene->AddObject(fpsText);
	fpsText->Font(font);
	fpsText->Text("");
	fpsText->VerticalAnchor(Maximum);
	fpsText->HorizontalAnchor(Minimum);
	mFpsText = fpsText.get();
	camera->AddChild(mFpsText);

	mInput = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

	return true;
}

void CameraControl::Update() {
	if (mInput->KeyDownFirst(GLFW_KEY_F1))
		mScene->DrawGizmos(!mScene->DrawGizmos());
	
	Camera* c = mCameras[0];
	float3 lp = (c->WorldToObject() * float4(c->ClipToWorld(float3(-.99f, -.96f, 0)), 1)).xyz;
	lp.z = c->Near() + .00001f;
	mFpsText->LocalPosition(lp);
	if (c->Orthographic()) {
		mFpsText->TextScale(.028f * c->OrthographicSize());
		c->OrthographicSize(c->OrthographicSize() * (1 - mInput->ScrollDelta().y * .06f));
	} else {
		mFpsText->TextScale(.0005f * tanf(c->FieldOfView() / 2));
		mCameraDistance = fmaxf(mCameraDistance * (1 - mInput->ScrollDelta().y * .06f), .025f);
	}

	if (mInput->KeyDownFirst(GLFW_KEY_O))
		c->Orthographic(!c->Orthographic());

	if (mInput->MouseButtonDown(GLFW_MOUSE_BUTTON_MIDDLE)) {
		float3 md = float3(mInput->CursorDelta(), 0);
		if (mInput->KeyDown(GLFW_KEY_LEFT_SHIFT)) {
			md.x = -md.x;
			md = md * .0005f * mCameraDistance;
		} else
			md = float3(md.y, md.x, 0) * .005f;

		if (mInput->KeyDown(GLFW_KEY_LEFT_SHIFT))
			// translate camera
			mCameraPivot->LocalPosition(mCameraPivot->LocalPosition() + mCameraPivot->LocalRotation() * md);
		else {
			mCameraEuler += md;
			mCameraEuler.x = clamp(mCameraEuler.x, -PI * .5f, PI * .5f);
			// rotate camera
		}
		mCameraPivot->LocalRotation(quaternion(mCameraEuler));
	}

	for (uint32_t i = 0; i < mCameras.size(); i++)
		mCameras[i]->LocalPosition(0, 0, -mCameraDistance);

	mFrameTimeAccum += mScene->Instance()->DeltaTime();
	mFrameCount++;
	if (mFrameTimeAccum > 1.f) {
		mFps = mFrameCount / mFrameTimeAccum;
		mFrameTimeAccum -= 1.f;
		mFrameCount = 0;
		char buf[256];
		sprintf(buf, "%.2f fps | %d tris\n", mFps, mTriangleCount);
		mFpsText->Text(buf);
	}
}

void CameraControl::PostRender(CommandBuffer* commandBuffer, Camera* camera) {
	mTriangleCount = commandBuffer->mTriangleCount;
}