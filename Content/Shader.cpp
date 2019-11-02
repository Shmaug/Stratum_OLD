#include <Content/Shader.hpp>

#include <string>

using namespace std;

void ReadBindingsAndPushConstants(ifstream& file,
	unordered_map<string, pair<uint32_t, VkDescriptorSetLayoutBinding>>& destBindings,
	unordered_map<string, VkPushConstantRange>& destPushConstants,
	vector<string>& destStaticSamplers) {

	uint32_t bc;
	file.read(reinterpret_cast<char*>(&bc), sizeof(uint32_t));
	for (uint32_t j = 0; j < bc; j++) {
		string name;
		uint32_t nlen;
		file.read(reinterpret_cast<char *>(&nlen), sizeof(uint32_t));
		name.resize(nlen);
		file.read(const_cast<char *>(name.data()), nlen);

		auto &binding = destBindings[name];
		file.read(reinterpret_cast<char *>(&binding.first), sizeof(uint32_t));
		file.read(reinterpret_cast<char *>(&binding.second.binding), sizeof(uint32_t));
		file.read(reinterpret_cast<char *>(&binding.second.descriptorCount), sizeof(uint32_t));
		file.read(reinterpret_cast<char *>(&binding.second.descriptorType), sizeof(uint32_t));
		file.read(reinterpret_cast<char *>(&binding.second.stageFlags), sizeof(VkShaderStageFlagBits));
		uint32_t static_sampler;
		file.read(reinterpret_cast<char *>(&static_sampler), sizeof(uint32_t));
		if (static_sampler)
			destStaticSamplers.push_back(name);
	}

	file.read(reinterpret_cast<char *>(&bc), sizeof(uint32_t));
	for (unsigned int j = 0; j < bc; j++) {
		string name;
		uint32_t nlen;
		file.read(reinterpret_cast<char *>(&nlen), sizeof(uint32_t));
		name.resize(nlen);
		file.read(const_cast<char *>(name.data()), nlen);

		auto &binding = destPushConstants[name];
		file.read(reinterpret_cast<char *>(&binding.offset), sizeof(uint32_t));
		file.read(reinterpret_cast<char *>(&binding.size), sizeof(uint32_t));
		file.read(reinterpret_cast<char *>(&binding.stageFlags), sizeof(VkShaderStageFlagBits));
	}
}
void EvalPushConstants(ifstream& file,
	const unordered_map<string, VkPushConstantRange>& pushConstants,
	vector<VkPushConstantRange>& constants) {

	unordered_map<VkShaderStageFlags, uint2> ranges;
	for (const auto &b : pushConstants) {
		if (ranges.count(b.second.stageFlags) == 0)
			ranges[b.second.stageFlags] = uint2(b.second.offset, b.second.offset + b.second.size);
		else {
			ranges[b.second.stageFlags].x = min(ranges[b.second.stageFlags].x, b.second.offset);
			ranges[b.second.stageFlags].y = max(ranges[b.second.stageFlags].y, b.second.offset + b.second.size);
		}
	}
	for (auto r : ranges) {
		constants.push_back({});
		constants.back().stageFlags = r.first;
		constants.back().offset = r.second.x;
		constants.back().size = r.second.y - r.second.x;
	}
}

bool PipelineInstance::operator==(const PipelineInstance& rhs) const {
	return rhs.mRenderPass == mRenderPass &&
		((!rhs.mVertexInput && !mVertexInput) || (rhs.mVertexInput && mVertexInput && *rhs.mVertexInput == *mVertexInput)) &&
		mTopology == rhs.mTopology &&
		mCullMode == rhs.mCullMode &&
		mBlendMode == rhs.mBlendMode;
}

Shader::Shader(const string& name, ::DeviceManager* devices, const string& filename)
	: mName(name), mViewportState({}), mRasterizationState({}), mDynamicState({}), mBlendMode(Opaque), mDepthStencilState({}) {
	ifstream file(filename, ios::binary);
	if (!file.is_open()) throw runtime_error("Could not load " + filename);

	uint32_t vc;
	file.read(reinterpret_cast<char*>(&vc), sizeof(uint32_t));
	for (unsigned int v = 0; v < vc; v++) {
		set<string> keywords;

		uint32_t kwc;
		file.read(reinterpret_cast<char*>(&kwc), sizeof(uint32_t));
		for (unsigned int i = 0; i < kwc; i++) {
			string keyword;
			uint32_t kwlen;
			file.read(reinterpret_cast<char*>(&kwlen), sizeof(uint32_t));
			keyword.resize(kwlen);
			file.read(const_cast<char*>(keyword.data()), kwlen);

			keywords.insert(keyword);
			mKeywords.insert(keyword);
		}

		string kw = "";
		for (const auto& k : keywords)
			kw += k + " ";

		vector<string> entryPoints;
		vector<vector<uint32_t>> modules;

		uint32_t is_compute;
		file.read(reinterpret_cast<char*>(&is_compute), sizeof(uint32_t));
		if (is_compute) {
			uint32_t kernelc;
			file.read(reinterpret_cast<char*>(&kernelc), sizeof(uint32_t));
			modules.resize(kernelc);
			entryPoints.resize(kernelc);
			vector<unordered_map<string, pair<uint32_t, VkDescriptorSetLayoutBinding>>> descriptorBindings(kernelc);
			vector<unordered_map<string, VkPushConstantRange>> pushConstants(kernelc);
			vector<vector<string>> staticSamplers(kernelc);

			vector<uint3> workgroupSizes(kernelc);
			for (unsigned int i = 0; i < kernelc; i++) {
				string entryp;
				uint32_t entryplen;
				file.read(reinterpret_cast<char*>(&entryplen), sizeof(uint32_t));
				entryp.resize(entryplen);
				file.read(const_cast<char*>(entryp.data()), entryplen);
				entryPoints[i] = entryp;

				vector<uint32_t>& spirv = modules[i];

				uint32_t spirvSize;
				file.read(reinterpret_cast<char*>(&spirvSize), sizeof(uint32_t));
				spirv.resize(spirvSize);
				file.read(reinterpret_cast<char*>(spirv.data()), spirvSize * sizeof(uint32_t));

				file.read(reinterpret_cast<char*>(&workgroupSizes[i]), sizeof(uint3));

				ReadBindingsAndPushConstants(file, descriptorBindings[i], pushConstants[i], staticSamplers[i]);
			}

			for (uint32_t i = 0; i < devices->DeviceCount(); i++) {
				Device* device = devices->GetDevice(i);
				DeviceData& d = mDeviceData[device];

				for (unsigned int j = 0; j < kernelc; j++) {
					ComputeShader*& cv = d.mComputeVariants[entryPoints[j]][kw];
					if (!cv) cv = new ComputeShader();

					cv->mWorkgroupSize = workgroupSizes[j];
					cv->mDescriptorBindings = descriptorBindings[j];
					cv->mPushConstants = pushConstants[j];
					cv->mEntryPoint = entryPoints[j];

					cv->mStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
					cv->mStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
					cv->mStage.pName = cv->mEntryPoint.c_str();

					VkShaderModuleCreateInfo module = {};
					module.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
					module.codeSize = modules[j].size() * sizeof(uint32_t);
					module.pCode = modules[j].data();
					vkCreateShaderModule(*device, &module, nullptr, &cv->mStage.module);

					vector<vector<VkDescriptorSetLayoutBinding>> bindings;
					for (const auto& b : cv->mDescriptorBindings) {
						if (bindings.size() <= b.second.first) bindings.resize((size_t)b.second.first + 1);
						bindings[b.second.first].push_back(b.second.second);
						bool static_sampler = false;
						for (const auto& s : staticSamplers[j])
							if (s == b.first) {
								static_sampler = true;
								break;
							}

						if (static_sampler) {
							Sampler* sampler = new Sampler(b.first, device, 12);
							VkSampler vksampler = *sampler;
							bindings[b.second.first].back().pImmutableSamplers = &vksampler;
							d.mStaticSamplers.push_back(sampler);
						}
					}

					cv->mDescriptorSetLayouts.resize(bindings.size());
					for (unsigned int b = 0; b < bindings.size(); b++) {
						VkDescriptorSetLayoutCreateInfo descriptorSetLayout = {};
						descriptorSetLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
						descriptorSetLayout.bindingCount = (uint32_t)bindings[b].size();
						descriptorSetLayout.pBindings = bindings[b].data();

						vkCreateDescriptorSetLayout(*device, &descriptorSetLayout, nullptr, &cv->mDescriptorSetLayouts[b]);
						device->SetObjectName(cv->mDescriptorSetLayouts[b], mName + " DescriptorSetLayout");
					}

					vector<VkPushConstantRange> constants;
					EvalPushConstants(file, cv->mPushConstants, constants);

					VkPipelineLayoutCreateInfo layout = {};
					layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
					layout.setLayoutCount = (uint32_t)cv->mDescriptorSetLayouts.size();
					layout.pSetLayouts = cv->mDescriptorSetLayouts.data();
					layout.pushConstantRangeCount = (uint32_t)constants.size();
					layout.pPushConstantRanges = constants.data();
					vkCreatePipelineLayout(*device, &layout, nullptr, &cv->mPipelineLayout);
					device->SetObjectName(cv->mPipelineLayout, mName + " PipelineLayout");

					VkComputePipelineCreateInfo pipeline = {};
					pipeline.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
					pipeline.stage = cv->mStage;
					pipeline.layout = cv->mPipelineLayout;
					pipeline.basePipelineIndex = -1;
					pipeline.basePipelineHandle = VK_NULL_HANDLE;
					vkCreateComputePipelines(*device, device->PipelineCache(), 1, &pipeline, nullptr, &cv->mPipeline);
					device->SetObjectName(cv->mPipeline, mName);
				}
			}
		} else {
			vector<VkPipelineShaderStageCreateInfo> stages;
			unordered_map<string, pair<uint32_t, VkDescriptorSetLayoutBinding>> descriptorBindings;
			unordered_map<string, VkPushConstantRange> pushConstants;
			vector<string> staticSamplers;

			uint32_t stagec;
			file.read(reinterpret_cast<char*>(&stagec), sizeof(uint32_t));
			for (unsigned int i = 0; i < stagec; i++) {
				stages.push_back({});
				VkPipelineShaderStageCreateInfo& stage = stages.back();
				stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				file.read(reinterpret_cast<char*>(&stage.stage), sizeof(VkShaderStageFlagBits));

				string entryp;
				uint32_t entryplen;
				file.read(reinterpret_cast<char*>(&entryplen), sizeof(uint32_t));
				entryp.resize(entryplen);
				file.read(const_cast<char*>(entryp.data()), entryplen);
				entryPoints.push_back(entryp);

				modules.push_back({});
				vector<uint32_t>& spirv = modules.back();

				uint32_t spirvSize;
				file.read(reinterpret_cast<char*>(&spirvSize), sizeof(uint32_t));
				spirv.resize(spirvSize);
				file.read(reinterpret_cast<char*>(spirv.data()), spirvSize * sizeof(uint32_t));
			}

			ReadBindingsAndPushConstants(file, descriptorBindings, pushConstants, staticSamplers);

			for (uint32_t i = 0; i < devices->DeviceCount(); i++) {
				Device* device = devices->GetDevice(i);
				DeviceData& d = mDeviceData[device];
				GraphicsShader*& gv = d.mGraphicsVariants[kw];
				if (!gv) gv = new GraphicsShader();

				gv->mShader = this;
				gv->mStages.resize(stages.size());
				gv->mEntryPoints.resize(stages.size());
				gv->mDescriptorBindings = descriptorBindings;
				gv->mPushConstants = pushConstants;

				for (uint32_t j = 0; j < modules.size(); j++) {
					gv->mEntryPoints[j] = entryPoints[j];
					gv->mStages[j] = stages[j];
					gv->mStages[j].pName = gv->mEntryPoints[j].c_str();

					VkShaderModuleCreateInfo module = {};
					module.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
					module.codeSize = modules[j].size() * sizeof(uint32_t);
					module.pCode = modules[j].data();
					vkCreateShaderModule(*device, &module, nullptr, &gv->mStages[j].module);
				}

				vector<vector<VkDescriptorSetLayoutBinding>> bindings;
				for (const auto& b : gv->mDescriptorBindings) {
					if (bindings.size() <= b.second.first) bindings.resize((size_t)b.second.first + 1);
					bindings[b.second.first].push_back(b.second.second);

					bool static_sampler = false;
					for (const auto& s : staticSamplers)
						if (s == b.first) {
							static_sampler = true;
							break;
						}

					if (static_sampler) {
						Sampler* sampler = new Sampler(b.first, device, 12);
						VkSampler vksampler = *sampler;
						bindings[b.second.first].back().pImmutableSamplers = &vksampler;
						d.mStaticSamplers.push_back(sampler);
					}
				}

				gv->mDescriptorSetLayouts.resize(bindings.size());
				for (uint32_t b = 0; b < bindings.size(); b++) {
					VkDescriptorSetLayoutCreateInfo descriptorSetLayout = {};
					descriptorSetLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
					descriptorSetLayout.bindingCount = (uint32_t)bindings[b].size();
					descriptorSetLayout.pBindings = bindings[b].data();

					vkCreateDescriptorSetLayout(*device, &descriptorSetLayout, nullptr, &gv->mDescriptorSetLayouts[b]);
					device->SetObjectName(gv->mDescriptorSetLayouts[b], mName + " DescriptorSetLayout");
				}

				vector<VkPushConstantRange> constants;
				EvalPushConstants(file, gv->mPushConstants, constants);

				VkPipelineLayoutCreateInfo layout = {};
				layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
				layout.setLayoutCount = (uint32_t)gv->mDescriptorSetLayouts.size();
				layout.pSetLayouts = gv->mDescriptorSetLayouts.data();
				layout.pushConstantRangeCount = (uint32_t)constants.size();
				layout.pPushConstantRanges = constants.data();
				vkCreatePipelineLayout(*device, &layout, nullptr, &gv->mPipelineLayout);
				device->SetObjectName(gv->mPipelineLayout, mName + " PipelineLayout");
			}
		}
	}

	mViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	mViewportState.viewportCount = 1;
	mViewportState.scissorCount = 1;

	mRasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	mRasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
	mRasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
	mRasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	mRasterizationState.depthClampEnable = VK_FALSE;
	mRasterizationState.lineWidth = 1.f;

	mDynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_LINE_WIDTH,
	};
	mDynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	mDynamicState.dynamicStateCount = (uint32_t)mDynamicStates.size();
	mDynamicState.pDynamicStates = mDynamicStates.data();

	file.read(reinterpret_cast<char*>(&mRenderQueue), sizeof(uint32_t));
	file.read(reinterpret_cast<char*>(&mRasterizationState.cullMode), sizeof(VkCullModeFlags));
	file.read(reinterpret_cast<char*>(&mRasterizationState.polygonMode), sizeof(VkPolygonMode));
	file.read(reinterpret_cast<char*>(&mBlendMode), sizeof(BlendMode));
	file.read(reinterpret_cast<char*>(&mDepthStencilState), sizeof(VkPipelineDepthStencilStateCreateInfo));
}
Shader::~Shader() {
	for (auto& d : mDeviceData) {
		for (auto& g : d.second.mStaticSamplers)
			safe_delete(g);

		for (auto& v : d.second.mGraphicsVariants) {
			for (auto& s : v.second->mPipelines)
				vkDestroyPipeline(*d.first, s.second, nullptr);
			for (auto& s : v.second->mDescriptorSetLayouts)
				vkDestroyDescriptorSetLayout(*d.first, s, nullptr);
			vkDestroyPipelineLayout(*d.first, v.second->mPipelineLayout, nullptr);
			for (auto& s : v.second->mStages)
				vkDestroyShaderModule(*d.first, s.module, nullptr);
			safe_delete(v.second);
		}
		for (auto& s : d.second.mComputeVariants) {
			for (auto& v : s.second) {
				for (auto& l : v.second->mDescriptorSetLayouts)
					vkDestroyDescriptorSetLayout(*d.first, l, nullptr);
				vkDestroyPipeline(*d.first, v.second->mPipeline, nullptr);
				vkDestroyPipelineLayout(*d.first, v.second->mPipelineLayout, nullptr);
				vkDestroyShaderModule(*d.first, v.second->mStage.module, nullptr);
				safe_delete(v.second);
			}
		}
	}
}

VkPipeline GraphicsShader::GetPipeline(RenderPass* renderPass, const VertexInput* vertexInput, VkPrimitiveTopology topology, VkCullModeFlags cullMode, BlendMode blendMode) {
	BlendMode blend = blendMode == BLEND_MODE_MAX_ENUM ? mShader->mBlendMode : blendMode;
	VkCullModeFlags cull = cullMode == VK_CULL_MODE_FLAG_BITS_MAX_ENUM ? mShader->mRasterizationState.cullMode : cullMode;
	PipelineInstance instance(*renderPass, vertexInput, topology, cull, blendMode);

	if (mPipelines.count(instance))
		return mPipelines.at(instance);
	else {
		if (mStages.size() == 0) return VK_NULL_HANDLE;

		VkPipelineColorBlendAttachmentState bs = {};
		bs.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		switch (blend) {
		case Opaque:
			bs.blendEnable = VK_FALSE;
			bs.colorBlendOp = VK_BLEND_OP_ADD;
			bs.alphaBlendOp = VK_BLEND_OP_ADD;
			bs.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			bs.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
			bs.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			bs.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		case Alpha:
			bs.blendEnable = VK_TRUE;
			bs.colorBlendOp = VK_BLEND_OP_ADD;
			bs.alphaBlendOp = VK_BLEND_OP_ADD;
			bs.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			bs.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			bs.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			bs.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			break;
		case Additive:
			bs.blendEnable = VK_TRUE;
			bs.colorBlendOp = VK_BLEND_OP_ADD;
			bs.alphaBlendOp = VK_BLEND_OP_ADD;
			bs.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			bs.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
			bs.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			bs.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			break;
		case Multiply:
			bs.blendEnable = VK_TRUE;
			bs.colorBlendOp = VK_BLEND_OP_MULTIPLY_EXT;
			bs.alphaBlendOp = VK_BLEND_OP_MULTIPLY_EXT;
			bs.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			bs.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
			bs.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			bs.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			break;
		}
		vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates(renderPass->ColorAttachmentCount(), bs);

		VkPipelineRasterizationStateCreateInfo rasterState = mShader->mRasterizationState;
		if (cullMode != VK_CULL_MODE_FLAG_BITS_MAX_ENUM)
			rasterState.cullMode = cullMode;

		VkPipelineColorBlendStateCreateInfo blendState = {};
		blendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		blendState.attachmentCount = (uint32_t)blendAttachmentStates.size();
		blendState.pAttachments = blendAttachmentStates.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
		inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyState.topology = topology;
		inputAssemblyState.primitiveRestartEnable = VK_FALSE;

		VkPipelineVertexInputStateCreateInfo vinput = {};
		vinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		if (vertexInput) {
			vinput.vertexBindingDescriptionCount = 1;
			vinput.pVertexBindingDescriptions = &vertexInput->mBinding;
			vinput.vertexAttributeDescriptionCount = (uint32_t)vertexInput->mAttributes.size();
			vinput.pVertexAttributeDescriptions = vertexInput->mAttributes.data();
		} else {
			vinput.vertexBindingDescriptionCount = 0;
			vinput.pVertexBindingDescriptions = nullptr;
			vinput.vertexAttributeDescriptionCount = 0;
			vinput.pVertexAttributeDescriptions = nullptr;
		}

		VkPipelineMultisampleStateCreateInfo multisampleState = {};
		multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleState.sampleShadingEnable = VK_FALSE;
		multisampleState.rasterizationSamples = renderPass->RasterizationSamples();

		VkGraphicsPipelineCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		info.stageCount = (uint32_t)mStages.size();
		info.pStages = mStages.data();
		info.pInputAssemblyState = &inputAssemblyState;
		info.pVertexInputState = &vinput;
		info.pTessellationState = nullptr;
		info.pViewportState = &mShader->mViewportState;
		info.pRasterizationState = &rasterState;
		info.pMultisampleState = &multisampleState;
		info.pDepthStencilState = &mShader->mDepthStencilState;
		info.pColorBlendState = &blendState;
		info.pDynamicState = &mShader->mDynamicState;
		info.layout = mPipelineLayout;
		info.basePipelineIndex = -1;
		info.basePipelineHandle = VK_NULL_HANDLE;
		info.renderPass = *renderPass;

		VkPipeline p;
		vkCreateGraphicsPipelines(*renderPass->Device(), renderPass->Device()->PipelineCache(), 1, &info, nullptr, &p);
		renderPass->Device()->SetObjectName(p, mShader->mName + " Variant");
		mPipelines.emplace(instance, p);

		const char* cullstr = "";
		if (cullMode == VK_CULL_MODE_NONE)
			cullstr = "VK_CULL_MODE_NONE";
		if (cullMode & VK_CULL_MODE_BACK_BIT)
			cullstr = "VK_CULL_MODE_BACK";
		if (cullMode & VK_CULL_MODE_FRONT_BIT)
			cullstr = "VK_CULL_MODE_FRONT";
		if (cullMode == VK_CULL_MODE_FRONT_AND_BACK)
			cullstr = "VK_CULL_MODE_FRONT_AND_BACK";
		
		const char* blendstr = "";
		switch (blend){
		case Opaque: blendstr = "Opaque"; break;
		case Alpha: blendstr = "Alpha"; break;
		case Additive: blendstr = "Additive"; break;
		case Multiply: blendstr = "Multiply"; break;
		}

		string kw = ""; 
		for (const auto& d : mShader->mDeviceData)
			for (const auto& k : d.second.mGraphicsVariants)
				if (k.second == this) {
					kw = k.first;
					break;
				}
		printf_color(Green, "%s [%s]: Generating graphics pipeline  %s  %s  %s\n",
			mShader->mName.c_str(), kw.c_str(), blendstr, cullstr, TopologyToString(topology));

		return p;
	}
}

GraphicsShader* Shader::GetGraphics(Device* device, const set<string>& keywords) const {
	if (!mDeviceData.count(device)) return nullptr;
	const auto& d = mDeviceData.at(device);

	string kw = "";
	for (const auto& k : keywords)
		if (mKeywords.count(k))
			kw += k + " ";

	if (!d.mGraphicsVariants.count(kw)) return nullptr;
	return d.mGraphicsVariants.at(kw);
}
ComputeShader* Shader::GetCompute(Device* device, const string& kernel, const set<string>& keywords) const {
	if (!mDeviceData.count(device)) return nullptr;
	const auto& d = mDeviceData.at(device);

	if (!d.mComputeVariants.count(kernel)) return nullptr;
	auto& k = d.mComputeVariants.at(kernel);

	string kw = "";
	for (const auto& k : keywords)
		if (mKeywords.count(k))
			kw += k + " ";

	if (!k.count(kw)) return nullptr;
	return k.at(kw);
}