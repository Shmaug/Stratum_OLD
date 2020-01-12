#include "Dicom.hpp"

#include <dcmtk/dcmimgle/dcmimage.h>
#include <dcmtk/dcmdata/dctk.h>

using namespace std;

struct Slice {
	DicomImage* image;
	double location;
};

DicomImage* ReadDicomSlice(const string& file, double& x, double3& spacing) {
	OFCondition cnd;

	DcmFileFormat fileFormat;
	assert((cnd = fileFormat.loadFile(file.c_str())).good());
	DcmDataset* dataset = fileFormat.getDataset();

	double3 s;
	cnd = dataset->findAndGetFloat64(DCM_PixelSpacing, s.x, 0);
	cnd = dataset->findAndGetFloat64(DCM_PixelSpacing, s.y, 1);
	cnd = dataset->findAndGetFloat64(DCM_SliceThickness, s.z, 0);

	spacing = max(spacing, s);

	cnd = dataset->findAndGetFloat64(DCM_SliceLocation, x, 0);

	DicomImage* img = new DicomImage(file.c_str());
	assert(img != NULL);
	assert(img->getStatus() == EIS_Normal);
	return img;
}
void ReadDicomImages(uint16_t* data, vector<Slice>& images, uint32_t j, uint32_t k, uint32_t w, uint32_t h) {
	for (int i = j; i < k; i++) {
		if (i >= (int)images.size()) break;
		images[i].image->setMinMaxWindow();
		uint16_t* pixels = (uint16_t*)images[i].image->getOutputData(16);
		memcpy(data + i * w * h, pixels, w*h*sizeof(uint16_t));
	}
}
Texture* LoadDicomStack(const string& folder, Instance* instance, float3* size) {
	vector<Slice> images;

	// Get information
	double3 spacing = 0.0;

	for (const auto& p : fs::directory_iterator(folder)) {
		#ifdef WINDOWS
		if (wcscmp(p.path().extension().c_str(), L".dcm") == 0) {
		#else
		if (strcmp(p.path().extension().c_str(), ".dcm") == 0) {
		#endif
			double x;
			DicomImage* img = ReadDicomSlice(p.path().string(), x, spacing);
			images.push_back({ img, x });
		}
	}

	std::sort(images.begin(), images.end(), [](const Slice& a, const Slice& b) {
		return a.location < b.location;
	});

	uint32_t w = images[0].image->getWidth();
	uint32_t h = images[0].image->getHeight();
	uint32_t d = (uint32_t)images.size();

	// volume size in meters
	if (size) {
		*size = float3(.001 * double3(spacing.xy * double2(w, h), (uint32_t)images.end()->location + spacing.z));
		printf("%fm x %fm x %fm\n", size->x, size->y, size->z);
	}

	uint16_t* data = new uint16_t[w * h * d];
	memset(data, 0xFFFF, w * h * d * sizeof(uint16_t));

	ReadDicomImages(data, images, 0, (int)images.size(), w, h);

	Texture* tex = new Texture(folder, instance, w, h, d,
		VK_FORMAT_R16_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
	delete[] data;

	return tex;
}