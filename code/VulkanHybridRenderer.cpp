// Adapted from Sascha Willems Vulkan examples : https ://github.com/SaschaWillems/Vulkan

#include "VulkanHybridRenderer.h"

namespace
{

	// Vertex layout for this example
	static std::vector<vkMeshLoader::VertexLayout> vertexLayout =
	{
		vkMeshLoader::VERTEX_LAYOUT_POSITION,
		vkMeshLoader::VERTEX_LAYOUT_UV,
		vkMeshLoader::VERTEX_LAYOUT_COLOR,
		vkMeshLoader::VERTEX_LAYOUT_NORMAL,
		vkMeshLoader::VERTEX_LAYOUT_TANGENT,
		vkMeshLoader::VERTEX_LAYOUT_MATERIALID_NORMALIZED,
	};
}



VulkanHybridRenderer::VulkanHybridRenderer(const std::string& fileName) : VulkanRenderer(fileName)
, m_offScreenCmdBuffer(VK_NULL_HANDLE)
, m_offscreenSemaphore(VK_NULL_HANDLE)
{
	m_appName = "Hybrid Renderer";
}

VulkanHybridRenderer::~VulkanHybridRenderer()
{}

void VulkanHybridRenderer::draw(SRendererContext& context)
{
	// The scene render command buffer has to wait for the offscreen
	// rendering to be finished before we can use the framebuffer 
	// color image for sampling during final rendering
	// To ensure this we use a dedicated offscreen synchronization
	// semaphore that will be signaled when offscreen renderin
	// has been finished
	// This is necessary as an implementation may start both
	// command buffers at the same time, there is no guarantee
	// that command buffers will be executed in the order they
	// have been submitted by the application

	// =====  Offscreen rendering

	// Wait for swap chain presentation to finish
	m_submitInfo.pWaitSemaphores = &m_semaphores.m_presentComplete;
	// Signal ready with offscreen semaphore
	m_submitInfo.pSignalSemaphores = &m_offscreenSemaphore;

	// Submit work
	m_submitInfo.commandBufferCount = 1;
	m_submitInfo.pCommandBuffers = &m_offScreenCmdBuffer;
	VK_CHECK_RESULT(vkQueueSubmit(m_queue, 1, &m_submitInfo, VK_NULL_HANDLE));

	// ==== Submit rendering out for final pass

	// Wait for offscreen semaphore
	m_submitInfo.pWaitSemaphores = &m_offscreenSemaphore;
	// Signal ready with render complete semaphpre
	m_submitInfo.pSignalSemaphores = &m_semaphores.m_renderComplete;

	m_submitInfo.pCommandBuffers = &m_drawCmdBuffers[m_currentBuffer];
	VK_CHECK_RESULT(vkQueueSubmit(m_queue, 1, &m_submitInfo, VK_NULL_HANDLE));
	updateUniformBufferDeferredLights(context);

	// ===== Raytracing

	//// Wait for offscreen semaphore
	//m_submitInfo.pWaitSemaphores = &m_offscreenSemaphore;
	//// Signal ready with render complete semaphpre
	//m_submitInfo.pSignalSemaphores = &m_semaphores.m_renderComplete;

	// Submit compute commands
	// Use a fence to ensure that compute command buffer has finished executing before using it again
	vkWaitForFences(m_device, 1, &m_compute.fence, VK_TRUE, UINT64_MAX);
	vkResetFences(m_device, 1, &m_compute.fence);

	VkSubmitInfo computeSubmitInfo = vkUtils::initializers::submitInfo();
	computeSubmitInfo.commandBufferCount = 1;
	computeSubmitInfo.pCommandBuffers = &m_compute.commandBuffer;

	VK_CHECK_RESULT(vkQueueSubmit(m_compute.queue, 1, &computeSubmitInfo, m_compute.fence));
	updateUniformBufferRaytracing(context);
}

void VulkanHybridRenderer::shutdownVulkan()
{
	// Flush device to make sure all resources can be freed 
	vkDeviceWaitIdle(m_device);

	// Clean up used Vulkan resources 
	// Note : Inherited destructor cleans up resources stored in base class

	vkDestroySampler(m_device, m_colorSampler, nullptr);

	// Frame buffer

	// Color attachments
	vkDestroyImageView(m_device, m_offScreenFrameBuf.position.view, nullptr);
	vkDestroyImage(m_device, m_offScreenFrameBuf.position.image, nullptr);
	vkFreeMemory(m_device, m_offScreenFrameBuf.position.mem, nullptr);

	vkDestroyImageView(m_device, m_offScreenFrameBuf.normal.view, nullptr);
	vkDestroyImage(m_device, m_offScreenFrameBuf.normal.image, nullptr);
	vkFreeMemory(m_device, m_offScreenFrameBuf.normal.mem, nullptr);

	vkDestroyImageView(m_device, m_offScreenFrameBuf.albedo.view, nullptr);
	vkDestroyImage(m_device, m_offScreenFrameBuf.albedo.image, nullptr);
	vkFreeMemory(m_device, m_offScreenFrameBuf.albedo.mem, nullptr);

	// Depth attachment
	vkDestroyImageView(m_device, m_offScreenFrameBuf.depth.view, nullptr);
	vkDestroyImage(m_device, m_offScreenFrameBuf.depth.image, nullptr);
	vkFreeMemory(m_device, m_offScreenFrameBuf.depth.mem, nullptr);

	vkDestroyFramebuffer(m_device, m_offScreenFrameBuf.frameBuffer, nullptr);

	vkDestroyPipeline(m_device, m_pipelines.m_onscreen, nullptr);
	vkDestroyPipeline(m_device, m_pipelines.m_offscreen, nullptr);
	vkDestroyPipeline(m_device, m_pipelines.m_debug, nullptr);
	vkDestroyPipeline(m_device, m_pipelines.m_raytrace, nullptr);

	vkDestroyPipelineLayout(m_device, m_pipelineLayouts.m_onscreen, nullptr);
	vkDestroyPipelineLayout(m_device, m_pipelineLayouts.m_offscreen, nullptr);
	vkDestroyPipelineLayout(m_device, m_pipelineLayouts.m_debug, nullptr);
	vkDestroyPipelineLayout(m_device, m_pipelineLayouts.m_raytrace, nullptr);

	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.m_onscreen, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.m_offscreen, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.m_debug, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.m_raytrace, nullptr);

	// Meshes
	VulkanMeshLoader::destroyBuffers(m_device, &m_sceneMeshes.m_model.meshBuffer);
	//VulkanMeshLoader::destroyBuffers(m_device, &m_sceneMeshes.m_floor.meshBuffer);
	VulkanMeshLoader::destroyBuffers(m_device, &m_sceneMeshes.m_quad);
	//VulkanMeshLoader::destroyBuffers(m_device, &m_sceneMeshes.m_transparentObj.meshBuffer);

	// Uniform buffers
	vkUtils::destroyUniformData(m_device, &m_uniformData.m_vsOffscreen);
	vkUtils::destroyUniformData(m_device, &m_uniformData.m_vsFullScreen);
	vkUtils::destroyUniformData(m_device, &m_uniformData.m_fsLights);
	vkUtils::destroyUniformData(m_device, &m_compute.m_buffers.materials);
	vkUtils::destroyUniformData(m_device, &m_compute.m_buffers.ubo);

	
	vkDestroyBuffer(m_device, m_compute.m_buffers.indicesAndMaterialIDs.buffer, nullptr);
	vkDestroyBuffer(m_device, m_compute.m_buffers.positions.buffer, nullptr);
	vkDestroyBuffer(m_device, m_compute.m_buffers.normals.buffer, nullptr);
	vkDestroyBuffer(m_device, m_compute.m_buffers.bvhAabbNodes.buffer, nullptr);

	vkFreeMemory(m_device, m_compute.m_buffers.indicesAndMaterialIDs.memory, nullptr);
	vkFreeMemory(m_device, m_compute.m_buffers.positions.memory, nullptr);
	vkFreeMemory(m_device, m_compute.m_buffers.normals.memory, nullptr);
	vkFreeMemory(m_device, m_compute.m_buffers.bvhAabbNodes.memory, nullptr);
	vkFreeMemory(m_device, m_compute.m_storageRaytraceImage.deviceMemory, nullptr);

	vkDestroyImage(m_device, m_compute.m_storageRaytraceImage.image, nullptr);
	vkDestroyImageView(m_device, m_compute.m_storageRaytraceImage.view, nullptr);
	vkDestroySampler(m_device, m_compute.m_storageRaytraceImage.sampler, nullptr);
	vkDestroyFence(m_device, m_compute.fence, nullptr);

	vkFreeCommandBuffers(m_device, m_cmdPool, 1, &m_offScreenCmdBuffer);

	vkDestroyRenderPass(m_device, m_offScreenFrameBuf.renderPass, nullptr);

	m_textureLoader->destroyTexture(m_modelTex.m_colorMap);
	m_textureLoader->destroyTexture(m_modelTex.m_normalMap);
	//m_textureLoader->destroyTexture(m_floorTex.m_colorMap);
	//m_textureLoader->destroyTexture(m_floorTex.m_normalMap);

	vkDestroySemaphore(m_device, m_offscreenSemaphore, nullptr);

	VulkanRenderer::shutdownVulkan();
}

// Create a frame buffer attachment
void VulkanHybridRenderer::createAttachment(
	VkFormat format,
	VkImageUsageFlagBits usage,
	SFrameBufferAttachment *attachment,
	VkCommandBuffer layoutCmd)
{
	VkImageAspectFlags aspectMask = 0;
	VkImageLayout imageLayout;

	attachment->format = format;

	if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
	{
		aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}
	if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
	{
		aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}

	assert(aspectMask > 0);

	VkImageCreateInfo image = vkUtils::initializers::imageCreateInfo();
	image.imageType = VK_IMAGE_TYPE_2D;
	image.format = format;
	image.extent.width = m_offScreenFrameBuf.width;
	image.extent.height = m_offScreenFrameBuf.height;
	image.extent.depth = 1;
	image.mipLevels = 1;
	image.arrayLayers = 1;
	image.samples = VK_SAMPLE_COUNT_1_BIT;
	image.tiling = VK_IMAGE_TILING_OPTIMAL;
	image.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT;

	VkMemoryAllocateInfo memAlloc = vkUtils::initializers::memoryAllocateInfo();
	VkMemoryRequirements memReqs;

	VK_CHECK_RESULT(vkCreateImage(m_device, &image, nullptr, &attachment->image));
	vkGetImageMemoryRequirements(m_device, attachment->image, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = m_vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(m_device, &memAlloc, nullptr, &attachment->mem));
	VK_CHECK_RESULT(vkBindImageMemory(m_device, attachment->image, attachment->mem, 0));

	VkImageViewCreateInfo imageView = vkUtils::initializers::imageViewCreateInfo();
	imageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageView.format = format;
	imageView.subresourceRange = {};
	imageView.subresourceRange.aspectMask = aspectMask;
	imageView.subresourceRange.baseMipLevel = 0;
	imageView.subresourceRange.levelCount = 1;
	imageView.subresourceRange.baseArrayLayer = 0;
	imageView.subresourceRange.layerCount = 1;
	imageView.image = attachment->image;
	VK_CHECK_RESULT(vkCreateImageView(m_device, &imageView, nullptr, &attachment->view));
}

void VulkanHybridRenderer::setupOnscreenPipeline() {

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

	shaderStages[0] = loadShader(getAssetPath() + "shaders/hybrid/hybrid.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(getAssetPath() + "shaders/hybrid/hybrid.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	VkGraphicsPipelineCreateInfo pipelineCreateInfo =
		vkUtils::initializers::pipelineCreateInfo(
		m_pipelineLayouts.m_onscreen,
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

	VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCreateInfo, nullptr, &m_pipelines.m_onscreen));
}

void VulkanHybridRenderer::setupDeferredPipeline() {
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

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

	// Debug display pipeline
	shaderStages[0] = loadShader(getAssetPath() + "shaders/hybrid/debug.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(getAssetPath() + "shaders/hybrid/debug.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	VkGraphicsPipelineCreateInfo pipelineCreateInfo =
		vkUtils::initializers::pipelineCreateInfo(
		m_pipelineLayouts.m_debug,
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

	VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCreateInfo, nullptr, &m_pipelines.m_debug));

	// Offscreen pipeline
	shaderStages[0] = loadShader(getAssetPath() + "shaders/hybrid/mrt.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(getAssetPath() + "shaders/hybrid/mrt.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	// Separate render pass
	pipelineCreateInfo.renderPass = m_offScreenFrameBuf.renderPass;

	// Separate layout
	pipelineCreateInfo.layout = m_pipelineLayouts.m_offscreen;

	// Blend attachment states required for all color attachments
	// This is important, as color write mask will otherwise be 0x0 and you
	// won't see anything rendered to the attachment
	std::array<VkPipelineColorBlendAttachmentState, 3> blendAttachmentStates = {
		vkUtils::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
		vkUtils::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
		vkUtils::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE)
	};

	colorBlendState.attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size());
	colorBlendState.pAttachments = blendAttachmentStates.data();

	VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCreateInfo, nullptr, &m_pipelines.m_offscreen));
}

// Prepare a new framebuffer for offscreen rendering
// The contents of this framebuffer are then
// blitted to our render target
void VulkanHybridRenderer::setupFrameBuffer() // prepareOffscreenFramebuffer
{
	VulkanRenderer::setupFrameBuffer();

	VkCommandBuffer layoutCmd = VulkanRenderer::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	m_offScreenFrameBuf.width = FB_DIM;
	m_offScreenFrameBuf.height = FB_DIM;

	// Color attachments

	// (World space) Positions
	createAttachment(
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		&m_offScreenFrameBuf.position,
		layoutCmd);

	// (World space) Normals
	createAttachment(
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		&m_offScreenFrameBuf.normal,
		layoutCmd);

	// Albedo (color)
	createAttachment(
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		&m_offScreenFrameBuf.albedo,
		layoutCmd);

	// Depth attachment

	// Find a suitable depth format
	VkFormat attDepthFormat;
	VkBool32 validDepthFormat = vkUtils::getSupportedDepthFormat(m_physicalDevice, &attDepthFormat);
	assert(validDepthFormat);

	createAttachment(
		attDepthFormat,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		&m_offScreenFrameBuf.depth,
		layoutCmd);

	VulkanRenderer::flushCommandBuffer(layoutCmd, m_queue, true);

	// Set up separate renderpass with references
	// to the color and depth attachments

	std::array<VkAttachmentDescription, 4> attachmentDescs = {};

	// Init attachment properties
	for (uint32_t i = 0; i < 4; ++i)
	{
		attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
		attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		if (i == 3)
		{
			attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		else
		{
			attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
	}

	// Formats
	attachmentDescs[0].format = m_offScreenFrameBuf.position.format;
	attachmentDescs[1].format = m_offScreenFrameBuf.normal.format;
	attachmentDescs[2].format = m_offScreenFrameBuf.albedo.format;
	attachmentDescs[3].format = m_offScreenFrameBuf.depth.format;

	std::vector<VkAttachmentReference> colorReferences;
	colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
	colorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
	colorReferences.push_back({ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 3;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.pColorAttachments = colorReferences.data();
	subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
	subpass.pDepthStencilAttachment = &depthReference;

	// Use subpass dependencies for attachment layput transitions
	std::array<VkSubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.pAttachments = attachmentDescs.data();
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 2;
	renderPassInfo.pDependencies = dependencies.data();

	VK_CHECK_RESULT(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_offScreenFrameBuf.renderPass));

	std::array<VkImageView, 4> attachments;
	attachments[0] = m_offScreenFrameBuf.position.view;
	attachments[1] = m_offScreenFrameBuf.normal.view;
	attachments[2] = m_offScreenFrameBuf.albedo.view;
	attachments[3] = m_offScreenFrameBuf.depth.view;

	VkFramebufferCreateInfo fbufCreateInfo = {};
	fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbufCreateInfo.pNext = NULL;
	fbufCreateInfo.renderPass = m_offScreenFrameBuf.renderPass;
	fbufCreateInfo.pAttachments = attachments.data();
	fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	fbufCreateInfo.width = m_offScreenFrameBuf.width;
	fbufCreateInfo.height = m_offScreenFrameBuf.height;
	fbufCreateInfo.layers = 1;
	VK_CHECK_RESULT(vkCreateFramebuffer(m_device, &fbufCreateInfo, nullptr, &m_offScreenFrameBuf.frameBuffer));

	// Create sampler to sample from the color attachments
	VkSamplerCreateInfo sampler = vkUtils::initializers::samplerCreateInfo();
	sampler.magFilter = VK_FILTER_NEAREST;
	sampler.minFilter = VK_FILTER_NEAREST;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.addressModeV = sampler.addressModeU;
	sampler.addressModeW = sampler.addressModeU;
	sampler.mipLodBias = 0.0f;
	sampler.maxAnisotropy = 0;
	sampler.minLod = 0.0f;
	sampler.maxLod = 1.0f;
	sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK_RESULT(vkCreateSampler(m_device, &sampler, nullptr, &m_colorSampler));
}

void VulkanHybridRenderer::setupRaytracingPipeline() {
	VkComputePipelineCreateInfo computePipelineCreateInfo =
		vkUtils::initializers::computePipelineCreateInfo(
		m_pipelineLayouts.m_raytrace,
		0);

	VkPipelineShaderStageCreateInfo shader;

	// Create shader modules from bytecodes
	shader = loadShader(getAssetPath() + "shaders/hybrid/raytrace.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
	computePipelineCreateInfo.stage = shader;

	VK_CHECK_RESULT(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_pipelines.m_raytrace));

	VkFenceCreateInfo fenceCreateInfo = vkUtils::initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
	VK_CHECK_RESULT(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_compute.fence));
}

// Build command buffer for rendering the scene to the offscreen frame buffer attachments
void VulkanHybridRenderer::buildDeferredCommandBuffer()
{
	if (m_offScreenCmdBuffer == VK_NULL_HANDLE)
	{
		m_offScreenCmdBuffer = VulkanRenderer::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
	}

	// Create a semaphore used to synchronize offscreen rendering and usage
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkUtils::initializers::semaphoreCreateInfo();
	VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_offscreenSemaphore));

	VkCommandBufferBeginInfo cmdBufInfo = vkUtils::initializers::commandBufferBeginInfo();

	// Clear values for all attachments written in the fragment sahder
	std::array<VkClearValue, 4> clearValues;
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[3].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = vkUtils::initializers::renderPassBeginInfo();
	renderPassBeginInfo.renderPass = m_offScreenFrameBuf.renderPass;
	renderPassBeginInfo.framebuffer = m_offScreenFrameBuf.frameBuffer;
	renderPassBeginInfo.renderArea.extent.width = m_offScreenFrameBuf.width;
	renderPassBeginInfo.renderArea.extent.height = m_offScreenFrameBuf.height;
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassBeginInfo.pClearValues = clearValues.data();

	VK_CHECK_RESULT(vkBeginCommandBuffer(m_offScreenCmdBuffer, &cmdBufInfo));

	vkCmdBeginRenderPass(m_offScreenCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport = vkUtils::initializers::viewport((float)m_offScreenFrameBuf.width, (float)m_offScreenFrameBuf.height, 0.0f, 1.0f);
	vkCmdSetViewport(m_offScreenCmdBuffer, 0, 1, &viewport);

	VkRect2D scissor = vkUtils::initializers::rect2D(m_offScreenFrameBuf.width, m_offScreenFrameBuf.height, 0, 0);
	vkCmdSetScissor(m_offScreenCmdBuffer, 0, 1, &scissor);

	vkCmdBindPipeline(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines.m_offscreen);

	VkDeviceSize offsets[1] = { 0 };

	//// Background
	//vkCmdBindDescriptorSets(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayouts.m_offscreen, 0, 1, &m_descriptorSets.m_model, 0, NULL);
	//vkCmdBindVertexBuffers(m_offScreenCmdBuffer, VERTEX_BUFFER_BIND_ID, 1, &m_sceneMeshes.m_floor.meshBuffer.vertices.buf, offsets);
	//vkCmdBindIndexBuffer(m_offScreenCmdBuffer, m_sceneMeshes.m_floor.meshBuffer.indices.buf, 0, VK_INDEX_TYPE_UINT32);
	//vkCmdDrawIndexed(m_offScreenCmdBuffer, m_sceneMeshes.m_floor.meshBuffer.indexCount, 3, 0, 0, 0);

	// Object
	vkCmdBindDescriptorSets(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayouts.m_offscreen, 0, 1, &m_descriptorSets.m_model, 0, NULL);
	vkCmdBindVertexBuffers(m_offScreenCmdBuffer, VERTEX_BUFFER_BIND_ID, 1, &m_sceneMeshes.m_model.meshBuffer.vertices.buf, offsets);
	vkCmdBindIndexBuffer(m_offScreenCmdBuffer, m_sceneMeshes.m_model.meshBuffer.indices.buf, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(m_offScreenCmdBuffer, m_sceneMeshes.m_model.meshBuffer.indexCount, 1, 0, 0, 0);

	// Transparent Object
	//vkCmdBindDescriptorSets(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayouts.m_offscreen, 0, 1, &m_descriptorSets.m_model, 0, NULL);
	//vkCmdBindVertexBuffers(m_offScreenCmdBuffer, VERTEX_BUFFER_BIND_ID, 1, &m_sceneMeshes.m_transparentObj.meshBuffer.vertices.buf, offsets);
	//vkCmdBindIndexBuffer(m_offScreenCmdBuffer, m_sceneMeshes.m_transparentObj.meshBuffer.indices.buf, 0, VK_INDEX_TYPE_UINT32);
	//vkCmdDrawIndexed(m_offScreenCmdBuffer, m_sceneMeshes.m_transparentObj.meshBuffer.indexCount, 1, 0, 0, 0);

	vkCmdEndRenderPass(m_offScreenCmdBuffer);

	VK_CHECK_RESULT(vkEndCommandBuffer(m_offScreenCmdBuffer));
}

void VulkanHybridRenderer::buildRaytracingCommandBuffer() {
	
	vkGetDeviceQueue(m_device, m_vulkanDevice->queueFamilyIndices.compute, 0, &m_compute.queue);

	m_compute.commandBuffer = VulkanRenderer::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

	VkCommandBufferBeginInfo cmdBufInfo = vkUtils::initializers::commandBufferBeginInfo();

	VK_CHECK_RESULT(vkBeginCommandBuffer(m_compute.commandBuffer, &cmdBufInfo));

	// Record binding to the compute pipeline
	vkCmdBindPipeline(m_compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelines.m_raytrace);

	// Bind descriptor sets
	vkCmdBindDescriptorSets(m_compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayouts.m_raytrace, 0, 1, &m_descriptorSets.m_raytrace, 0, nullptr);

	vkCmdDispatch(m_compute.commandBuffer, m_compute.m_storageRaytraceImage.width / 16, m_compute.m_storageRaytraceImage.height / 16, 1);

	VK_CHECK_RESULT(vkEndCommandBuffer(m_compute.commandBuffer));
}

void VulkanHybridRenderer::loadTextures()
{
	m_textureLoader->loadTexture(getAssetPath() + "textures/pattern_35_bc3.ktx", VK_FORMAT_BC3_UNORM_BLOCK, &m_modelTex.m_colorMap);
	m_textureLoader->loadTexture(getAssetPath() + "textures/pattern_57_normal_bc3.ktx", VK_FORMAT_BC3_UNORM_BLOCK, &m_modelTex.m_normalMap);

	//m_textureLoader->loadTexture(getAssetPath() + "textures/pattern_35_bc3.ktx", VK_FORMAT_BC3_UNORM_BLOCK, &m_floorTex.m_colorMap);
	//m_textureLoader->loadTexture(getAssetPath() + "textures/pattern_35_normalmap_bc3.ktx", VK_FORMAT_BC3_UNORM_BLOCK, &m_floorTex.m_normalMap);
}

void VulkanHybridRenderer::reBuildCommandBuffers()
{
	if (!checkCommandBuffers())
	{
		destroyCommandBuffers();
		createCommandBuffers();
	}
	buildCommandBuffers();
}

void VulkanHybridRenderer::reBuildRaytracingCommandBuffers() {
	// Rebuild compute command buffers
	vkFreeCommandBuffers(m_device, m_cmdPool, 1, &m_compute.commandBuffer);
	m_compute.commandBuffer = VulkanRenderer::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
	buildRaytracingCommandBuffer();
}

void VulkanHybridRenderer::prepareTextureTarget(vkUtils::VulkanTexture* tex, uint32_t width, uint32_t height, VkFormat format) {
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

void VulkanHybridRenderer::buildCommandBuffers()
{
	VkCommandBufferBeginInfo cmdBufInfo = vkUtils::initializers::commandBufferBeginInfo();

	VkClearValue clearValues[2];
	clearValues[0].color = { { 0.0f, 0.0f, 0.1f, 0.0f } };
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
		imageMemoryBarrier.image = m_compute.m_storageRaytraceImage.image;
		imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
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

		vkCmdBeginRenderPass(m_drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport = vkUtils::initializers::viewport((float)m_windowWidth, (float)m_windowHeight, 0.0f, 1.0f);
		vkCmdSetViewport(m_drawCmdBuffers[i], 0, 1, &viewport);

		VkRect2D scissor = vkUtils::initializers::rect2D(m_windowWidth, m_windowHeight, 0, 0);
		vkCmdSetScissor(m_drawCmdBuffers[i], 0, 1, &scissor);

		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindDescriptorSets(m_drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayouts.m_debug, 0, 1, &m_descriptorSets.m_debug, 0, NULL);

		if (m_debugDisplay)
		{
			vkCmdBindPipeline(m_drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines.m_debug);
			vkCmdBindVertexBuffers(m_drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &m_sceneMeshes.m_quad.vertices.buf, offsets);
			vkCmdBindIndexBuffer(m_drawCmdBuffers[i], m_sceneMeshes.m_quad.indices.buf, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(m_drawCmdBuffers[i], m_sceneMeshes.m_quad.indexCount, 1, 0, 0, 1);
			// Move viewport to display final composition in lower right corner
			viewport.x = viewport.width * 0.5f;
			viewport.y = viewport.height * 0.5f;
			vkCmdSetViewport(m_drawCmdBuffers[i], 0, 1, &viewport);
		}

		// Final composition as full screen quad
		vkCmdBindPipeline(m_drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines.m_onscreen);

		vkCmdBindDescriptorSets(m_drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayouts.m_onscreen, 0, 1, &m_descriptorSets.m_onscreen, 0, NULL);


		vkCmdDraw(m_drawCmdBuffers[i], 3, 1, 0, 0);

		vkCmdEndRenderPass(m_drawCmdBuffers[i]);

		VK_CHECK_RESULT(vkEndCommandBuffer(m_drawCmdBuffers[i]));
	}

	buildDeferredCommandBuffer();
	buildRaytracingCommandBuffer();
}

void VulkanHybridRenderer::loadMeshes()
{
	//{
	//	vkMeshLoader::MeshCreateInfo meshCreateInfo;
	//	meshCreateInfo.m_scale = glm::vec3(2.0f);
	//	meshCreateInfo.m_uvscale = glm::vec2(4.0f);
	//	meshCreateInfo.m_pos = glm::vec3(0.0f, 1.5f, 0.0f);
	//	loadMesh(getAssetPath() + "models/plane.obj", &m_sceneMeshes.m_floor.meshBuffer, &m_sceneMeshes.m_floor.meshAttributes, vertexLayout, &meshCreateInfo);
	//}

	{
		vkMeshLoader::MeshCreateInfo meshCreateInfo;

		loadMesh(getAssetPath() + m_fileName, &m_sceneMeshes.m_model.meshBuffer, &m_sceneMeshes.m_model.meshAttributes, vertexLayout, &meshCreateInfo, &m_bvhTree);
		std::cout << "Number of vertices: " << m_sceneMeshes.m_model.meshAttributes.m_verticePositions.size() << std::endl;
		std::cout << "Number of triangles: " << m_sceneMeshes.m_model.meshAttributes.m_verticePositions.size() / 3 << std::endl;
	}

	//{
	//	vkMeshLoader::MeshCreateInfo meshCreateInfo;
	//	meshCreateInfo.m_scale = glm::vec3(0.3f);
	//	meshCreateInfo.m_rotAxisAndAngle = glm::vec4(1, 0, 0, 0);
	//	meshCreateInfo.m_pos = glm::vec3(0.0f, 0.f, 0.0f);

	//	loadMesh(getAssetPath() + "models/gltfs/cornell/cornell.dae", &m_sceneMeshes.m_transparentObj.meshBuffer, &m_sceneMeshes.m_transparentObj.meshAttributes, vertexLayout, &meshCreateInfo);
	//}


	SMaterial temp;
	temp.m_colorDiffuse = glm::vec4(1, 1, 0, 1);
	m_sceneMeshes.m_model.meshAttributes.m_materials.push_back(temp);

	// === Binding description
	m_vertices.m_bindingDescriptions.resize(1);
	m_vertices.m_bindingDescriptions[0] =
		vkUtils::initializers::vertexInputBindingDescription(
		VERTEX_BUFFER_BIND_ID,
		vkMeshLoader::vertexSize(vertexLayout),
		VK_VERTEX_INPUT_RATE_VERTEX);

	// === Attribute descriptions
	m_vertices.m_attributeDescriptions.resize(vertexLayout.size());
	// Location 0: Position
	m_vertices.m_attributeDescriptions[0] =
		vkUtils::initializers::vertexInputAttributeDescription(
		VERTEX_BUFFER_BIND_ID,
		0,
		VK_FORMAT_R32G32B32_SFLOAT,
		0);
	// Location 1: Texture coordinates
	m_vertices.m_attributeDescriptions[1] =
		vkUtils::initializers::vertexInputAttributeDescription(
		VERTEX_BUFFER_BIND_ID,
		1,
		VK_FORMAT_R32G32_SFLOAT,
		sizeof(float) * 3);
	// Location 2: Color
	m_vertices.m_attributeDescriptions[2] =
		vkUtils::initializers::vertexInputAttributeDescription(
		VERTEX_BUFFER_BIND_ID,
		2,
		VK_FORMAT_R32G32B32_SFLOAT,
		sizeof(float) * 5);
	// Location 3: Normal
	m_vertices.m_attributeDescriptions[3] =
		vkUtils::initializers::vertexInputAttributeDescription(
		VERTEX_BUFFER_BIND_ID,
		3,
		VK_FORMAT_R32G32B32_SFLOAT,
		sizeof(float) * 8);
	// Location 4: Tangent
	m_vertices.m_attributeDescriptions[4] =
		vkUtils::initializers::vertexInputAttributeDescription(
		VERTEX_BUFFER_BIND_ID,
		4,
		VK_FORMAT_R32G32B32_SFLOAT,
		sizeof(float) * 11);
	// Location 5: Normalized material ID
	m_vertices.m_attributeDescriptions[5] =
		vkUtils::initializers::vertexInputAttributeDescription(
		VERTEX_BUFFER_BIND_ID,
		5,
		VK_FORMAT_R32_SFLOAT,
		sizeof(float) * 14);

	m_vertices.m_inputState = vkUtils::initializers::pipelineVertexInputStateCreateInfo();
	m_vertices.m_inputState.vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertices.m_bindingDescriptions.size());
	m_vertices.m_inputState.pVertexBindingDescriptions = m_vertices.m_bindingDescriptions.data();
	m_vertices.m_inputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertices.m_attributeDescriptions.size());
	m_vertices.m_inputState.pVertexAttributeDescriptions = m_vertices.m_attributeDescriptions.data();

	// ==== @todo Parse meshes into triangle soups from glTF

	// Get a queue from the device for copy operation
	vkGetDeviceQueue(m_device, m_vulkanDevice->queueFamilyIndices.compute, 0, &m_compute.queue);

	std::vector<glm::ivec4> indices = {
		glm::ivec4(0, 1, 2, 0)
	};

	vk::Buffer stagingBuffer;

	// --  Index buffer
	VkDeviceSize bufferSize = m_sceneMeshes.m_model.meshAttributes.m_indices.size() * sizeof(glm::ivec4);
	//bufferSize = 100 * sizeof(glm::ivec4);


	createBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		bufferSize,
		m_sceneMeshes.m_model.meshAttributes.m_indices.data(),
		&stagingBuffer.buffer,
		&stagingBuffer.memory,
		&stagingBuffer.descriptor);

	createBuffer(
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		bufferSize,
		nullptr,
		&m_compute.m_buffers.indicesAndMaterialIDs.buffer,
		&m_compute.m_buffers.indicesAndMaterialIDs.memory,
		&m_compute.m_buffers.indicesAndMaterialIDs.descriptor);

	// Copy to staging buffer
	VkCommandBuffer copyCmd = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	VkBufferCopy copyRegion = {};
	copyRegion.size = bufferSize;
	vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer, m_compute.m_buffers.indicesAndMaterialIDs.buffer, 1, &copyRegion);
	flushCommandBuffer(copyCmd, m_compute.queue, true);

	vkDestroyBuffer(m_device, stagingBuffer.buffer, nullptr);
	vkFreeMemory(m_device, stagingBuffer.memory, nullptr);


	// --  Positions buffer
	std::vector<glm::vec4> positions = {
		glm::vec4(-1.0f, 0.0f, 0.0f, 1.0f),
		glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
		glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)
	};
	bufferSize = m_sceneMeshes.m_model.meshAttributes.m_verticePositions.size() * sizeof(glm::vec4);

	createBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		bufferSize,
		m_sceneMeshes.m_model.meshAttributes.m_verticePositions.data(),
		&stagingBuffer.buffer,
		&stagingBuffer.memory,
		&stagingBuffer.descriptor);

	createBuffer(
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		bufferSize,
		nullptr,
		&m_compute.m_buffers.positions.buffer,
		&m_compute.m_buffers.positions.memory,
		&m_compute.m_buffers.positions.descriptor);

	// Copy to staging buffer
	VkCommandBuffer copyPositionsCmd = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	copyRegion = {};
	copyRegion.size = bufferSize;
	vkCmdCopyBuffer(copyPositionsCmd, stagingBuffer.buffer, m_compute.m_buffers.positions.buffer, 1, &copyRegion);
	flushCommandBuffer(copyPositionsCmd, m_compute.queue, true);

	vkDestroyBuffer(m_device, stagingBuffer.buffer, nullptr);
	vkFreeMemory(m_device, stagingBuffer.memory, nullptr);


	// --  Normals buffer
	std::vector<glm::vec4> normals = {
		glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
		glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
		glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)
	};

	bufferSize = m_sceneMeshes.m_model.meshAttributes.m_verticeNormals.size() * sizeof(glm::vec4);

	createBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		bufferSize,
		m_sceneMeshes.m_model.meshAttributes.m_verticeNormals.data(),
		&stagingBuffer.buffer,
		&stagingBuffer.memory,
		&stagingBuffer.descriptor);

	createBuffer(
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		bufferSize,
		nullptr,
		&m_compute.m_buffers.normals.buffer,
		&m_compute.m_buffers.normals.memory,
		&m_compute.m_buffers.normals.descriptor);

	// Copy to staging buffer
	VkCommandBuffer copyNormalsCmd = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	copyRegion = {};
	copyRegion.size = bufferSize;
	vkCmdCopyBuffer(copyNormalsCmd, stagingBuffer.buffer, m_compute.m_buffers.normals.buffer, 1, &copyRegion);
	flushCommandBuffer(copyNormalsCmd, m_compute.queue, true);

	vkDestroyBuffer(m_device, stagingBuffer.buffer, nullptr);
	vkFreeMemory(m_device, stagingBuffer.memory, nullptr);


	// --  BVH AABBs
	bufferSize = m_bvhTree.m_aabbNodes.size() * sizeof(BVHTree::BVHNode);

	createBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		bufferSize,
		m_bvhTree.m_aabbNodes.data(),
		&stagingBuffer.buffer,
		&stagingBuffer.memory,
		&stagingBuffer.descriptor);

	createBuffer(
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		bufferSize,
		nullptr,
		&m_compute.m_buffers.bvhAabbNodes.buffer,
		&m_compute.m_buffers.bvhAabbNodes.memory,
		&m_compute.m_buffers.bvhAabbNodes.descriptor);

	// Copy to staging buffer
	VkCommandBuffer copyAabbNodesCmd = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	copyRegion = {};
	copyRegion.size = bufferSize;
	vkCmdCopyBuffer(copyAabbNodesCmd, stagingBuffer.buffer, m_compute.m_buffers.bvhAabbNodes.buffer, 1, &copyRegion);
	flushCommandBuffer(copyAabbNodesCmd, m_compute.queue, true);

	vkDestroyBuffer(m_device, stagingBuffer.buffer, nullptr);
	vkFreeMemory(m_device, stagingBuffer.memory, nullptr);
}

void VulkanHybridRenderer::generateQuads()
{
	// Setup vertices for multiple screen aligned quads
	// Used for displaying final result and debug 
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
	for (uint32_t i = 0; i < 3; i++)
	{
		// Last component of normal is used for debug display sampler index
		vertexBuffer.push_back({ { x + 1.0f, y + 1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, (float)i } });
		vertexBuffer.push_back({ { x, y + 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, (float)i } });
		vertexBuffer.push_back({ { x, y, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, (float)i } });
		vertexBuffer.push_back({ { x + 1.0f, y, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, (float)i } });
		x += 1.0f;
		if (x > 1.0f)
		{
			x = 0.0f;
			y += 1.0f;
		}
	}

	createBuffer(
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		vertexBuffer.size() * sizeof(Vertex),
		vertexBuffer.data(),
		&m_sceneMeshes.m_quad.vertices.buf,
		&m_sceneMeshes.m_quad.vertices.mem);

	// Setup indices
	std::vector<uint32_t> indexBuffer = { 0, 1, 2, 2, 3, 0 };
	for (uint32_t i = 0; i < 3; ++i)
	{
		uint32_t indices[6] = { 0, 1, 2, 2, 3, 0 };
		for (auto index : indices)
		{
			indexBuffer.push_back(i * 4 + index);
		}
	}
	m_sceneMeshes.m_quad.indexCount = static_cast<uint32_t>(indexBuffer.size());

	createBuffer(
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		indexBuffer.size() * sizeof(uint32_t),
		indexBuffer.data(),
		&m_sceneMeshes.m_quad.indices.buf,
		&m_sceneMeshes.m_quad.indices.mem);
}

void VulkanHybridRenderer::setupDescriptorFramework()
{
	// Set-up descriptor pool
	std::vector<VkDescriptorPoolSize> poolSizes =
	{
		vkUtils::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10),
		vkUtils::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 15), // Model + floor + out texture
		vkUtils::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1),
		vkUtils::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10),
	};

	VkDescriptorPoolCreateInfo descriptorPoolInfo =
		vkUtils::initializers::descriptorPoolCreateInfo(
		static_cast<uint32_t>(poolSizes.size()),
		poolSizes.data(),
		5);

	VK_CHECK_RESULT(vkCreateDescriptorPool(m_device, &descriptorPoolInfo, nullptr, &m_descriptorPool));

	// Set-up descriptor set layout
	// === Deferred shading layout
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
	{
		// Binding 0 : Vertex shader uniform buffer
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_SHADER_STAGE_VERTEX_BIT,
		0),
		// Binding 1 : Position texture target / Scene colormap
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		1),
		// Binding 2 : Normals texture target
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		2),
		// Binding 3 : Albedo texture target
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		3)
	};

	VkDescriptorSetLayoutCreateInfo descriptorLayout =
		vkUtils::initializers::descriptorSetLayoutCreateInfo(
		setLayoutBindings.data(),
		static_cast<uint32_t>(setLayoutBindings.size()));

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &descriptorLayout, nullptr, &m_descriptorSetLayouts.m_offscreen));

	VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
		vkUtils::initializers::pipelineLayoutCreateInfo(
		&m_descriptorSetLayouts.m_offscreen,
		1);

	VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pPipelineLayoutCreateInfo, nullptr, &m_pipelineLayouts.m_offscreen));

	// === Debug set layout
	setLayoutBindings =
	{
		// Binding 0 : Vertex shader uniform buffer
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_SHADER_STAGE_VERTEX_BIT,
		0),
		// Binding 1 : Position texture target / Scene colormap
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		1),
		// Binding 2 : Normals texture target
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		2),
		// Binding 3 : Albedo texture target
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		3)
	};

	descriptorLayout =
		vkUtils::initializers::descriptorSetLayoutCreateInfo(
		setLayoutBindings.data(),
		static_cast<uint32_t>(setLayoutBindings.size()));

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &descriptorLayout, nullptr, &m_descriptorSetLayouts.m_debug));

	pPipelineLayoutCreateInfo =
		vkUtils::initializers::pipelineLayoutCreateInfo(
		&m_descriptorSetLayouts.m_debug,
		1);

	VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pPipelineLayoutCreateInfo, nullptr, &m_pipelineLayouts.m_debug));


	// === Onscreen set layout
	setLayoutBindings =
	{
		// Binding 0 : Color sampler
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		0),
	};

	descriptorLayout =
		vkUtils::initializers::descriptorSetLayoutCreateInfo(
		setLayoutBindings.data(),
		static_cast<uint32_t>(setLayoutBindings.size()));

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &descriptorLayout, nullptr, &m_descriptorSetLayouts.m_onscreen));

	pPipelineLayoutCreateInfo =
		vkUtils::initializers::pipelineLayoutCreateInfo(
		&m_descriptorSetLayouts.m_onscreen,
		1);

	VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pPipelineLayoutCreateInfo, nullptr, &m_pipelineLayouts.m_onscreen));

	// === Compute for raytracing
	setLayoutBindings =
	{
		// Binding 0 :  Position storage image 
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_SHADER_STAGE_COMPUTE_BIT,
		0),
		// Binding 1 : Normal storage image
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_SHADER_STAGE_COMPUTE_BIT,
		1),
		// Binding 2 : materialIDs storage image
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_SHADER_STAGE_COMPUTE_BIT,
		2),
		// Binding 3 : result of raytracing image
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		VK_SHADER_STAGE_COMPUTE_BIT,
		3),
		// Binding 4 : triangle indices + materialID of index
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		VK_SHADER_STAGE_COMPUTE_BIT,
		4),
		// Binding 5 : triangle positions
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		VK_SHADER_STAGE_COMPUTE_BIT,
		5),
		// Binding 6 : triangle normals
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		VK_SHADER_STAGE_COMPUTE_BIT,
		6),
		// Binding 7 : ubo
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_SHADER_STAGE_COMPUTE_BIT,
		7),
		// Binding 8 : materials
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_SHADER_STAGE_COMPUTE_BIT,
		8),
		// Binding 9 : bvhAabbNodes buffer
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		VK_SHADER_STAGE_COMPUTE_BIT,
		9),
	};

	descriptorLayout =
		vkUtils::initializers::descriptorSetLayoutCreateInfo(
		setLayoutBindings.data(),
		static_cast<uint32_t>(setLayoutBindings.size()));

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &descriptorLayout, nullptr, &m_descriptorSetLayouts.m_raytrace));

	pPipelineLayoutCreateInfo =
		vkUtils::initializers::pipelineLayoutCreateInfo(
		&m_descriptorSetLayouts.m_raytrace,
		1);

	VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pPipelineLayoutCreateInfo, nullptr, &m_pipelineLayouts.m_raytrace));

}

void VulkanHybridRenderer::setupDescriptors()
{
	loadTextures();

	std::vector<VkWriteDescriptorSet> writeDescriptorSets;

	// === Textured quad descriptor set
	// Image descriptors for the offscreen color attachments
	VkDescriptorImageInfo texDescriptorPosition =
		vkUtils::initializers::descriptorImageInfo(
		m_colorSampler,
		m_offScreenFrameBuf.position.view,
		VK_IMAGE_LAYOUT_GENERAL);

	VkDescriptorImageInfo texDescriptorNormal =
		vkUtils::initializers::descriptorImageInfo(
		m_colorSampler,
		m_offScreenFrameBuf.normal.view,
		VK_IMAGE_LAYOUT_GENERAL);

	VkDescriptorImageInfo texDescriptorAlbedo =
		vkUtils::initializers::descriptorImageInfo(
		m_colorSampler,
		m_offScreenFrameBuf.albedo.view,
		VK_IMAGE_LAYOUT_GENERAL);

	// === Debug descriptors
	VkDescriptorSetAllocateInfo allocInfo =
		vkUtils::initializers::descriptorSetAllocateInfo(
		m_descriptorPool,
		&m_descriptorSetLayouts.m_debug,
		1);

	VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSets.m_debug));

	writeDescriptorSets = {
		// Binding 0 : Vertex shader uniform buffer
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_debug,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		0,
		&m_uniformData.m_vsFullScreen.descriptor),
		// Binding 1 : Position texture target
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_debug,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1,
		&texDescriptorPosition),
		// Binding 2 : Normals texture target
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_debug,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		2,
		&texDescriptorNormal),
		// Binding 3 : Albedo texture target
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_debug,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		3,
		&texDescriptorAlbedo)
	};

	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

	// === Offscreen (scene)
	allocInfo =
		vkUtils::initializers::descriptorSetAllocateInfo(
		m_descriptorPool,
		&m_descriptorSetLayouts.m_offscreen,
		1);

	// Model
	VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSets.m_model));
	writeDescriptorSets =
	{
		// Binding 0: Vertex shader uniform buffer
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_model,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		0,
		&m_uniformData.m_vsOffscreen.descriptor),
		// Binding 1: Color map
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_model,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1,
		&m_modelTex.m_colorMap.descriptor),
		// Binding 2: Normal map
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_model,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		2,
		&m_modelTex.m_normalMap.descriptor)
	};
	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

	//// Floor
	//VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSets.m_floor));
	//writeDescriptorSets =
	//{
	//	// Binding 0: Vertex shader uniform buffer
	//	vkUtils::initializers::writeDescriptorSet(
	//	m_descriptorSets.m_floor,
	//	VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	//	0,
	//	&m_uniformData.m_vsOffscreen.descriptor),
	//	// Binding 1: Color map
	//	vkUtils::initializers::writeDescriptorSet(
	//	m_descriptorSets.m_floor,
	//	VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	//	1,
	//	&m_floorTex.m_colorMap.descriptor),
	//	// Binding 2: Normal map
	//	vkUtils::initializers::writeDescriptorSet(
	//	m_descriptorSets.m_floor,
	//	VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	//	2,
	//	&m_floorTex.m_normalMap.descriptor)
	//};
	//vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

	// === On screen
	allocInfo =
		vkUtils::initializers::descriptorSetAllocateInfo(
		m_descriptorPool,
		&m_descriptorSetLayouts.m_onscreen,
		1);

	VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSets.m_onscreen));
	
	writeDescriptorSets =
	{
		// Binding 0: Fragment shader color sampler for output
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_onscreen,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		0,
		&m_compute.m_storageRaytraceImage.descriptor),
	};
	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

	// === Compute descriptor set for ray tracing
	allocInfo =
		vkUtils::initializers::descriptorSetAllocateInfo(
		m_descriptorPool,
		&m_descriptorSetLayouts.m_raytrace,
		1);

	VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSets.m_raytrace));

	writeDescriptorSets = {
		// Binding 0 : Positions storage image
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_raytrace,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		0,
		&texDescriptorPosition
		),
		// Binding 1 : Normals storage image
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_raytrace,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1,
		&texDescriptorNormal
		),
		// Binding 2 : MaterialIDs storage image
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_raytrace,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		2,
		&texDescriptorAlbedo // @todo: change this to material IDs instead
		),
		// Binding 3 : Result of raytracing storage image
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_raytrace,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		3,
		&m_compute.m_storageRaytraceImage.descriptor
		),
		// Binding 4 : Index buffer
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_raytrace,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		4,
		&m_compute.m_buffers.indicesAndMaterialIDs.descriptor
		),
		// Binding 5 : Position buffer
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_raytrace,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		5,
		&m_compute.m_buffers.positions.descriptor
		),
		// Binding 6 : Normal buffer
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_raytrace,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		6,
		&m_compute.m_buffers.normals.descriptor
		),
		// Binding 7 : UBO
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_raytrace,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		7,
		&m_compute.m_buffers.ubo.descriptor
		),
		// Binding 8 : Materials buffer
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_raytrace,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		8,
		&m_compute.m_buffers.materials.descriptor
		),
		// Binding 9 : bvhAabbNodes buffer
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_raytrace,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		9,
		&m_compute.m_buffers.bvhAabbNodes.descriptor
		)
	};

	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
}

void VulkanHybridRenderer::setupPipelines()
{
	VulkanRenderer::setupPipelines();

	setupDeferredPipeline();
	setupOnscreenPipeline();
	setupRaytracingPipeline();
}

// Prepare and initialize uniform buffer containing shader uniforms
void VulkanHybridRenderer::setupUniformBuffers(SRendererContext& context)
{
	// Setup target compute texture
	prepareTextureTarget(&m_compute.m_storageRaytraceImage, TEX_DIM, TEX_DIM, VK_FORMAT_R8G8B8A8_UNORM);
	loadMeshes();
	generateQuads();

	// Fullscreen vertex shader
	createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		sizeof(m_uboVS),
		nullptr,
		&m_uniformData.m_vsFullScreen.buffer,
		&m_uniformData.m_vsFullScreen.memory,
		&m_uniformData.m_vsFullScreen.descriptor);

	// Deferred vertex shader
	createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		sizeof(m_uboOffscreenVS),
		nullptr,
		&m_uniformData.m_vsOffscreen.buffer,
		&m_uniformData.m_vsOffscreen.memory,
		&m_uniformData.m_vsOffscreen.descriptor);

	// Deferred fragment shader
	createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		sizeof(m_uboFragmentLights),
		nullptr,
		&m_uniformData.m_fsLights.buffer,
		&m_uniformData.m_fsLights.memory,
		&m_uniformData.m_fsLights.descriptor);

	// Init some values
	m_uboOffscreenVS.m_instancePos[0] = glm::vec4(0.0f);
	//m_uboOffscreenVS.m_instancePos[1] = glm::vec4(-11.0f, -1.f, -4.f, 1.f);
	//m_uboOffscreenVS.m_instancePos[2] = glm::vec4(-20.0f, 2.f, 0.f, 1.f);

	// ====== COMPUTE UBO
	VkDeviceSize bufferSize = sizeof(m_compute.ubo);
	createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		bufferSize,
		&m_compute.ubo,
		&m_compute.m_buffers.ubo.buffer,
		&m_compute.m_buffers.ubo.memory,
		&m_compute.m_buffers.ubo.descriptor);

	// ====== MATERIALS
	bufferSize = sizeof(SMaterial) * m_sceneMeshes.m_model.meshAttributes.m_materials.size();
	createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		bufferSize,
		m_sceneMeshes.m_model.meshAttributes.m_materials.data(),
		&m_compute.m_buffers.materials.buffer,
		&m_compute.m_buffers.materials.memory,
		&m_compute.m_buffers.materials.descriptor);

	// Update
	updateUniformBuffersScreen();
	updateUniformBufferDeferredMatrices(context);
	updateUniformBufferDeferredLights(context);
	updateUniformBufferRaytracing(context);
}

void VulkanHybridRenderer::updateUniformBuffersScreen()
{
	if (m_debugDisplay)
	{
		m_uboVS.m_projection = glm::ortho(0.0f, 2.0f, 0.0f, 2.0f, -1.0f, 1.0f);
	}
	else
	{
		m_uboVS.m_projection = glm::ortho(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);
	}
	m_uboVS.m_model = glm::mat4();

	uint8_t *pData;
	VK_CHECK_RESULT(vkMapMemory(m_device, m_uniformData.m_vsFullScreen.memory, 0, sizeof(m_uboVS), 0, (void **)&pData));
	memcpy(pData, &m_uboVS, sizeof(m_uboVS));
	vkUnmapMemory(m_device, m_uniformData.m_vsFullScreen.memory);
}

void VulkanHybridRenderer::updateUniformBufferDeferredMatrices(SRendererContext& context)
{
	m_uboOffscreenVS.m_model = glm::mat4();
	m_uboOffscreenVS.m_projection = context.m_camera.m_matrices.m_projMtx;
	m_uboOffscreenVS.m_view = context.m_camera.m_matrices.m_viewMtx;

	uint8_t *pData;
	VK_CHECK_RESULT(vkMapMemory(m_device, m_uniformData.m_vsOffscreen.memory, 0, sizeof(m_uboOffscreenVS), 0, (void **)&pData));
	memcpy(pData, &m_uboOffscreenVS, sizeof(m_uboOffscreenVS));
	vkUnmapMemory(m_device, m_uniformData.m_vsOffscreen.memory);
}

// Update fragment shader light position uniform block
void VulkanHybridRenderer::updateUniformBufferDeferredLights(SRendererContext& context)
{
	static float timer = 0.0f;
	timer += 0.005f;
	float SPEED = 36.0f;

	// White
	m_uboFragmentLights.m_lights[0].position = glm::vec4(0.0f, -2.0f, 0.0f, 1.0f);
	m_uboFragmentLights.m_lights[0].color = glm::vec3(0.8f, 0.8f, 0.7f);
	m_uboFragmentLights.m_lights[0].radius = 15.0f;
	// Red
	m_uboFragmentLights.m_lights[1].position = glm::vec4(-2.0f, -6.0f, 0.0f, 0.0f);
	m_uboFragmentLights.m_lights[1].color = glm::vec3(0.6f, 0.2f, 0.2f);
	m_uboFragmentLights.m_lights[1].radius = 10.0f;
	// Blue
	m_uboFragmentLights.m_lights[2].position = glm::vec4(2.0f, 0.0f, 0.0f, 0.0f);
	m_uboFragmentLights.m_lights[2].color = glm::vec3(0.0f, 0.0f, 2.5f);
	m_uboFragmentLights.m_lights[2].radius = 5.0f;
	//// Yellow
	m_uboFragmentLights.m_lights[3].position = glm::vec4(0.0f, 0.9f, 0.5f, 0.0f);
	m_uboFragmentLights.m_lights[3].color = glm::vec3(1.0f, 1.0f, 0.0f);
	m_uboFragmentLights.m_lights[3].radius = 2.0f;
	// Green
	m_uboFragmentLights.m_lights[4].position = glm::vec4(0.0f, 0.5f, 0.0f, 0.0f);
	m_uboFragmentLights.m_lights[4].color = glm::vec3(0.0f, 1.0f, 0.2f);
	m_uboFragmentLights.m_lights[4].radius = 5.0f;
	// Yellow
	m_uboFragmentLights.m_lights[5].position = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
	m_uboFragmentLights.m_lights[5].color = glm::vec3(1.0f, 0.7f, 0.3f);
	m_uboFragmentLights.m_lights[5].radius = 25.0f;

	m_uboFragmentLights.m_lights[0].position.x = sin(glm::radians(SPEED * timer)) * 5.0f;
	m_uboFragmentLights.m_lights[0].position.z = cos(glm::radians(SPEED * timer)) * 5.0f;

	m_uboFragmentLights.m_lights[1].position.x = -4.0f + sin(glm::radians(SPEED * timer) + 45.0f) * 2.0f;
	m_uboFragmentLights.m_lights[1].position.z = 0.0f + cos(glm::radians(SPEED * timer) + 45.0f) * 2.0f;

	m_uboFragmentLights.m_lights[2].position.x = 4.0f + sin(glm::radians(SPEED * timer)) * 2.0f;
	m_uboFragmentLights.m_lights[2].position.z = 0.0f + cos(glm::radians(SPEED * timer)) * 2.0f;

	m_uboFragmentLights.m_lights[4].position.x = 0.0f + sin(glm::radians(SPEED * timer + 90.0f)) * 5.0f;
	m_uboFragmentLights.m_lights[4].position.z = 0.0f - cos(glm::radians(SPEED * timer + 45.0f)) * 5.0f;

	m_uboFragmentLights.m_lights[5].position.x = 0.0f + sin(glm::radians(-SPEED * timer + 135.0f)) * 10.0f;
	m_uboFragmentLights.m_lights[5].position.z = 0.0f - cos(glm::radians(-SPEED * timer - 45.0f)) * 10.0f;

	// Current view position
	m_uboFragmentLights.m_viewPos = glm::vec4(context.m_camera.m_position, 0.0f) * glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f);

	uint8_t *pData;
	VK_CHECK_RESULT(vkMapMemory(m_device, m_uniformData.m_fsLights.memory, 0, sizeof(m_uboFragmentLights), 0, (void **)&pData));
	memcpy(pData, &m_uboFragmentLights, sizeof(m_uboFragmentLights));
	vkUnmapMemory(m_device, m_uniformData.m_fsLights.memory);
}

void VulkanHybridRenderer::updateUniformBufferRaytracing(SRendererContext& context) {
	
	m_compute.ubo.m_cameraPosition = glm::vec4(context.m_camera.m_position, 1);
	for (int i = 0; i < 6; ++i) {
		m_compute.ubo.m_lights[i] = m_uboFragmentLights.m_lights[i];
	}
	m_compute.ubo.m_lightCount = 1 + m_addLight;
	m_compute.ubo.m_materialCount = m_sceneMeshes.m_model.meshAttributes.m_materials.size();

	// Update user flags
	m_compute.ubo.m_isBVH = context.m_enableBVH;
	m_compute.ubo.m_isShadows = context.m_enableShadows;
	m_compute.ubo.m_isTransparency = context.m_enableTransparency;
	m_compute.ubo.m_isReflection = context.m_enableReflection;

	uint8_t *pData;
	VK_CHECK_RESULT(vkMapMemory(m_device, m_compute.m_buffers.ubo.memory, 0, sizeof(m_compute.ubo), 0, (void **)&pData));
	memcpy(pData, &m_compute.ubo, sizeof(m_compute.ubo));
	vkUnmapMemory(m_device, m_compute.m_buffers.ubo.memory);
}

void VulkanHybridRenderer::viewChanged(SRendererContext& context)
{
	updateUniformBufferDeferredMatrices(context);
}

void VulkanHybridRenderer::toggleDebugDisplay()
{
	VulkanRenderer::toggleDebugDisplay();
	reBuildCommandBuffers();
	updateUniformBuffersScreen();
}

void VulkanHybridRenderer::toggleBVH()
{
	VulkanRenderer::toggleBVH();
	reBuildRaytracingCommandBuffers();

	// Toggle bvh flag
	m_compute.ubo.m_isBVH = m_enableBVH;

	uint8_t *pData;
	VK_CHECK_RESULT(vkMapMemory(m_device, m_compute.m_buffers.ubo.memory, 0, sizeof(m_compute.ubo), 0, (void **)&pData));
	memcpy(pData, &m_compute.ubo, sizeof(m_compute.ubo));
	vkUnmapMemory(m_device, m_compute.m_buffers.ubo.memory);
}

void VulkanHybridRenderer::toggleShadows()
{
	VulkanRenderer::toggleShadows();
	reBuildRaytracingCommandBuffers();

	// Toggle flag
	m_compute.ubo.m_isShadows = m_enableShadows;

	uint8_t *pData;
	VK_CHECK_RESULT(vkMapMemory(m_device, m_compute.m_buffers.ubo.memory, 0, sizeof(m_compute.ubo), 0, (void **)&pData));
	memcpy(pData, &m_compute.ubo, sizeof(m_compute.ubo));
	vkUnmapMemory(m_device, m_compute.m_buffers.ubo.memory);
}

void VulkanHybridRenderer::toggleTransparency()
{
	VulkanRenderer::toggleTransparency();
	reBuildRaytracingCommandBuffers();

	// Toggle flag
	m_compute.ubo.m_isTransparency = m_enableTransparency;

	uint8_t *pData;
	VK_CHECK_RESULT(vkMapMemory(m_device, m_compute.m_buffers.ubo.memory, 0, sizeof(m_compute.ubo), 0, (void **)&pData));
	memcpy(pData, &m_compute.ubo, sizeof(m_compute.ubo));
	vkUnmapMemory(m_device, m_compute.m_buffers.ubo.memory);
}

void VulkanHybridRenderer::toggleReflection()
{
	VulkanRenderer::toggleReflection();
	reBuildRaytracingCommandBuffers();

	// Toggle flag
	m_compute.ubo.m_isReflection = m_enableReflection;

	uint8_t *pData;
	VK_CHECK_RESULT(vkMapMemory(m_device, m_compute.m_buffers.ubo.memory, 0, sizeof(m_compute.ubo), 0, (void **)&pData));
	memcpy(pData, &m_compute.ubo, sizeof(m_compute.ubo));
	vkUnmapMemory(m_device, m_compute.m_buffers.ubo.memory);
}

void VulkanHybridRenderer::addLight() {
	VulkanRenderer::addLight();
	reBuildRaytracingCommandBuffers();

	m_compute.ubo.m_lightCount += m_addLight;

	uint8_t *pData;
	VK_CHECK_RESULT(vkMapMemory(m_device, m_compute.m_buffers.ubo.memory, 0, sizeof(m_compute.ubo), 0, (void **)&pData));
	memcpy(pData, &m_compute.ubo, sizeof(m_compute.ubo));
	vkUnmapMemory(m_device, m_compute.m_buffers.ubo.memory);
}
