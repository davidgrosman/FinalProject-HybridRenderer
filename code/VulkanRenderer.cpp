/******************************************************************************/
/*!
\file	VulkanRenderer.cpp
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

#include "VulkanUtilities.h"
#include "VulkanRenderer.h"

namespace
{
	static VkClearColorValue defaultClearColor = { { 0.025f, 0.025f, 0.025f, 1.0f } };
}

VkResult VulkanRenderer::createInstance(bool enableValidation)
{
	this->enableValidation = enableValidation;

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = m_appName.c_str();
	appInfo.pEngineName = m_appName.c_str();
	appInfo.apiVersion = VK_API_VERSION_1_0;

	std::vector<const char*> enabledExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

	// Enable surface extensions depending on os
	enabledExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pNext = NULL;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	if (enabledExtensions.size() > 0)
	{
		if (enableValidation)
		{
			enabledExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}
		instanceCreateInfo.enabledExtensionCount = (uint32_t)enabledExtensions.size();
		instanceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();
	}
	if (enableValidation)
	{
		instanceCreateInfo.enabledLayerCount = vkDebug::validationLayerCount;
		instanceCreateInfo.ppEnabledLayerNames = vkDebug::validationLayerNames;
	}
	return vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance);
}

std::string VulkanRenderer::getWindowTitle()
{
	std::string device(m_deviceProperties.deviceName);
	std::string windowTitle;
	windowTitle = m_appName + " - " + device;

	return windowTitle;
}

const std::string VulkanRenderer::getAssetPath()
{
	return "../data/";
}

bool VulkanRenderer::checkCommandBuffers()
{
	for (auto& cmdBuffer : m_drawCmdBuffers)
	{
		if (cmdBuffer == VK_NULL_HANDLE)
		{
			return false;
		}
	}
	return true;
}

void VulkanRenderer::createCommandBuffers()
{
	// Create one command buffer for each swap chain image and reuse for rendering
	m_drawCmdBuffers.resize(m_swapChain.imageCount);

	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
		vkUtils::initializers::commandBufferAllocateInfo(
		m_cmdPool,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		static_cast<uint32_t>(m_drawCmdBuffers.size()));

	VK_CHECK_RESULT(vkAllocateCommandBuffers(m_device, &cmdBufAllocateInfo, m_drawCmdBuffers.data()));
}

void VulkanRenderer::destroyCommandBuffers()
{
	vkFreeCommandBuffers(m_device, m_cmdPool, static_cast<uint32_t>(m_drawCmdBuffers.size()), m_drawCmdBuffers.data());
}

void VulkanRenderer::createSetupCommandBuffer()
{
	if (m_setupCmdBuffer != VK_NULL_HANDLE)
	{
		vkFreeCommandBuffers(m_device, m_cmdPool, 1, &m_setupCmdBuffer);
		m_setupCmdBuffer = VK_NULL_HANDLE; // todo : check if still necessary
	}

	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
		vkUtils::initializers::commandBufferAllocateInfo(
		m_cmdPool,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		1);

	VK_CHECK_RESULT(vkAllocateCommandBuffers(m_device, &cmdBufAllocateInfo, &m_setupCmdBuffer));

	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	VK_CHECK_RESULT(vkBeginCommandBuffer(m_setupCmdBuffer, &cmdBufInfo));
}

void VulkanRenderer::flushSetupCommandBuffer()
{
	if (m_setupCmdBuffer == VK_NULL_HANDLE)
		return;

	VK_CHECK_RESULT(vkEndCommandBuffer(m_setupCmdBuffer));

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_setupCmdBuffer;

	VK_CHECK_RESULT(vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CHECK_RESULT(vkQueueWaitIdle(m_queue));

	vkFreeCommandBuffers(m_device, m_cmdPool, 1, &m_setupCmdBuffer);
	m_setupCmdBuffer = VK_NULL_HANDLE;
}

VkCommandBuffer VulkanRenderer::createCommandBuffer(VkCommandBufferLevel level, bool begin)
{
	VkCommandBuffer cmdBuffer;

	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
		vkUtils::initializers::commandBufferAllocateInfo(
		m_cmdPool,
		level,
		1);

	VK_CHECK_RESULT(vkAllocateCommandBuffers(m_device, &cmdBufAllocateInfo, &cmdBuffer));

	// If requested, also start the new command buffer
	if (begin)
	{
		VkCommandBufferBeginInfo cmdBufInfo = vkUtils::initializers::commandBufferBeginInfo();
		VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
	}

	return cmdBuffer;
}

void VulkanRenderer::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free)
{
	if (commandBuffer == VK_NULL_HANDLE)
	{
		return;
	}

	VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CHECK_RESULT(vkQueueWaitIdle(queue));

	if (free)
	{
		vkFreeCommandBuffers(m_device, m_cmdPool, 1, &commandBuffer);
	}
}

void VulkanRenderer::setupPipelines()
{
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VK_CHECK_RESULT(vkCreatePipelineCache(m_device, &pipelineCacheCreateInfo, nullptr, &m_pipelineCache));
}

VkPipelineShaderStageCreateInfo VulkanRenderer::loadShader(std::string fileName, VkShaderStageFlagBits stage)
{
	VkPipelineShaderStageCreateInfo shaderStage = {};
	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.stage = stage;

	shaderStage.module = vkUtils::loadShader(fileName.c_str(), m_device, stage);

	shaderStage.pName = "main"; // todo : make param
	assert(shaderStage.module != NULL);
	m_shaderModules.push_back(shaderStage.module);
	return shaderStage;
}

VkBool32 VulkanRenderer::createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, void* data, VkBuffer* buffer, VkDeviceMemory* memory)
{
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo memAlloc = vkUtils::initializers::memoryAllocateInfo();
	VkBufferCreateInfo bufferCreateInfo = vkUtils::initializers::bufferCreateInfo(usageFlags, size);

	VK_CHECK_RESULT(vkCreateBuffer(m_device, &bufferCreateInfo, nullptr, buffer));

	vkGetBufferMemoryRequirements(m_device, *buffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = m_vulkanDevice->getMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags);
	VK_CHECK_RESULT(vkAllocateMemory(m_device, &memAlloc, nullptr, memory));
	if (data != nullptr)
	{
		void *mapped;
		VK_CHECK_RESULT(vkMapMemory(m_device, *memory, 0, size, 0, &mapped));
		memcpy(mapped, data, size);
		vkUnmapMemory(m_device, *memory);
	}
	VK_CHECK_RESULT(vkBindBufferMemory(m_device, *buffer, *memory, 0));

	return true;
}

VkBool32 VulkanRenderer::createBuffer(VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, void * data, VkBuffer * buffer, VkDeviceMemory * memory, VkDescriptorBufferInfo * descriptor)
{
	VkBool32 res = createBuffer(usage, memoryPropertyFlags, size, data, buffer, memory);
	if (res)
	{
		descriptor->offset = 0;
		descriptor->buffer = *buffer;
		descriptor->range = size;
		return true;
	}
	else
	{
		return false;
	}
}

VkBool32 VulkanRenderer::createBuffer(VkBufferUsageFlags usageFlags, VkDeviceSize size, void * data, VkBuffer *buffer, VkDeviceMemory *memory)
{
	return createBuffer(usageFlags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, size, data, buffer, memory);
}

VkBool32 VulkanRenderer::createBuffer(VkBufferUsageFlags usageFlags, VkDeviceSize size, void *data, VkBuffer *buffer, VkDeviceMemory *memory, VkDescriptorBufferInfo *descriptor)
{
	VkBool32 res = createBuffer(usageFlags, size, data, buffer, memory);
	if (res)
	{
		descriptor->offset = 0;
		descriptor->buffer = *buffer;
		descriptor->range = size;
		return true;
	}
	else
	{
		return false;
	}
}

void VulkanRenderer::loadMesh(std::string filename, vkMeshLoader::MeshBuffer * meshBuffer, std::vector<vkMeshLoader::VertexLayout> vertexLayout, vkMeshLoader::MeshCreateInfo *meshCreateInfo)
{
	VulkanMeshLoader *mesh = new VulkanMeshLoader();
	mesh->LoadMesh(filename);

	VkCommandBuffer copyCmd = VulkanRenderer::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

	mesh->createBuffers(
		m_vulkanDevice,
		meshBuffer,
		vertexLayout,
		meshCreateInfo,
		true,
		copyCmd,
		m_queue);

	vkFreeCommandBuffers(m_device, m_cmdPool, 1, &copyCmd);

	meshBuffer->dim = mesh->dim.size;

	delete(mesh);
}

void VulkanRenderer::loadMeshAsTriangleSoup(std::string filename, std::vector<glm::ivec4>& outIndices, std::vector<glm::vec4>& outPositions, std::vector<glm::vec4>& outNormals)
{
	VulkanMeshLoader *mesh = new VulkanMeshLoader();
	mesh->LoadMesh(filename);
	
	outIndices = mesh->m_indices;
	outPositions = mesh->m_verticePositions;
	outNormals = mesh->m_verticeNormals;
}

VulkanRenderer::VulkanRenderer()
: m_wasInitialized(false)
, m_debugDisplay(false)
{
}

VulkanRenderer::~VulkanRenderer()
{
}

void VulkanRenderer::initVulkan(SRendererContext& context, bool enableValidation)
{
	VkResult err;

	// Step 1a- Create Vulkan Instance:
	//	A Vulkan application starts by setting up the Vulkan API through a VkInstance.
	//	An instance is created by describing your application and any API extensions you will be using.
	{
		err = createInstance(enableValidation);
		if (err)
		{
			vkUtils::exitFatal("Could not create Vulkan instance : \n" + vkUtils::errorString(err), "Fatal error");
		}
	}

	// Step 1b- Set-up Validation layers:
	// Since Vulkan is designed for high performance and low driver overhead, it will include very limited error checking and debugging capabilities by default.
	// To compensate, Vulkan allows you to enable extensive checks through a feature known as validation layers.
	// Validation layers are pieces of code that can be inserted between the API and the graphics driver to do things like running extra checks on function parameters
	// and tracking memory management problems.
	if (enableValidation) // If requested, we enable the default validation layers for debugging.
	{
		// The report flags determine what type of messages for the layers will be displayed
		// For validating (debugging) an appplication the error and warning bits should suffice
		VkDebugReportFlagsEXT debugReportFlags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		// Additional flags include performance info, loader and layer debug messages, etc.
		vkDebug::initDebugCallback(m_instance, debugReportFlags, VK_NULL_HANDLE);
	}

	// Step 2 - Physical device selection:
	//	After creating the instance, we can now query for Vulkan supported hardware and select one or more
	//	VkPhysicalDevices to use for operations.
	{
		uint32_t gpuCount = 0;
		// Get number of available physical devices
		VK_CHECK_RESULT(vkEnumeratePhysicalDevices(m_instance, &gpuCount, nullptr));
		assert(gpuCount > 0);
		// Enumerate devices
		std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
		err = vkEnumeratePhysicalDevices(m_instance, &gpuCount, physicalDevices.data());
		if (err)
		{
			vkUtils::exitFatal("Could not enumerate phyiscal devices : \n" + vkUtils::errorString(err), "Fatal error");
		}

		// Note :
		// This example will always use the first physical device reported,
		// change the vector index if you have multiple Vulkan devices installed
		// and want to use another one
		m_physicalDevice = physicalDevices[0];
	}

	// Step 3 - Logical device and queue families
	//	After selecting the right hardware device to use, you need to create a VkDevice(logical device),
	//	where you describe more specifically which VkPhysicalDeviceFeatures you will be using, like
	//	multi-viewport rendering and 64 bit floats.You also need to specify which queue families you would
	//	like to use.Most operations performed with Vulkan, like draw commands and memory operations, are
	//	asynchronously executed by submitting them to a VkQueue.Queues are allocated from queue families,
	//	where each queue family supports a specific set of operations in its queues.
	{
		// Vulkan device creation
		// This is handled by a separate class that gets a logical device representation
		// and encapsulates functions related to a device
		m_vulkanDevice = new vk::VulkanDevice(m_physicalDevice);
		VK_CHECK_RESULT(m_vulkanDevice->createLogicalDevice(enabledFeatures));
		m_device = m_vulkanDevice->logicalDevice;

		// todo: remove
		// Store properties (including limits) and features of the phyiscal device
		// So examples can check against them and see if a feature is actually supported
		vkGetPhysicalDeviceProperties(m_physicalDevice, &m_deviceProperties);
		vkGetPhysicalDeviceFeatures(m_physicalDevice, &m_deviceFeatures);
		// Gather physical device memory properties
		vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_deviceMemoryProperties);

		// Get a graphics queue from the device
		vkGetDeviceQueue(m_device, m_vulkanDevice->queueFamilyIndices.graphics, 0, &m_queue);
	}

	// Step 4 - Window surface and swap chain:
	//	We need 3 components to render to a window : a window, a window surface(VkSurfaceKHR) and a swap chain(VkSwapChainKHR).
	//	Note the KHR postfix, which means that these objects are part of a Vulkan extension. The Vulkan API itself is platform
	//	agnostic while The surface is a cross - platform abstraction over windows to render to.
	//	The swap chain is a collection of render targets which ensures that the image that we're currently rendering to is not
	//	the one that is currently on the screen. Every time we want to draw a frame we have to ask the swap chain to provide us
	//	with an image to render to. When we've finished drawing a frame, the image is returned to the swap chain for it to be
	//	presented to the screen.
	//	Note: The number of render targets and conditions for presenting finished images to the screen depends on the present mode.
	{
		m_swapChain.connect(m_instance, m_physicalDevice, m_device);
		setupSwapChain(context.m_window);
	}

	// Step 5 - Command pools and command buffers
	//	As mentioned earlier, many of the operations in Vulkan that we want to execute, like drawing operations, need to be submitted to a queue.
	//	These operations first need to be recorded into a VkCommandBuffer before they can be submitted.
	{
		createCommandPool();
		createSetupCommandBuffer();
		flushSetupCommandBuffer();
		createCommandBuffers();

		// Recreate setup command buffer for derived class
		createSetupCommandBuffer();
	}

	// Step 6 - Image views and framebuffers
	//	To draw to an image acquired from the swap chain, we have to wrap it into a VkImageView and VkFramebuffer.
	//	An image view references a specific part of an image to be used, and a framebuffer references image views
	//	that are to be used for color, depth and stencil targets.
	{
		// Find a suitable depth format
		VkBool32 validDepthFormat = vkUtils::getSupportedDepthFormat(m_physicalDevice, &m_depthFormat);
		assert(validDepthFormat);
		setupDepthStencil();

		setupRenderPass();

		setupFrameBuffer();
	}

	// Step 7 - Uniform buffers
	//	Specify the buffer that contains the UBO data for the shader. We're going to copy new data to the uniform buffer every frame,
	//	so this time the staging buffer actually needs to stick around.
	{
		setupUniformBuffers(context);
	}

	// Create a simple texture loader class
	m_textureLoader = new VulkanTextureLoader(m_vulkanDevice, m_queue, m_cmdPool);

	// Step 8 - Descriptors
	//	A descriptor is a way for shaders to freely access resources like buffers and images.
	{
		// Usage of descriptors consists of two parts :

		// A descriptor layout specifies the types of resources that are going to be accessed by the pipeline,
		// just like a render pass specifies the types of attachments that will be accessed.
		setupDescriptorFramework();

		// A descriptor set specifies the actual buffer or image resources that will be bound to the descriptors.
		setupDescriptors();
	}

	// Step 9 - Graphics pipeline
	//	The graphics pipeline in Vulkan is set up by creating a VkPipeline object. It describes the configurable state of
	//	the graphics card, like the viewport size and depth buffer operation and the programmable state using VkShaderModule objects.
	//	The driver also needs to know which render targets will be used in the pipeline, which we specify by referencing the render pass.
	//	One of the most distinctive features of Vulkan compared to existing APIs, is that almost all configuration of the graphics pipeline
	// needs to be created in advance.
	setupPipelines();

	// Create synchronization objects
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkUtils::initializers::semaphoreCreateInfo();
	// Create a semaphore used to synchronize image presentation
	// Ensures that the image is displayed before we start submitting new commands to the queu
	VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_semaphores.m_presentComplete));
	// Create a semaphore used to synchronize command submission
	// Ensures that the image is not presented until all commands have been sumbitted and executed
	VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_semaphores.m_renderComplete));

	// Set up submit info structure
	// Semaphores will stay the same during application lifetime
	// Command buffer submission info is set by each example
	m_submitInfo = vkUtils::initializers::submitInfo();
	m_submitInfo.pWaitDstStageMask = &m_submitPipelineStages;
	m_submitInfo.waitSemaphoreCount = 1;
	m_submitInfo.pWaitSemaphores = &m_semaphores.m_presentComplete;
	m_submitInfo.signalSemaphoreCount = 1;
	m_submitInfo.pSignalSemaphores = &m_semaphores.m_renderComplete;

	// Step 10 - Build command buffers
	buildCommandBuffers();

	m_wasInitialized = true;
}

void VulkanRenderer::render(SRendererContext& context)
{
	if (!m_wasInitialized)
		return;
	if (context.m_debugDraw != m_debugDisplay)
	{
		toggleDebugDisplay();
	}

	// Step 11 - Main loop
	//	Since the drawing commands have been wrapped into a command buffer in initVulkan(),
	//	the main loop is quite straightforward.
	
	//	We first acquire the next image from the swap chaing
	VK_CHECK_RESULT(m_swapChain.acquireNextImage(m_semaphores.m_presentComplete, &m_currentBuffer));

	// We can then select the appropriate command buffer for that image and execute it with vkQueueSubmit.
	draw(context);
		
	//	Finally, we return the image to the swap chain for presentation to the screen with vkQueuePresentKHR.
	VK_CHECK_RESULT(m_swapChain.queuePresent(m_queue, m_currentBuffer, m_semaphores.m_renderComplete));
	//The vkQueuePresentKHR call in turn needs to wait for rendering to be finished, for which we'll use a second semaphore that is signaled after rendering completes.
	VK_CHECK_RESULT(vkQueueWaitIdle(m_queue));
}

// Clean up Vulkan resources
void VulkanRenderer::shutdownVulkan()
{
	if (!m_wasInitialized)
		return;
	
	m_wasInitialized = false;

	// Flush device to make sure all resources can be freed 
	vkDeviceWaitIdle(m_device);

	vkDestroySemaphore(m_device, m_semaphores.m_presentComplete, nullptr);
	vkDestroySemaphore(m_device, m_semaphores.m_renderComplete, nullptr);

	vkDestroyPipelineCache(m_device, m_pipelineCache, nullptr);
	
	if (m_descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
	}

	if (m_textureLoader)
	{
		delete m_textureLoader;
	}

	for (uint32_t i = 0; i < m_frameBuffers.size(); i++)
	{
		vkDestroyFramebuffer(m_device, m_frameBuffers[i], nullptr);
	}

	vkDestroyRenderPass(m_device, m_renderPass, nullptr);

	vkDestroyImageView(m_device, m_depthStencil.m_view, nullptr);
	vkDestroyImage(m_device, m_depthStencil.m_image, nullptr);
	vkFreeMemory(m_device, m_depthStencil.m_mem, nullptr);

	if (m_setupCmdBuffer != VK_NULL_HANDLE)
	{
		vkFreeCommandBuffers(m_device, m_cmdPool, 1, &m_setupCmdBuffer);
	}
	destroyCommandBuffers();
	vkDestroyCommandPool(m_device, m_cmdPool, nullptr);

	m_swapChain.cleanup();

	for (auto& shaderModule : m_shaderModules)
	{
		vkDestroyShaderModule(m_device, shaderModule, nullptr);
	}

	delete m_vulkanDevice;

	if (enableValidation)
	{
		vkDebug::freeDebugCallback(m_instance);
	}

	vkDestroyInstance(m_instance, nullptr);
}

void VulkanRenderer::viewChanged(SRendererContext& context)
{
	// Can be overrdiden in derived class
}

void VulkanRenderer::buildCommandBuffers()
{
	// Can be overriden in derived class
}

void VulkanRenderer::prepareFrame() {

	VK_CHECK_RESULT(m_swapChain.acquireNextImage(m_semaphores.m_presentComplete, &m_currentBuffer));
}

void VulkanRenderer::submitFrame() {
	VK_CHECK_RESULT(m_swapChain.queuePresent(m_queue, m_currentBuffer, m_semaphores.m_renderComplete));
	VK_CHECK_RESULT(vkQueueWaitIdle(m_queue));
}

void VulkanRenderer::createCommandPool()
{
	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = m_swapChain.queueNodeIndex;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK_RESULT(vkCreateCommandPool(m_device, &cmdPoolInfo, nullptr, &m_cmdPool));
}

void VulkanRenderer::setupDepthStencil()
{
	VkImageCreateInfo image = {};
	image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image.pNext = NULL;
	image.imageType = VK_IMAGE_TYPE_2D;
	image.format = m_depthFormat;
	image.extent = { m_windowWidth, m_windowHeight, 1 };
	image.mipLevels = 1;
	image.arrayLayers = 1;
	image.samples = VK_SAMPLE_COUNT_1_BIT;
	image.tiling = VK_IMAGE_TILING_OPTIMAL;
	image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	image.flags = 0;

	VkMemoryAllocateInfo mem_alloc = {};
	mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mem_alloc.pNext = NULL;
	mem_alloc.allocationSize = 0;
	mem_alloc.memoryTypeIndex = 0;

	VkImageViewCreateInfo depthStencilView = {};
	depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	depthStencilView.pNext = NULL;
	depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depthStencilView.format = m_depthFormat;
	depthStencilView.flags = 0;
	depthStencilView.subresourceRange = {};
	depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	depthStencilView.subresourceRange.baseMipLevel = 0;
	depthStencilView.subresourceRange.levelCount = 1;
	depthStencilView.subresourceRange.baseArrayLayer = 0;
	depthStencilView.subresourceRange.layerCount = 1;

	VkMemoryRequirements memReqs;

	VK_CHECK_RESULT(vkCreateImage(m_device, &image, nullptr, &m_depthStencil.m_image));
	vkGetImageMemoryRequirements(m_device, m_depthStencil.m_image, &memReqs);
	mem_alloc.allocationSize = memReqs.size;
	mem_alloc.memoryTypeIndex = m_vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(m_device, &mem_alloc, nullptr, &m_depthStencil.m_mem));
	VK_CHECK_RESULT(vkBindImageMemory(m_device, m_depthStencil.m_image, m_depthStencil.m_mem, 0));

	depthStencilView.image = m_depthStencil.m_image;
	VK_CHECK_RESULT(vkCreateImageView(m_device, &depthStencilView, nullptr, &m_depthStencil.m_view));
}

void VulkanRenderer::setupFrameBuffer()
{
	VkImageView attachments[2];

	// Depth/Stencil attachment is the same for all frame buffers
	attachments[1] = m_depthStencil.m_view;

	VkFramebufferCreateInfo frameBufferCreateInfo = {};
	frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frameBufferCreateInfo.pNext = NULL;
	frameBufferCreateInfo.renderPass = m_renderPass;
	frameBufferCreateInfo.attachmentCount = 2;
	frameBufferCreateInfo.pAttachments = attachments;
	frameBufferCreateInfo.width = m_windowWidth;
	frameBufferCreateInfo.height = m_windowHeight;
	frameBufferCreateInfo.layers = 1;

	// Create frame buffers for every swap chain image
	m_frameBuffers.resize(m_swapChain.imageCount);
	for (uint32_t i = 0; i < m_frameBuffers.size(); i++)
	{
		attachments[0] = m_swapChain.buffers[i].view;
		VK_CHECK_RESULT(vkCreateFramebuffer(m_device, &frameBufferCreateInfo, nullptr, &m_frameBuffers[i]));
	}
}

void VulkanRenderer::setupRenderPass()
{
	std::array<VkAttachmentDescription, 2> attachments = {};
	// Color attachment
	attachments[0].format = m_colorformat;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	// Depth attachment
	attachments[1].format = m_depthFormat;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorReference = {};
	colorReference.attachment = 0;
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 1;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDescription = {};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorReference;
	subpassDescription.pDepthStencilAttachment = &depthReference;
	subpassDescription.inputAttachmentCount = 0;
	subpassDescription.pInputAttachments = nullptr;
	subpassDescription.preserveAttachmentCount = 0;
	subpassDescription.pPreserveAttachments = nullptr;
	subpassDescription.pResolveAttachments = nullptr;

	// Subpass dependencies for layout transitions
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
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	VK_CHECK_RESULT(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass));
}

void VulkanRenderer::windowResized(SRendererContext& context)
{
	if (!m_wasInitialized)
	{
		return;
	}
	m_wasInitialized = false;

	// Recreate swap chain
	context.getWindowSize(m_windowWidth, m_windowHeight);
	
	createSetupCommandBuffer();
	setupSwapChain(NULL);

	// Recreate the frame buffers

	vkDestroyImageView(m_device, m_depthStencil.m_view, nullptr);
	vkDestroyImage(m_device, m_depthStencil.m_image, nullptr);
	vkFreeMemory(m_device, m_depthStencil.m_mem, nullptr);
	setupDepthStencil();

	for (uint32_t i = 0; i < m_frameBuffers.size(); i++)
	{
		vkDestroyFramebuffer(m_device, m_frameBuffers[i], nullptr);
	}
	setupFrameBuffer();

	flushSetupCommandBuffer();

	// Command buffers need to be recreated as they may store
	// references to the recreated frame buffer
	destroyCommandBuffers();
	createCommandBuffers();
	buildCommandBuffers();

	vkQueueWaitIdle(m_queue);
	vkDeviceWaitIdle(m_device);

	context.m_camera.updateAspectRatio((float)m_windowWidth / (float)m_windowHeight);

	// Notify derived class
	viewChanged(context);

	m_wasInitialized = true;
}

void VulkanRenderer::setupSwapChain(GLFWwindow* window)
{
	m_swapChain.initSurface(window);
	m_swapChain.create(&m_windowWidth, &m_windowHeight, enableVSync);
}
