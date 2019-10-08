#include <thread>
#include "CameraControl.hpp"

#include <Util/Profiler.hpp>

using namespace std;

ENGINE_PLUGIN(CameraControl)

CameraControl::CameraControl()
	: mScene(nullptr), mCameraPivot(nullptr), mFpsText(nullptr), mInput(nullptr), mCameraDistance(1.5f), mCameraEuler(vec3(0, 0, 0)), mFps(0), mFrameTimeAccum(0), mFrameCount(0) {}
CameraControl::~CameraControl() {
	mScene->RemoveObject(mCameraPivot.get());
	mScene->RemoveObject(mFpsText.get());
	mFpsText.reset();
	mCameraPivot.reset();
}

bool CameraControl::Init(Scene* scene) {
	mScene = scene;

	mInput = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

	Shader* fontshader = scene->DeviceManager()->AssetDatabase()->LoadShader("Shaders/font.shader");
	Font* font = scene->DeviceManager()->AssetDatabase()->LoadFont("Assets/segoeui.ttf", 24);

	shared_ptr<Material> fontMat = make_shared<Material>("Segoe UI", fontshader);
	fontMat->SetParameter("MainTexture", font->Texture());

	mFpsText = make_shared<TextRenderer>("Fps Text");
	mFpsText->Font(font);
	mFpsText->Material(fontMat);
	mFpsText->Text("0 fps");
	mFpsText->VerticalAnchor(Maximum);
	mFpsText->HorizontalAnchor(Minimum);

	scene->AddObject(mFpsText);
	scene->AddObject(mCameraPivot = make_shared<Object>("CameraPivot"));

	Camera* c = mScene->Cameras()[0];
	mFpsText->Parent(c);
	float d = c->Near() + .001f;
	float x = c->Near() * tanf(c->FieldOfView() * .5f * c->Aspect());
	float y = c->Near() * tanf(c->FieldOfView() * .5f);
	mFpsText->LocalPosition(-x, y, d);
	mFpsText->TextScale(d * .015f);

	for (auto& camera : mScene->Cameras()) {
		camera->Parent(mCameraPivot.get());
		camera->LocalPosition(0, 0, -mCameraDistance);
	}

	return true;
}

void CameraControl::Update(const FrameTime& frameTime) {
	Camera* c = mScene->Cameras()[0];
	mFpsText->Parent(c);
	float d = c->Near() + .001f;
	float x = c->Near() * tanf(c->FieldOfView() * .5f * c->Aspect());
	float y = c->Near() * tanf(c->FieldOfView() * .5f);
	mFpsText->LocalPosition(-x, y, d);
	mFpsText->TextScale(d * .015f);

	mCameraDistance = gmax(mCameraDistance * (1 - mInput->ScrollDelta().y * .03f), .025f);

	vec3 md = vec3(mInput->CursorDelta(), 0);
	if (mInput->KeyDown(GLFW_KEY_LEFT_SHIFT)) {
		md.x = -md.x;
		md = md * .0005f * mCameraDistance;
	} else
		md = vec3(md.y, md.x, 0) * .005f;

	if (mInput->MouseButtonDown(1)) { // right mouse
		if (mInput->KeyDown(GLFW_KEY_LEFT_SHIFT))
			// translate camera
			mCameraPivot->LocalPosition(mCameraPivot->LocalPosition() + mCameraPivot->LocalRotation() * md);
		else {
			mCameraEuler += md;
			mCameraEuler.x = gclamp(mCameraEuler.x, -pi<float>() * .5f, pi<float>() * .5f);
			// rotate camera
			mCameraPivot->LocalRotation(mCameraEuler);
		}
	}
	for (uint32_t i = 0; i < mCameraPivot->ChildCount(); i++)
		if (Camera* c = dynamic_cast<Camera*>(mCameraPivot->Child(i)))
			c->LocalPosition(0, 0, -mCameraDistance);

	mFrameTimeAccum += frameTime.mDeltaTime;
	mFrameCount++;
	if (mFrameTimeAccum > 1.f) {
		mFps = mFrameCount / mFrameTimeAccum;
		mFrameTimeAccum -= 1.f;
		mFrameCount = 0;

		char txt[2048];
		size_t c = 0;
		c += sprintf_s(txt, 2048, "%.1f fps\n", mFps);

		Profiler::PrintLastFrame(txt + c, 2048 - c);

		mFpsText->Text(txt);
	}
}