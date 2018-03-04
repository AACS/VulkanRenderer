#pragma once

#include <thread>
#include <vector>
#include <memory>
  
  
#include "../core/CoreTools.h"
#include "../core/TimeManager.h"
#include "../core/MemoryPool.h"

#include "../rendering/VulkanRenderer.hpp"
#include "../resources/ResourceManager.h"

#include "../gui/InternalGraph.h"

#include "Camera.h"
#include "Terrain.h"
#include "Water.h"

#include "InstancedSceneObject.h"


class TerrainChunk {
public:
	glm::vec2 position;
	glm::vec2 size;
	glm::i32vec2 noisePosition;
	glm::i32vec2 noiseSize;
	int sourceImageResolution;

	std::shared_ptr<Terrain> terrain;

	TerrainChunk();

	void UpdateChunk(std::shared_ptr<ResourceManager> resourceMan, std::shared_ptr<VulkanRenderer> renderer, std::shared_ptr<Camera> camera, std::shared_ptr<TimeManager> timeManager);
	
	void RenderChunk(VkCommandBuffer commandBuffer, bool wireframe);
};

class TerrainManager
{
public:
	TerrainManager(InternalGraph::GraphPrototype& protoGraph);
	~TerrainManager();

	void SetupResources(std::shared_ptr<ResourceManager> resourceMan, std::shared_ptr<VulkanRenderer> renderer);
	void CleanUpResources();

	void GenerateTerrain(std::shared_ptr<ResourceManager> resourceMan, std::shared_ptr<VulkanRenderer> renderer, std::shared_ptr<Camera> camera);

	void UpdateTerrains(std::shared_ptr<ResourceManager> resourceMan, std::shared_ptr<VulkanRenderer> renderer, std::shared_ptr<Camera> camera, std::shared_ptr<TimeManager> timeManager);

	void RenderTerrain(VkCommandBuffer commandBuffer, bool wireframe);

	void UpdateTerrainGUI();

	void CleanUpTerrain();

	float GetTerrainHeightAtLocation(float x, float z);

	VkCommandBuffer CreateTerrainMeshUpdateCommandBuffer(VkCommandPool commandPool, VkCommandBufferLevel level);
	void FlushTerrainMeshUpdateCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free);

	void RecreateTerrain();

private:
	InternalGraph::GraphPrototype& protoGraph;
	//NewNodeGraph::TerGenNodeGraph& nodeGraph;

	std::shared_ptr<VulkanRenderer> renderer;

	std::vector<std::shared_ptr<Terrain>> terrains;
	std::shared_ptr<MemoryPool<TerrainQuadData>> terrainQuadPool;

	std::unique_ptr<InstancedSceneObject> instancedWaters;

	std::vector<std::shared_ptr<Water>> waters;

	std::shared_ptr<Mesh> WaterMesh;
	VulkanModel WaterModel;

	std::shared_ptr<TextureArray> terrainTextureArray;
	VulkanTexture2DArray terrainVulkanTextureArray;

	std::shared_ptr<Texture> WaterTexture;
	VulkanTexture2D WaterVulkanTexture;
	
	bool show_terrain_manager_window = true;
	bool recreateTerrain = false;
	float terrainWidth = 1000;
	float terrainHeightScale = 100.0f;
	int terrainMaxLevels = 4;
	int terrainGridDimentions = 1;
	int sourceImageResolution = 256;
	SimpleTimer terrainUpdateTimer;
	int numCells = 64;

	int maxNumQuads = 1; //maximum quads managed by this

	std::vector<std::string> terrainTextureFileNames = {
		"dirt.jpg",
		"grass.jpg",
		"rockSmall.jpg",
		"Snow.jpg",
	};
	// Extra splatmap images, starting with just 4 for now
	//	"OakTreeLeaf.png",
	//	"Sand.png",
	//	"DeadOakTreeTrunk.png",
	//	"OakTreeTrunk.png",
	//	"SpruceTreeTrunk.png"};
	//
};

