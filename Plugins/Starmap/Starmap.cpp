#include <Scene/MeshRenderer.hpp>
#include <Scene/TextRenderer.hpp>
#include <Scene/Scene.hpp>
#include <Util/Profiler.hpp>

#include "Starmap.hpp"
#include "TelnetClient.hpp"

using namespace std;

ENGINE_PLUGIN(Starmap)

Starmap::Starmap() : mScene(nullptr), mRunning(false), mTelnetClient(nullptr) {
	mEnabled = true;
}
Starmap::~Starmap() {
	mRunning = false;
	if (mLoadThread.joinable()) mLoadThread.join();
	safe_delete(mTelnetClient);
	for (auto& o : mObjects)
		mScene->RemoveObject(o);
}

void Starmap::RunTelnet() {
	if (!mTelnetClient) mTelnetClient = new TelnetClient("horizons.jpl.nasa.gov", 6775);
	mRunning = mTelnetClient->Connected();

	mLoadThread = thread([&]() {
		uint32_t idx = 199;
		while (mRunning) {
			string body;
			mTelnetClient->WaitForText("Horizons>");
			if (!mTelnetClient->Send(to_string(idx) + "\n")) { mRunning = false; break; }
			mTelnetClient->WaitForText("[E]phemeris");
			body = mTelnetClient->Text();
			if (!mTelnetClient->Send("e\n")) { mRunning = false; break; }
			mTelnetClient->WaitForText("Elements");
			if (!mTelnetClient->Send("e\n")) { mRunning = false; break; }
			mTelnetClient->WaitForText("Coordinate");
			if (!mTelnetClient->Send("0\n")) { mRunning = false; break; }
			mTelnetClient->WaitForText("Reference");
			if (!mTelnetClient->Send("eclip\n")) { mRunning = false; break; }
			mTelnetClient->WaitForText("Starting");
			if (!mTelnetClient->Send("\n")) { mRunning = false; break; }
			mTelnetClient->WaitForText("Ending");
			if (!mTelnetClient->Send("\n")) { mRunning = false; break; }
			mTelnetClient->WaitForText("Output");
			if (!mTelnetClient->Send("20d\n")) { mRunning = false; break; }
			mTelnetClient->WaitForText("default");
			if (!mTelnetClient->Send("y\n")) { mRunning = false; break; }
			mTelnetClient->WaitForText("[N]ew-case");

			string eph = mTelnetClient->Text();
			size_t beg = eph.find("$$SOE");
			size_t end = eph.find("$$EOE");

			if (beg == string::npos || end == string::npos)
				printf("Failed to find SOE or EOE!\n");
			else {
				beg += 5;
				string data = eph.substr(beg, end - beg);
				printf("Got body %u\n", idx);
			}

			mTelnetClient->Clear();
			if (!mTelnetClient->Send("n\n")) { mRunning = false; break; }
			idx++;
		}
		});
}

bool Starmap::Init(Scene* scene) {
	mScene = scene;

	Shader* fontshader = mScene->AssetManager()->LoadShader("Shaders/font.shader");
	Font* font = mScene->AssetManager()->LoadFont("Assets/segoeui.ttf", 24);

	shared_ptr<Material> fontMat = make_shared<Material>("Segoe UI", fontshader);
	fontMat->SetParameter("Texture", font->Texture());



	return true;
}