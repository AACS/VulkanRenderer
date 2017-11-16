#pragma once


#include <stdlib.h>  
#include <crtdbg.h>  

#include "..\core\TimeManager.h"
#include "..\core\MemoryPool.h"

#include "..\vulkan\VulkanRenderer.hpp"

#include "Camera.h"
#include "Terrain.h"
#include "Water.h"

class TerrainManager
{
public:
	TerrainManager();
	~TerrainManager();

	void GenerateTerrain(std::shared_ptr<VulkanRenderer> renderer, VulkanBuffer globalVariableBuffer,
		VulkanBuffer lightsInfoBuffer, std::shared_ptr<Camera> camera);

	void ReInitTerrain(std::shared_ptr<VulkanRenderer> renderer);

	void UpdateTerrains(std::shared_ptr<VulkanRenderer> renderer, VulkanBuffer globalVariableBuffer,
		VulkanBuffer lightsInfoBuffer, std::shared_ptr<Camera> camera, std::shared_ptr<TimeManager> timeManager);

	void RenderTerrain(VkCommandBuffer commandBuffer, bool wireframe);

	void UpdateTerrainGUI();

	void CleanUpTerrain();

	VkCommandBuffer CreateTerrainMeshUpdateCommandBuffer(VkCommandPool commandPool, VkCommandBufferLevel level);
	void FlushTerrainMeshUpdateCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free);

private:
	std::shared_ptr<VulkanRenderer> renderer;

	std::vector<std::shared_ptr<Terrain>> terrains;
	std::shared_ptr<MemoryPool<TerrainQuadData, 2 * sizeof(TerrainQuadData)>> terrainQuadPool;

	std::vector<std::shared_ptr<Water>> waters;

	bool show_terrain_manager_window = true;
	bool recreateTerrain = false;
	float terrainWidth = 1000;
	float terrainHeightScale = 100.0f;
	int terrainMaxLevels = 1;
	int terrainGridDimentions = 1;
	int sourceImageResolution = 256;
	SimpleTimer terrainUpdateTimer;

	int maxNumQuads = 1; //maximum quads managed by this
};

