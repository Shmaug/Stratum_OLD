#include <chrono>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include "ShaderCompiler.hpp"
#include <shaderc/shaderc.hpp>
#include <../spirv_cross.hpp>

using namespace std;
using namespace shaderc;

CompileOptions options;

class Includer : public CompileOptions::IncluderInterface {
public:
	inline Includer(const string& globalPath) : mIncludePath(globalPath) {}

	inline virtual shaderc_include_result* GetInclude(const char* requested_source, shaderc_include_type type, const char* requesting_source, size_t include_depth) override {
		fs::path folder;
		
		if (type == shaderc_include_type_relative)
			folder = fs::absolute(requesting_source).parent_path();
		else
			folder = mIncludePath;

		string fullpath = folder.string() + "/" + requested_source;

		shaderc_include_result* response = new shaderc_include_result();
		string& data = mFiles[fullpath];
		if (data.empty() && !ReadFile(fullpath, data)) {
			char* err = new char[128];
			sprintf(err, "Failed to read include file: %s while compiling %s\n", fullpath.c_str(), requesting_source);

			response->source_name = "";
			response->source_name_length = 0;
			response->content = err;
			response->content_length = strlen(response->content);;
			response->user_data = err;
		} else {
			mFullPaths.emplace(fullpath, fullpath);
			const string& name = mFullPaths.at(fullpath);

			response->source_name = name.data();
			response->source_name_length = strlen(response->source_name);
			response->content = data.data();
			response->content_length = strlen(response->content);
			response->user_data = nullptr;
		}

		return response;
	}
	inline void ReleaseInclude(shaderc_include_result* data) override {
		if (data->user_data) delete[] (char*)data->user_data;
		delete data;
	}

private:
	string mIncludePath;

	unordered_map<string, string> mFiles;
	unordered_map<string, string> mFullPaths;
};

bool CompileStage(Compiler* compiler, const CompileOptions& options, const string& source, const string& filename, shaderc_shader_kind stage, const string& entryPoint,
	CompiledVariant& dest, CompiledShader& destShader) {
	
	SpvCompilationResult result = compiler->CompileGlslToSpv(source, stage, filename.c_str(), entryPoint.c_str(), options);
	
	string error = result.GetErrorMessage();
	if (error.size()) fprintf_color(COLOR_RED, stderr, "%s\n", error.c_str());

	switch (result.GetCompilationStatus()) {
	case shaderc_compilation_status_success:
		// store SPIRV module
		destShader.mModules.push_back({});
		SpirvModule& m = destShader.mModules.back();
		for (auto d = result.cbegin(); d != result.cend(); d++)
			m.mSpirv.push_back(*d);

		// assign SPIRV module
		VkShaderStageFlagBits vkstage;
		switch (stage) {
		case shaderc_vertex_shader:
			dest.mModules[0] = (uint32_t)destShader.mModules.size() - 1;
			vkstage = VK_SHADER_STAGE_VERTEX_BIT;
			break;
		case shaderc_fragment_shader:
			dest.mModules[1] = (uint32_t)destShader.mModules.size() - 1;
			vkstage = VK_SHADER_STAGE_FRAGMENT_BIT;
			break;
		case shaderc_compute_shader:
			dest.mModules[0] = (uint32_t)destShader.mModules.size() - 1;
			vkstage = VK_SHADER_STAGE_COMPUTE_BIT;
			break;
		}

		spirv_cross::Compiler comp(m.mSpirv.data(), m.mSpirv.size());
		spirv_cross::ShaderResources res = comp.get_shader_resources();
		
		#pragma region register resource bindings
		auto registerResource = [&](const spirv_cross::Resource& res, VkDescriptorType type) {
			auto& binding = dest.mDescriptorBindings[res.name];

			binding.first = comp.get_decoration(res.id, spv::DecorationDescriptorSet);

			binding.second.stageFlags |= vkstage;
			binding.second.binding = comp.get_decoration(res.id, spv::DecorationBinding);
			binding.second.descriptorCount = 1;
			binding.second.descriptorType = type;
		};

		for (const auto& r : res.sampled_images)
			registerResource(r, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		for (const auto& r : res.separate_images)
			if (comp.get_type(r.type_id).image.dim == spv::DimBuffer)
				registerResource(r, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER);
			else
				registerResource(r, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
		for (const auto& r : res.storage_images)
			if (comp.get_type(r.type_id).image.dim == spv::DimBuffer)
				registerResource(r, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER);
			else
				registerResource(r, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		for (const auto& r : res.storage_buffers)
			registerResource(r, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		for (const auto& r : res.separate_samplers)
			registerResource(r, VK_DESCRIPTOR_TYPE_SAMPLER);
		for (const auto& r : res.uniform_buffers)
			registerResource(r, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

		for (const auto& r : res.push_constant_buffers) {
			uint32_t index = 0;

			const auto& type = comp.get_type(r.base_type_id);

			if (type.basetype == spirv_cross::SPIRType::Struct) {
				for (uint32_t i = 0; i < type.member_types.size(); i++) {
					const auto& mtype = comp.get_type(type.member_types[i]);

					const string name = comp.get_member_name(r.base_type_id, index);
					
					VkPushConstantRange range = {};
					range.stageFlags = vkstage == VK_SHADER_STAGE_COMPUTE_BIT ? VK_SHADER_STAGE_COMPUTE_BIT : (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
					range.offset = comp.type_struct_member_offset(type, index);

					switch (mtype.basetype) {
					case spirv_cross::SPIRType::Boolean:
					case spirv_cross::SPIRType::SByte:
					case spirv_cross::SPIRType::UByte:
						range.size = 1;
						break;
					case spirv_cross::SPIRType::Short:
					case spirv_cross::SPIRType::UShort:
					case spirv_cross::SPIRType::Half:
						range.size = 2;
						break;
					case spirv_cross::SPIRType::Int:
					case spirv_cross::SPIRType::UInt:
					case spirv_cross::SPIRType::Float:
						range.size = 4;
						break;
					case spirv_cross::SPIRType::Int64:
					case spirv_cross::SPIRType::UInt64:
					case spirv_cross::SPIRType::Double:
						range.size = 8;
						break;
					case spirv_cross::SPIRType::Struct:
						range.size = (uint32_t)comp.get_declared_struct_size(mtype);
						break;
					case spirv_cross::SPIRType::Unknown:
					case spirv_cross::SPIRType::Void:
					case spirv_cross::SPIRType::AtomicCounter:
					case spirv_cross::SPIRType::Image:
					case spirv_cross::SPIRType::SampledImage:
					case spirv_cross::SPIRType::Sampler:
					case spirv_cross::SPIRType::AccelerationStructureNV:
						fprintf(stderr, "Unknown type for push constant: %s\n", name.c_str());
						range.size = 0;
						break;
					}

					range.size *= mtype.columns * mtype.vecsize;

					vector<pair<string, VkPushConstantRange>> ranges;
					ranges.push_back(make_pair(name, range));

					for (uint32_t dim : mtype.array) {
						for (auto& r : ranges)
							r.second.size *= dim;
						// TODO: support individual element ranges
						//uint32_t sz = ranges.size();
						//for (uint32_t j = 0; j < sz; j++)
						//	for (uint32_t c = 0; c < dim; c++)
						//		ranges.push_back(make_pair(ranges[j].first + "[" + to_string(c) + "]", range));
					}

					for (auto& r : ranges)
						dest.mPushConstants[r.first] = r.second;

					index++;
				}
			} else
				fprintf(stderr, "Push constant data is not a struct! Reflection will not work.\n");
		}
		#pragma endregion
		
		if (vkstage == VK_SHADER_STAGE_COMPUTE_BIT) {
			auto entryPoints = comp.get_entry_points_and_stages();
			for (const auto& e : entryPoints) {
				if (e.name == entryPoint) {
					auto& ep = comp.get_entry_point(e.name, e.execution_model);
					dest.mWorkgroupSize[0] = ep.workgroup_size.x;
					dest.mWorkgroupSize[1] = ep.workgroup_size.y;
					dest.mWorkgroupSize[2] = ep.workgroup_size.z;
				}
			}
		} else
			dest.mWorkgroupSize = 0;

		return true;
	}
	return false;
}

CompiledShader* Compile(shaderc::Compiler* compiler, const string& filename) {
	string source;
	if (!ReadFile(filename, source)) {
		fprintf_color(COLOR_RED, stderr, "Failed to read %s!\n", filename.c_str());
		return nullptr;
	}

	unordered_map<PassType, pair<string, string>> passes; // pass -> (vertex, fragment)
	vector<string> kernels;

	std::vector<std::pair<std::string, uint32_t>> arrays; // name, size
	std::vector<std::pair<std::string, VkSamplerCreateInfo>> staticSamplers;

	CompiledShader* result = new CompiledShader();
	result->mRenderQueue = 1000;
	result->mColorMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	result->mCullMode = VK_CULL_MODE_BACK_BIT;
	result->mFillMode = VK_POLYGON_MODE_FILL;
	result->mBlendMode = BLEND_MODE_OPAQUE;
	result->mDepthStencilState = {};
	result->mDepthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	result->mDepthStencilState.depthTestEnable = VK_TRUE;
	result->mDepthStencilState.depthWriteEnable = VK_TRUE;
	result->mDepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	result->mDepthStencilState.front = result->mDepthStencilState.back;
	result->mDepthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;

	vector<set<string>> variants{ {} };

	istringstream srcstream(source);

	string line;
	while (getline(srcstream, line)) {
		istringstream linestream(line);
		vector<string> words{ istream_iterator<string>{linestream}, istream_iterator<string>{} };
		
		size_t kwc = variants.size();

		for (auto it = words.begin(); it != words.end(); ++it) {
			if (*it == "#pragma") {
				if (++it == words.end()) break;
				if (*it == "multi_compile") {
					if (++it == words.end()) break;
					// iterate all the keywords added by this multi_compile
					while (it != words.end()) {
						// duplicate all existing variants, add this keyword to each
						for (uint32_t i = 0; i < kwc; i++) {
							variants.push_back(variants[i]);
							variants.back().insert(*it);
						}
						++it;
					}
				
				} else if (*it == "vertex") {
					if (++it == words.end()) return nullptr;
					string ep = *it;
					PassType pass = PASS_MAIN;
					if (++it != words.end()) pass = atopass(*it);
					if (passes.count(pass) && passes[pass].first != "") {
						fprintf_color(COLOR_RED, stderr, "Duplicate vertex shader for pass %s!\n", it->c_str());
						return nullptr;
					}
					passes[pass].first = ep;

				} else if (*it == "fragment") {
					if (++it == words.end()) return nullptr;
					string ep = *it;
					PassType pass = PASS_MAIN;
					if (++it != words.end()) pass = atopass(*it);
					if (passes.count(pass) && passes[pass].second != "") {
						fprintf_color(COLOR_RED, stderr, "Duplicate vertex shader for pass %s!\n", it->c_str());
						return nullptr;
					}
					passes[pass].second = ep;

				} else if (*it == "kernel") {
					if (++it == words.end()) return nullptr;
					kernels.push_back(*it);

				} else if (*it == "render_queue"){
					if (++it == words.end()) return nullptr;
					result->mRenderQueue = atoi(it->c_str());

				} else if (*it == "color_mask") {
					if (++it == words.end()) return nullptr;
					result->mColorMask = atomask(*it);

				} else if (*it == "zwrite") {
					if (++it == words.end()) return nullptr;
					if (*it == "true") result->mDepthStencilState.depthWriteEnable = VK_TRUE;
					else if (*it == "false") result->mDepthStencilState.depthWriteEnable = VK_FALSE;
					else {
						fprintf_color(COLOR_RED, stderr, "zwrite must be true or false.\n");
						return nullptr;
					}

				} else if (*it == "ztest") {
					if (++it == words.end()) return nullptr;
					if (*it == "true")
						result->mDepthStencilState.depthTestEnable = VK_TRUE;
					else if (*it == "false")
						result->mDepthStencilState.depthTestEnable = VK_FALSE;
					else{
						fprintf_color(COLOR_RED, stderr, "ztest must be true or false.\n");
						return nullptr;
					}

				} else if (*it == "depth_op") {
					if (++it == words.end()) return nullptr;
					result->mDepthStencilState.depthCompareOp = atocmp(*it);

				} else if (*it == "cull") {
					if (++it == words.end()) return nullptr;
					if (*it == "front") result->mCullMode = VK_CULL_MODE_FRONT_BIT;
					else if (*it == "back") result->mCullMode = VK_CULL_MODE_BACK_BIT;
					else if (*it == "false") result->mCullMode = VK_CULL_MODE_NONE;
					else {
						fprintf_color(COLOR_RED, stderr, "Unknown cull mode: %s\n", it->c_str());
						return nullptr;
					}

				} else if (*it == "fill") {
					if (++it == words.end()) return nullptr;
					if (*it == "solid") result->mFillMode = VK_POLYGON_MODE_FILL;
					else if (*it == "line") result->mFillMode = VK_POLYGON_MODE_LINE;
					else if (*it == "point") result->mFillMode = VK_POLYGON_MODE_POINT;
					else {
						fprintf_color(COLOR_RED, stderr, "Unknown fill mode: %s\n", it->c_str());
						return nullptr;
					}

				} else if (*it == "blend") {
					if (++it == words.end()) return nullptr;
					if (*it == "opaque")		result->mBlendMode = BLEND_MODE_OPAQUE;
					else if (*it == "alpha")	result->mBlendMode = BLEND_MODE_ALPHA;
					else if (*it == "add")		result->mBlendMode = BLEND_MODE_ADDITIVE;
					else if (*it == "multiply")	result->mBlendMode = BLEND_MODE_MULTIPLY;
					else {
						fprintf_color(COLOR_RED, stderr, "Unknown blend mode: %s\n", it->c_str());
						return nullptr;
					}

				} else if (*it == "static_sampler") {
					if (++it == words.end()) return nullptr;
					string name = *it;

					VkSamplerCreateInfo samplerInfo = {};
					samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
					samplerInfo.magFilter = VK_FILTER_LINEAR;
					samplerInfo.minFilter = VK_FILTER_LINEAR;
					samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
					samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
					samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
					samplerInfo.anisotropyEnable = VK_TRUE;
					samplerInfo.maxAnisotropy = 2;
					samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
					samplerInfo.unnormalizedCoordinates = VK_FALSE;
					samplerInfo.compareEnable = VK_FALSE;
					samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
					samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
					samplerInfo.minLod = 0;
					samplerInfo.maxLod = 12;
					samplerInfo.mipLodBias = 0;

					while (++it != words.end()) {
						size_t eq = it->find('=');
						if (eq == string::npos) continue;
						string id = it->substr(0, eq);
						string val = it->substr(eq + 1);
						if (id == "magFilter")		    samplerInfo.magFilter = atofilter(val);
						else if (id == "minFilter")		samplerInfo.minFilter = atofilter(val);
						else if (id == "filter")		samplerInfo.minFilter = samplerInfo.magFilter = atofilter(val);
						else if (id == "addressModeU")	samplerInfo.addressModeU = atoaddressmode(val);
						else if (id == "addressModeV")	samplerInfo.addressModeV = atoaddressmode(val);
						else if (id == "addressModeW")	samplerInfo.addressModeW = atoaddressmode(val);
						else if (id == "addressMode")	samplerInfo.addressModeU = samplerInfo.addressModeV = samplerInfo.addressModeW = atoaddressmode(val);
						else if (id == "maxAnisotropy") {
							float aniso = (float)atof(val.c_str());
							samplerInfo.anisotropyEnable = aniso <= 0 ? VK_FALSE : VK_TRUE;
							samplerInfo.maxAnisotropy = aniso;
						} else if (id == "borderColor")				samplerInfo.borderColor = atobordercolor(val);
						else if (id == "unnormalizedCoordinates")	samplerInfo.unnormalizedCoordinates = val == "true" ? VK_TRUE : VK_FALSE;
						else if (id == "compareOp") {
							VkCompareOp cmp = atocmp(val);
							samplerInfo.compareEnable = cmp == VK_COMPARE_OP_MAX_ENUM ? VK_FALSE : VK_TRUE;
							samplerInfo.compareOp = cmp;
						} else if (id == "mipmapMode") samplerInfo.mipmapMode = atomipmapmode(val);
						else if (id == "minLod") samplerInfo.minLod = (float)atof(val.c_str());
						else if (id == "maxLod") samplerInfo.maxLod = (float)atof(val.c_str());
						else if (id == "mipLodBias") samplerInfo.mipLodBias = (float)atof(val.c_str());
					}

					staticSamplers.push_back(make_pair(name, samplerInfo));

				} else if (*it == "array") {
					if (++it == words.end()) return nullptr;
					string name = *it;
					if (++it == words.end()) return nullptr;
					arrays.push_back(make_pair(name, (uint32_t)atoi(it->c_str())));
				}
				break;
			}
		}
	}

	for (const auto& variant : variants) {
		auto variantOptions = options;

		vector<string> keywords;
		for (const auto& kw : variant) {
			if (kw.empty()) continue;
			keywords.push_back(kw);
			variantOptions.AddMacroDefinition(kw);
		}

		/// applies array and static_sampler pragmas
		auto UpdateBindings = [&](CompiledVariant& input) {
			for (auto& b : input.mDescriptorBindings) {
				for (const auto& s : staticSamplers)
					if (s.first == b.first) {
						input.mStaticSamplers.emplace(s.first, s.second);
						break;
					}
				for (const auto& s : arrays)
					if (s.first == b.first) {
						b.second.second.descriptorCount = s.second;
						break;
					}
			}
		};

		if (kernels.size()) {
			auto stageOptions = variantOptions;
			stageOptions.AddMacroDefinition("SHADER_STAGE_COMPUTE");
			for (const auto& k : kernels) {
				// compile all kernels for this variant
				CompiledVariant v = {};
				v.mPass = (PassType)0;
				v.mKeywords = keywords;
				v.mEntryPoints[0] = k;
				if (!CompileStage(compiler, stageOptions, source, filename, shaderc_compute_shader, k, v, *result)) return nullptr;
				UpdateBindings(v);
				result->mVariants.push_back(v);
			}
		} else {
			unordered_map<string, uint32_t> compiled;

			for (auto& stagep : passes) {
				auto vsOptions = variantOptions;
				auto fsOptions = variantOptions;
				vsOptions.AddMacroDefinition("SHADER_STAGE_VERTEX");
				fsOptions.AddMacroDefinition("SHADER_STAGE_FRAGMENT");
				// compile all passes for this variant
				switch (stagep.first) {
				case PASS_MAIN:
					vsOptions.AddMacroDefinition("PASS_MAIN");
					fsOptions.AddMacroDefinition("PASS_MAIN");
					break;
				case PASS_DEPTH:
					vsOptions.AddMacroDefinition("PASS_DEPTH");
					fsOptions.AddMacroDefinition("PASS_DEPTH");
					break;
				}

				string vs = stagep.second.first;
				string fs = stagep.second.second;

				if (vs == "" && passes.count(PASS_MAIN)) vs = passes.at(PASS_MAIN).first;
				if (vs == "") {
					fprintf_color(COLOR_RED, stderr, "No vertex shader entry point found for fragment shader entry point: \n", fs.c_str());
					return nullptr;
				}
				if (fs == "" && passes.count(PASS_MAIN)) fs = passes.at(PASS_MAIN).second;
				if (fs == "") {
					fprintf_color(COLOR_RED, stderr, "No fragment shader entry point found for vertex shader entry point: \n", vs.c_str());
					return nullptr;
				}

				CompiledVariant v = {};
				v.mKeywords = keywords;
				v.mPass = stagep.first;
				v.mEntryPoints[0] = vs;
				v.mEntryPoints[1] = fs;

				// Don't recompile shared stages
				//if (compiled.count(vs)) {
				//	v.mModules[0] = compiled.at(vs);
				//} else {
					if (!CompileStage(compiler, vsOptions, source, filename, shaderc_vertex_shader, vs, v, *result)) return nullptr;
				//	compiled.emplace(vs, v.mModules[0]);
				//}
				
				//if (compiled.count(fs)) {
				//	v.mModules[1] = compiled.at(fs);
				//} else {
					if (!CompileStage(compiler, fsOptions, source, filename, shaderc_fragment_shader, fs, v, *result)) return nullptr;
				//	compiled.emplace(fs, v.mModules[1]);
				//}

				UpdateBindings(v);

				result->mVariants.push_back(v);
			}
		}
	}
	return result;
}

int main(int argc, char* argv[]) {
	const char* inputFile;
	const char* outputFile;
	const char* include;

	if (argc < 3) {
		fprintf(stderr, "Usage: %s <input> <output> <global include path>\n", argv[0]);
		return EXIT_FAILURE;
	} else {
		inputFile = argv[1];
		outputFile = argv[2];
		include = argv[3];
	}

	printf("Compiling %s\n", inputFile);
	
	if (fs::path(inputFile).extension().string() == ".hlsl")
		options.SetSourceLanguage(shaderc_source_language_hlsl);
	else
		options.SetSourceLanguage(shaderc_source_language_glsl);
	
	options.SetIncluder(make_unique<Includer>(include));
	options.SetOptimizationLevel(shaderc_optimization_level_zero);
	options.SetAutoBindUniforms(false);
	options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_1);

	Compiler* compiler = new Compiler();
	CompiledShader* shader = Compile(compiler, inputFile);
	
	if (!shader) return EXIT_FAILURE;

	// write shader
	ofstream output(outputFile, ios::binary);
	shader->Write(output);
	output.close();

	delete shader;

	delete compiler;

	return EXIT_SUCCESS;
}