#include "Terrain.h"
#include <glm/gtc/matrix_transform.hpp>

#include "../core/Logger.h"

#include "../rendering/RenderStructs.h"

#include "TerrainManager.h"

TerrainQuad::TerrainQuad(TerrainChunkBuffer& chunkBuffer, int index) : 
	state(ChunkState::free), chunkBuffer(chunkBuffer), index(index)
{

}

void TerrainQuad::Setup(glm::vec2 pos, glm::vec2 size, 
	glm::i32vec2 logicalPos, glm::i32vec2 logicalSize,
	int level, glm::i32vec2 subDivPos, float centerHeightValue,
	Terrain* terrain)

	// pos(pos), size(size),
	// logicalPos(logicalPos), logicalSize(logicalSize),
	// subDivPos(subDivPos), isSubdivided(false),
	// level(level), heightValAtCenter(0),
	// chunkBuffer(chunkBuffer), index(index),
	// vertices(chunkBuffer.GetDeviceVertexBufferPtr(index)),
	// indices(chunkBuffer.GetDeviceIndexBufferPtr(index))
{
	this->state = ChunkState::waiting_create;
	this->pos = pos; 
	this->size = size;
	this->logicalPos = logicalPos; 
	this->logicalSize = logicalSize;
	this->subDivPos = subDivPos; 
	this->isSubdivided = false;
	this->level = level; 
	this->heightValAtCenter = 0;
	this->terrain = terrain;
	this->vertices = chunkBuffer.GetDeviceVertexBufferPtr(index);
	this->indices = chunkBuffer.GetDeviceIndexBufferPtr(index);


	//isReady = std::make_shared<bool>(false);
}

//Manual reset... (shouldtn' be needed but hey, better safe than sorry)
void TerrainQuad::CleanUp(){
	state = ChunkState::free;
	isSubdivided = false;
	subQuads.UpLeft = -1;
	subQuads.DownLeft = -1;
	subQuads.UpRight = -1;
	subQuads.DownRight = -1;
}

void TerrainQuad::RecursiveCleanUp(){
	chunkBuffer.GetChunk(subQuads.UpLeft)->RecursiveCleanUp();
	chunkBuffer.GetChunk(subQuads.DownLeft)->RecursiveCleanUp();
	chunkBuffer.GetChunk(subQuads.UpRight)->RecursiveCleanUp();
	chunkBuffer.GetChunk(subQuads.DownRight)->RecursiveCleanUp();
	CleanUp();
}

float TerrainQuad::GetUVvalueFromLocalIndex(float i, int numCells, int level, int subDivPos) {
	return glm::clamp((float)(i) / ((1 << level) * (float)(numCells)) + (float)(subDivPos) / (float)(1 << level), 0.0f, 1.0f);
}


void TerrainQuad::GenerateTerrainChunk(InternalGraph::GraphUser& graphUser,	float heightScale, float widthScale) 
{

	const int numCells = NumCells;

	float uvUs[numCells + 3];
	float uvVs[numCells + 3];

	int powLevel = 1 << (level);
	for (int i = 0; i < numCells + 3; i++)
	{
		uvUs[i] = glm::clamp((float)(i - 1) / ((float)(powLevel) * (numCells)) + (float)subDivPos.x / (float)(powLevel), 0.0f, 1.0f);
		uvVs[i] = glm::clamp((float)(i - 1) / ((float)(powLevel) * (numCells)) + (float)subDivPos.y / (float)(powLevel), 0.0f, 1.0f);
	}

	for (int i = 0; i < numCells + 1; i++)
	{
		for (int j = 0; j < numCells + 1; j++)
		{
			float uvU = uvUs[(i + 1)];
			float uvV = uvVs[(j + 1)];

			float uvUminus = uvUs[(i + 1) - 1];
			float uvUplus = uvUs[(i + 1) + 1];
			float uvVminus = uvVs[(j + 1) - 1];
			float uvVplus = uvVs[(j + 1) + 1];

			float outheight = graphUser.SampleHeightMap(uvU, uvV);
			float outheightum = graphUser.SampleHeightMap(uvUminus, uvV);
			float outheightup = graphUser.SampleHeightMap(uvUplus, uvV);
			float outheightvm = graphUser.SampleHeightMap(uvU, uvVminus);
			float outheightvp = graphUser.SampleHeightMap(uvU, uvVplus);

			glm::vec3 normal = glm::normalize(glm::vec3((outheightum - outheightup),
				((uvUplus - uvUminus) + (uvVplus - uvVminus)) * 2,
				(outheightvm - outheightvp)));

			(*vertices)[((i)*(numCells + 1) + j)* vertElementCount + 0] = uvU * (widthScale);
			(*vertices)[((i)*(numCells + 1) + j)* vertElementCount + 1] = outheight * heightScale;
			(*vertices)[((i)*(numCells + 1) + j)* vertElementCount + 2] = uvV * (widthScale);
			(*vertices)[((i)*(numCells + 1) + j)* vertElementCount + 3] = normal.x;
			(*vertices)[((i)*(numCells + 1) + j)* vertElementCount + 4] = normal.y;
			(*vertices)[((i)*(numCells + 1) + j)* vertElementCount + 5] = normal.z;
			(*vertices)[((i)*(numCells + 1) + j)* vertElementCount + 6] = uvU;
			(*vertices)[((i)*(numCells + 1) + j)* vertElementCount + 7] = uvV;
		}
	}

	int counter = 0;
	for (int i = 0; i < numCells; i++)
	{
		for (int j = 0; j < numCells; j++)
		{
			(*indices)[counter++] = i * (numCells + 1) + j;
			(*indices)[counter++] = i * (numCells + 1) + j + 1;
			(*indices)[counter++] = (i + 1) * (numCells + 1) + j;
			(*indices)[counter++] = i * (numCells + 1) + j + 1;
			(*indices)[counter++] = (i + 1) * (numCells + 1) + j + 1;
			(*indices)[counter++] = (i + 1) * (numCells + 1) + j;
		}
	}
}


void TerrainQuad::Draw(VkCommandBuffer cmdBuf) {

	std::vector<VkDeviceSize> vertexOffsettings(count);
	std::vector<VkDeviceSize> indexOffsettings(count);

	int chunksToDraw = 0;
	for (int i = 0; i < count; i++) {
		if(chunks[i].state == ChunkState::ready){
			chunksToDraw++;
			vertexOffsettings[i] = (i * sizeof(TerrainMeshVertices));
			indexOffsettings[i] = (i * sizeof(TerrainMeshIndices));
		}
	}

	vkCmdBindPipeline(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, ifWireframe ? mvp->pipelines->at(1) : mvp->pipelines->at(0));
	vkCmdBindDescriptorSets(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, mvp->layout, 2, 1, &descriptorSet.set, 0, nullptr);
	
	for (int i = 0; i < chunksToDraw; i++) {
		vkCmdBindVertexBuffers(cmdBuff, 0, 1, &vert_buffer->buffer.buffer, &vertexOffsettings[i]);
		vkCmdBindIndexBuffer(cmdBuff, index_buffer->buffer.buffer, indexOffsettings[i], VK_INDEX_TYPE_UINT32);

		vkCmdDrawIndexed(cmdBuff, static_cast<uint32_t>(indCount), 1, 0, 0, 0);
	}
	//vkCmdBindVertexBuffers(cmdBuff, 0, 1, &(quadHandles[i]->deviceVertices.buffer.buffer), offsets);
	//vkCmdBindIndexBuffer(cmdBuff, quadHandles[i]->deviceIndices.buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

	
}



Terrain::Terrain(VulkanRenderer* renderer,
	TerrainChunkBuffer& chunkBuffer,
	InternalGraph::GraphPrototype& protoGraph,
	int numCells, int maxLevels, float heightScale,
	TerrainCoordinateData coords)
	:	
	renderer(renderer)
	chunkBuffer(chunkBuffer),
	maxLevels(maxLevels), heightScale(heightScale), 
	coordinateData(coords),
	fastGraphUser(protoGraph, 1337, coords.sourceImageResolution, coords.noisePos, coords.noiseSize.x)

{

	//simple calculation right now, does the absolute max number of quads possible with given max level
	//in future should calculate the actual number of max quads, based on distance calculation
	if (maxLevels <= 0) {
		maxNumQuads = 1;
	}
	else {
		//with current quad density this is the average upper bound (kidna a guess but its probably more than enough for now (had to add 25 cause it wasn't enough lol!)
		maxNumQuads = 1 + 16 + 20 + 25 + 50 * maxLevels;
		//maxNumQuads = (int)((1.0 - glm::pow(4, maxLevels + 1)) / (-3.0)); //legitimate max number of quads (like if everything was subdivided)
	}
	//terrainQuads = new MemoryPool<TerrainQuad, 2 * sizeof(TerrainQuad)>();
	//terrainQuads = pool;
	//quadHandles.reserve(maxNumQuads);

	//TerrainQuad* test = terrainQuadPool->allocate();
	//test->init(posX, posY, sizeX, sizeY, 0, meshVertexPool->allocate(), meshIndexPool->allocate());
	
	rootQuadIndex = chunkBuffer.Allocate());
	rootQuad = chunkBuffer.GetChunk(rootQuadIndex);

	rootQuad->Setup(coordinateData.pos, coordinateData.size,
		coordinateData.noisePos, coordinateData.noiseSize,
		0, glm::i32vec2(0, 0),
		GetHeightAtLocation(TerrainQuad::GetUVvalueFromLocalIndex(NumCells / 2, NumCells, 0, 0),
			TerrainQuad::GetUVvalueFromLocalIndex(NumCells / 2, NumCells, 0, 0)),
		this);

	// quadHandles.push_back(std::make_unique<TerrainQuad>(
	// 	coordinateData.pos, coordinateData.size,
	// 	coordinateData.noisePos, coordinateData.noiseSize,
	// 	0, glm::i32vec2(0, 0),
	// 	GetHeightAtLocation(TerrainQuad::GetUVvalueFromLocalIndex(NumCells / 2, NumCells, 0, 0),
	// 		TerrainQuad::GetUVvalueFromLocalIndex(NumCells / 2, NumCells, 0, 0)),
	// 	chunkBuffer.allocate()
	// 	));

	//modelMatrixData.model = glm::translate(glm::mat4(), glm::vec3(coordinateData.pos.x, 0, coordinateData.pos.y));
}

Terrain::~Terrain() {
	CleanUp();
}

void Terrain::CleanUp()
{
	rootQuad.RecursiveCleanUp();
	//for (auto& quad : quadHandles) {
	//	//meshPool_vertices.deallocate(quad->vertices);
	//	//meshPool_indices.deallocate(quad->indices);
	//	chunkBuffer.GetChunk(quad)->CleanUp();
	//}
	//quadHandles.clear();
	uniformBuffer->CleanBuffer();
	//for(auto& buf : vertexBuffers)
	//	buf.CleanBuffer();
	//for(auto& buf : indexBuffers)
	//	buf.CleanBuffer();

	terrainVulkanSplatMap->destroy();
}


void Terrain::InitTerrain(VulkanRenderer* renderer, glm::vec3 cameraPos, std::shared_ptr<VulkanTexture2DArray> terrainVulkanTextureArray)
{
	this->renderer = renderer;

	SetupMeshbuffers();
	SetupUniformBuffer();
	SetupImage();
	SetupDescriptorSets(terrainVulkanTextureArray);
	SetupPipeline();

	InitTerrainQuad(rootQuad, cameraPos);

	//UpdateMeshBuffer();
}

void Terrain::UpdateTerrain(glm::vec3 viewerPos) {
	SimpleTimer updateTime;
	updateTime.StartTimer();

	bool shouldUpdateBuffers = UpdateTerrainQuad(rootQuad, viewerPos);

	//if (shouldUpdateBuffers)
	//	UpdateMeshBuffer();

	PrevQuadHandles.clear();
	for (auto& quad : quadHandles)
		PrevQuadHandles.push_back(quad.get());

	updateTime.EndTimer();

	//if (updateTime.GetElapsedTimeMicroSeconds() > 1500)
	//	Log::Debug << " Update time " << updateTime.GetElapsedTimeMicroSeconds() << "\n";
}

std::vector<RGBA_pixel>*  Terrain::LoadSplatMapFromGenerator() {
	return fastGraphUser.GetSplatMap().GetImageVectorData();
}

void Terrain::SetupMeshbuffers() {
	//uint32_t vBufferSize = static_cast<uint32_t>(maxNumQuads) * sizeof(TerrainMeshVertices);
	//uint32_t iBufferSize = static_cast<uint32_t>(maxNumQuads) * sizeof(TerrainMeshIndices);

	// Create device local target buffers
	// Vertex buffer
	//vertexBuffer = std::make_shared<VulkanBufferVertex>(renderer->device);
	//indexBuffer = std::make_shared<VulkanBufferIndex>(renderer->device)//;

	//vertexBuffer->CreateVertexBuffer(maxNumQuads * vertCount, 8);
	//indexBuffer->CreateIndexBuffer(maxNumQuads * indCount);
}

void Terrain::SetupUniformBuffer()
{
	//modelUniformBuffer.CreateUniformBuffer(renderer->device, sizeof(ModelBufferObject));
	//renderer->device.createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, (VkMemoryPropertyFlags)(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
	//	&modelUniformBuffer, sizeof(ModelBufferObject) * maxNumQuads);

	uniformBuffer = std::make_shared<VulkanBufferUniform>(renderer->device);
	uniformBuffer->CreateUniformBufferPersitantlyMapped(sizeof(ModelBufferObject));

	ModelBufferObject mbo;
	mbo.model = glm::mat4();
	mbo.model = glm::translate(mbo.model, glm::vec3(coordinateData.pos.x, 0, coordinateData.pos.y));

	uniformBuffer->CopyToBuffer(&mbo, sizeof(ModelBufferObject));

}

void Terrain::SetupImage()
{
	terrainVulkanSplatMap = std::make_shared<VulkanTexture2D>(renderer->device);
	if (terrainSplatMap != nullptr) {

		terrainVulkanSplatMap->loadFromTexture(terrainSplatMap, VK_FORMAT_R8G8B8A8_UNORM, *renderer,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false, false, 1, false);

	}
	else {
		throw std::runtime_error("failed to load terrain splat map!");

	}
}

void Terrain::SetupDescriptorSets(std::shared_ptr<VulkanTexture2DArray> terrainVulkanTextureArray)
{
	descriptor = renderer->GetVulkanDescriptor();

	std::vector<VkDescriptorSetLayoutBinding> m_bindings;
	m_bindings.push_back(VulkanDescriptor::CreateBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0, 1));
	m_bindings.push_back(VulkanDescriptor::CreateBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1, 1));
	m_bindings.push_back(VulkanDescriptor::CreateBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2, 1));
	descriptor->SetupLayout(m_bindings);

	std::vector<DescriptorPoolSize> poolSizes;
	poolSizes.push_back(DescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1));
	poolSizes.push_back(DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1));
	poolSizes.push_back(DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1));
	descriptor->SetupPool(poolSizes, maxNumQuads);

	//VkDescriptorSetAllocateInfo allocInfoTerrain = initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
	descriptorSet = descriptor->CreateDescriptorSet();

	std::vector<DescriptorUse> writes;
	writes.push_back(DescriptorUse(0, 1, uniformBuffer->resource));
	writes.push_back(DescriptorUse(1, 1, terrainVulkanSplatMap->resource));
	writes.push_back(DescriptorUse(2, 1, terrainVulkanTextureArray->resource));
	descriptor->UpdateDescriptorSet(descriptorSet, writes);

}

void Terrain::SetupPipeline()
{
	VulkanPipeline &pipeMan = renderer->pipelineManager;
	mvp = pipeMan.CreateManagedPipeline();

	//pipeMan.SetVertexShader(mvp, loadShaderModule(renderer->device.device, "assets/shaders/terrain.vert.spv"));
	//pipeMan.SetFragmentShader(mvp, loadShaderModule(renderer->device.device, "assets/shaders/terrain.frag.spv"));

	auto vert = renderer->shaderManager.loadShaderModule("assets/shaders/terrain.vert.spv", ShaderModuleType::vertex);
	auto frag = renderer->shaderManager.loadShaderModule("assets/shaders/terrain.frag.spv", ShaderModuleType::fragment);

	ShaderModuleSet set(vert, frag, {}, {}, {});
	pipeMan.SetShaderModuleSet(mvp, set);

	pipeMan.SetVertexInput(mvp, Vertex_PosNormTex::getBindingDescription(), Vertex_PosNormTex::getAttributeDescriptions());
	pipeMan.SetInputAssembly(mvp, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
	pipeMan.SetViewport(mvp, (float)renderer->vulkanSwapChain.swapChainExtent.width, (float)renderer->vulkanSwapChain.swapChainExtent.height, 0.0f, 1.0f, 0.0f, 0.0f);
	pipeMan.SetScissor(mvp, renderer->vulkanSwapChain.swapChainExtent.width, renderer->vulkanSwapChain.swapChainExtent.height, 0, 0);
	pipeMan.SetViewportState(mvp, 1, 1, 0);
	pipeMan.SetRasterizer(mvp, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE, VK_FALSE, 1.0f, VK_TRUE);
	pipeMan.SetMultisampling(mvp, VK_SAMPLE_COUNT_1_BIT);
	pipeMan.SetDepthStencil(mvp, VK_TRUE, VK_TRUE, VK_COMPARE_OP_GREATER, VK_FALSE, VK_FALSE);
	pipeMan.SetColorBlendingAttachment(mvp, VK_FALSE, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		VK_BLEND_OP_ADD, VK_BLEND_FACTOR_SRC_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
		VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO);
	pipeMan.SetColorBlending(mvp, 1, &mvp->pco.colorBlendAttachment);

	std::vector<VkDynamicState> dynamicStateEnables = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};

	pipeMan.SetDynamicState(mvp, dynamicStateEnables);

	std::vector<VkDescriptorSetLayout> layouts;
	renderer->AddGlobalLayouts(layouts);
	layouts.push_back(descriptor->GetLayout());
	pipeMan.SetDescriptorSetLayout(mvp, layouts);

	//VkPushConstantRange pushConstantRange = {};
	//pushConstantRange.offset = 0;
	//pushConstantRange.size = sizeof(TerrainPushConstant);
	//pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	//pipeMan.SetModelPushConstant(mvp, pushConstantRange);

	pipeMan.BuildPipelineLayout(mvp);
	pipeMan.BuildPipeline(mvp, renderer->renderPass, 0);

	pipeMan.SetRasterizer(mvp, VK_POLYGON_MODE_LINE, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE, VK_FALSE, 1.0f, VK_TRUE);
	pipeMan.BuildPipeline(mvp, renderer->renderPass, 0);

	//pipeMan.CleanShaderResources(mvp);

	//pipeMan.SetVertexShader(mvp, loadShaderModule(renderer->device.device, "assets/shaders/normalVecDebug.vert.spv"));
	//pipeMan.SetFragmentShader(mvp, loadShaderModule(renderer->device.device, "assets/shaders/normalVecDebug.frag.spv"));
	//pipeMan.SetGeometryShader(mvp, loadShaderModule(renderer->device.device, "assets/shaders/normalVecDebug.geom.spv"));
	//
	//pipeMan.BuildPipeline(mvp, renderer->renderPass, 0);
	//pipeMan.CleanShaderResources(mvp);

}

struct CopyCommand {
	VkBuffer src;
	VkBuffer dst;
	VkBufferCopy region;

	CopyCommand(VkBuffer src, VkBuffer dst, VkDeviceSize size) :
		src(src), dst(dst), region(initializers::bufferCopyCreate(size, 0, 0))
	{
	}
};

void Terrain::UpdateMeshBuffer() {

	// while (terrainGenerationWorkers.size() > 0) {
	// 	terrainGenerationWorkers.back()->join();
	// 	//terrainGenerationWorkers.front() = std::move(terrainGenerationWorkers.back());
	// 	terrainGenerationWorkers.pop_back();
	// }

	// //std::vector<VkBufferCopy> vertexCopyRegions;
	// //std::vector<VkBufferCopy> indexCopyRegions;

	// std::vector<VulkanBufferVertex> vertexStagingBuffers;
	// std::vector<VulkanBufferIndex> indexStagingBuffers;

	// //std::vector<Signal> flags;	

	// std::vector<CopyCommand> commands;

	// TransferCommandWork transfer;

	// std::vector<TerrainQuad*>::iterator prevQuadIter = PrevQuadHandles.begin();
	// for (auto& quad : quadHandles) {
	// 	if (PrevQuadHandles.size() == 0 || quad.get() != *prevQuadIter) {

	// 		quad->deviceVertices.CleanBuffer();
	// 		quad->deviceIndices.CleanBuffer();

	// 		vertexStagingBuffers.push_back(VulkanBufferVertex(renderer->device));
	// 		indexStagingBuffers.push_back(VulkanBufferIndex(renderer->device));

	// 		vertexStagingBuffers.back().CreateStagingVertexBuffer(quad->vertices->data(), vertCount, 8);
	// 		indexStagingBuffers.back().CreateStagingIndexBuffer(quad->indices->data(), indCount);

	// 		quad->deviceVertices.CreateVertexBuffer(vertCount, 8);
	// 		quad->deviceIndices.CreateIndexBuffer(indCount);

	// 		commands.push_back(CopyCommand(vertexStagingBuffers.back().buffer.buffer,
	// 			quad->deviceVertices.buffer.buffer,
	// 			sizeof(TerrainMeshVertices)));
	// 		commands.push_back(CopyCommand(indexStagingBuffers.back().buffer.buffer,
	// 			quad->deviceIndices.buffer.buffer,
	// 			sizeof(TerrainMeshIndices)));

	// 		//transfer.buffersToClean.push_back(vertexStagingBuffers.back());
	// 		//transfer.buffersToClean.push_back(indexStagingBuffers.back());
	// 		transfer.flags.push_back(quad->isReady);

	// 	}
	// }
	// transfer.buffersToClean.insert(transfer.buffersToClean.end(), vertexStagingBuffers.begin(), vertexStagingBuffers.end());
	// transfer.buffersToClean.insert(transfer.buffersToClean.end(), indexStagingBuffers.begin(), indexStagingBuffers.end());


	// transfer.work = std::function<void(VkCommandBuffer)>(
	// 	[=](const VkCommandBuffer cmdBuf) {
	// 	for (auto& cmd : commands) {
	// 		vkCmdCopyBuffer(cmdBuf, cmd.src, cmd.dst, 1, &cmd.region);
	// 	}
	// }
	// );
	// renderer->SubmitTransferWork(std::move(transfer));

	//------------------------------------------
		// SimpleTimer cpuDataTime;

		// std::vector<VkBufferCopy> vertexCopyRegions;
		// std::vector<VkBufferCopy> indexCopyRegions;

		// uint32_t vBufferSize = static_cast<uint32_t>(quadHandles.size()) * sizeof(TerrainMeshVertices);
		// uint32_t iBufferSize = static_cast<uint32_t>(quadHandles.size()) * sizeof(TerrainMeshIndices);

		// verts.resize(quadHandles.size());
		// inds.resize(quadHandles.size());

		// std::vector<Signal> flags;
		// flags.reserve(quadHandles.size());

		// if (quadHandles.size() > PrevQuadHandles.size()) //more meshes than before
		// {
		// 	for (int i = 0; i < PrevQuadHandles.size(); i++) {
		// 		if (quadHandles[i] != PrevQuadHandles[i]) {
		// 			verts[i] = *(quadHandles[i]->vertices);
		// 			inds[i] = *(quadHandles[i]->indices);

		// 			vertexCopyRegions.push_back(
		// 				initializers::bufferCopyCreate(sizeof(TerrainMeshVertices),
		// 					i * sizeof(TerrainMeshVertices), i * sizeof(TerrainMeshVertices)));

		// 			indexCopyRegions.push_back(
		// 				initializers::bufferCopyCreate(sizeof(TerrainMeshIndices),
		// 					i * sizeof(TerrainMeshIndices), i * sizeof(TerrainMeshIndices)));
		// 			flags.push_back(quadHandles[i]->isReady);
		// 		}
		// 	}
		// 	for (uint32_t i = (uint32_t)PrevQuadHandles.size(); i < quadHandles.size(); i++) {
		// 		verts[i] = *(quadHandles[i]->vertices);
		// 		inds[i] = *(quadHandles[i]->indices);

		// 		vertexCopyRegions.push_back(
		// 			initializers::bufferCopyCreate(sizeof(TerrainMeshVertices),
		// 				i * sizeof(TerrainMeshVertices), i * sizeof(TerrainMeshVertices)));

		// 		indexCopyRegions.push_back(
		// 			initializers::bufferCopyCreate(sizeof(TerrainMeshIndices),
		// 				i * sizeof(TerrainMeshIndices), i * sizeof(TerrainMeshIndices)));
		// 		flags.push_back(quadHandles[i]->isReady);
		// 	}
		// }
		// else { //less meshes than before, can erase at end.
		// 	for (int i = 0; i < quadHandles.size(); i++) {
		// 		if (quadHandles[i] != PrevQuadHandles[i]) {
		// 			verts[i] = *(quadHandles[i]->vertices);
		// 			inds[i] = *(quadHandles[i]->indices);

		// 			vertexCopyRegions.push_back(
		// 				initializers::bufferCopyCreate(sizeof(TerrainMeshVertices),
		// 					i * sizeof(TerrainMeshVertices), i * sizeof(TerrainMeshVertices)));

		// 			indexCopyRegions.push_back(
		// 				initializers::bufferCopyCreate(sizeof(TerrainMeshIndices),
		// 					i * sizeof(TerrainMeshIndices), i * sizeof(TerrainMeshIndices)));
		// 			flags.push_back(quadHandles[i]->isReady);
		// 		}
		// 	}
		// }
		// cpuDataTime.EndTimer();
		// SimpleTimer gpuTransferTime;

		// if (vertexCopyRegions.size() > 0 || indexCopyRegions.size() > 0)
		// {
		// 	VulkanBufferVertex vertexStaging(renderer->device);
		// 	VulkanBufferIndex indexStaging(renderer->device);

		// 	vertexStaging.CreateStagingVertexBuffer(verts.data(), (uint32_t)verts.size() * vertCount, 8);
		// 	indexStaging.CreateStagingIndexBuffer(inds.data(), (uint32_t)inds.size() * indCount);

		// 	TransferCommandWork transfer;
		// 	VkBuffer vbuf = vertexBuffer->buffer.buffer;
		// 	VkBuffer ibuf = indexBuffer->buffer.buffer;

		// 	transfer.work = std::function<void(VkCommandBuffer)>(
		// 		[=](const VkCommandBuffer cmdBuf) {
		// 			vkCmdCopyBuffer(cmdBuf, vertexStaging.buffer.buffer, vbuf, (uint32_t)vertexCopyRegions.size(), vertexCopyRegions.data());

		// 			//copyRegion.size = indexBuffer->size;
		// 			vkCmdCopyBuffer(cmdBuf, indexStaging.buffer.buffer, ibuf, (uint32_t)indexCopyRegions.size(), indexCopyRegions.data());

		// 		}
		// 	);
		// 	transfer.buffersToClean.push_back(vertexStaging);
		// 	transfer.buffersToClean.push_back(indexStaging);
		// 	transfer.flags.insert(transfer.flags.end(), flags.begin(), flags.end());

		// 	renderer->SubmitTransferWork(std::move(transfer));

	//----------------------------------------------------------------------------

			//transfer.flags.insert(transfer.flags.end(), indexCopyRegions.begin(), indexCopyRegions.end());


			//VkCommandBuffer copyCmd = renderer->GetTransferCommandBuffer();

			//vkCmdCopyBuffer(copyCmd, vertexStaging.buffer.buffer, vertexBuffer->buffer.buffer, (uint32_t)vertexCopyRegions.size(), vertexCopyRegions.data());

			//copyRegion.size = indexBuffer->size;
			//vkCmdCopyBuffer(copyCmd, indexStaging.buffer.buffer, indexBuffer->buffer.buffer, (uint32_t)indexCopyRegions.size(), indexCopyRegions.data());

			//renderer->SubmitTransferCommandBuffer(copyCmd, flags, std::vector<VulkanBuffer>({ std::move(vertexStaging), std::move(indexStaging) }));

			//vertexStaging.CleanBuffer(renderer->device);
			//indexStaging.CleanBuffer(renderer->device);

			//renderer->device.DestroyVmaAllocatedBuffer(&vmaStagingBufVertex, &vmaStagingVertices);
			//renderer->device.DestroyVmaAllocatedBuffer(&vmaStagingBufIndex, &vmaStagingIndecies);

		//}
		//gpuTransferTime.EndTimer();

		//Log::Debug << "Create copy command: " << cpuDataTime.GetElapsedTimeMicroSeconds() << "\n";
		//Log::Debug << "Execute buffer copies: " << gpuTransferTime.GetElapsedTimeMicroSeconds() << "\n";
}

void Terrain::InitTerrainQuad(TerrainQuad* quad,
	glm::vec3 viewerPos) {

	//SimpleTimer terrainQuadCreateTime;


	// TODO - get new quad and set it to generate a new quad
	//std::thread* worker = new std::thread(GenerateTerrainChunk, std::ref(fastGraphUser), quad, heightScale);
	//terrainGenerationWorkers.push_back(worker);

	//terrainQuadCreateTime.EndTimer();
	//Log::Debug << "From Parent " << terrainQuadCreateTime.GetElapsedTimeMicroSeconds() << "\n";

	UpdateTerrainQuad(quad, viewerPos);
}


bool Terrain::UpdateTerrainQuad(TerrainQuad* quad, glm::vec3 viewerPos) {

	float SubdivideDistanceBias = 2.0f;

	glm::vec3 center = glm::vec3(quad->pos.x + quad->size.x / 2.0f, quad->heightValAtCenter, quad->pos.y + quad->size.y / 2.0f);
	float distanceToViewer = glm::distance(viewerPos, center);

	if (!quad->isSubdivided) { //can only subdivide if this quad isn't already subdivided
		if (distanceToViewer < quad->size.x * SubdivideDistanceBias && quad->level < maxLevels) { //must be 
			SubdivideTerrain(quad, viewerPos);
			return true;
		}
	}

	else if (distanceToViewer > quad->size.x * SubdivideDistanceBias) {
		UnSubdivide(quad);
		return true;
	}
	else {
		bool uR = UpdateTerrainQuad(chunkBuffer.GetChunk(quad->subQuads.UpRight), viewerPos);
		bool uL = UpdateTerrainQuad(chunkBuffer.GetChunk(quad->subQuads.UpLeft), viewerPos);
		bool dR = UpdateTerrainQuad(chunkBuffer.GetChunk(quad->subQuads.DownRight), viewerPos);
		bool dL = UpdateTerrainQuad(chunkBuffer.GetChunk(quad->subQuads.DownLeft), viewerPos);

		if (uR || uL || dR || dL)
			return true;
	}
	return false;
}

void Terrain::SubdivideTerrain(TerrainQuad* quad, glm::vec3 viewerPos) {
	quad->isSubdivided = true;
	numQuads += 4;

	glm::vec2 new_pos = glm::vec2(quad->pos.x, quad->pos.y);
	glm::vec2 new_size = glm::vec2(quad->size.x / 2.0, quad->size.y / 2.0);

	glm::i32vec2 new_lpos = glm::i32vec2(quad->logicalPos.x, quad->logicalPos.y);
	glm::i32vec2 new_lsize = glm::i32vec2(quad->logicalSize.x / 2.0, quad->logicalSize.y / 2.0);

	quad->subQuads.UpRight = chunkBuffer.Allocate();
	chunkBuffer.GetChunk(quad->subQuads.UpRight).Setup(
		glm::vec2(new_pos.x, new_pos.y),
		new_size,
		glm::i32vec2(new_lpos.x, new_lpos.y),
		new_lsize,
		quad->level + 1,
		glm::i32vec2(quad->subDivPos.x * 2, quad->subDivPos.y * 2),
		GetHeightAtLocation(
			TerrainQuad::GetUVvalueFromLocalIndex(NumCells / 2, NumCells, quad->level + 1, quad->subDivPos.x * 2),
			TerrainQuad::GetUVvalueFromLocalIndex(NumCells / 2, NumCells, quad->level + 1, quad->subDivPos.y * 2)),
		this);

	quad->subQuads.UpLeft = chunkBuffer.Allocate();
	chunkBuffer.GetChunk(quad->subQuads.UpLeft).Setup(
		glm::vec2(new_pos.x, new_pos.y + new_size.y),
		new_size,
		glm::i32vec2(new_lpos.x, new_lpos.y + new_lsize.y),
		new_lsize,
		quad->level + 1,
		glm::i32vec2(quad->subDivPos.x * 2, quad->subDivPos.y * 2 + 1),
		GetHeightAtLocation(
			TerrainQuad::GetUVvalueFromLocalIndex(NumCells / 2, NumCells, quad->level + 1, quad->subDivPos.x * 2),
			TerrainQuad::GetUVvalueFromLocalIndex(NumCells / 2, NumCells, quad->level + 1, quad->subDivPos.y * 2 + 1)),
		this);

	quad->subQuads.DownRight = chunkBuffer.Allocate();
	chunkBuffer.GetChunk(quad->subQuads.DownRight).Setup(
		glm::vec2(new_pos.x + new_size.x, new_pos.y), new_size,
		glm::i32vec2(new_lpos.x + new_lsize.x, new_lpos.y),
		new_lsize,
		quad->level + 1,
		glm::i32vec2(quad->subDivPos.x * 2 + 1, quad->subDivPos.y * 2),
		GetHeightAtLocation(
			TerrainQuad::GetUVvalueFromLocalIndex(NumCells / 2, NumCells, quad->level + 1, quad->subDivPos.x * 2 + 1),
			TerrainQuad::GetUVvalueFromLocalIndex(NumCells / 2, NumCells, quad->level + 1, quad->subDivPos.y * 2)),
		this);

	quad->subQuads.DownLeft = chunkBuffer.Allocate();
	chunkBuffer.GetChunk(quad->subQuads.DownLeft).Setup(
		glm::vec2(new_pos.x + new_size.x, new_pos.y + new_size.y),
		new_size,
		glm::i32vec2(new_lpos.x + new_lsize.x, new_lpos.y + new_lsize.y),
		new_lsize,
		quad->level + 1,
		glm::i32vec2(quad->subDivPos.x * 2 + 1, quad->subDivPos.y * 2 + 1),
		GetHeightAtLocation(
			TerrainQuad::GetUVvalueFromLocalIndex(NumCells / 2, NumCells, quad->level + 1, quad->subDivPos.x * 2 + 1),
			TerrainQuad::GetUVvalueFromLocalIndex(NumCells / 2, NumCells, quad->level + 1, quad->subDivPos.y * 2 + 1)),
		this);

	InitTerrainQuad(chunkBuffer.GetChunk(quad->subQuads.UpRight), viewerPos);
	InitTerrainQuad(chunkBuffer.GetChunk(quad->subQuads.UpLeft), viewerPos);
	InitTerrainQuad(chunkBuffer.GetChunk(quad->subQuads.DownRight), viewerPos);
	InitTerrainQuad(chunkBuffer.GetChunk(quad->subQuads.DownLeft), viewerPos);

	// quadHandles.push_back(std::make_unique<TerrainQuad>(
	// 	glm::vec2(new_pos.x, new_pos.y),
	// 	new_size,
	// 	glm::i32vec2(new_lpos.x, new_lpos.y),
	// 	new_lsize,
	// 	quad->level + 1,
	// 	glm::i32vec2(quad->subDivPos.x * 2, quad->subDivPos.y * 2),
	// 	GetHeightAtLocation(
	// 		TerrainQuad::GetUVvalueFromLocalIndex(NumCells / 2, NumCells, quad->level + 1, quad->subDivPos.x * 2),
	// 		TerrainQuad::GetUVvalueFromLocalIndex(NumCells / 2, NumCells, quad->level + 1, quad->subDivPos.y * 2)),
	// 	meshPool_vertices.allocate(), meshPool_indices.allocate(), renderer->device));
	// quad->subQuads.UpRight = quadHandles.back().get();

	// quadHandles.push_back(std::make_unique<TerrainQuad>(
	// 	glm::vec2(new_pos.x, new_pos.y + new_size.y),
	// 	new_size,
	// 	glm::i32vec2(new_lpos.x, new_lpos.y + new_lsize.y),
	// 	new_lsize,
	// 	quad->level + 1,
	// 	glm::i32vec2(quad->subDivPos.x * 2, quad->subDivPos.y * 2 + 1),
	// 	GetHeightAtLocation(
	// 		TerrainQuad::GetUVvalueFromLocalIndex(NumCells / 2, NumCells, quad->level + 1, quad->subDivPos.x * 2),
	// 		TerrainQuad::GetUVvalueFromLocalIndex(NumCells / 2, NumCells, quad->level + 1, quad->subDivPos.y * 2 + 1)),
	// 	meshPool_vertices.allocate(), meshPool_indices.allocate(), renderer->device));
	// quad->subQuads.UpLeft = quadHandles.back().get();

	// quadHandles.push_back(std::make_unique<TerrainQuad>(
	// 	glm::vec2(new_pos.x + new_size.x, new_pos.y), new_size,
	// 	glm::i32vec2(new_lpos.x + new_lsize.x, new_lpos.y),
	// 	new_lsize,
	// 	quad->level + 1,
	// 	glm::i32vec2(quad->subDivPos.x * 2 + 1, quad->subDivPos.y * 2),
	// 	GetHeightAtLocation(
	// 		TerrainQuad::GetUVvalueFromLocalIndex(NumCells / 2, NumCells, quad->level + 1, quad->subDivPos.x * 2 + 1),
	// 		TerrainQuad::GetUVvalueFromLocalIndex(NumCells / 2, NumCells, quad->level + 1, quad->subDivPos.y * 2)),
	// 	meshPool_vertices.allocate(), meshPool_indices.allocate(), renderer->device));
	// quad->subQuads.DownRight = quadHandles.back().get();

	// quadHandles.push_back(std::make_unique<TerrainQuad>(
	// 	glm::vec2(new_pos.x + new_size.x, new_pos.y + new_size.y),
	// 	new_size,
	// 	glm::i32vec2(new_lpos.x + new_lsize.x, new_lpos.y + new_lsize.y),
	// 	new_lsize,
	// 	quad->level + 1,
	// 	glm::i32vec2(quad->subDivPos.x * 2 + 1, quad->subDivPos.y * 2 + 1),
	// 	GetHeightAtLocation(
	// 		TerrainQuad::GetUVvalueFromLocalIndex(NumCells / 2, NumCells, quad->level + 1, quad->subDivPos.x * 2 + 1),
	// 		TerrainQuad::GetUVvalueFromLocalIndex(NumCells / 2, NumCells, quad->level + 1, quad->subDivPos.y * 2 + 1)),
	// 	meshPool_vertices.allocate(), meshPool_indices.allocate(), renderer->device));
	// quad->subQuads.DownLeft = quadHandles.back().get();


	// InitTerrainQuad(quad->subQuads.UpRight, viewerPos);
	// InitTerrainQuad(quad->subQuads.UpLeft, viewerPos);
	// InitTerrainQuad(quad->subQuads.DownRight, viewerPos);
	// InitTerrainQuad(quad->subQuads.DownLeft, viewerPos);

	//Log::Debug << "Terrain subdivided: Level: " << quad->level << " Position: " << quad->pos.x << ", " <<quad->pos.z << " Size: " << quad->size.x << ", " << quad->size.z << "\n";

}

void Terrain::UnSubdivide(TerrainQuad* quad) {
	if (quad->isSubdivided)
	{
		quad->isSubdivided = false;
		
		auto urDel = chunkBuffer.GetChunk(quad->subQuads.UpRight);
		auto ulDel = chunkBuffer.GetChunk(quad->subQuads.UpLeft);
		auto drDel = chunkBuffer.GetChunk(quad->subQuads.DownRight);
		auto dlDel = chunkBuffer.GetChunk(quad->subQuads.DownLeft);

		urDel->RecursiveCleanUp();
		ulDel->RecursiveCleanUp();
		drDel->RecursiveCleanUp();
		dlDel->RecursiveCleanUp();
		
		numQuads -= 4;

		// auto delUR = std::find_if(quadHandles.begin(), quadHandles.end(),
		// 	[&](std::unique_ptr<TerrainQuad>& q)
		// {return q.get() == quad->subQuads.UpRight; });

		// if (delUR != quadHandles.end())
		// {
		// 	UnSubdivide((*delUR).get());
		// 	meshPool_vertices.deallocate((*delUR)->vertices);
		// 	meshPool_indices.deallocate((*delUR)->indices);
		// 	//(*delUR)->CleanUp();
		// 	//delUR->reset();
		// 	quadHandles.erase(delUR);
		// 	numQuads--;
		// }

		// auto delDR = std::find_if(quadHandles.begin(), quadHandles.end(),
		// 	[&](std::unique_ptr<TerrainQuad>& q)
		// {return q.get() == quad->subQuads.DownRight; });

		// if (delDR != quadHandles.end())
		// {
		// 	UnSubdivide((*delDR).get());
		// 	meshPool_vertices.deallocate((*delDR)->vertices);
		// 	meshPool_indices.deallocate((*delDR)->indices);
		// 	//(*delDR)->CleanUp();
		// 	//delDR->reset();
		// 	quadHandles.erase(delDR);
		// 	numQuads--;
		// }

		// auto delUL = std::find_if(quadHandles.begin(), quadHandles.end(),
		// 	[&](std::unique_ptr<TerrainQuad>& q)
		// {return q.get() == quad->subQuads.UpLeft; });
		// if (delUL != quadHandles.end())
		// {
		// 	UnSubdivide((*delUL).get());
		// 	meshPool_vertices.deallocate((*delUL)->vertices);
		// 	meshPool_indices.deallocate((*delUL)->indices);
		// 	//(*delUL)->CleanUp();
		// 	//delUL->reset();
		// 	quadHandles.erase(delUL);
		// 	numQuads--;
		// }

		// auto delDL = std::find_if(quadHandles.begin(), quadHandles.end(),
		// 	[&](std::unique_ptr<TerrainQuad>& q)
		// { return q.get() == quad->subQuads.DownLeft; });
		// if (delDL != quadHandles.end())
		// {
		// 	UnSubdivide((*delDL).get());
		// 	meshPool_vertices.deallocate((*delDL)->vertices);
		// 	meshPool_indices.deallocate((*delDL)->indices);
		// 	//(*delDL)->CleanUp();
		// 	//delDL->reset();
		// 	quadHandles.erase(delDL);
		// 	numQuads--;
		// }
	}
	
	//Log::Debug << "Terrain un-subdivided: Level: " << quad->level << " Position: " << quad->pos.x << ", " << quad->pos.z << " Size: " << quad->size.x << ", " << quad->size.z << "\n";
}

void Terrain::DrawTerrain(VkCommandBuffer cmdBuff, bool ifWireframe) {
	VkDeviceSize offsets[] = { 0 };
	//return;
	if (!terrainVulkanSplatMap->readyToUse)
		return;
	//	std::vector<VkDeviceSize> vertexOffsettings(quadHandles.size());
	//	std::vector<VkDeviceSize> indexOffsettings(quadHandles.size());
	//
	//	for (int i = 0; i < quadHandles.size(); i++) {
	//		vertexOffsettings[i] = (i * sizeof(TerrainMeshVertices));
	//		indexOffsettings[i] = (i * sizeof(TerrainMeshIndices));
	//	}

		//vkCmdPushConstants(
		//	cmdBuff,
		//	mvp->layout,
		//	VK_SHADER_STAGE_VERTEX_BIT,
		//	0,
		//	sizeof(TerrainPushConstant),
		//	&modelMatrixData);

	// vkCmdBindPipeline(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, ifWireframe ? mvp->pipelines->at(1) : mvp->pipelines->at(0));
	// vkCmdBindDescriptorSets(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, mvp->layout, 2, 1, &descriptorSet.set, 0, nullptr);

	// drawTimer.StartTimer();
	// for (int i = 0; i < quadHandles.size(); ++i) {
	// 	if ((!quadHandles[i]->isSubdivided && (*(quadHandles[i]->isReady)) == true)
	// 		|| (quadHandles[i]->isSubdivided &&
	// 			!(*quadHandles[i]->subQuads.DownLeft->isReady == true //note the inverse logic: (!a & !b) = !(a | b)
	// 				|| *quadHandles[i]->subQuads.DownRight->isReady == true
	// 				|| *quadHandles[i]->subQuads.UpLeft->isReady == true
	// 				|| *quadHandles[i]->subQuads.UpRight->isReady == true))) {



			//vkCmdBindVertexBuffers(cmdBuff, 0, 1, &vertexBuffer->buffer.buffer, &vertexOffsettings[i]);
			//vkCmdBindIndexBuffer(cmdBuff, indexBuffer->buffer.buffer, indexOffsettings[i], VK_INDEX_TYPE_UINT32);

			// vkCmdBindVertexBuffers(cmdBuff, 0, 1, &(quadHandles[i]->deviceVertices.buffer.buffer), offsets);
			// vkCmdBindIndexBuffer(cmdBuff, quadHandles[i]->deviceIndices.buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

			// vkCmdDrawIndexed(cmdBuff, static_cast<uint32_t>(indCount), 1, 0, 0, 0);

			//Vertex normals (yay geometry shaders!)
			//vkCmdBindPipeline(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, mvp->pipelines->at(2));
			//vkCmdDrawIndexed(cmdBuff, static_cast<uint32_t>(indCount), 1, 0, 0, 0);
	// 	}
	// }
	// drawTimer.EndTimer();



}

float Terrain::GetHeightAtLocation(float x, float z) {

	return fastGraphUser.SampleHeightMap(x, z) * heightScale;
}

glm::vec3 CalcNormal(double L, double R, double U, double D, double UL, double DL, double UR, double DR, double vertexDistance, int numCells) {

	return glm::normalize(glm::vec3(L + UL + DL - (R + UR + DR), 2 * vertexDistance / numCells, U + UL + UR - (D + DL + DR)));
}


void RecalculateNormals(int numCells, TerrainMeshVertices& verts, TerrainMeshIndices& indices) {
	int index = 0;
	for (int i = 0; i < indCount / 3; i++) {
		glm::vec3 p1 = glm::vec3((verts)[(indices)[i * 3 + 0] * vertElementCount + 0], (verts)[(indices)[i * 3 + 0] * vertElementCount + 1], (verts)[(indices)[i * 3 + 0] * vertElementCount + 2]);
		glm::vec3 p2 = glm::vec3((verts)[(indices)[i * 3 + 1] * vertElementCount + 0], (verts)[(indices)[i * 3 + 1] * vertElementCount + 1], (verts)[(indices)[i * 3 + 1] * vertElementCount + 2]);
		glm::vec3 p3 = glm::vec3((verts)[(indices)[i * 3 + 2] * vertElementCount + 0], (verts)[(indices)[i * 3 + 2] * vertElementCount + 1], (verts)[(indices)[i * 3 + 2] * vertElementCount + 2]);

		glm::vec3 t1 = p2 - p1;
		glm::vec3 t2 = p3 - p1;

		glm::vec3 normal(glm::cross(t1, t2));

		(verts)[(indices)[i * 3 + 0] * vertElementCount + 3] += normal.x; (verts)[(indices)[i * 3 + 0] * vertElementCount + 4] += normal.y; (verts)[(indices)[i * 3 + 0] * vertElementCount + 5] += normal.z;
		(verts)[(indices)[i * 3 + 1] * vertElementCount + 3] += normal.x; (verts)[(indices)[i * 3 + 1] * vertElementCount + 4] += normal.y; (verts)[(indices)[i * 3 + 1] * vertElementCount + 5] += normal.z;
		(verts)[(indices)[i * 3 + 2] * vertElementCount + 3] += normal.x; (verts)[(indices)[i * 3 + 2] * vertElementCount + 4] += normal.y; (verts)[(indices)[i * 3 + 2] * vertElementCount + 5] += normal.z;
	}

	for (int i = 0; i < (numCells + 1) * (numCells + 1); i++) {
		glm::vec3 normal = glm::normalize(glm::vec3((verts)[i * vertElementCount + 3], (verts)[i * vertElementCount + 4], (verts)[i * vertElementCount + 5]));
		(verts)[i * vertElementCount + 3] = normal.x;
		(verts)[i * vertElementCount + 4] = normal.y;
		(verts)[i * vertElementCount + 5] = normal.z;
	}
}