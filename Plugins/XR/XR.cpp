#include "XR.hpp"
#include <Scene/Scene.hpp>
#include <Scene/GUI.hpp>
#include <Content/Font.hpp>

using namespace std;

ENGINE_PLUGIN(XR)

XR::XR() : mScene(nullptr) {
	mEnabled = true;
}
XR::~XR() {
	for (Object* o : mObjects)
		mScene->RemoveObject(o);
}

bool XR::Init(Scene* scene) {
	mScene = scene;

	uint32_t layerPropertyCount;
	xrEnumerateApiLayerProperties(0, &layerPropertyCount, nullptr);

	if (layerPropertyCount == 0) {
		printf_color(COLOR_YELLOW, "No available XR layers!\n");
		return false;
	}

	vector<XrApiLayerProperties> layerProperties;
	xrEnumerateApiLayerProperties(layerPropertyCount, &layerPropertyCount, layerProperties.data());

	printf("Available XR layers: \n");

	for (uint32_t i = 0; i < layerPropertyCount; i++) {
		uint32_t extensionCount;
		xrEnumerateInstanceExtensionProperties(layerProperties[i].layerName, 0, &extensionCount, nullptr);
		vector<XrExtensionProperties> extensions;
		xrEnumerateInstanceExtensionProperties(layerProperties[i].layerName, extensionCount, &extensionCount, extensions.data());
		
		printf("\t%s\t%u extensions\n", layerProperties[i].layerName, extensionCount);

		for (uint32_t j = 0; j < extensionCount; j++) {
			printf("\t%s\n", extensions[j].extensionName);
		}
	}

	XrInstanceCreateInfo info = {};
	info.type = XR_TYPE_INSTANCE_CREATE_INFO;

	info.applicationInfo.apiVersion = XR_VERSION_1_0;
	memcpy(info.applicationInfo.engineName, "Stratum", strlen("Stratum"));
	info.applicationInfo.engineVersion = STRATUM_VERSION;
	memcpy(info.applicationInfo.applicationName, "Stratum", strlen("Stratum"));
	info.applicationInfo.applicationVersion = STRATUM_VERSION;

	info.enabledExtensionCount = 0;
	info.enabledApiLayerCount = 0;
	xrCreateInstance(&info, &mInstance);

	return true;
}

void XR::Update(CommandBuffer* commandBuffer) {

}

void XR::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {

}