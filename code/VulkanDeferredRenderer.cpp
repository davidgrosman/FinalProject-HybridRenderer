/******************************************************************************/
/*!
\file	VulkanDeferredRenderer.cpp
\author David Grosman
\par    email: ToDavidGrosman\@gmail.com
\par    Project: CIS 565: GPU Programming and Architecture - Final Project.
\date   11/18/2016
\brief

Compiled using Microsoft (R) C/C++ Optimizing Compiler Version 18.00.21005.1 for
x86 which is my default VS2013 compiler.

This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)

*/
/******************************************************************************/

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



VulkanDeferredRenderer::VulkanDeferredRenderer() : VulkanRenderer()
	, m_offScreenCmdBuffer(VK_NULL_HANDLE)
	, m_offscreenSemaphore(VK_NULL_HANDLE)
{
	m_appName = "Deferred Renderer";
}

VulkanDeferredRenderer::~VulkanDeferredRenderer()
{}

void VulkanDeferredRenderer::draw(SRendererContext& context)
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

	// Offscreen rendering

	// Wait for swap chain presentation to finish
	m_submitInfo.pWaitSemaphores = &m_semaphores.m_presentComplete;
	// Signal ready with offscreen semaphore
	m_submitInfo.pSignalSemaphores = &m_offscreenSemaphore;

	// Submit work
	m_submitInfo.commandBufferCount = 1;
	m_submitInfo.pCommandBuffers = &m_offScreenCmdBuffer;
	VK_CHECK_RESULT(vkQueueSubmit(m_queue, 1, &m_submitInfo, VK_NULL_HANDLE));

	// Scene rendering

	// Wait for offscreen semaphore
	m_submitInfo.pWaitSemaphores = &m_offscreenSemaphore;
	// Signal ready with render complete semaphpre
	m_submitInfo.pSignalSemaphores = &m_semaphores.m_renderComplete;

	// Submit work
	m_submitInfo.pCommandBuffers = &m_drawCmdBuffers[m_currentBuffer];
	VK_CHECK_RESULT(vkQueueSubmit(m_queue, 1, &m_submitInfo, VK_NULL_HANDLE));
	updateUniformBufferDeferredLights(context);
}

void VulkanDeferredRenderer::shutdownVulkan()
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

	vkDestroyPipeline(m_device, m_pipelines.m_deferred, nullptr);
	vkDestroyPipeline(m_device, m_pipelines.m_offscreen, nullptr);
	vkDestroyPipeline(m_device, m_pipelines.m_debug, nullptr);

	vkDestroyPipelineLayout(m_device, m_pipelineLayouts.m_deferred, nullptr);
	vkDestroyPipelineLayout(m_device, m_pipelineLayouts.m_offscreen, nullptr);

	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);

	// Meshes
	VulkanMeshLoader::destroyBuffers(m_device, &m_sceneMeshes.m_model);
	VulkanMeshLoader::destroyBuffers(m_device, &m_sceneMeshes.m_floor);
	VulkanMeshLoader::destroyBuffers(m_device, &m_sceneMeshes.m_quad);

	// Uniform buffers
	vkUtils::destroyUniformData(m_device, &m_uniformData.m_vsOffscreen);
	vkUtils::destroyUniformData(m_device, &m_uniformData.m_vsFullScreen);
	vkUtils::destroyUniformData(m_device, &m_uniformData.m_fsLights);

	vkFreeCommandBuffers(m_device, m_cmdPool, 1, &m_offScreenCmdBuffer);

	vkDestroyRenderPass(m_device, m_offScreenFrameBuf.renderPass, nullptr);

	m_textureLoader->destroyTexture(m_modelTex.m_colorMap);
	m_textureLoader->destroyTexture(m_modelTex.m_normalMap);
	m_textureLoader->destroyTexture(m_floorTex.m_colorMap);
	m_textureLoader->destroyTexture(m_floorTex.m_normalMap);

	vkDestroySemaphore(m_device, m_offscreenSemaphore, nullptr);

	VulkanRenderer::shutdownVulkan();
}

// Create a frame buffer attachment
void VulkanDeferredRenderer::createAttachment(
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

// Prepare a new framebuffer for offscreen rendering
// The contents of this framebuffer are then
// blitted to our render target
void VulkanDeferredRenderer::setupFrameBuffer() // prepareOffscreenFramebuffer
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

// Build command buffer for rendering the scene to the offscreen frame buffer attachments
void VulkanDeferredRenderer::buildDeferredCommandBuffer()
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

	// Background
	vkCmdBindDescriptorSets(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayouts.m_offscreen, 0, 1, &m_descriptorSets.m_floor, 0, NULL);
	vkCmdBindVertexBuffers(m_offScreenCmdBuffer, VERTEX_BUFFER_BIND_ID, 1, &m_sceneMeshes.m_floor.vertices.buf, offsets);
	vkCmdBindIndexBuffer(m_offScreenCmdBuffer, m_sceneMeshes.m_floor.indices.buf, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(m_offScreenCmdBuffer, m_sceneMeshes.m_floor.indexCount, 1, 0, 0, 0);

	// Object
	vkCmdBindDescriptorSets(m_offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayouts.m_offscreen, 0, 1, &m_descriptorSets.m_model, 0, NULL);
	vkCmdBindVertexBuffers(m_offScreenCmdBuffer, VERTEX_BUFFER_BIND_ID, 1, &m_sceneMeshes.m_model.vertices.buf, offsets);
	vkCmdBindIndexBuffer(m_offScreenCmdBuffer, m_sceneMeshes.m_model.indices.buf, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(m_offScreenCmdBuffer, m_sceneMeshes.m_model.indexCount, 3, 0, 0, 0);

	vkCmdEndRenderPass(m_offScreenCmdBuffer);

	VK_CHECK_RESULT(vkEndCommandBuffer(m_offScreenCmdBuffer));
}

void VulkanDeferredRenderer::loadTextures()
{
	m_textureLoader->loadTexture(getAssetPath() + "models/armor/colormap.ktx", VK_FORMAT_BC3_UNORM_BLOCK, &m_modelTex.m_colorMap);
	m_textureLoader->loadTexture(getAssetPath() + "models/armor/normalmap.ktx", VK_FORMAT_BC3_UNORM_BLOCK, &m_modelTex.m_normalMap);

	m_textureLoader->loadTexture(getAssetPath() + "textures/pattern_35_bc3.ktx", VK_FORMAT_BC3_UNORM_BLOCK, &m_floorTex.m_colorMap);
	m_textureLoader->loadTexture(getAssetPath() + "textures/pattern_35_normalmap_bc3.ktx", VK_FORMAT_BC3_UNORM_BLOCK, &m_floorTex.m_normalMap);
}

void VulkanDeferredRenderer::reBuildCommandBuffers()
{
	if (!checkCommandBuffers())
	{
		destroyCommandBuffers();
		createCommandBuffers();
	}
	buildCommandBuffers();
}

void VulkanDeferredRenderer::buildCommandBuffers()
{
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

		vkCmdBeginRenderPass(m_drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport = vkUtils::initializers::viewport((float)m_windowWidth, (float)m_windowHeight, 0.0f, 1.0f);
		vkCmdSetViewport(m_drawCmdBuffers[i], 0, 1, &viewport);

		VkRect2D scissor = vkUtils::initializers::rect2D(m_windowWidth, m_windowHeight, 0, 0);
		vkCmdSetScissor(m_drawCmdBuffers[i], 0, 1, &scissor);

		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindDescriptorSets(m_drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayouts.m_deferred, 0, 1, &m_descriptorSets.m_quad, 0, NULL);

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
		vkCmdBindPipeline(m_drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines.m_deferred);
		vkCmdBindVertexBuffers(m_drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &m_sceneMeshes.m_quad.vertices.buf, offsets);
		vkCmdBindIndexBuffer(m_drawCmdBuffers[i], m_sceneMeshes.m_quad.indices.buf, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(m_drawCmdBuffers[i], 6, 1, 0, 0, 1);

		vkCmdEndRenderPass(m_drawCmdBuffers[i]);

		VK_CHECK_RESULT(vkEndCommandBuffer(m_drawCmdBuffers[i]));
	}

	buildDeferredCommandBuffer();
}

void VulkanDeferredRenderer::loadMeshes()
{
	loadMesh(getAssetPath() + "models/armor/armor.dae", &m_sceneMeshes.m_model, vertexLayout, 1.0f);

	vkMeshLoader::MeshCreateInfo meshCreateInfo;
	meshCreateInfo.scale = glm::vec3(2.0f);
	meshCreateInfo.uvscale = glm::vec2(4.0f);
	meshCreateInfo.center = glm::vec3(0.0f, 2.35f, 0.0f);
	loadMesh(getAssetPath() + "models/plane.obj", &m_sceneMeshes.m_floor, vertexLayout, &meshCreateInfo);
}

void VulkanDeferredRenderer::generateQuads()
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

void VulkanDeferredRenderer::setupDescriptorFramework()
{
	// Set-up descriptor pool
	std::vector<VkDescriptorPoolSize> poolSizes =
	{
		vkUtils::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8),
		vkUtils::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 9)
	};

	VkDescriptorPoolCreateInfo descriptorPoolInfo =
		vkUtils::initializers::descriptorPoolCreateInfo(
		static_cast<uint32_t>(poolSizes.size()),
		poolSizes.data(),
		3);

	VK_CHECK_RESULT(vkCreateDescriptorPool(m_device, &descriptorPoolInfo, nullptr, &m_descriptorPool));

	// Set-up descriptor set layout
	// Deferred shading layout
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
		3),
		// Binding 4 : Fragment shader uniform buffer
		vkUtils::initializers::descriptorSetLayoutBinding(
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		4),
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

	VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pPipelineLayoutCreateInfo, nullptr, &m_pipelineLayouts.m_deferred));

	// Offscreen (scene) rendering pipeline layout
	VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pPipelineLayoutCreateInfo, nullptr, &m_pipelineLayouts.m_offscreen));
}

void VulkanDeferredRenderer::setupDescriptors()
{
	loadTextures();
	generateQuads();
	loadMeshes();

	std::vector<VkWriteDescriptorSet> writeDescriptorSets;

	// Textured quad descriptor set
	VkDescriptorSetAllocateInfo allocInfo =
		vkUtils::initializers::descriptorSetAllocateInfo(
		m_descriptorPool,
		&m_descriptorSetLayout,
		1);

	VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSets.m_quad));

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

	writeDescriptorSets = {
		// Binding 0 : Vertex shader uniform buffer
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_quad,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		0,
		&m_uniformData.m_vsFullScreen.descriptor),
		// Binding 1 : Position texture target
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_quad,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1,
		&texDescriptorPosition),
		// Binding 2 : Normals texture target
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_quad,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		2,
		&texDescriptorNormal),
		// Binding 3 : Albedo texture target
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_quad,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		3,
		&texDescriptorAlbedo),
		// Binding 4 : Fragment shader uniform buffer
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_quad,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		4,
		&m_uniformData.m_fsLights.descriptor),
	};

	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

	// Offscreen (scene)

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

	// Backbround
	VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSets.m_floor));
	writeDescriptorSets =
	{
		// Binding 0: Vertex shader uniform buffer
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_floor,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		0,
		&m_uniformData.m_vsOffscreen.descriptor),
		// Binding 1: Color map
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_floor,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1,
		&m_floorTex.m_colorMap.descriptor),
		// Binding 2: Normal map
		vkUtils::initializers::writeDescriptorSet(
		m_descriptorSets.m_floor,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		2,
		&m_floorTex.m_normalMap.descriptor)
	};
	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

	// Binding description
	m_vertices.m_bindingDescriptions.resize(1);
	m_vertices.m_bindingDescriptions[0] =
		vkUtils::initializers::vertexInputBindingDescription(
		VERTEX_BUFFER_BIND_ID,
		vkMeshLoader::vertexSize(vertexLayout),
		VK_VERTEX_INPUT_RATE_VERTEX);

	// Attribute descriptions
	m_vertices.m_attributeDescriptions.resize(5);
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
		sizeof(float)* 3);
	// Location 2: Color
	m_vertices.m_attributeDescriptions[2] =
		vkUtils::initializers::vertexInputAttributeDescription(
		VERTEX_BUFFER_BIND_ID,
		2,
		VK_FORMAT_R32G32B32_SFLOAT,
		sizeof(float)* 5);
	// Location 3: Normal
	m_vertices.m_attributeDescriptions[3] =
		vkUtils::initializers::vertexInputAttributeDescription(
		VERTEX_BUFFER_BIND_ID,
		3,
		VK_FORMAT_R32G32B32_SFLOAT,
		sizeof(float)* 8);
	// Location 4: Tangent
	m_vertices.m_attributeDescriptions[4] =
		vkUtils::initializers::vertexInputAttributeDescription(
		VERTEX_BUFFER_BIND_ID,
		4,
		VK_FORMAT_R32G32B32_SFLOAT,
		sizeof(float)* 11);

	m_vertices.m_inputState = vkUtils::initializers::pipelineVertexInputStateCreateInfo();
	m_vertices.m_inputState.vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertices.m_bindingDescriptions.size());
	m_vertices.m_inputState.pVertexBindingDescriptions = m_vertices.m_bindingDescriptions.data();
	m_vertices.m_inputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertices.m_attributeDescriptions.size());
	m_vertices.m_inputState.pVertexAttributeDescriptions = m_vertices.m_attributeDescriptions.data();
}

void VulkanDeferredRenderer::setupPipelines()
{
	VulkanRenderer::setupPipelines();

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

	shaderStages[0] = loadShader(getAssetPath() + "shaders/deferred/deferred.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(getAssetPath() + "shaders/deferred/deferred.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	VkGraphicsPipelineCreateInfo pipelineCreateInfo =
		vkUtils::initializers::pipelineCreateInfo(
		m_pipelineLayouts.m_deferred,
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

	VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCreateInfo, nullptr, &m_pipelines.m_deferred));

	// Debug display pipeline
	shaderStages[0] = loadShader(getAssetPath() + "shaders/deferred/debug.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(getAssetPath() + "shaders/deferred/debug.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &pipelineCreateInfo, nullptr, &m_pipelines.m_debug));

	// Offscreen pipeline
	shaderStages[0] = loadShader(getAssetPath() + "shaders/deferred/mrt.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(getAssetPath() + "shaders/deferred/mrt.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

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

// Prepare and initialize uniform buffer containing shader uniforms
void VulkanDeferredRenderer::setupUniformBuffers(SRendererContext& context)
{
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
	m_uboOffscreenVS.m_instancePos[1] = glm::vec4(-4.0f, 0.0, -4.0f, 0.0f);
	m_uboOffscreenVS.m_instancePos[2] = glm::vec4(4.0f, 0.0, -4.0f, 0.0f);

	// Update
	updateUniformBuffersScreen();
	updateUniformBufferDeferredMatrices(context);
	updateUniformBufferDeferredLights(context);
}

void VulkanDeferredRenderer::updateUniformBuffersScreen()
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

void VulkanDeferredRenderer::updateUniformBufferDeferredMatrices(SRendererContext& context)
{
	glm::vec3 modelRotation = glm::vec3(0,0,0);
	glm::vec3 modelPosition = glm::vec3(0,0,0);

	m_uboOffscreenVS.m_projection = glm::perspective(glm::radians(45.0f), (float)m_windowWidth / (float)m_windowHeight, 0.1f, 256.0f);

	m_uboOffscreenVS.m_model = glm::mat4();
	m_uboOffscreenVS.m_model = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.25f, 0.0f) + modelPosition);
	m_uboOffscreenVS.m_model = glm::rotate(m_uboOffscreenVS.m_model, glm::radians(modelRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	m_uboOffscreenVS.m_model = glm::rotate(m_uboOffscreenVS.m_model, glm::radians(modelRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	m_uboOffscreenVS.m_model = glm::rotate(m_uboOffscreenVS.m_model, glm::radians(modelRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	m_uboOffscreenVS.m_projection = context.m_camera.m_matrices.m_projMtx;
	m_uboOffscreenVS.m_view = context.m_camera.m_matrices.m_viewMtx;
	m_uboOffscreenVS.m_model = glm::mat4();

	uint8_t *pData;
	VK_CHECK_RESULT(vkMapMemory(m_device, m_uniformData.m_vsOffscreen.memory, 0, sizeof(m_uboOffscreenVS), 0, (void **)&pData));
	memcpy(pData, &m_uboOffscreenVS, sizeof(m_uboOffscreenVS));
	vkUnmapMemory(m_device, m_uniformData.m_vsOffscreen.memory);
}

// Update fragment shader light position uniform block
void VulkanDeferredRenderer::updateUniformBufferDeferredLights(SRendererContext& context)
{
	static float timer = 0.0f;
	timer += 0.005f;

	// White
	m_uboFragmentLights.m_lights[0].position = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
	m_uboFragmentLights.m_lights[0].color = glm::vec3(1.5f);
	m_uboFragmentLights.m_lights[0].radius = 15.0f * 0.25f;
	// Red
	m_uboFragmentLights.m_lights[1].position = glm::vec4(-2.0f, 0.0f, 0.0f, 0.0f);
	m_uboFragmentLights.m_lights[1].color = glm::vec3(1.0f, 0.0f, 0.0f);
	m_uboFragmentLights.m_lights[1].radius = 15.0f;
	// Blue
	m_uboFragmentLights.m_lights[2].position = glm::vec4(2.0f, 1.0f, 0.0f, 0.0f);
	m_uboFragmentLights.m_lights[2].color = glm::vec3(0.0f, 0.0f, 2.5f);
	m_uboFragmentLights.m_lights[2].radius = 5.0f;
	// Yellow
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

	m_uboFragmentLights.m_lights[0].position.x = sin(glm::radians(360.0f * timer)) * 5.0f;
	m_uboFragmentLights.m_lights[0].position.z = cos(glm::radians(360.0f * timer)) * 5.0f;

	m_uboFragmentLights.m_lights[1].position.x = -4.0f + sin(glm::radians(360.0f * timer) + 45.0f) * 2.0f;
	m_uboFragmentLights.m_lights[1].position.z = 0.0f + cos(glm::radians(360.0f * timer) + 45.0f) * 2.0f;

	m_uboFragmentLights.m_lights[2].position.x = 4.0f + sin(glm::radians(360.0f * timer)) * 2.0f;
	m_uboFragmentLights.m_lights[2].position.z = 0.0f + cos(glm::radians(360.0f * timer)) * 2.0f;

	m_uboFragmentLights.m_lights[4].position.x = 0.0f + sin(glm::radians(360.0f * timer + 90.0f)) * 5.0f;
	m_uboFragmentLights.m_lights[4].position.z = 0.0f - cos(glm::radians(360.0f * timer + 45.0f)) * 5.0f;

	m_uboFragmentLights.m_lights[5].position.x = 0.0f + sin(glm::radians(-360.0f * timer + 135.0f)) * 10.0f;
	m_uboFragmentLights.m_lights[5].position.z = 0.0f - cos(glm::radians(-360.0f * timer - 45.0f)) * 10.0f;

	// Current view position
	m_uboFragmentLights.m_viewPos = glm::vec4(context.m_camera.m_position, 0.0f) * glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f);

	uint8_t *pData;
	VK_CHECK_RESULT(vkMapMemory(m_device, m_uniformData.m_fsLights.memory, 0, sizeof(m_uboFragmentLights), 0, (void **)&pData));
	memcpy(pData, &m_uboFragmentLights, sizeof(m_uboFragmentLights));
	vkUnmapMemory(m_device, m_uniformData.m_fsLights.memory);
}

void VulkanDeferredRenderer::viewChanged(SRendererContext& context)
{
	updateUniformBufferDeferredMatrices(context);
}

void VulkanDeferredRenderer::toggleDebugDisplay()
{
	VulkanRenderer::toggleDebugDisplay();
	reBuildCommandBuffers();
	updateUniformBuffersScreen();
}

//void VulkanDeferredRenderer::keyPressed(uint32_t keyCode)
//{
//	switch (keyCode)
//	{
//	case KEY_F1:
//	case GAMEPAD_BUTTON_A:
//		toggleDebugDisplay();
//		updateTextOverlay();
//		break;
//	}
//}