#include "VulkanRaytracer.h"
#include "Utilities.h"
#include "VulkanDeferredRenderer.h"

namespace
{
	static std::vector<vkMeshLoader::VertexLayout> vertexLayout =
	{
		vkMeshLoader::VERTEX_LAYOUT_POSITION,
		vkMeshLoader::VERTEX_LAYOUT_UV,
		vkMeshLoader::VERTEX_LAYOUT_COLOR,
		vkMeshLoader::VERTEX_LAYOUT_NORMAL,
		vkMeshLoader::VERTEX_LAYOUT_TANGENT
	};
}

VulkanRaytracer::VulkanRaytracer() : VulkanRenderer()
{
	m_appName = "Raytracer Renderer";
}

VulkanRaytracer::~VulkanRaytracer()
{}

void VulkanRaytracer::draw(SRendererContext& context)
{
	// Wait for swap chain presentation to finish
	m_submitInfo.pWaitSemaphores = &m_semaphores.m_presentComplete;
	// Signal ready with offscreen semaphore
	m_submitInfo.pSignalSemaphores = &m_compute.semaphore;

	// Submit work
	m_submitInfo.commandBufferCount = 1;
	m_submitInfo.pCommandBuffers = &m_compute.commandBuffer;
	VK_CHECK_RESULT(vkQueueSubmit(m_queue, 1, &m_submitInfo, VK_NULL_HANDLE));

	// Scene rendering
	// -- Submit compute command
	vkWaitForFences(m_device, 1, &m_compute.fence, VK_TRUE, UINT64_MAX);
	vkResetFences(m_device, 1, &m_compute.fence);


	// Wait for offscreen semaphore
	m_submitInfo.pWaitSemaphores = &m_compute.semaphore;
	// Signal ready with render complete semaphpre
	m_submitInfo.pSignalSemaphores = &m_semaphores.m_renderComplete;

	// Submit work
	m_submitInfo.pCommandBuffers = &m_drawCmdBuffers[m_currentBuffer];
	VK_CHECK_RESULT(vkQueueSubmit(m_queue, 1, &m_submitInfo, VK_NULL_HANDLE));
}

void VulkanRaytracer::shutdownVulkan()
{
	// Flush device to make sure all resources can be freed 
	vkDeviceWaitIdle(m_device);

	// Clean up used Vulkan resources 
	// Note : Inherited destructor cleans up resources stored in base class

	vkDestroySampler(m_device, m_compute.storageRaytraceImage.sampler, nullptr);

	vkDestroyPipeline(m_device, m_pipelines.m_graphics, nullptr);
	vkDestroyPipeline(m_device, m_pipelines.m_compute, nullptr);

	vkDestroyPipelineLayout(m_device, m_pipelineLayouts.m_graphics, nullptr);
	vkDestroyPipelineLayout(m_device, m_pipelineLayouts.m_compute, nullptr);

	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);

	// Meshes
	VulkanMeshLoader::destroyBuffers(m_device, &m_sceneMeshes.m_model);
	VulkanMeshLoader::destroyBuffers(m_device, &m_sceneMeshes.m_quad);

	// Uniform buffers
	vkUtils::destroyUniformData(m_device, &m_compute.buffers.camera);
	vkUtils::destroyUniformData(m_device, &m_compute.buffers.materials);

	vkFreeCommandBuffers(m_device, m_cmdPool, 1, &m_compute.commandBuffer);

	vkDestroySemaphore(m_device, m_compute.semaphore, nullptr);

	VulkanRenderer::shutdownVulkan();
}

void VulkanRaytracer::setupUniformBuffers(SRendererContext& context) {
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
		nullptr,
		&m_compute.buffers.camera.buffer,
		&m_compute.buffers.camera.memory,
		&m_compute.buffers.camera.descriptor);

	// ====== MATERIALS

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

	// Set-up descriptor set layout
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
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
		),
		// Binding 6: Fragment shader image sampler
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		6
		),

	};

	VkDescriptorSetLayoutCreateInfo descriptorLayout =
		vkUtils::initializers::descriptorSetLayoutCreateInfo(
		setLayoutBindings.data(),
		static_cast<uint32_t>(setLayoutBindings.size()));

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &descriptorLayout, nullptr, &m_descriptorSetLayout));

	VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
		vkUtils::initializers::pipelineLayoutCreateInfo(
		&m_descriptorSetLayout,
		1);

	VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pPipelineLayoutCreateInfo, nullptr, &m_pipelineLayouts.m_graphics));

	// Offscreen (scene) rendering pipeline layout
	VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pPipelineLayoutCreateInfo, nullptr, &m_pipelineLayouts.m_compute));

}

void VulkanRaytracer::setupDescriptors()
{
	generateQuad();
	loadMeshes();

	std::vector<VkWriteDescriptorSet> writeDescriptorSets;

	// Textured quad descriptor set
	VkDescriptorSetAllocateInfo allocInfo =
		vkUtils::initializers::descriptorSetAllocateInfo(
		m_descriptorPool,
		&m_descriptorSetLayout,
		1);

	VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSets.m_graphics));

	writeDescriptorSets = {
		// Binding 6 : Fragment shader texture sampler
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_graphics,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		6,
		&m_compute.storageRaytraceImage.descriptor
		)
	};

	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

	// Ray tracing compute descriptor set
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
		&m_compute.storageRaytraceImage.descriptor
		),
		// Binding 3 : Position buffer
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_compute,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		3,
		&m_compute.storageRaytraceImage.descriptor
		),
		// Binding 4 : Normal buffer
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_compute,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		4,
		&m_compute.storageRaytraceImage.descriptor
		),
		// Binding 5 : Materials buffer
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_compute,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		5,
		&m_compute.storageRaytraceImage.descriptor
		)
	};

	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
}

void VulkanRaytracer::setupPipelines() {
	VulkanRenderer::setupPipelines();

	// --- GRAPHICS
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
		vkUtils::initializers::pipelineInputAssemblyStateCreateInfo(
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		0,
		VK_FALSE);

	VkPipelineRasterizationStateCreateInfo rasterizationState =
		vkUtils::initializers::pipelineRasterizationStateCreateInfo(
		VK_POLYGON_MODE_FILL,
		VK_CULL_MODE_BACK_BIT,
		VK_FRONT_FACE_CLOCKWISE,
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
		VK_TRUE,
		VK_TRUE,
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

	pipelineCreateInfo.pVertexInputState = &m_vertices.m_inputState;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pDepthStencilState = &depthStencilState;
	pipelineCreateInfo.pDynamicState = &dynamicState;
	pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCreateInfo.pStages = shaderStages.data();

	VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCreateInfo, nullptr, &m_pipelines.m_graphics));

	// --- COMPUTE
	VkComputePipelineCreateInfo computePipelineCreateInfo = 
		vkUtils::initializers::computePipelineCreateInfo(
			m_compute.pipelineLayout, 
			0);

	// Create shader modules from bytecodes
	shaderStages[0] = loadShader(getAssetPath() + "shaders/raytracing/raytrace.comp.sp", VK_SHADER_STAGE_COMPUTE_BIT);
	computePipelineCreateInfo.stage = shaderStages[0];

	VK_CHECK_RESULT(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_pipelines.m_compute));

	VkFenceCreateInfo fenceCreateInfo = vkUtils::initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
	VK_CHECK_RESULT(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_compute.fence));

}

void VulkanRaytracer::buildCommandBuffers() {

	VkCommandBufferBeginInfo cmdBufInfo = vkUtils::initializers::commandBufferBeginInfo();

	VkClearValue clearValues[2];
	clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 0.0f } };
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
			0,
			0, nullptr,
			0, nullptr,
			1, &imageMemoryBarrier);


		// Record begin renderpass
		vkCmdBeginRenderPass(m_drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport = vkUtils::initializers::viewport((float)m_windowWidth, (float)m_windowHeight, 0.0f, 1.0f);
		vkCmdSetViewport(m_drawCmdBuffers[i], 0, 1, &viewport);

		VkRect2D scissor = vkUtils::initializers::rect2D(m_windowWidth, m_windowHeight, 0, 0);
		vkCmdSetScissor(m_drawCmdBuffers[i], 0, 1, &scissor);

		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindDescriptorSets(m_drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayouts.m_graphics, 0, 1, &m_descriptorSets.m_graphics, 0, NULL);

		// Record binding the graphics pipeline
		vkCmdBindPipeline(m_drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines.m_graphics);

		// Final composition as full screen quad
		vkCmdBindPipeline(m_drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines.m_graphics);
		vkCmdBindVertexBuffers(m_drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &m_sceneMeshes.m_quad.vertices.buf, offsets);
		vkCmdBindIndexBuffer(m_drawCmdBuffers[i], m_sceneMeshes.m_quad.indices.buf, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(m_drawCmdBuffers[i], 6, 1, 0, 0, 1);

		vkCmdEndRenderPass(m_drawCmdBuffers[i]);

		VK_CHECK_RESULT(vkEndCommandBuffer(m_drawCmdBuffers[i]));
	}

	buildRaytracingCommandBuffer();
}

void VulkanRaytracer::toggleDebugDisplay() {
}

void VulkanRaytracer::viewChanged(SRendererContext& context) {
}

void VulkanRaytracer::generateQuad() {

	struct Vertex {
		float pos[3];
		float uv[2];
		float col[3];
		float normal[3];
		float tangent[3];
	};

	std::vector<Vertex> vertexBuffer;

	float x = 0.0f;
	float y = 0.0f;

	vertexBuffer.push_back({ { x + 1.0f, y + 1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f } });
	vertexBuffer.push_back({ { x, y + 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f } });
	vertexBuffer.push_back({ { x, y, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f } });
	vertexBuffer.push_back({ { x + 1.0f, y, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f } });

	createBuffer(
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		vertexBuffer.size() * sizeof(Vertex),
		vertexBuffer.data(),
		&m_sceneMeshes.m_quad.vertices.buf,
		&m_sceneMeshes.m_quad.vertices.mem);

	// Setup indices
	std::vector<uint16_t> indexBuffer = { 0, 1, 2, 2, 3, 0 };

	createBuffer(
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		indexBuffer.size() * sizeof(uint16_t),
		indexBuffer.data(),
		&m_sceneMeshes.m_quad.indices.buf,
		&m_sceneMeshes.m_quad.indices.mem);

}


void VulkanRaytracer::loadMeshes()
{
	loadMesh(getAssetPath() + "models/armor/armor.dae", &m_sceneMeshes.m_model, vertexLayout, 1.0f);
}


// ===========================================================================================
//
// COMPUTE (for raytracing)
//
// ===========================================================================================


void VulkanRaytracer::buildRaytracingCommandBuffer() {

	if (m_compute.commandBuffer == VK_NULL_HANDLE)
	{
		m_compute.commandBuffer = VulkanRenderer::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
	}

	// Create a semaphore used to synchronize offscreen rendering and usage
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkUtils::initializers::semaphoreCreateInfo();
	VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_compute.semaphore));

	VkCommandBufferBeginInfo cmdBufInfo = vkUtils::initializers::commandBufferBeginInfo();

	VK_CHECK_RESULT(vkBeginCommandBuffer(m_compute.commandBuffer, &cmdBufInfo));

	// Record binding to the compute pipeline
	vkCmdBindPipeline(m_compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelines.m_compute);

	// Bind descriptor sets
	vkCmdBindDescriptorSets(m_compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_compute.pipelineLayout, 0, 1, &m_descriptorSets.m_compute, 0, nullptr);

	vkCmdDispatch(m_compute.commandBuffer, m_compute.storageRaytraceImage.width / 16, m_compute.storageRaytraceImage.height / 16, 1);

	VK_CHECK_RESULT(vkEndCommandBuffer(m_compute.commandBuffer));
}
