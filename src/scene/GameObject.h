#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include <glm/common.hpp>
#include <vulkan/vulkan.h>




#include "../rendering/Model.h"
#include "../rendering/Renderer.h"
#include "../rendering/Texture.h"
#include "../rendering/Descriptor.h"

#include "../resources/Mesh.h"
#include "../resources/Texture.h"

class GameObject
{
  public:
    GameObject();
    ~GameObject();

    void InitGameObject(VulkanRenderer* renderer);
    void CleanUp();

    void LoadModel(std::string fileName);
    void LoadModel(std::shared_ptr<Mesh> mesh);

	void SetupUniformBuffer();
    void SetupImage();
    void SetupModel();
    void SetupPipeline();
    void SetupDescriptor();


    void UpdateUniformBuffer(float time);

	void Draw(VkCommandBuffer commandBuffer, bool wireframe, bool drawNormals);

	VulkanRenderer* renderer;

	std::shared_ptr<ManagedVulkanPipeline> mvp;

	std::shared_ptr<VulkanDescriptor> descriptor;
	DescriptorSet m_descriptorSet;

	std::shared_ptr<VulkanDescriptor> materialDescriptor;
	DescriptorSet material_descriptorSet;

	std::shared_ptr<Mesh> gameObjectMesh;
	std::shared_ptr<VulkanModel> gameObjectModel;

	std::shared_ptr<Texture> gameObjectTexture;
	std::shared_ptr<VulkanTexture2D> gameObjectVulkanTexture;

	std::shared_ptr<VulkanBufferUniform> uniformBuffer;
	//ModelPushConstant modelPushConstant;

	bool usePBR;
	bool usePBR_Tex;

	Phong_Material phong_mat;
	PBR_Material pbr_mat;
	PBR_Mat_Tex pbr_mat_tex;
	
	std::shared_ptr<VulkanBufferUniform> materialBuffer;

	glm::vec3 position;

	bool isReadyToRender = false;
};
