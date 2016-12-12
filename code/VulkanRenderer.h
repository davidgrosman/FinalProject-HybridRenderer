/******************************************************************************/
/*!
\file	VulkanRenderer.h
\author David Grosman
\par    email: ToDavidGrosman\@gmail.com
\par    Project: CIS 565: GPU Programming and Architecture - Final Project.
\date   11/17/2016
\brief

Compiled using Microsoft (R) C/C++ Optimizing Compiler Version 18.00.21005.1 for
x86 which is my default VS2013 compiler.

This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)

*/
/******************************************************************************/

#ifndef _VULKAN_RENDERER_H_
#define _VULKAN_RENDERER_H_

#include <windows.h>
#include <io.h>

#include <iostream>
#include <chrono>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <array>

#include "vulkan/vulkan.h"

#include "vulkanMeshLoader.h"
#include "Utilities.h"

enum ERenderingMode {
	DEFERRED,
	RAYTRACING,
	DEFFERRED_RAYTRACING
};

class VulkanRenderer
{
public:

	VulkanRenderer(const std::string& fileName);
	~VulkanRenderer();

	// Setup the vulkan instance, enable required extensions and connect to the physical device (GPU)
	// Prepare commonly used Vulkan functions
	void initVulkan(SRendererContext& context, bool enableValidation);
	virtual void render(SRendererContext& context);
	virtual void shutdownVulkan();

	// Load a SPIR-V shader
	VkPipelineShaderStageCreateInfo loadShader(std::string fileName, VkShaderStageFlagBits stage);

	// Pure virtual render function (override in derived class)
	//	Note: Operations that are submitted to queues are executed asynchronously. Therefore we have to use
	//	synchronization objects like semaphores to ensure a correct order of execution. Execution of the draw
	//	command buffer must be set up to wait on image acquisition to finish, otherwise it may occur that we start
	//	rendering to an image that is still being read for presentation on the screen.
	virtual void draw(SRendererContext& context) = 0;

	virtual void toggleDebugDisplay() { m_debugDisplay = !m_debugDisplay; }
	virtual void toggleBVH() { m_enableBVH = !m_enableBVH; }
	virtual void toggleShadows() { m_enableShadows = !m_enableShadows; }
	virtual void toggleTransparency() { m_enableTransparency = !m_enableTransparency; }
	virtual void toggleReflection() { m_enableReflection = !m_enableReflection; }
	virtual void toggleColorByRayBounces() { m_enableColorByRayBounces = !m_enableColorByRayBounces; }
	virtual void addLight() { m_addLight = m_addLight == 0 ? 1 : 0; }

	// Prepare the frame for workload submission
	// - Acquires the next image from the swap chain 
	// - Sets the default wait and signal semaphores
	void prepareFrame();

	// Submit the frames' workload 
	// - Submits the text overlay (if enabled)
	void submitFrame();
	
	/////////////////////////////////////////////////////////////////////////////////////////////////
	////////					Command-Buffer												 ////////

	// Creates a new (graphics) command pool object storing command buffers
	void createCommandPool();

	// Create command buffer for setup commands
	void createSetupCommandBuffer();
	// Create command buffers for drawing commands
	void createCommandBuffers();
	// Command buffer creation
	// Creates and returns a new command buffer
	VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin);

	// Pure virtual function to be overriden by the dervice class
	// Called in case of an event where e.g. the framebuffer has to be rebuild and thus
	// all command buffers that may reference this
	virtual void buildCommandBuffers();

	// Finalize setup command bufferm submit it to the queue and remove it
	void flushSetupCommandBuffer();
	// End the command buffer, submit it to the queue and free (if requested)
	// Note : Waits for the queue to become idle
	void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free);

	// Destroy all command buffers and set their handles to VK_NULL_HANDLE
	// May be necessary during runtime if options are toggled 
	void destroyCommandBuffers();

	// Check if command buffers are valid (!= VK_NULL_HANDLE)
	bool checkCommandBuffers();

	/////////////////////////////////////////////////////////////////////////////////////////////////
	////////					Frame-Buffer												 ////////
	
	// Setup default depth and stencil views
	virtual void setupDepthStencil();
	// Create framebuffers for all requested swap chain images
	// Can be overriden in derived class to setup a custom framebuffer (e.g. for MSAA)
	virtual void setupFrameBuffer();
	// Render passes in Vulkan describe the type of images that are used during rendering operations,
	// how they will be used, and how their contents should be treated.
	// Can be overriden in derived class to setup a custom render pass (e.g. for MSAA)
	virtual void setupRenderPass();
	// Create, connect and prepare swap chain images
	void setupSwapChain(GLFWwindow* window);

	// Create a cache pool for rendering pipelines
	virtual void setupPipelines();

	virtual void setupUniformBuffers(SRendererContext& context) {}

	// set up the resources (eg: desriptorPool, descriptorSetLayout) that are going to be used by the descriptors.
	virtual void setupDescriptorFramework() {}
	// set up the scene shaders' descriptors.
	virtual void setupDescriptors() {}

	/////////////////////////////////////////////////////////////////////////////////////////////////
	////////					Event-Handler Functions  								     ////////

	// Called when view change occurs
	// Can be overriden in derived class to e.g. update uniform buffers 
	// Containing view dependant matrices
	virtual void viewChanged(SRendererContext& context);

	// Called when the window has been resized
	// Can be overriden in derived class to recreate or rebuild resources attached to the frame buffer / swapchain
	virtual void windowResized(SRendererContext& context);

	/////////////////////////////////////////////////////////////////////////////////////////////////
	////////					Helper-Functions  										     ////////

	// Create a buffer, fill it with data (if != NULL) and bind buffer memory
	VkBool32 createBuffer( VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size,
		void *data, VkBuffer *buffer, VkDeviceMemory *memory);
	// Overload to pass memory property flags
	VkBool32 createBuffer( VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size,
		void *data, VkBuffer *buffer, VkDeviceMemory *memory, VkDescriptorBufferInfo *descriptor);

	// This version always uses HOST_VISIBLE memory
	VkBool32 createBuffer( VkBufferUsageFlags usageFlags, VkDeviceSize size, void *data, VkBuffer *buffer, VkDeviceMemory *memory);
	// Overload that assigns buffer info to descriptor
	VkBool32 createBuffer( VkBufferUsageFlags usageFlags, VkDeviceSize size, void *data, VkBuffer *buffer, VkDeviceMemory *memory,
		VkDescriptorBufferInfo *descriptor);
	

	// Load a mesh and create vulkan vertex and index buffers with given vertex layout
	void loadMesh(std::string filename, vkMeshLoader::MeshBuffer *meshBuffer, SSceneAttributes* meshAttributes, std::vector<vkMeshLoader::VertexLayout> vertexLayout,
		vkMeshLoader::MeshCreateInfo *meshCreateInfo = NULL, BVHTree* tree = NULL);

	std::string m_appName = "Vulkan Renderer";
	std::string m_fileName = "";
protected:

	// Returns the base asset path (for shaders, models, textures) depending on the os
	const std::string getAssetPath();

private:

	// Create application wide Vulkan instance
	VkResult createInstance(bool enableValidation);
	
	// Get window title with example name, device, et.
	std::string getWindowTitle();

	// Set to true when example is created with enabled validation layers
	bool enableValidation = false;
	// Set to true if v-sync will be forced for the swapchain
	bool enableVSync = false;
	// Device features enabled by the example
	// If not set, no additional features are enabled (may result in validation layer errors)
	VkPhysicalDeviceFeatures enabledFeatures /*= {}*/;
	
	

protected:

	////////////////////////////////////////////////////////////////////////////////////////////////
	////////				Context-Data													////////

	uint32_t m_windowWidth = 1280;
	uint32_t m_windowHeight = 720;

	////////////////////////////////////////////////////////////////////////////////////////////////
	////////				Utility-Data													////////

	bool m_wasInitialized;

	bool m_debugDisplay;
	bool m_enableBVH;
	bool m_enableShadows;
	bool m_enableTransparency;
	bool m_enableReflection;
	bool m_enableColorByRayBounces;
	uint32_t m_addLight;

	// Last frame time, measured using a high performance timer (if available)
	float m_frameTimer = 1.0f;
	// Frame counter to display fps
	uint32_t m_frameCounter = 0;
	uint32_t m_lastFPS = 0;

protected:

	////////////////////////////////////////////////////////////////////////////////////////////////
	////////				Vulkan-Data														////////

	// Vulkan instance, stores all per-application states
	VkInstance m_instance;
	// Physical device (GPU) that Vulkan will ise
	VkPhysicalDevice m_physicalDevice;
	// Stores physical device properties (for e.g. checking device limits)
	VkPhysicalDeviceProperties m_deviceProperties;
	// Stores phyiscal device features (for e.g. checking if a feature is available)
	VkPhysicalDeviceFeatures m_deviceFeatures;
	// Stores all available memory (type) properties for the physical device
	VkPhysicalDeviceMemoryProperties m_deviceMemoryProperties;
	/** @brief Logical device, application's view of the physical device (GPU) */
	// todo: getter? should always point to VulkanDevice->device
	VkDevice m_device;
	/** @brief Encapsulated physical and logical vulkan device */
	vk::VulkanDevice* m_vulkanDevice;
	// Handle to the device graphics queue that command buffers are submitted to
	VkQueue m_queue;
	// Color buffer format
	VkFormat m_colorformat = VK_FORMAT_B8G8R8A8_UNORM;
	// Depth buffer format
	// Depth format is selected during Vulkan initialization
	VkFormat m_depthFormat;
	// Command buffer pool
	VkCommandPool m_cmdPool;
	// Command buffer used for setup
	VkCommandBuffer m_setupCmdBuffer = VK_NULL_HANDLE;
	/** @brief Pipeline stages used to wait at for graphics queue submissions */
	VkPipelineStageFlags m_submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	// Contains command buffers and semaphores to be presented to the queue
	VkSubmitInfo m_submitInfo;
	// Command buffers used for rendering
	std::vector<VkCommandBuffer> m_drawCmdBuffers;
	// Global render pass for frame buffer writes
	VkRenderPass m_renderPass;
	// List of available frame buffers (same as number of swap chain images)
	std::vector<VkFramebuffer> m_frameBuffers;
	// Active frame buffer index
	uint32_t m_currentBuffer = 0;
	// Descriptor set pool
	VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
	// List of shader modules created (stored for cleanup)
	std::vector<VkShaderModule> m_shaderModules;
	// Pipeline cache object
	VkPipelineCache m_pipelineCache;
	// Wraps the swap chain to present images (framebuffers) to the windowing system
	VulkanSwapChain m_swapChain;

	struct SVkDepthStencil
	{
		VkImage m_image;
		VkDeviceMemory m_mem;
		VkImageView m_view;
	};

	// Synchronization semaphores
	struct SVkSemaphores
	{
		// Swap chain image presentation
		VkSemaphore m_presentComplete;
		// Command buffer submission and execution
		VkSemaphore m_renderComplete;
	};

	SVkDepthStencil m_depthStencil;

	SVkSemaphores m_semaphores;
	
	// Simple texture loader
	VulkanTextureLoader* m_textureLoader = nullptr;
};

#endif // _VULKAN_RENDERER_H_