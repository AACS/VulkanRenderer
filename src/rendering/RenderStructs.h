#pragma once

#include <vector>
#include <memory>

#include <glm/glm.hpp>

#include "../../third-party/VulkanMemoryAllocator/vk_mem_alloc.h"

class VulkanTexture;

/* Synchronization */

using Signal = std::shared_ptr<bool>;

/* Common uniform buffers */

struct GlobalData {
	float time;
};

struct CameraData {
	glm::mat4 projView;
	glm::mat4 view;
	glm::vec3 cameraDir;
	float dummy;
	glm::vec3 cameraPos;
} ;

/* Lighting */

struct DirectionalLight {
	glm::vec3 direction;
	float intensity;
	glm::vec3 color;
	float dum;
};

struct PointLight {
	glm::vec3 position;
	float attenuation;
	glm::vec3 color;
};

struct  SpotLight {
	glm::vec3 position;
	float attenuation;
	glm::vec3 color;
	float cutoff;
	float outerCutOff;
	float padding;
};

/* Model and Normal matrices */

struct ModelBufferObject {
	glm::mat4 model;
	glm::mat4 normal;

	glm::mat4 paddingOne;
	glm::mat4 paddingTwo;
};

struct ModelPushConstant {
	glm::mat4 model;
	glm::mat4 normal;
};

struct StaticModelBuffer {
	glm::mat4 model;
};

struct StaticModelPushConstant {
	glm::mat4 model;
};

/* Materials */

struct Phong_Material {
	glm::vec4 color = glm::vec4(0.5, 0.5, 0.5, 1.0);
	float diffuse = 0.8;
	float specular = 0.2;
	float reflectivity = 4;
	float padding = 0;
};

struct PBR_Material {
	glm::vec3 albedo = glm::vec3(0.5,0.5,0.5);
	float metallic = 0.1;
	float roughness = 0.5;
	float ao = 1;
	glm::vec3 emmisive = glm::vec3(0.0, 0.0, 0.0);

	bool useTexAlbedo = false;
	bool useTexMetallic = false;
	bool useTexRoughness = false;
	bool useTexAmbientOcclusion = false;
	bool useTexEmmisive = false;
	bool useTexNormal = false;

	std::shared_ptr<VulkanTexture> tx_albedo;
	std::shared_ptr<VulkanTexture> tx_metallic;
	std::shared_ptr<VulkanTexture> tx_roughness;
	std::shared_ptr<VulkanTexture> tx_ao;
	std::shared_ptr<VulkanTexture> tx_emissiveTexture;
	std::shared_ptr<VulkanTexture> tx_normal;
};

const glm::mat4 depthReverserMatrix = glm::mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0, 1, 1);