#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "gl_shadergen.h"

namespace gpu {

// Generates Vulkan 1.0 GLSL (version 450) with standard Vulkan descriptor bindings (SSBOs, UBO, samplers)
std::string genVulkanVertexShader(const ShaderUid& uid);
std::string genVulkanFragmentShader(const ShaderUid& uid);

// Compiles Vulkan GLSL source to SPIR-V bytecode targeting Vulkan 1.0 using shaderc
bool compileShaderToSPIRV(const std::string& src, bool isVertex, std::vector<uint32_t>& outSpirv, std::string& outError);

// High-level: generates and compiles both vertex and fragment shaders for a ShaderUid
bool compileVulkanShader(const ShaderUid& uid, std::vector<uint32_t>& outVsSpirv, std::vector<uint32_t>& outFsSpirv);

}  // namespace gpu
