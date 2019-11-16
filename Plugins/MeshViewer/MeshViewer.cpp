#include <Core/EnginePlugin.hpp>

#include <Scene/Camera.hpp>
#include <Scene/Scene.hpp>
#include <Scene/SkinnedMeshRenderer.hpp>
#include <Interface/UICanvas.hpp>
#include <Interface/UIImage.hpp>
#include <Interface/UILabel.hpp>
#include <Interface/UILayout.hpp>
#include <Util/Profiler.hpp>

#include <Plugins/Environment/Environment.hpp>

#ifdef __GNUC__
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif

#include "SplineRenderer.hpp"

using namespace std;

class Robot {
public:
	Object* mBody;
	Object* mLArm;
	Object* mRArm;
	Object* mHead;
};

class MeshViewer : public EnginePlugin {
public:
	PLUGIN_EXPORT MeshViewer();
	PLUGIN_EXPORT ~MeshViewer();

	PLUGIN_EXPORT bool Init(Scene* scene) override;
	PLUGIN_EXPORT void Update() override;
	PLUGIN_EXPORT void DrawGizmos(CommandBuffer* commandBuffer, Camera* camera);

private:
	float mLastClick;

	// for editing lights
	Object* mSelected;
	vector<Light*> mLights;

	vector<Object*> mObjects;
	unordered_map<string, Object*> mLoaded;
	vector<shared_ptr<Material>> mMaterials;
	Scene* mScene;

	UICanvas* mPanel;
	UIElement* mFileLoadPanel;
	UIElement* mLightSettingsPanel;
	vector<UIImage*> mLoadFileButtons;
	UILabel* mTitleText;
	UILabel* mLoadText;
	UIImage* mLoadBar;

	SplineRenderer* mSpline;

	vector<Robot*> mRobots;

	Shader* mPBRShader;

	bool mDraggingPanel;
	bool mLoading;
	float mLoadProgress;
	std::thread mLoadThread;

	Robot* AddRobot();

	void LoadRobot();
	void LoadSpline();
	void LoadAsync(fs::path filename, float scale = .05f);
};

ENGINE_PLUGIN(MeshViewer)

MeshViewer::MeshViewer()
	: mScene(nullptr), mPBRShader(nullptr), mLoading(false), mLastClick(0),
	mSelected(nullptr), mPanel(nullptr), mDraggingPanel(false), mSpline(nullptr),
	mFileLoadPanel(nullptr), mLightSettingsPanel(nullptr), mLoadBar(nullptr), mLoadProgress(0.f), mLoadText(nullptr), mTitleText(nullptr) {
	mEnabled = true;
}
MeshViewer::~MeshViewer() {
	if (mLoadThread.joinable()) mLoadThread.join();
	for (Object* obj : mObjects)
		mScene->RemoveObject(obj);
}

void MeshViewer::LoadAsync(fs::path path, float scale) {
	Environment* env = mScene->PluginManager()->GetPlugin<Environment>();

	// disable all the other models
	for (auto& l : mLoaded)
		l.second->mEnabled = false;

	if (mLoaded.count(path.string()))
		// we've already loaded this model, just enable it
		mLoaded.at(path.string())->mEnabled = true;
	else {
		// load the model asynchronously
		mLoading = true;
		mLoadProgress = 0.f;
	
		Mesh* mesh = mScene->AssetManager()->LoadMesh(path.string().c_str(), scale);

		auto material = make_shared<Material>(path.filename().string(), mPBRShader);
		material->SetParameter("BrdfTexture", mScene->AssetManager()->LoadTexture("Assets/BrdfLut.png", false));
		material->SetParameter("Color", float4(1.f));
		material->SetParameter("Metallic", 0.f);
		material->SetParameter("Roughness", .5f);
		material->SetParameter("ReflectionTexture", env->ReflectionMap());
		material->SetParameter("ReflectionStrength", env->ReflectionMapStrength());
		
		if (mesh->Rig()) {
			shared_ptr<SkinnedMeshRenderer> meshRenderer = make_shared<SkinnedMeshRenderer>(path.filename().string());
			mScene->AddObject(meshRenderer);
			meshRenderer->Mesh(mesh, meshRenderer.get());
			meshRenderer->Material(material);
			mLoaded.emplace(path.string(), meshRenderer.get());
		} else {
			shared_ptr<MeshRenderer> meshRenderer = make_shared<MeshRenderer>(path.filename().string());
			meshRenderer->Mesh(mesh);
			meshRenderer->Material(material);
			mLoaded.emplace(path.string(), meshRenderer.get());
			mScene->AddObject(meshRenderer);
		}
		mLoading = false;
	}
}

Robot* MeshViewer::AddRobot() {
	Mesh* antennaMesh = mScene->AssetManager()->LoadMesh("Assets/robot/antenna_s.obj", 1.f);
	Mesh* headMesh = mScene->AssetManager()->LoadMesh("Assets/robot/head_s.obj", 1.f);
	Mesh* limbMesh = mScene->AssetManager()->LoadMesh("Assets/robot/limb_s.obj", 1.f);
	Mesh* bodyMesh = mScene->AssetManager()->LoadMesh("Assets/robot/body_s.obj", 1.f);
	Mesh* eyeMesh = mScene->AssetManager()->LoadMesh ("Assets/robot/eyeball_s.obj", 1.f);

	Environment* env = mScene->PluginManager()->GetPlugin<Environment>();
	
	static shared_ptr<Material> metal =nullptr;
	static shared_ptr<Material> facem = nullptr;
	static shared_ptr<Material> eye = nullptr;

	if (!metal) {
		metal = make_shared<Material>("RobotMetal", mPBRShader);
		metal->SetParameter("BrdfTexture", mScene->AssetManager()->LoadTexture("Assets/BrdfLut.png", false));
		metal->SetParameter("Color", float4(.9f));
		metal->SetParameter("Metallic", 1.f);
		metal->SetParameter("Roughness", 0);
		metal->SetParameter("ReflectionTexture", env->ReflectionMap());
		metal->SetParameter("ReflectionStrength", env->ReflectionMapStrength());
	}

	if (!facem) {
		facem = make_shared<Material>("RobotFace", mPBRShader);
		facem->SetParameter("BrdfTexture", mScene->AssetManager()->LoadTexture("Assets/BrdfLut.png", false));
		facem->SetParameter("Color", float4(.01f));
		facem->SetParameter("Metallic", 1.f);
		facem->SetParameter("Roughness", .01f);
		facem->SetParameter("ReflectionTexture", env->ReflectionMap());
		facem->SetParameter("ReflectionStrength", env->ReflectionMapStrength());
	}

	if (!eye){
		eye = make_shared<Material>("RobotEye", mPBRShader);
		eye->SetParameter("BrdfTexture", mScene->AssetManager()->LoadTexture("Assets/BrdfLut.png", false));
		eye->SetParameter("Color", float4(0.2f, 0.5f, 0.75f));
		eye->SetParameter("Metallic", 0.f);
		eye->SetParameter("Roughness", .025f);
		eye->SetParameter("Emission", float3(.5f, .75f, 1.f));
		eye->SetParameter("EmissionTexture", mScene->AssetManager()->LoadTexture("Assets/white.png"));
		eye->SetParameter("ReflectionTexture", env->ReflectionMap());
		eye->SetParameter("ReflectionStrength", env->ReflectionMapStrength());
		eye->EnableKeyword("EMISSION");
	}

	shared_ptr<MeshRenderer> body = make_shared<MeshRenderer>("Body");
	mScene->AddObject(body);
	mObjects.push_back(body.get());
	body->LocalRotation(quaternion(0, 1, 0, 0));
	body->LocalScale(float3(.625f, .219f, 1.f) * .2f);
	body->Mesh(bodyMesh);
	body->Material(metal);

	shared_ptr<MeshRenderer> bottom = make_shared<MeshRenderer>("Bottom");
	mScene->AddObject(bottom);
	mObjects.push_back(bottom.get());
	body->AddChild(bottom.get());
	bottom->LocalPosition(0.f, -.99965f, 0.f);
	bottom->LocalRotation(quaternion(1, 0, 0, 0));
	bottom->LocalScale(1.0203343629837036f, 9.632735252380371f, 1.0203343629837036f);
	bottom->Mesh(headMesh);
	bottom->Material(metal);

	shared_ptr<MeshRenderer> top = make_shared<MeshRenderer>("Top");
	mScene->AddObject(top);
	mObjects.push_back(top.get());
	body->AddChild(top.get());
	top->LocalPosition(0, 1.003046f, 0);
	top->LocalRotation(quaternion(0, 1, 0, 0));
	top->LocalScale(1.02033436f, 0.9687414f, 1.02033436f);
	top->Mesh(headMesh);
	top->Material(metal);

	shared_ptr<Object> head = make_shared<Object>("Head");
	mScene->AddObject(head);
	mObjects.push_back(head.get());
	body->AddChild(head.get());
	head->LocalPosition(-7.604217557855009e-07f, 5.057405471801758f, -0.001944709219969809f);
	head->LocalRotation(quaternion(-0.5000000596046448f, -0.5000000596046448f, -0.4999999701976776f, .5f));
	head->LocalScale(0.9999998807907104f, 1.6000767946243286f, 4.570376873016357f);

	shared_ptr<Object> neck = make_shared<Object>("Neck");
	mScene->AddObject(neck);
	mObjects.push_back(neck.get());
	head->AddChild(neck.get());

	shared_ptr<MeshRenderer> htop = make_shared<MeshRenderer>("Head Top");
	mScene->AddObject(htop);
	mObjects.push_back(htop.get());
	neck->AddChild(htop.get());
	htop->LocalPosition(0.0019446611404418945f, 4.753257769607444e-07f, -0.3489670753479004f);
	htop->LocalRotation(quaternion(0.5000003576278687f, 0.4999997615814209f, 0.5000002980232239f, 0.4999997913837433f));
	htop->LocalScale(.6117526888847351f, 0.9861886501312256f, 0.9283669590950012f);
	htop->Mesh(headMesh);
	htop->Material(metal);

	shared_ptr<MeshRenderer> face = make_shared<MeshRenderer>("Face");
	mScene->AddObject(face);
	mObjects.push_back(face.get());
	htop->AddChild(face.get());
	face->LocalPosition(-0.46354931592941284f, 0.4908345341682434f, 0.0016714046942070127f);
	face->LocalRotation(quaternion(-0.5031242370605469f, 0.8642140626907349f, -2.5014125881739346e-08f, -1.1258141974224145e-08f));
	face->LocalScale(0.5513290166854858f, 0.3918508291244507f, 0.7281637787818909f);
	face->Mesh(headMesh);
	face->Material(facem);

	shared_ptr<MeshRenderer> leye = make_shared<MeshRenderer>("LEye");
	mScene->AddObject(leye);
	mObjects.push_back(leye.get());
	face->AddChild(leye.get());
	leye->LocalPosition(0.04395657777786255f, 0.6733275651931763f, -0.48340997099876404f);
	leye->LocalRotation(quaternion(-0.024556323885917664f, 0.17886871099472046f, 0.5156598091125488f, 0.8375547528266907f));
	leye->LocalScale(4.487833499908447f, 4.02925968170166f, 3.8776533603668213f);
	leye->Mesh(eyeMesh);
	leye->Material(eye);

	shared_ptr<MeshRenderer> reye = make_shared<MeshRenderer>("REye");
	mScene->AddObject(reye);
	mObjects.push_back(reye.get());
	face->AddChild(reye.get());
	reye->LocalPosition(0.04395657777786255f, 0.6733278036117554f, 0.4880000650882721f);
	reye->LocalRotation(quaternion(-0.5141682028770447f, 0.5729449391365051f, -0.18911446630954742f, 0.6095907688140869f));
	reye->LocalScale(4.66795015335083f, 3.7310101985931396f, 3.7737956047058105f);
	reye->Mesh(eyeMesh);
	reye->Material(eye);

	shared_ptr<MeshRenderer> hbtm = make_shared<MeshRenderer>("Head Bottom");
	mScene->AddObject(hbtm);
	mObjects.push_back(hbtm.get());
	htop->AddChild(hbtm.get());
	hbtm->LocalPosition(-3.698136089892723e-13f, 2.384185791015625e-07f, -4.2254308237897986e-21f);
	hbtm->LocalRotation(quaternion(1.0f, -9.635839433030924e-07f, -1.7729925616549735e-07, 5.971345728994493e-08));
	hbtm->LocalScale(1.0f, 0.24752794206142426f, 0.9999998211860657f);
	hbtm->Mesh(headMesh);
	hbtm->Material(metal);

	shared_ptr<MeshRenderer> rantenna = make_shared<MeshRenderer>("RAntenna");
	mScene->AddObject(rantenna);
	mObjects.push_back(rantenna.get());
	htop->AddChild(rantenna.get());
	rantenna->LocalPosition(0.01295844092965126f, 1.0729432106018066f, -0.7886744141578674f);
	rantenna->LocalRotation(quaternion(-0.15088200569152832f, 0.695493221282959f, -0.13628718256950378f, 0.6891658902168274f));
	rantenna->LocalScale(0.36139366030693054f, 0.19080094993114471f, 0.5534285306930542f);
	rantenna->Mesh(antennaMesh);
	rantenna->Material(metal);
	shared_ptr<MeshRenderer> lantenna = make_shared<MeshRenderer>("LAntenna");
	mScene->AddObject(lantenna);
	mObjects.push_back(lantenna.get());
	htop->AddChild(lantenna.get());
	lantenna->LocalPosition(0.012958616018295288f, 1.0729432106018066f, 0.739460825920105f);
	lantenna->LocalRotation(quaternion(0.15088188648223877f, -0.6954931616783142f, -0.13628722727298737f, 0.6891659498214722f));
	lantenna->LocalScale(0.36139366030693054f, 0.19080094993114471f, 0.5534285306930542f);
	lantenna->Mesh(antennaMesh);
	lantenna->Material(metal);

	shared_ptr<Object> rarm = make_shared<Object>("RArm");
	mScene->AddObject(rarm);
	mObjects.push_back(rarm.get());
	body->AddChild(rarm.get());
	rarm->LocalPosition(-4.6921653051867906e-07f, 0.5858743190765381f, -1.1581997871398926f);
	rarm->LocalRotation(quaternion(-0.5000000596046448f, -0.5000000596046448f, -0.4999999701976776f, 0.5f));
	rarm->LocalScale(0.9999998807907104f, 1.6000767946243286f, 4.570376873016357f);

	shared_ptr<MeshRenderer> rarmm = make_shared<MeshRenderer>("RArm");
	mScene->AddObject(rarmm);
	mObjects.push_back(rarmm.get());
	rarmm->LocalPosition(-0.06043827533721924f, 3.438727560478583e-07f, -0.8278734683990479f);
	rarmm->LocalRotation(quaternion(0.7071068286895752, 1.5700925641616852e-16, 1.403772174824528e-22, 0.7071067094802856f));
	rarmm->LocalScale(0.734440267086029f, 1.9024626016616821f, 1.9024626016616821f);
	rarmm->Mesh(limbMesh);
	rarmm->Material(metal);

	shared_ptr<Object> larm = make_shared<Object>("LArm");
	mScene->AddObject(larm);
	mObjects.push_back(larm.get());
	body->AddChild(larm.get());
	larm->LocalPosition(-6.312293976407091e-07f, 0.5858747959136963f, 1.1582022905349731f);
	larm->LocalRotation(quaternion(-0.5000000596046448f, -0.5000000596046448f, -0.4999999701976776f, 0.5f));
	larm->LocalScale(0.9999998807907104f, 1.6000767946243286f, 4.570376873016357f);

	shared_ptr<MeshRenderer> larmm = make_shared<MeshRenderer>("LArm");
	mScene->AddObject(larmm);
	mObjects.push_back(larmm.get());
	larmm->LocalPosition(-0.0005675554275512695f, 3.438727844695677e-07f, -0.827873706817627f);
	larmm->LocalRotation(quaternion(0.7071068286895752f, 1.570089917183725e-16f, -3.7433923820535783e-23f, 0.7071067094802856f));
	larmm->LocalScale(0.7344402074813843f, 1.9024626016616821f, 1.9024626016616821f);
	larmm->Mesh(limbMesh);
	larmm->Material(metal);

	shared_ptr<Object> lshoulder = make_shared<Object>("LShoulder");
	mScene->AddObject(lshoulder);
	mObjects.push_back(lshoulder.get());
	larm->AddChild(lshoulder.get());
	lshoulder->AddChild(larmm.get());

	shared_ptr<Object> rshoulder = make_shared<Object>("LShoulder");
	mScene->AddObject(rshoulder);
	mObjects.push_back(rshoulder.get());
	rarm->AddChild(rshoulder.get());
	rshoulder->AddChild(rarmm.get());

	shared_ptr<Object> b = make_shared<Object>("Robot");
	mScene->AddObject(b);
	mObjects.push_back(b.get());
	b->AddChild(body.get());

	Robot* robot = new Robot();
	robot->mBody = b.get();
	robot->mHead = neck.get();
	robot->mLArm = lshoulder.get();
	robot->mRArm = rshoulder.get();
	mRobots.push_back(robot);
	return robot;
}

void MeshViewer::LoadRobot() {
	for (int i = 0; i < 100; i++) {
		float t = 2 * PI * (float)rand() / (float)RAND_MAX;
		float r = (float)rand() / (float)RAND_MAX;
		AddRobot()->mBody->LocalPosition(100.f*r*r*float3(cosf(t), 0, sinf(t)));
	}
	mLoading = false;
}

void MeshViewer::LoadSpline() {
	for (auto& l : mLoaded) l.second->mEnabled = false;

	if (mLoaded.count("_Spline"))
		mLoaded.at("_Spline")->mEnabled = true;
	else {
		shared_ptr<SplineRenderer> spline = make_shared<SplineRenderer>("Spline");
		vector<float3> pts;
		for (uint32_t i = 0; i < 8; i++){
			float t = 2 * PI * i / 8.f;
			float3 p(3*cosf(t), 3.f + sinf((float)i), 3*sinf(t));
			pts.push_back(p);
			pts.push_back(p + float3(.5f*sinf(t), .3f*cosf((float)i), .5f*cosf(t)));
		}
		spline->Points(pts);
		mScene->AddObject(spline);
		mObjects.push_back(spline.get());
		mLoaded.emplace("_Spline", spline.get());

		mSpline = spline.get();
	}
}

void GetFiles(const string& path, vector<fs::path>& files) {
	for (auto f : fs::directory_iterator(path)) {
		if (fs::is_directory(f.path()))
			GetFiles(f.path().string(), files);
		else if (fs::is_regular_file(f.path()))
			files.push_back(f.path());
	}
}

bool MeshViewer::Init(Scene* scene) {
	mScene = scene;

	Texture* white = mScene->AssetManager()->LoadTexture("Assets/white.png");
	mPBRShader  = mScene->AssetManager()->LoadShader("Shaders/pbr.shader");

	#pragma region Lights
	shared_ptr<Light> light0 = make_shared<Light>("Spot");
	light0->Type(Spot);
	light0->Intensity(10.f);
	light0->InnerSpotAngle(radians(10.f));
	light0->OuterSpotAngle(radians(25.f));
	light0->Range(12);
	light0->Color(float3(1.f, .7f, .5f));
	light0->LocalRotation(quaternion(radians(float3(45, 45, 0))));
	light0->LocalPosition(light0->LocalRotation() * float3(0, 0, -3) + float3(0, 1, 0));
	light0->CastShadows(true);
	mObjects.push_back(light0.get());
	mLights.push_back(light0.get());
	mScene->AddObject(light0);

	shared_ptr<Light> light1 = make_shared<Light>("Point");
	light1->Type(Point);
	light1->Intensity(.8f);
	light1->Range(3.f);
	light1->LocalPosition(-1.25f, 1.5f, -.25f);
	light1->Color(float3(.5f, .5f, 1.f));
	mObjects.push_back(light1.get());
	mLights.push_back(light1.get());
	mScene->AddObject(light1);

	shared_ptr<Light> light2 = make_shared<Light>("Point");
	light2->Type(Point);
	light2->Intensity(.8f);
	light2->Range(3.f);
	light2->LocalPosition(1.25f, 1.5f, -.25f);
	light2->Color(float3(1.f, .5f, .5f));
	mObjects.push_back(light2.get());
	mLights.push_back(light2.get());
	mScene->AddObject(light2);

	shared_ptr<Light> light3 = make_shared<Light>("Sun");
	light3->Type(Sun);
	light3->Intensity(.1f);
	light3->Color(float3(1.f, .95f, .9f));
	light3->LocalRotation(quaternion(radians(float3(45, -45, 0))));
	light3->LocalPosition(float3(0, 3, 0));
	light3->CastShadows(true);
	mLights.push_back(light3.get());
	mObjects.push_back(light3.get());
	mScene->AddObject(light3);
	#pragma endregion

	#pragma region Menu
	Font* font = mScene->AssetManager()->LoadFont("Assets/OpenSans-Regular.ttf", 96);
	Font* boldfont = mScene->AssetManager()->LoadFont("Assets/OpenSans-Bold.ttf", 96);

	#pragma region Header
	shared_ptr<UICanvas> panel = make_shared<UICanvas>("MeshViewerPanel", float2(.15f, .25f));
	panel->RenderQueue(5000);
	panel->LocalPosition(0.2f, 1.f, -0.1f);
	mPanel = panel.get();

	shared_ptr<UIImage> panelbg = panel->AddElement<UIImage>("Background", panel.get());
	panelbg->Depth(1.f);
	panelbg->Texture(white);
	panelbg->Color(float4(0, 0, 0, .5f));
	panelbg->Extent(1, 1, 0, 0);
	panelbg->Outline(true);

	shared_ptr<UILabel> title = panel->AddElement<UILabel>("Title", panel.get());
	title->VerticalAnchor(TextAnchor::Minimum);
	title->Font(boldfont);
	title->Position(0, 1, 0, -.02f);
	title->Extent(1, 0, 0, .02f);
	title->TextScale(.05f);
	title->Text("MeshViewer");
	title->mRecieveRaycast = true;
	mTitleText = title.get();
	
	shared_ptr<UIImage> separator = panel->AddElement<UIImage>("Separator", panel.get());
	separator->Texture(white);
	separator->Color(float4(0));
	separator->Outline(true);
	separator->Position(0, 1, 0, -.05f);
	separator->Extent(.95f, 0, 0, 0);
	#pragma endregion

	#pragma region File loading
	shared_ptr<UILayout> fileLayout = panel->AddElement<UILayout>("File Loader", panel.get());
	fileLayout->Extent(.9f, 1, 0, -.025f);
	fileLayout->Position(0, 0, 0, -.05f);
	fileLayout->Spacing(.0025f);
	mFileLoadPanel = fileLayout.get();

	shared_ptr<UIImage> loadBar = panel->AddElement<UIImage>("LoadBar", panel.get());
	loadBar->Depth(.1f);
	loadBar->Texture(white);
	loadBar->Color(float4(.05f, .25f, .05f, 1));
	loadBar->Position(0, -1, 0, .01f);
	loadBar->Extent(1, 0, 0, .01f);
	loadBar->Outline(false);
	mLoadBar = loadBar.get();

	shared_ptr<UILabel> loadText = panel->AddElement<UILabel>("LoadText", panel.get());
	loadText->Depth(.01f);
	loadText->Font(font);
	loadText->TextScale(.0175f);
	loadText->Position(0, -1, 0, .01f);
	loadText->Extent(1, 0, 0, .01f);
	mLoadText = loadText.get();

	auto AddButton = [&](const string& name, const string& text){
		shared_ptr<UIImage> bg = panel->AddElement<UIImage>(name, panel.get());
		bg->Texture(white);
		bg->Color(float4(0));
		bg->Extent(1, 0, 0, .015f);
		bg->Outline(false);
		bg->mRecieveRaycast = true;
		fileLayout->AddChild(bg.get());
		mLoadFileButtons.push_back(bg.get());

		shared_ptr<UILabel> btn = panel->AddElement<UILabel>(name, panel.get());
		btn->VerticalAnchor(TextAnchor::Middle);
		btn->HorizontalAnchor(TextAnchor::Minimum);
		btn->Font(font);
		btn->Extent(.9f, 1, 0, 0);
		btn->TextScale(bg->AbsoluteExtent().y * 2);
		btn->Text(text);
		bg->AddChild(btn.get());
	};

	vector<fs::path> files;
	GetFiles("Assets", files);
	for (auto f : files) {
		string ext = f.extension().string();
		if (ext != ".obj" && ext != ".fbx" && ext != ".gltf" && ext != ".glb") continue;
		AddButton(f.string(), f.filename().string());
	}

	AddButton("_Robot", "Robot");
	AddButton("_Spline", "Spline");

	fileLayout->UpdateLayout();
	#pragma endregion

	#pragma region Light editor
	shared_ptr<UILayout> lightLayout = panel->AddElement<UILayout>("Light Editor", panel.get());
	lightLayout->Extent(.9f, 1, 0, -.025f);
	lightLayout->Position(0, 0, 0, -.05f);
	lightLayout->Spacing(.0015f);
	mLightSettingsPanel = lightLayout.get();

	shared_ptr<UILabel> colorLabel = panel->AddElement<UILabel>("Color Label", panel.get());
	colorLabel->VerticalAnchor(TextAnchor::Middle);
	colorLabel->HorizontalAnchor(TextAnchor::Minimum);
	colorLabel->Font(font);
	colorLabel->Extent(.98f, 0, 0, .01f);
	colorLabel->TextScale(.02f);
	colorLabel->Text("Color");
	lightLayout->AddChild(colorLabel.get());

	shared_ptr<UILabel> intensityLabel = panel->AddElement<UILabel>("Intensity Label", panel.get());
	intensityLabel->VerticalAnchor(TextAnchor::Middle);
	intensityLabel->HorizontalAnchor(TextAnchor::Minimum);
	intensityLabel->Font(font);
	intensityLabel->Extent(.98f, 0, 0, .01f);
	intensityLabel->TextScale(.02f);
	intensityLabel->Text("Intensity");
	lightLayout->AddChild(intensityLabel.get());
	
	shared_ptr<UILabel> typeLabel = panel->AddElement<UILabel>("Type Label", panel.get());
	typeLabel->VerticalAnchor(TextAnchor::Middle);
	typeLabel->HorizontalAnchor(TextAnchor::Minimum);
	typeLabel->Font(font);
	typeLabel->Extent(.98f, 0, 0, .01f);
	typeLabel->TextScale(.02f);
	typeLabel->Text("Type");
	lightLayout->AddChild(typeLabel.get());

	shared_ptr<UILabel> radiusLabel = panel->AddElement<UILabel>("Radius Label", panel.get());
	radiusLabel->VerticalAnchor(TextAnchor::Middle);
	radiusLabel->HorizontalAnchor(TextAnchor::Minimum);
	radiusLabel->Font(font);
	radiusLabel->Extent(.98f, 0, 0, .01f);
	radiusLabel->TextScale(.02f);
	radiusLabel->Text("Radius");
	lightLayout->AddChild(radiusLabel.get());

	shared_ptr<UILabel> rangeLabel = panel->AddElement<UILabel>("Range Label", panel.get());
	rangeLabel->VerticalAnchor(TextAnchor::Middle);
	rangeLabel->HorizontalAnchor(TextAnchor::Minimum);
	rangeLabel->Font(font);
	rangeLabel->Extent(.98f, 0, 0, .01f);
	rangeLabel->TextScale(.02f);
	rangeLabel->Text("Range");
	lightLayout->AddChild(rangeLabel.get());

	shared_ptr<UILabel> outerAngleLabel = panel->AddElement<UILabel>("Outer Angle Label", panel.get());
	outerAngleLabel->VerticalAnchor(TextAnchor::Middle);
	outerAngleLabel->HorizontalAnchor(TextAnchor::Minimum);
	outerAngleLabel->Font(font);
	outerAngleLabel->Extent(.98f, 0, 0, .01f);
	outerAngleLabel->TextScale(.02f);
	outerAngleLabel->Text("Outer Angle");
	lightLayout->AddChild(outerAngleLabel.get());

	shared_ptr<UILabel> innerAngleLabel = panel->AddElement<UILabel>("Inner Angle Label", panel.get());
	innerAngleLabel->VerticalAnchor(TextAnchor::Middle);
	innerAngleLabel->HorizontalAnchor(TextAnchor::Minimum);
	innerAngleLabel->Font(font);
	innerAngleLabel->Extent(.98f, 0, 0, .01f);
	innerAngleLabel->TextScale(.02f);
	innerAngleLabel->Text("Inner Angle");
	lightLayout->AddChild(innerAngleLabel.get());

	lightLayout->UpdateLayout();
	lightLayout->mVisible = false;
	#pragma endregion

	mScene->AddObject(panel);
	mObjects.push_back(panel.get());
	#pragma endregion
	
	return true;
}

void MeshViewer::Update() {
	MouseKeyboardInput* input = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

	if (mLoading) {
		char str[16];
		sprintf(str, "%d%%", (int)(mLoadProgress*100.f+.5f));
		mLoadText->Text(str);
		mLoadText->mVisible = true;

		UDim2 position = mLoadBar->Position();
		UDim2 extent = mLoadBar->Extent();
		mLoadBar->Extent(mLoadProgress, extent.mScale.y, extent.mOffset.x, extent.mOffset.y);
		mLoadBar->Position(mLoadProgress - 1.f, position.mScale.y, position.mOffset.x, position.mOffset.y);
		mLoadBar->mVisible = true;
	} else {
		if (mLoadThread.joinable()) mLoadThread.join();
		mLoadText->mVisible = false;
		mLoadBar->mVisible = false;
	}

	if (mDraggingPanel) {
		if (!input->MouseButtonDown(GLFW_MOUSE_BUTTON_LEFT))
			mDraggingPanel = false;
		else
			mPanel->LocalPosition() += mScene->Cameras()[0]->WorldRotation() * float3(input->CursorDelta() * float2(1, -1) * .1f, 0);
	} else {
		const Ray& ray = input->GetPointer(0)->mWorldRay;
		float hitT;
		Collider* hit = mScene->Raycast(ray, hitT);
		if (hit) {
			if (hit == mPanel) {
				UIElement* elem = mPanel->Raycast(ray);
				if (elem == mTitleText) {
					if (input->MouseButtonDownFirst(GLFW_MOUSE_BUTTON_LEFT))
						mDraggingPanel = true;
				} else {
					for (UIImage* i : mLoadFileButtons) {
						if (!mLoading && i == elem) {
							i->Color(float4(1, 1, 1, .25f));
							i->Outline(true);
							if (input->MouseButtonDownFirst(GLFW_MOUSE_BUTTON_LEFT)) {
								if (elem->mName == "_Robot")
									LoadRobot();
								else if (elem->mName == "_Spline")
									LoadSpline();
								else
									LoadAsync(elem->mName, 1);
							}
						} else {
							i->Color(float4(0));
							i->Outline(false);
						}
					}
				}
			}
		}
		
		// Toggle menu on/off
		if (input->MouseButtonDownFirst(GLFW_MOUSE_BUTTON_RIGHT)) {
			if (mScene->Instance()->TotalTime() - mLastClick < .2f) {
				mPanel->mEnabled = !mPanel->mEnabled;
				const InputPointer* ptr = input->GetPointer(0);
				mPanel->LocalPosition(ptr->mWorldRay.mOrigin + ptr->mWorldRay.mDirection * 1.5f);
				mPanel->LocalRotation(mScene->Cameras()[0]->WorldRotation());
			}
			mLastClick = mScene->Instance()->TotalTime();
		}
	}

	uint32_t x = 0;
	for (Robot* r : mRobots) {
		float t = mScene->Instance()->TotalTime() + (x % 100);
		t *= 3.f;

		float3 v = mScene->Cameras()[0]->WorldPosition() - r->mBody->WorldPosition();
		if (mSpline){
			//static float t2 = 0;
			//t2 += mScene->Instance()->DeltaTime() * .1f  / length(mSpline->Derivative(t2));
			float t2 = t * .01f;
			r->mBody->LocalPosition(mSpline->Evaluate(t2));
			r->mBody->LocalRotation(quaternion(float3(0,1,0), normalize(mSpline->Derivative(t2))));
		}else{
			r->mBody->LocalRotation(slerp(r->mBody->LocalRotation(), quaternion(float3(0, -atan2f(v.z, v.x), 0)), .075f));
			r->mBody->LocalPosition(r->mBody->LocalPosition().x, .6f + sinf(t + PI * .25f) * .05f, r->mBody->LocalPosition().z);
		}

		v = mScene->Cameras()[0]->WorldPosition() - r->mHead->WorldPosition();
		quaternion q(float3(-atan2f(v.y, sqrtf(v.x*v.x + v.z*v.z)) - PI / 2, -atan2f(v.z, v.x) + PI / 2, 0));
		q = inverse(r->mHead->Parent()->WorldRotation()) * q;
		r->mHead->LocalRotation(slerp(r->mHead->LocalRotation(), q, .2f));
		r->mHead->LocalPosition(r->mHead->LocalPosition().x, sinf(t + PI * .25f - .01f) * .01f, r->mHead->LocalPosition().z);

		r->mRArm->LocalRotation(quaternion(float3(-sinf(t) * PI * .25f, 0, 0)));
		r->mLArm->LocalRotation(quaternion(float3( sinf(t) * PI * .25f, 0, 0)));


		x ^= 0x12f343a5;
		x += 0xa234fab;
	}
}

void MeshViewer::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
	MouseKeyboardInput* input = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

	const Ray& ray = input->GetPointer(0)->mWorldRay;
	float hitT;
	Collider* hit = mScene->Raycast(ray, hitT);

	Gizmos* gizmos = mScene->Gizmos();

	bool change = input->MouseButtonDownFirst(GLFW_MOUSE_BUTTON_LEFT);

	// manipulate selection
	Light* selectedLight = nullptr;
	if (mSelected) {
		selectedLight = dynamic_cast<Light*>(mSelected);
		if (selectedLight) {
			mLightSettingsPanel->mVisible = true;
			mFileLoadPanel->mVisible = false;

			switch (selectedLight->Type()) {
			case LightType::Spot:
				gizmos->DrawWireSphere(selectedLight->WorldPosition(), selectedLight->Radius(), float4(selectedLight->Color(), .5f));
				gizmos->DrawWireCircle(selectedLight->WorldPosition() + selectedLight->WorldRotation() * float3(0, 0, selectedLight->Range()),
					selectedLight->Range() * tanf(selectedLight->InnerSpotAngle() * .5f), selectedLight->WorldRotation(), float4(selectedLight->Color(), .5f));
				gizmos->DrawWireCircle(
					selectedLight->WorldPosition() + selectedLight->WorldRotation() * float3(0, 0, selectedLight->Range()),
					selectedLight->Range() * tanf(selectedLight->OuterSpotAngle() * .5f), selectedLight->WorldRotation(), float4(selectedLight->Color(), .5f));
				break;

			case LightType::Point:
				gizmos->DrawWireSphere(selectedLight->WorldPosition(), selectedLight->Radius(), float4(selectedLight->Color(), .5f));
				gizmos->DrawWireSphere(selectedLight->WorldPosition(), selectedLight->Range(), float4(selectedLight->Color(), .2f));
				break;
			}
		}

		if (input->KeyDown(GLFW_KEY_LEFT_SHIFT)) {
			quaternion r = mSelected->WorldRotation();
			if (mScene->Gizmos()->RotationHandle(input->GetPointer(0), mSelected->WorldPosition(), r)) {
				mSelected->LocalRotation(r);
				change = false;
			}
		} else {
			float3 p = mSelected->WorldPosition();
			if (mScene->Gizmos()->PositionHandle(input->GetPointer(0), camera->WorldRotation(), p)) {
				mSelected->LocalPosition(p);
				change = false;
			}
		}
	} else {
		mLightSettingsPanel->mVisible = false;
		mFileLoadPanel->mVisible = true;
	}

	// change selection
	if (change) mSelected = nullptr;
	for (Light* light : mLights) {
		float lt = ray.Intersect(Sphere(light->WorldPosition(), .09f)).x;
		bool hover = lt > 0 && (hitT < 0 || lt < hitT);

		float3 col = light->mEnabled ? light->Color() : light->Color() * .2f;
		gizmos->DrawBillboard(light->WorldPosition(), hover && light != selectedLight ? .09f : .075f, camera->WorldRotation(), float4(col, 1),
			mScene->AssetManager()->LoadTexture("Assets/icons.png"), float4(.5f, .5f, 0, 0));

		if (hover) {
			hitT = lt;
			if (input->MouseButtonDownFirst(GLFW_MOUSE_BUTTON_RIGHT))
				light->mEnabled = !light->mEnabled;
			if (change) mSelected = light;
		}
	}
}