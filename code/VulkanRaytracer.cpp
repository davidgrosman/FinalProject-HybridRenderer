#include "VulkanRaytracer.h"
#include "Utilities.h"
#include "VulkanDeferredRenderer.h"

namespace
{

	// Vertex layout for this example
	static std::vector<vkMeshLoader::VertexLayout> vertexLayout =
	{
		vkMeshLoader::VERTEX_LAYOUT_POSITION,
		vkMeshLoader::VERTEX_LAYOUT_UV,
		vkMeshLoader::VERTEX_LAYOUT_COLOR,
		vkMeshLoader::VERTEX_LAYOUT_NORMAL,
		vkMeshLoader::VERTEX_LAYOUT_TANGENT
	};
}

VulkanRaytracer::VulkanRaytracer(const std::string& fileName) : VulkanRenderer(fileName)
{
	m_appName = "Raytracer Renderer";
}

VulkanRaytracer::~VulkanRaytracer()
{}

void VulkanRaytracer::draw(SRendererContext& context)
{
	// Command buffer to be sumitted to the queue
	m_submitInfo.commandBufferCount = 1;
	m_submitInfo.pCommandBuffers = &m_drawCmdBuffers[m_currentBuffer];
	VK_CHECK_RESULT(vkQueueSubmit(m_queue, 1, &m_submitInfo, VK_NULL_HANDLE));

	// Submit compute commands
	// Use a fence to ensure that compute command buffer has finished executing before using it again
	vkWaitForFences(m_device, 1, &m_compute.fence, VK_TRUE, UINT64_MAX);
	vkResetFences(m_device, 1, &m_compute.fence);

	VkSubmitInfo computeSubmitInfo = vkUtils::initializers::submitInfo();
	computeSubmitInfo.commandBufferCount = 1;
	computeSubmitInfo.pCommandBuffers = &m_compute.commandBuffer;

	VK_CHECK_RESULT(vkQueueSubmit(m_compute.queue, 1, &computeSubmitInfo, m_compute.fence));
}

void VulkanRaytracer::shutdownVulkan()
{
	// Flush device to make sure all resources can be freed 
	vkDeviceWaitIdle(m_device);

	// Clean up used Vulkan resources 
	// Note : Inherited destructor cleans up resources stored in base class

	vkDestroyPipeline(m_device, m_pipelines.m_graphics, nullptr);
	vkDestroyPipeline(m_device, m_pipelines.m_compute, nullptr);

	vkDestroyPipelineLayout(m_device, m_pipelineLayouts.m_graphics, nullptr);
	vkDestroyPipelineLayout(m_device, m_pipelineLayouts.m_compute, nullptr);

	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.m_graphics, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.m_compute, nullptr);

	// Meshes
	vkDestroyBuffer(m_device, m_compute.buffers.indices.buffer, nullptr);
	vkDestroyBuffer(m_device, m_compute.buffers.positions.buffer, nullptr);
	vkDestroyBuffer(m_device, m_compute.buffers.normals.buffer, nullptr);

	// Uniform buffers
	vkUtils::destroyUniformData(m_device, &m_compute.buffers.camera);
	vkUtils::destroyUniformData(m_device, &m_compute.buffers.materials);

	vkDestroyFence(m_device, m_compute.fence, nullptr);
	vkFreeCommandBuffers(m_device, m_cmdPool, 1, &m_compute.commandBuffer);

	vkDestroySemaphore(m_device, m_compute.semaphore, nullptr);

	m_textureLoader->destroyTexture(m_compute.storageRaytraceImage);

	VulkanRenderer::shutdownVulkan();
}

void VulkanRaytracer::setupUniformBuffers(SRendererContext& context) {

	prepareResources();

	// Initialize camera's ubo
	//calculate fov based on resolution
	float aspectRatio = m_windowWidth / m_windowHeight;
	float yscaled = tan(m_compute.cameraUnif.fov * (glm::pi<float>() / 180.0f));
	float xscaled = (yscaled * aspectRatio);

	m_compute.cameraUnif.forward = glm::normalize(m_compute.cameraUnif.lookat - m_compute.cameraUnif.position);
	m_compute.cameraUnif.pixelLength = glm::vec2(2 * xscaled / (float)m_windowWidth
		, 2 * yscaled / (float)m_windowHeight);

	m_compute.cameraUnif.aspectRatio = (float)aspectRatio;

	// ====== CAMERA
	VkDeviceSize bufferSize = sizeof(m_compute.cameraUnif);
	createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		bufferSize,
		&m_compute.cameraUnif,
		&m_compute.buffers.camera.buffer,
		&m_compute.buffers.camera.memory,
		&m_compute.buffers.camera.descriptor);

	// ====== MATERIALS
	bufferSize = sizeof(SMaterial) * m_sceneAttributes.m_materials.size();
	createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		bufferSize,
		m_sceneAttributes.m_materials.data(),
		&m_compute.buffers.materials.buffer,
		&m_compute.buffers.materials.memory,
		&m_compute.buffers.materials.descriptor);
}

void VulkanRaytracer::setupDescriptorFramework()
{
	// Set-up descriptor pool
	std::vector<VkDescriptorPoolSize> poolSizes =
	{
		vkUtils::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1),
		vkUtils::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3),
		vkUtils::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2),
		vkUtils::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
	};

	VkDescriptorPoolCreateInfo descriptorPoolInfo =
		vkUtils::initializers::descriptorPoolCreateInfo(
		static_cast<uint32_t>(poolSizes.size()),
		poolSizes.data(),
		3);

	VK_CHECK_RESULT(vkCreateDescriptorPool(m_device, &descriptorPoolInfo, nullptr, &m_descriptorPool));

	// ==== Graphics descriptor set layout
	// Binding 0: Fragment shader image sampler
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		0 // Binding 0, image sampler
		)
	};

	VkDescriptorSetLayoutCreateInfo descriptorLayout =
		vkUtils::initializers::descriptorSetLayoutCreateInfo(
		setLayoutBindings.data(),
		static_cast<uint32_t>(setLayoutBindings.size()));

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &descriptorLayout, nullptr, &m_descriptorSetLayouts.m_graphics));

	VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
		vkUtils::initializers::pipelineLayoutCreateInfo(
		&m_descriptorSetLayouts.m_graphics,
		1);

	VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pPipelineLayoutCreateInfo, nullptr, &m_pipelineLayouts.m_graphics));


	// ==== Compute descriptor set layout
	setLayoutBindings = {
		// Binding 0: output storage image
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		VK_SHADER_STAGE_COMPUTE_BIT,
		0),
		// Binding 1: uniform buffer for compute
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_SHADER_STAGE_COMPUTE_BIT,
		1
		),
		// Binding 2: storage buffer for triangle indices
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		VK_SHADER_STAGE_COMPUTE_BIT,
		2
		),
		// Binding 3: storage buffer for triangle positions
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		VK_SHADER_STAGE_COMPUTE_BIT,
		3
		),
		// Binding 4: storage buffer for triangle normals
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		VK_SHADER_STAGE_COMPUTE_BIT,
		4
		),
		// Binding 5: uniform buffer for materials
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_SHADER_STAGE_COMPUTE_BIT,
		5
		)
	};

	descriptorLayout =
		vkUtils::initializers::descriptorSetLayoutCreateInfo(
		setLayoutBindings.data(),
		static_cast<uint32_t>(setLayoutBindings.size()));

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &descriptorLayout, nullptr, &m_descriptorSetLayouts.m_compute));

	pPipelineLayoutCreateInfo =
		vkUtils::initializers::pipelineLayoutCreateInfo(
		&m_descriptorSetLayouts.m_compute,
		1);

	VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pPipelineLayoutCreateInfo, nullptr, &m_pipelineLayouts.m_compute));

}

void VulkanRaytracer::setupDescriptors()
{
	std::vector<VkWriteDescriptorSet> writeDescriptorSets;

	// Textured quad descriptor set
	VkDescriptorSetAllocateInfo allocInfo =
		vkUtils::initializers::descriptorSetAllocateInfo(
		m_descriptorPool,
		&m_descriptorSetLayouts.m_graphics,
		1);

	VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSets.m_graphics));

	writeDescriptorSets = {
		// Binding 0 : Fragment shader texture sampler
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_graphics,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		0,
		&m_compute.storageRaytraceImage.descriptor
		)
	};

	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

	// Ray tracing compute descriptor set

	allocInfo =
		vkUtils::initializers::descriptorSetAllocateInfo(
		m_descriptorPool,
		&m_descriptorSetLayouts.m_compute,
		1);

	VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSets.m_compute));
	
	writeDescriptorSets = {
		// Binding 0 : Storage image
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_compute,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		0,
		&m_compute.storageRaytraceImage.descriptor
		),
		// Binding 1 : Camera buffer
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_compute,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		1,
		&m_compute.buffers.camera.descriptor
		),
		// Binding 2 : Index buffer
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_compute,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		2,
		&m_compute.buffers.indices.descriptor
		),
		// Binding 3 : Position buffer
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_compute,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		3,
		&m_compute.buffers.positions.descriptor
		),
		// Binding 4 : Normal buffer
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_compute,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		4,
		&m_compute.buffers.normals.descriptor
		),
		// Binding 5 : Materials buffer
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_compute,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		5,
		&m_compute.buffers.materials.descriptor
		)
	};

	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
}

void VulkanRaytracer::setupPipelines() {
	VulkanRenderer::setupPipelines();

	// ===== GRAPHICS
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
		vkUtils::initializers::pipelineInputAssemblyStateCreateInfo(
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		0,
		VK_FALSE);

	VkPipelineRasterizationStateCreateInfo rasterizationState =
		vkUtils::initializers::pipelineRasterizationStateCreateInfo(
		VK_POLYGON_MODE_FILL,
		VK_CULL_MODE_FRONT_BIT,
		VK_FRONT_FACE_COUNTER_CLOCKWISE,
		0);

	VkPipelineColorBlendAttachmentState blendAttachmentState =
		vkUtils::initializers::pipelineColorBlendAttachmentState(
		0xf,
		VK_FALSE);

	VkPipelineColorBlendStateCreateInfo colorBlendState =
		vkUtils::initializers::pipelineColorBlendStateCreateInfo(
		1,
		&blendAttachmentState);

	VkPipelineDepthStencilStateCreateInfo depthStencilState =
		vkUtils::initializers::pipelineDepthStencilStateCreateInfo(
		VK_FALSE,
		VK_FALSE,
		VK_COMPARE_OP_LESS_OR_EQUAL);

	VkPipelineViewportStateCreateInfo viewportState =
		vkUtils::initializers::pipelineViewportStateCreateInfo(1, 1, 0);

	VkPipelineMultisampleStateCreateInfo multisampleState =
		vkUtils::initializers::pipelineMultisampleStateCreateInfo(
		VK_SAMPLE_COUNT_1_BIT,
		0);

	std::vector<VkDynamicState> dynamicStateEnables = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	VkPipelineDynamicStateCreateInfo dynamicState =
		vkUtils::initializers::pipelineDynamicStateCreateInfo(
		dynamicStateEnables.data(),
		static_cast<uint32_t>(dynamicStateEnables.size()),
		0);

	// Final fullscreen pass pipeline
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

	shaderStages[0] = loadShader(getAssetPath() + "shaders/raytracing/raytrace.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(getAssetPath() + "shaders/raytracing/raytrace.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	VkGraphicsPipelineCreateInfo pipelineCreateInfo =
		vkUtils::initializers::pipelineCreateInfo(
		m_pipelineLayouts.m_graphics,
		m_renderPass,
		0);

	VkPipelineVertexInputStateCreateInfo emptyInputState{};
	emptyInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	emptyInputState.vertexAttributeDescriptionCount = 0;
	emptyInputState.pVertexAttributeDescriptions = nullptr;
	emptyInputState.vertexBindingDescriptionCount = 0;
	emptyInputState.pVertexBindingDescriptions = nullptr;
	pipelineCreateInfo.pVertexInputState = &emptyInputState;

	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pDepthStencilState = &depthStencilState;
	pipelineCreateInfo.pDynamicState = &dynamicState;
	pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCreateInfo.pStages = shaderStages.data();
	pipelineCreateInfo.renderPass = m_renderPass;

	VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCreateInfo, nullptr, &m_pipelines.m_graphics));

	// ===== COMPUTE
	VkComputePipelineCreateInfo computePipelineCreateInfo = 
		vkUtils::initializers::computePipelineCreateInfo(
			m_pipelineLayouts.m_compute, 
			0);

	// Create shader modules from bytecodes
	shaderStages[0] = loadShader(getAssetPath() + "shaders/raytracing/raytrace.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
	computePipelineCreateInfo.stage = shaderStages[0];

	VK_CHECK_RESULT(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_pipelines.m_compute));

	VkFenceCreateInfo fenceCreateInfo = vkUtils::initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
	VK_CHECK_RESULT(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_compute.fence));

}

void VulkanRaytracer::buildCommandBuffers() {

	VkCommandBufferBeginInfo cmdBufInfo = vkUtils::initializers::commandBufferBeginInfo();

	VkClearValue clearValues[2];
	clearValues[0].color = { { 0.0f, 0.2f, 0.2f, 0.0f } };
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = vkUtils::initializers::renderPassBeginInfo();
	renderPassBeginInfo.renderPass = m_renderPass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = m_windowWidth;
	renderPassBeginInfo.renderArea.extent.height = m_windowHeight;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	for (int32_t i = 0; i < m_drawCmdBuffers.size(); ++i)
	{
		// Set target frame buffer
		renderPassBeginInfo.framebuffer = m_frameBuffers[i];

		VK_CHECK_RESULT(vkBeginCommandBuffer(m_drawCmdBuffers[i], &cmdBufInfo));

		// Image memory barrier to make sure that compute shader writes are finished before sampling from the texture
		VkImageMemoryBarrier imageMemoryBarrier = {};
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageMemoryBarrier.image = m_compute.storageRaytraceImage.image;
		imageMemoryBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(
			m_drawCmdBuffers[i],
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_FLAGS_NONE,
			0, nullptr,
			0, nullptr,
			1, &imageMemoryBarrier);


		// Record begin renderpass
		vkCmdBeginRenderPass(m_drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport = vkUtils::initializers::viewport((float)m_windowWidth, (float)m_windowHeight, 0.0f, 1.0f);
		vkCmdSetViewport(m_drawCmdBuffers[i], 0, 1, &viewport);

		VkRect2D scissor = vkUtils::initializers::rect2D(m_windowWidth, m_windowHeight, 0, 0);
		vkCmdSetScissor(m_drawCmdBuffers[i], 0, 1, &scissor);

		vkCmdBindDescriptorSets(m_drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayouts.m_graphics, 0, 1, &m_descriptorSets.m_graphics, 0, NULL);

		vkCmdBindPipeline(m_drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines.m_graphics);

		vkCmdDraw(m_drawCmdBuffers[i], 3, 1, 0, 0);
		vkCmdEndRenderPass(m_drawCmdBuffers[i]);

		VK_CHECK_RESULT(vkEndCommandBuffer(m_drawCmdBuffers[i]));
	}

	buildRaytracingCommandBuffer();
}

void VulkanRaytracer::toggleDebugDisplay() {
}

void VulkanRaytracer::viewChanged(SRendererContext& context) {
}

void VulkanRaytracer::prepareResources() {
	
	loadMeshes();

	// Get a queue from the device for copy operation
	vkGetDeviceQueue(m_device, m_vulkanDevice->queueFamilyIndices.compute, 0, &m_compute.queue);

	// Setup target compute texture
	prepareTextureTarget(&m_compute.storageRaytraceImage, TEX_DIM, TEX_DIM, VK_FORMAT_R8G8B8A8_UNORM);

	std::vector<glm::ivec4> indices = {
		glm::ivec4(0, 1, 2, 0) 
	};

	vk::Buffer stagingBuffer;

	// --  Index buffer
	VkDeviceSize bufferSize = m_sceneAttributes.m_indices.size() * sizeof(glm::ivec4);
	bufferSize = 1000 * sizeof(glm::ivec4);

	createBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		bufferSize,
		m_sceneAttributes.m_indices.data(),
		&stagingBuffer.buffer,
		&stagingBuffer.memory,
		&stagingBuffer.descriptor);

	createBuffer(
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		bufferSize,
		nullptr,
		&m_compute.buffers.indices.buffer,
		&m_compute.buffers.indices.memory,
		&m_compute.buffers.indices.descriptor);

	// Copy to staging buffer
	VkCommandBuffer copyCmd = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	VkBufferCopy copyRegion = {};
	copyRegion.size = bufferSize;
	vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer, m_compute.buffers.indices.buffer, 1, &copyRegion);
	flushCommandBuffer(copyCmd, m_compute.queue, true);

	vkDestroyBuffer(m_device, stagingBuffer.buffer, nullptr);
	vkFreeMemory(m_device, stagingBuffer.memory, nullptr);


	// --  Positions buffer
	std::vector<glm::vec4> positions = {
		glm::vec4(-1.0f, 0.0f, 0.0f, 1.0f),
		glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
		glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)
	};
	bufferSize = m_sceneAttributes.m_verticePositions.size() * sizeof(glm::vec4);
	//bufferSize = 3 * sizeof(glm::vec4);

	createBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		bufferSize,
		m_sceneAttributes.m_verticePositions.data(),
		&stagingBuffer.buffer,
		&stagingBuffer.memory,
		&stagingBuffer.descriptor);

	createBuffer(
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		bufferSize,
		nullptr,
		&m_compute.buffers.positions.buffer,
		&m_compute.buffers.positions.memory,
		&m_compute.buffers.positions.descriptor);

	// Copy to staging buffer
	VkCommandBuffer copyPositionsCmd = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	copyRegion = {};
	copyRegion.size = bufferSize;
	vkCmdCopyBuffer(copyPositionsCmd, stagingBuffer.buffer, m_compute.buffers.positions.buffer, 1, &copyRegion);
	flushCommandBuffer(copyPositionsCmd, m_compute.queue, true);

	vkDestroyBuffer(m_device, stagingBuffer.buffer, nullptr);
	vkFreeMemory(m_device, stagingBuffer.memory, nullptr);


	// --  Normals buffer
	std::vector<glm::vec4> normals = {
		glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
		glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
		glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)
	};	
	
	bufferSize = m_sceneAttributes.m_verticeNormals.size() * sizeof(glm::vec4);
	//bufferSize = 3 * sizeof(glm::vec4);

	createBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		bufferSize,
		m_sceneAttributes.m_verticeNormals.data(),
		&stagingBuffer.buffer,
		&stagingBuffer.memory,
		&stagingBuffer.descriptor);

	createBuffer(
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		bufferSize,
		nullptr,
		&m_compute.buffers.normals.buffer,
		&m_compute.buffers.normals.memory,
		&m_compute.buffers.normals.descriptor);

	// Copy to staging buffer
	VkCommandBuffer copyNormalsCmd = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	copyRegion = {};
	copyRegion.size = bufferSize;
	vkCmdCopyBuffer(copyNormalsCmd, stagingBuffer.buffer, m_compute.buffers.normals.buffer, 1, &copyRegion);
	flushCommandBuffer(copyNormalsCmd, m_compute.queue, true);

	vkDestroyBuffer(m_device, stagingBuffer.buffer, nullptr);
	vkFreeMemory(m_device, stagingBuffer.memory, nullptr);
}

void VulkanRaytracer::loadMeshes()
{

	{
		vkMeshLoader::MeshCreateInfo meshCreateInfo;

		loadMesh(getAssetPath() + m_fileName, nullptr, &m_sceneAttributes, vertexLayout, &meshCreateInfo);
		std::cout << "Number of vertices: " << m_sceneAttributes.m_verticePositions.size() << std::endl;
		std::cout << "Number of triangles: " << m_sceneAttributes.m_verticePositions.size() / 3 << std::endl;
	}
}

// Prepare a texture target that is used to store compute shader calculations
void VulkanRaytracer::prepareTextureTarget(vkUtils::VulkanTexture *tex, uint32_t width, uint32_t height, VkFormat format)
{
	// Get device properties for the requested texture format
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &formatProperties);
	// Check if requested image format supports image storage operations
	assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);

	// Prepare blit target texture
	tex->width = width;
	tex->height = height;

	VkImageCreateInfo imageCreateInfo = vkUtils::initializers::imageCreateInfo();
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = format;
	imageCreateInfo.extent = { width, height, 1 };
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	// Image will be sampled in the fragment shader and used as storage target in the compute shader
	imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	imageCreateInfo.flags = 0;

	VkMemoryAllocateInfo memAllocInfo = vkUtils::initializers::memoryAllocateInfo();
	VkMemoryRequirements memReqs;

	VK_CHECK_RESULT(vkCreateImage(m_device, &imageCreateInfo, nullptr, &tex->image));
	vkGetImageMemoryRequirements(m_device, tex->image, &memReqs);
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = m_vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(m_device, &memAllocInfo, nullptr, &tex->deviceMemory));
	VK_CHECK_RESULT(vkBindImageMemory(m_device, tex->image, tex->deviceMemory, 0));

	VkCommandBuffer layoutCmd = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	tex->imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	vkUtils::setImageLayout(
		layoutCmd,
		tex->image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		tex->imageLayout);

	flushCommandBuffer(layoutCmd, m_queue, true);

	// Create sampler
	VkSamplerCreateInfo sampler = vkUtils::initializers::samplerCreateInfo();
	sampler.magFilter = VK_FILTER_LINEAR;
	sampler.minFilter = VK_FILTER_LINEAR;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	sampler.addressModeV = sampler.addressModeU;
	sampler.addressModeW = sampler.addressModeU;
	sampler.mipLodBias = 0.0f;
	sampler.maxAnisotropy = 0;
	sampler.compareOp = VK_COMPARE_OP_NEVER;
	sampler.minLod = 0.0f;
	sampler.maxLod = 0.0f;
	sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK_RESULT(vkCreateSampler(m_device, &sampler, nullptr, &tex->sampler));

	// Create image view
	VkImageViewCreateInfo view = vkUtils::initializers::imageViewCreateInfo();
	view.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view.format = format;
	view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	view.image = tex->image;
	VK_CHECK_RESULT(vkCreateImageView(m_device, &view, nullptr, &tex->view));

	// Initialize a descriptor for later use
	tex->descriptor.imageLayout = tex->imageLayout;
	tex->descriptor.imageView = tex->view;
	tex->descriptor.sampler = tex->sampler;
}

// ===========================================================================================
//
// COMPUTE (for raytracing)
//
// ===========================================================================================


void VulkanRaytracer::buildRaytracingCommandBuffer() {

	m_compute.commandBuffer = VulkanRenderer::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

	// Create a semaphore used to synchronize offscreen rendering and usage
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkUtils::initializers::semaphoreCreateInfo();
	VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_compute.semaphore));

	VkCommandBufferBeginInfo cmdBufInfo = vkUtils::initializers::commandBufferBeginInfo();

	VK_CHECK_RESULT(vkBeginCommandBuffer(m_compute.commandBuffer, &cmdBufInfo));

	// Record binding to the compute pipeline
	vkCmdBindPipeline(m_compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelines.m_compute);

	// Bind descriptor sets
	vkCmdBindDescriptorSets(m_compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayouts.m_compute, 0, 1, &m_descriptorSets.m_compute, 0, nullptr);

	vkCmdDispatch(m_compute.commandBuffer, m_compute.storageRaytraceImage.width / 16, m_compute.storageRaytraceImage.height / 16, 1);

	VK_CHECK_RESULT(vkEndCommandBuffer(m_compute.commandBuffer));
}
