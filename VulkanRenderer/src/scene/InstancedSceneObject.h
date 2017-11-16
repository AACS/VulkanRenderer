#pragma once

#include <vector>

#include <vulkan\vulkan.h>
#include <glm\common.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "..\vulkan\VulkanRenderer.hpp"
#include "..\vulkan\VulkanModel.hpp"
#include "..\vulkan\VulkanTexture.hpp"

#include "..\core\Mesh.h"
#include "..\core\Texture.h"


class InstancedSceneObject
{
public:

	// Per-instance data block
	struct InstanceData {
		glm::vec3 pos;
		glm::vec3 rot;
		float scale;
	};
	// Contains the instanced data
	struct InstanceBuffer {
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		size_t size = 0;
		VkDescriptorBufferInfo descriptor;
	} instanceBuffer;



	InstancedSceneObject();
	~InstancedSceneObject();

	void InitInstancedSceneObject(std::shared_ptr<VulkanRenderer> renderer, VulkanBuffer &global, VulkanBuffer &lighting);
	void ReinitInstancedSceneObject(std::shared_ptr<VulkanRenderer> renderer);
	void CleanUp();
	void UpdateUniformBuffer();

	void LoadModel(std::string fileName);
	void LoadModel(std::shared_ptr<Mesh> mesh);
	void LoadTexture(std::string filename);

	void SetupUniformBuffer();
	void SetupImage();
	void SetupModel();

	void SetupDescriptor(VulkanBuffer &global, VulkanBuffer &lighting);
	void SetupPipeline();

	void InstancedSceneObject::AddInstances(std::vector<glm::vec3> positions);
	//void InstancedSceneObject::RemoveInstance(std::vector<glm::vec3> positions);

	void WriteToCommandBuffer(VkCommandBuffer commandBuffer, bool wireframe);

	std::shared_ptr<VulkanRenderer> renderer;

	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	VkPipeline wireframe;
	VkPipeline debugNormals;

	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorPool descriptorPool;
	VkDescriptorSet descriptorSet;

	std::shared_ptr<Mesh> mesh;
	VulkanModel vulkanModel;

	std::shared_ptr<Texture> texture;
	VulkanTexture2D vulkanTexture;

	std::vector<InstanceData> instancesData;
	std::vector<glm::vec3> modelPositions;
	std::vector<ModelBufferObject> modelUniforms;
	VulkanBuffer uniformBuffer;
	int instanceCount;
};
