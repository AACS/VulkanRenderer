#include "Buffer.h"

#include "Device.h"


const uint32_t VERTEX_BUFFER_BIND_ID = 0;
const uint32_t INSTANCE_BUFFER_BIND_ID = 1;

//VulkanBuffer::VulkanBuffer(VulkanDevice& device)
//	: device(&device), resource(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) 
//{
//}

VulkanBuffer::VulkanBuffer(VulkanDevice& device, VkDescriptorType type,
	VkDeviceSize bufferSize,
	VkBufferUsageFlags bufferUsage, 
	VmaMemoryUsage allocUsage,
	VmaAllocationCreateFlags allocFlags, void* memToCopy,
	PersistantlyMapped persistantlyMapped,
	DynamicallyAligned dynamicAlignment, int count)
	: device(&device), resource(type),
	m_size(bufferSize)
{
	if (dynamicAlignment == DynamicallyAligned::T)
	{
		size_t minUboAlignment = device.physical_device_properties.limits.minUniformBufferOffsetAlignment;
		size_t dynamicAlignment = m_size;
		if (minUboAlignment > 0) {
			dynamicAlignment = (dynamicAlignment + minUboAlignment - 1) & ~(minUboAlignment - 1);
		}

		m_size = count * dynamicAlignment;
	}

	VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.size = m_size;
	bufferInfo.usage = bufferUsage;

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = allocUsage;
	allocInfo.flags = allocFlags;

	buffer.allocator = device.GetGeneralAllocator();
	//Log::Debug << buffer.allocator << "\n";
	if (buffer.allocator == nullptr)
		throw std::runtime_error("Allocator was null!");
	VK_CHECK_RESULT(vmaCreateBuffer(buffer.allocator, &bufferInfo, &allocInfo, &buffer.buffer, &buffer.allocation, &buffer.allocationInfo));

	if (memToCopy != nullptr) {
		memcpy(buffer.allocationInfo.pMappedData, memToCopy, m_size);
	}

	if (persistantlyMapped == PersistantlyMapped::T) {
		this->persistantlyMapped = true;
		Map(&mapped);
	}

	SetupResource();

	//Log::Debug << "Allocated buffer Memory\n";
}

// void VulkanBuffer::CleanBuffer() {

// 	//if (persistantlyMapped) {
// 	//	Unmap();
// 	//}

// 	//vmaDestroyBuffer(buffer.allocator, buffer.buffer, buffer.allocation);
// 	//device->DestroyVmaAllocatedBuffer(buffer);

// 	Log::Debug << "This function shouldn't be called!\n";

// }

VulkanBuffer::VulkanBuffer(VulkanBuffer&& buf) :
	resource(buf.resource),
	device(buf.device),
	m_size(buf.m_size),
	mapped(buf.mapped),
	created(buf.created),
	persistantlyMapped(buf.persistantlyMapped)
{
	movedFrom = false;
	buf.movedFrom = true;
}

VulkanBuffer& VulkanBuffer::operator=(VulkanBuffer&& buf) {
	buffer = buf.buffer;
	resource = buf.resource;
	device = buf.device;
	m_size = buf.m_size;
	mapped = buf.mapped;
	created = buf.created;
	persistantlyMapped = buf.persistantlyMapped;
	movedFrom = false;
	buf.movedFrom = true;
	return *this;
}

VulkanBuffer::~VulkanBuffer() {
	if (!movedFrom) {
		if (persistantlyMapped) {
			Unmap();
		}

		vmaDestroyBuffer(buffer.allocator, buffer.buffer, buffer.allocation);
		//device->DestroyVmaAllocatedBuffer(buffer);

		//Log::Debug << "Freed buffer Memory\n";
	}
}

void VulkanBuffer::Map(void** pData) {
	vmaMapMemory(buffer.allocator, buffer.allocation, pData);
	//device->VmaMapMemory(buffer, &mapped);
}
void VulkanBuffer::Unmap() {
	vmaUnmapMemory(buffer.allocator, buffer.allocation);
	//device->VmaUnmapMemory(buffer);
}

void VulkanBuffer::Flush() {
	VkMemoryPropertyFlags memFlags;
	vmaGetMemoryTypeProperties(buffer.allocator, buffer.allocationInfo.memoryType, &memFlags);
	if ((memFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
	{
		VkMappedMemoryRange memRange = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
		memRange.memory = buffer.allocationInfo.deviceMemory;
		memRange.offset = buffer.allocationInfo.offset;
		memRange.size = buffer.allocationInfo.size;
		vkFlushMappedMemoryRanges(device->device, 1, &memRange);
	}

	//device->FlushBuffer(buffer);
}

void VulkanBuffer::SetupResource() {
	resource.FillResource(buffer.buffer, 0, m_size);
	//Log::Debug << "Resource details: Size: " << m_size << "\n";
}

VkDeviceSize VulkanBuffer::Size() const {
	return m_size;
}

bool VulkanBuffer::IsCreated() const {
	return created;
}

void AlignedMemcpy(uint8_t bytes, VkDeviceSize destMemAlignment, void* src, void* dst) {
	int src_offset = 0;
	int dest_offset = 0;
	for (int i = 0; i < bytes; i++) {

		memcpy((char*)dst + dest_offset,
			(char*)src + src_offset,
			sizeof(bytes));
		src_offset += 1;
		dest_offset += (int)destMemAlignment;

	}
}

void VulkanBuffer::CopyToBuffer(void* pData, VkDeviceSize size)
{
	if (persistantlyMapped) {
		memcpy(mapped, pData, (size_t)size);

	}
	else {
		this->Map(&mapped);
		memcpy(mapped, pData, (size_t)size);

		//VkDeviceSize bufAlignment = device->physical_device_properties.limits.minUniformBufferOffsetAlignment;

		//AlignedMemcpy((size_t)size, bufAlignment, pData, mapped);

		this->Unmap();
	}
}

VulkanBufferUniform::VulkanBufferUniform(VulkanDevice& device, VkDeviceSize size)
	: VulkanBuffer(device, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		size, (VkBufferUsageFlags)(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
		(VmaMemoryUsage)(VMA_MEMORY_USAGE_CPU_TO_GPU))
{

}

//void VulkanBufferUniform::CreateUniformBuffer(VkDeviceSize size) {
//	m_size = size;
//	SetupBuffer(size, (VkBufferUsageFlags)(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
//		(VmaMemoryUsage)(VMA_MEMORY_USAGE_CPU_TO_GPU));
//
//	//device->CreateUniformBuffer(buffer, size);
//
//
//}

VulkanBufferUniformPersistant::VulkanBufferUniformPersistant(VulkanDevice& device, VkDeviceSize size)
	: VulkanBuffer(device, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		size, (VkBufferUsageFlags)(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
		(VmaMemoryUsage)VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, nullptr, PersistantlyMapped::T)
{
	//persistantlyMapped = true;
	////CreateUniformBuffer(size);
	//SetupBuffer(size, (VkBufferUsageFlags)(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
	//	(VmaMemoryUsage)VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);

	//Map(&mapped);


}

VulkanBufferUniformStaging::VulkanBufferUniformStaging(VulkanDevice& device, VkDeviceSize size, void* pData)
	: VulkanBuffer(device, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT)
{
	//m_size = size;
	////device->CreateStagingUniformBuffer(buffer, pData, size);
	//SetupBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	//	VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT, pData);

}

VulkanBufferUniformDynamic::VulkanBufferUniformDynamic(VulkanDevice& device, VkDeviceSize size, uint32_t count) :
	VulkanBuffer(device, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
		size, (VkBufferUsageFlags)(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
		VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, nullptr, PersistantlyMapped::F, DynamicallyAligned::T, count)
{

}

//void VulkanBufferUniformDynamic::CreateDynamicUniformBuffer(uint32_t count, VkDeviceSize size) {
//	size_t minUboAlignment = device->physical_device_properties.limits.minUniformBufferOffsetAlignment;
//	size_t dynamicAlignment = size;
//	if (minUboAlignment > 0) {
//		dynamicAlignment = (dynamicAlignment + minUboAlignment - 1) & ~(minUboAlignment - 1);
//	}
//
//	m_size = count * dynamicAlignment;
//
//	SetupBuffer(m_size, (VkBufferUsageFlags)(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
//		VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);
//
//	//device->CreateDynamicUniformBuffer(buffer, count, size);
//
//
//}

VulkanBufferStagingResource::VulkanBufferStagingResource(VulkanDevice& device, VkDeviceSize size, void* pData) :
	VulkanBuffer(device, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT, pData)
{
}

//void VulkanBufferStagingResource::CreateStagingResourceBuffer(void* pData, VkDeviceSize size)
//{
//	m_size = size;
//	SetupBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//		VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT, pData);
//
//	//device->CreateStagingImageBuffer(buffer, pData, size);
//
//
//}


VulkanBufferData::VulkanBufferData(VulkanDevice& device, VkDeviceSize size) :
	VulkanBuffer(device, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, size, 
	(VkBufferUsageFlags)(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |VK_BUFFER_USAGE_TRANSFER_DST_BIT),
		VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT)
{}

//void VulkanBufferData::CreateDataBuffer(VkDeviceSize size) {
//	m_size = size;
//
//	SetupBuffer(size, (VkBufferUsageFlags)(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
//		VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);
//
//	//device->CreateDataBuffer(buffer, size);
//
//
//}


VulkanBufferVertex::VulkanBufferVertex(VulkanDevice& device, uint32_t count, uint32_t vertexElementCount) :
	VulkanBuffer(device, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, count * vertexElementCount * sizeof(float),
	(VmaMemoryUsage)(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
		VMA_MEMORY_USAGE_GPU_ONLY) {

}

void VulkanBufferVertex::BindVertexBuffer(VkCommandBuffer cmdBuf) {
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(cmdBuf, VERTEX_BUFFER_BIND_ID, 1, &buffer.buffer, offsets);
}

//void VulkanBufferVertex::CreateVertexBuffer(uint32_t count, uint32_t vertexElementCount) {
//
//	VkDeviceSize size = count * vertexElementCount * sizeof(float);
//	m_size = size;
//
//	SetupBuffer(size, (VmaMemoryUsage)(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
//		VMA_MEMORY_USAGE_GPU_ONLY);
//	//device->CreateMeshBufferVertex(buffer, size);
//
//
//
//
//}

VulkanBufferStagingVertex::VulkanBufferStagingVertex(VulkanDevice& device, uint32_t count, uint32_t vertexElementCount, void* pData) :
	VulkanBuffer(device, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, count * vertexElementCount * sizeof(float),
		(VmaMemoryUsage)(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
		VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, pData) {

}


//void VulkanBufferVertex::CreateStagingVertexBuffer(void* pData, uint32_t count, uint32_t vertexElementCount) {
//	VkDeviceSize size = count * vertexElementCount * sizeof(float);
//	m_size = size;
//
//	SetupBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//		VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT, pData);
//	//device->CreateMeshStagingBuffer(buffer, pData, size);
//
//
//
//}

VulkanBufferIndex::VulkanBufferIndex(VulkanDevice& device, uint32_t count) :
	VulkanBuffer(device, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		sizeof(int) * count, (VkBufferUsageFlags)(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
		VMA_MEMORY_USAGE_GPU_ONLY) {}

void VulkanBufferIndex::BindIndexBuffer(VkCommandBuffer cmdBuf) {
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindIndexBuffer(cmdBuf, buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
}

//void VulkanBufferIndex::CreateIndexBuffer(uint32_t count) {
//	m_size = sizeof(int) * count;
//
//	SetupBuffer(m_size, (VkBufferUsageFlags)(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
//		VMA_MEMORY_USAGE_GPU_ONLY);
//	//device->CreateMeshBufferIndex(buffer, sizeof(int) * count);
//}

VulkanBufferStagingIndex::VulkanBufferStagingIndex(VulkanDevice& device, uint32_t count, void* pData) :
	VulkanBuffer(device, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		sizeof(int) * count, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT, pData)
{}

//void VulkanBufferIndex::CreateStagingIndexBuffer(void* pData, uint32_t count) {
//	m_size = sizeof(int) * count;
//
//	SetupBuffer(m_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//		VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT, pData);
//	//device->CreateMeshStagingBuffer(buffer, pData, sizeof(int) * count);
//}

VulkanBufferInstance::VulkanBufferInstance(VulkanDevice& device, uint32_t count, uint32_t indexElementCount) :
	VulkanBuffer(device, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, count * indexElementCount * sizeof(float),
	(VkBufferUsageFlags)(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
		VMA_MEMORY_USAGE_GPU_ONLY) {
}

void VulkanBufferInstance::BindInstanceBuffer(VkCommandBuffer cmdBuf) {
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(cmdBuf, INSTANCE_BUFFER_BIND_ID, 1, &buffer.buffer, offsets);
}

//void VulkanBufferInstance::CreateInstanceBuffer(uint32_t count, uint32_t indexElementCount) {
//
//	VkDeviceSize size = count * indexElementCount * sizeof(float);
//	m_size = size;
//	//device->CreateInstancingBuffer(buffer, size);
//
//	SetupBuffer(size, (VkBufferUsageFlags)(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
//		VMA_MEMORY_USAGE_GPU_ONLY);
//}

VulkanBufferStagingInstance::VulkanBufferStagingInstance(VulkanDevice& device, uint32_t count, uint32_t indexElementCount, void* pData) :
	VulkanBuffer(device, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, count * indexElementCount * sizeof(float), 
	VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT, pData) {

}
//void VulkanBufferInstance::CreateStagingInstanceBuffer(void* pData, uint32_t count, uint32_t indexElementCount) {
//	VkDeviceSize size = count * indexElementCount * sizeof(float);
//	m_size = size;
//	//device->CreateStagingInstancingBuffer(buffer, pData, size);
//
//	SetupBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//		VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT, pData);
//
//
//
//}
VulkanBufferInstancePersistant::VulkanBufferInstancePersistant(VulkanDevice& device, uint32_t count, uint32_t indexElementCount) :
	VulkanBuffer(device, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, count * indexElementCount * sizeof(float),
		(VkBufferUsageFlags)(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
		VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT, nullptr, PersistantlyMapped::T) {}

void VulkanBufferInstancePersistant::BindInstanceBuffer(VkCommandBuffer cmdBuf) {
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(cmdBuf, INSTANCE_BUFFER_BIND_ID, 1, &buffer.buffer, offsets);
}

//	void VulkanBufferInstance::CreatePersistantInstanceBuffer(
//		uint32_t count, uint32_t indexElementCount)
//{
//	persistantlyMapped = true;
//	VkDeviceSize size = count * indexElementCount * sizeof(float);
//	m_size = size;
//	//device->CreateMappedInstancingBuffer(buffer, size);
//
//	SetupBuffer(size, (VkBufferUsageFlags)(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
//		VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);
//
//
//	Map(&mapped);
//
//
//
//}
