/******************************************************************************/
/*!
\file	VulkanUtilities.cpp
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

#include <iostream>
#include <fstream>

#include <vector>
#include "VulkanUtilities.h"

void VulkanSwapChain::initSurface(GLFWwindow* window)
{
VkResult err;

VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
VK_CHECK_RESULT(result);

// Get available queue family properties
uint32_t queueCount;
vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, NULL);
assert(queueCount >= 1);

std::vector<VkQueueFamilyProperties> queueProps(queueCount);
vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queueProps.data());

// Iterate over each queue to learn whether it supports presenting:
// Find a queue with present support
// Will be used to present the swap chain images to the windowing system
std::vector<VkBool32> supportsPresent(queueCount);
for (uint32_t i = 0; i < queueCount; i++)
{
	fpGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supportsPresent[i]);
}

// Search for a graphics and a present queue in the array of queue
// families, try to find one that supports both
uint32_t graphicsQueueNodeIndex = UINT32_MAX;
uint32_t presentQueueNodeIndex = UINT32_MAX;
for (uint32_t i = 0; i < queueCount; i++)
{
	if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
	{
		if (graphicsQueueNodeIndex == UINT32_MAX)
		{
			graphicsQueueNodeIndex = i;
		}

		if (supportsPresent[i] == VK_TRUE)
		{
			graphicsQueueNodeIndex = i;
			presentQueueNodeIndex = i;
			break;
		}
	}
}
if (presentQueueNodeIndex == UINT32_MAX)
{
	// If there's no queue that supports both present and graphics
	// try to find a separate present queue
	for (uint32_t i = 0; i < queueCount; ++i)
	{
		if (supportsPresent[i] == VK_TRUE)
		{
			presentQueueNodeIndex = i;
			break;
		}
	}
}

// Exit if either a graphics or a presenting queue hasn't been found
if (graphicsQueueNodeIndex == UINT32_MAX || presentQueueNodeIndex == UINT32_MAX)
{
	vkUtils::exitFatal("Could not find a graphics and/or presenting queue!", "Fatal error");
}

// todo : Add support for separate graphics and presenting queue
if (graphicsQueueNodeIndex != presentQueueNodeIndex)
{
	vkUtils::exitFatal("Separate graphics and presenting queues are not supported yet!", "Fatal error");
}

queueNodeIndex = graphicsQueueNodeIndex;

// Get list of supported surface formats
uint32_t formatCount;
err = fpGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, NULL);
assert(!err);
assert(formatCount > 0);

std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
err = fpGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, surfaceFormats.data());
assert(!err);

// If the surface format list only includes one entry with VK_FORMAT_UNDEFINED,
// there is no preferered format, so we assume VK_FORMAT_B8G8R8A8_UNORM
if ((formatCount == 1) && (surfaceFormats[0].format == VK_FORMAT_UNDEFINED))
{
	colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
}
else
{
	// Always select the first available color format
	// If you need a specific format (e.g. SRGB) you'd need to
	// iterate over the list of available surface format and
	// check for it's presence
	colorFormat = surfaceFormats[0].format;
}
colorSpace = surfaceFormats[0].colorSpace;
}
void vkDebug::initDebugCallback(VkInstance instance, VkDebugReportFlagsEXT flags, VkDebugReportCallbackEXT callBack)
{
	CreateDebugReportCallback = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT"));
	DestroyDebugReportCallback = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT"));
	dbgBreakCallback = reinterpret_cast<PFN_vkDebugReportMessageEXT>(vkGetInstanceProcAddr(instance, "vkDebugReportMessageEXT"));

	VkDebugReportCallbackCreateInfoEXT dbgCreateInfo = {};
	dbgCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
	dbgCreateInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)messageCallback;
	dbgCreateInfo.flags = flags;

	VkResult err = CreateDebugReportCallback(
		instance,
		&dbgCreateInfo,
		nullptr,
		(callBack != VK_NULL_HANDLE) ? &callBack : &msgCallback);
	assert(!err);
}

void vkDebug::freeDebugCallback(VkInstance instance)
{
	if (msgCallback != VK_NULL_HANDLE)
	{
		DestroyDebugReportCallback(instance, msgCallback, nullptr);
	}
}

VkBool32 vkDebug::messageCallback(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT objType,
	uint64_t srcObject,
	size_t location,
	int32_t msgCode,
	const char* pLayerPrefix,
	const char* pMsg,
	void* pUserData)
{
	// Select prefix depending on flags passed to the callback
	// Note that multiple flags may be set for a single validation message
	std::string prefix("");

	// Error that may result in undefined behaviour
	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
	{
		prefix += "ERROR:";
	};
	// Warnings may hint at unexpected / non-spec API usage
	if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
	{
		prefix += "WARNING:";
	};
	// May indicate sub-optimal usage of the API
	if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
	{
		prefix += "PERFORMANCE:";
	};
	// Informal messages that may become handy during debugging
	if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
	{
		prefix += "INFO:";
	}
	// Diagnostic info from the Vulkan loader and layers
	// Usually not helpful in terms of API usage, but may help to debug layer and loader problems 
	if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
	{
		prefix += "DEBUG:";
	}

	// Display message to default output (console if activated)
	std::cout << prefix << " [" << pLayerPrefix << "] Code " << msgCode << " : " << pMsg << "\n";

	fflush(stdout);

	// The return value of this callback controls wether the Vulkan call that caused
	// the validation message will be aborted or not
	// We return VK_FALSE as we DON'T want Vulkan calls that cause a validation message 
	// (and return a VkResult) to abort
	// If you instead want to have calls abort, pass in VK_TRUE and the function will 
	// return VK_ERROR_VALIDATION_FAILED_EXT 
	return VK_FALSE;
}

VkBool32 vkUtils::checkGlobalExtensionPresent(const char* extensionName)
{
	uint32_t extensionCount = 0;
	std::vector<VkExtensionProperties> extensions;
	vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL);
	extensions.resize(extensionCount);
	vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, extensions.data());
	for (auto& ext : extensions)
	{
		if (!strcmp(extensionName, ext.extensionName))
		{
			return true;
		}
	}
	return false;
}

VkBool32 vkUtils::checkDeviceExtensionPresent(VkPhysicalDevice physicalDevice, const char* extensionName)
{
	uint32_t extensionCount = 0;
	std::vector<VkExtensionProperties> extensions;
	vkEnumerateDeviceExtensionProperties(physicalDevice, NULL, &extensionCount, NULL);
	extensions.resize(extensionCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, NULL, &extensionCount, extensions.data());
	for (auto& ext : extensions)
	{
		if (!strcmp(extensionName, ext.extensionName))
		{
			return true;
		}
	}
	return false;
}

std::string vkUtils::errorString(VkResult errorCode)
{
	switch (errorCode)
	{
#define STR(r) case VK_ ##r: return #r
		STR(NOT_READY);
		STR(TIMEOUT);
		STR(EVENT_SET);
		STR(EVENT_RESET);
		STR(INCOMPLETE);
		STR(ERROR_OUT_OF_HOST_MEMORY);
		STR(ERROR_OUT_OF_DEVICE_MEMORY);
		STR(ERROR_INITIALIZATION_FAILED);
		STR(ERROR_DEVICE_LOST);
		STR(ERROR_MEMORY_MAP_FAILED);
		STR(ERROR_LAYER_NOT_PRESENT);
		STR(ERROR_EXTENSION_NOT_PRESENT);
		STR(ERROR_FEATURE_NOT_PRESENT);
		STR(ERROR_INCOMPATIBLE_DRIVER);
		STR(ERROR_TOO_MANY_OBJECTS);
		STR(ERROR_FORMAT_NOT_SUPPORTED);
		STR(ERROR_SURFACE_LOST_KHR);
		STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
		STR(SUBOPTIMAL_KHR);
		STR(ERROR_OUT_OF_DATE_KHR);
		STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
		STR(ERROR_VALIDATION_FAILED_EXT);
		STR(ERROR_INVALID_SHADER_NV);
#undef STR
default:
	return "UNKNOWN_ERROR";
	}
}

VkBool32 vkUtils::getSupportedDepthFormat(VkPhysicalDevice physicalDevice, VkFormat *depthFormat)
{
	// Since all depth formats may be optional, we need to find a suitable depth format to use
	// Start with the highest precision packed format
	std::vector<VkFormat> depthFormats = {
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D16_UNORM
	};

	for (auto& format : depthFormats)
	{
		VkFormatProperties formatProps;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProps);
		// Format must support depth stencil attachment for optimal tiling
		if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			*depthFormat = format;
			return true;
		}
	}

	return false;
}

// Create an image memory barrier for changing the layout of
// an image and put it into an active command buffer
// See chapter 11.4 "Image Layout" for details

void vkUtils::setImageLayout(
	VkCommandBuffer cmdbuffer,
	VkImage image,
	VkImageAspectFlags aspectMask,
	VkImageLayout oldImageLayout,
	VkImageLayout newImageLayout,
	VkImageSubresourceRange subresourceRange)
{
	// Create an image barrier object
	VkImageMemoryBarrier imageMemoryBarrier = vkUtils::initializers::imageMemoryBarrier();
	imageMemoryBarrier.oldLayout = oldImageLayout;
	imageMemoryBarrier.newLayout = newImageLayout;
	imageMemoryBarrier.image = image;
	imageMemoryBarrier.subresourceRange = subresourceRange;

	// Source layouts (old)
	// Source access mask controls actions that have to be finished on the old layout
	// before it will be transitioned to the new layout
	switch (oldImageLayout)
	{
	case VK_IMAGE_LAYOUT_UNDEFINED:
		// Image layout is undefined (or does not matter)
		// Only valid as initial layout
		// No flags required, listed only for completeness
		imageMemoryBarrier.srcAccessMask = 0;
		break;

	case VK_IMAGE_LAYOUT_PREINITIALIZED:
		// Image is preinitialized
		// Only valid as initial layout for linear images, preserves memory contents
		// Make sure host writes have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		// Image is a color attachment
		// Make sure any writes to the color buffer have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		// Image is a depth/stencil attachment
		// Make sure any writes to the depth/stencil buffer have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		// Image is a transfer source 
		// Make sure any reads from the image have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		// Image is a transfer destination
		// Make sure any writes to the image have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		// Image is read by a shader
		// Make sure any shader reads from the image have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		break;
	}

	// Target layouts (new)
	// Destination access mask controls the dependency for the new image layout
	switch (newImageLayout)
	{
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		// Image will be used as a transfer destination
		// Make sure any writes to the image have been finished
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		// Image will be used as a transfer source
		// Make sure any reads from and writes to the image have been finished
		imageMemoryBarrier.srcAccessMask = imageMemoryBarrier.srcAccessMask | VK_ACCESS_TRANSFER_READ_BIT;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		break;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		// Image will be used as a color attachment
		// Make sure any writes to the color buffer have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		// Image layout will be used as a depth/stencil attachment
		// Make sure any writes to depth/stencil buffer have been finished
		imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		// Image will be read in a shader (sampler, input attachment)
		// Make sure any writes to the image have been finished
		if (imageMemoryBarrier.srcAccessMask == 0)
		{
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
		}
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		break;
	}

	// Put barrier on top
	VkPipelineStageFlags srcStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkPipelineStageFlags destStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

	// Put barrier inside setup command buffer
	vkCmdPipelineBarrier(
		cmdbuffer,
		srcStageFlags,
		destStageFlags,
		0,
		0, nullptr,
		0, nullptr,
		1, &imageMemoryBarrier);
}

// Fixed sub resource on first mip level and layer
void vkUtils::setImageLayout(
	VkCommandBuffer cmdbuffer,
	VkImage image,
	VkImageAspectFlags aspectMask,
	VkImageLayout oldImageLayout,
	VkImageLayout newImageLayout)
{
	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = aspectMask;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.layerCount = 1;
	setImageLayout(cmdbuffer, image, aspectMask, oldImageLayout, newImageLayout, subresourceRange);
}

void vkUtils::exitFatal(std::string message, std::string caption)
{
	MessageBox(NULL, message.c_str(), caption.c_str(), MB_OK | MB_ICONERROR);
	std::cerr << message << "\n";
	exit(1);
}

std::string vkUtils::readTextFile(const char *fileName)
{
	std::string fileContent;
	std::ifstream fileStream(fileName, std::ios::in);
	if (!fileStream.is_open()) {
		printf("File %s not found\n", fileName);
		return "";
	}
	std::string line = "";
	while (!fileStream.eof()) {
		getline(fileStream, line);
		fileContent.append(line + "\n");
	}
	fileStream.close();
	return fileContent;
}

VkShaderModule vkUtils::loadShader(const char *fileName, VkDevice device, VkShaderStageFlagBits stage)
{
	size_t size;

	FILE *fp = fopen(fileName, "rb");
	assert(fp);

	fseek(fp, 0L, SEEK_END);
	size = ftell(fp);

	fseek(fp, 0L, SEEK_SET);

	//shaderCode = malloc(size);
	char *shaderCode = new char[size];
	size_t retval = fread(shaderCode, size, 1, fp);
	assert(retval == 1);
	assert(size > 0);

	fclose(fp);

	VkShaderModule shaderModule;
	VkShaderModuleCreateInfo moduleCreateInfo;
	moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleCreateInfo.pNext = NULL;
	moduleCreateInfo.codeSize = size;
	moduleCreateInfo.pCode = (uint32_t*)shaderCode;
	moduleCreateInfo.flags = 0;

	VK_CHECK_RESULT(vkCreateShaderModule(device, &moduleCreateInfo, NULL, &shaderModule));

	delete[] shaderCode;

	return shaderModule;
}

VkShaderModule vkUtils::loadShaderGLSL(const char *fileName, VkDevice device, VkShaderStageFlagBits stage)
{
	std::string shaderSrc = readTextFile(fileName);
	const char *shaderCode = shaderSrc.c_str();
	size_t size = strlen(shaderCode);
	assert(size > 0);

	VkShaderModule shaderModule;
	VkShaderModuleCreateInfo moduleCreateInfo;
	moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleCreateInfo.pNext = NULL;
	moduleCreateInfo.codeSize = 3 * sizeof(uint32_t)+size + 1;
	moduleCreateInfo.pCode = (uint32_t*)malloc(moduleCreateInfo.codeSize);
	moduleCreateInfo.flags = 0;

	// Magic SPV number
	((uint32_t *)moduleCreateInfo.pCode)[0] = 0x07230203;
	((uint32_t *)moduleCreateInfo.pCode)[1] = 0;
	((uint32_t *)moduleCreateInfo.pCode)[2] = stage;
	memcpy(((uint32_t *)moduleCreateInfo.pCode + 3), shaderCode, size + 1);

	VK_CHECK_RESULT(vkCreateShaderModule(device, &moduleCreateInfo, NULL, &shaderModule));

	return shaderModule;
}

VkImageMemoryBarrier vkUtils::prePresentBarrier(VkImage presentImage)
{
	VkImageMemoryBarrier imageMemoryBarrier = vkUtils::initializers::imageMemoryBarrier();
	imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	imageMemoryBarrier.dstAccessMask = 0;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	imageMemoryBarrier.image = presentImage;
	return imageMemoryBarrier;
}

VkImageMemoryBarrier vkUtils::postPresentBarrier(VkImage presentImage)
{
	VkImageMemoryBarrier imageMemoryBarrier = vkUtils::initializers::imageMemoryBarrier();
	imageMemoryBarrier.srcAccessMask = 0;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	imageMemoryBarrier.image = presentImage;
	return imageMemoryBarrier;
}

void vkUtils::destroyUniformData(VkDevice device, vkUtils::UniformData *uniformData)
{
	if (uniformData->mapped != nullptr)
	{
		vkUnmapMemory(device, uniformData->memory);
	}
	vkDestroyBuffer(device, uniformData->buffer, nullptr);
	vkFreeMemory(device, uniformData->memory, nullptr);
}

VkMemoryAllocateInfo vkUtils::initializers::memoryAllocateInfo()
{
	VkMemoryAllocateInfo memAllocInfo = {};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.pNext = NULL;
	memAllocInfo.allocationSize = 0;
	memAllocInfo.memoryTypeIndex = 0;
	return memAllocInfo;
}

VkCommandBufferAllocateInfo vkUtils::initializers::commandBufferAllocateInfo(VkCommandPool commandPool, VkCommandBufferLevel level, uint32_t bufferCount)
{
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.commandPool = commandPool;
	commandBufferAllocateInfo.level = level;
	commandBufferAllocateInfo.commandBufferCount = bufferCount;
	return commandBufferAllocateInfo;
}

VkCommandPoolCreateInfo vkUtils::initializers::commandPoolCreateInfo()
{
	VkCommandPoolCreateInfo cmdPoolCreateInfo = {};
	cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	return cmdPoolCreateInfo;
}

VkCommandBufferBeginInfo vkUtils::initializers::commandBufferBeginInfo()
{
	VkCommandBufferBeginInfo cmdBufferBeginInfo = {};
	cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufferBeginInfo.pNext = NULL;
	return cmdBufferBeginInfo;
}

VkCommandBufferInheritanceInfo vkUtils::initializers::commandBufferInheritanceInfo()
{
	VkCommandBufferInheritanceInfo cmdBufferInheritanceInfo = {};
	cmdBufferInheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	return cmdBufferInheritanceInfo;
}

VkRenderPassBeginInfo vkUtils::initializers::renderPassBeginInfo()
{
	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.pNext = NULL;
	return renderPassBeginInfo;
}

VkRenderPassCreateInfo vkUtils::initializers::renderPassCreateInfo()
{
	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.pNext = NULL;
	return renderPassCreateInfo;
}

VkImageMemoryBarrier vkUtils::initializers::imageMemoryBarrier()
{
	VkImageMemoryBarrier imageMemoryBarrier = {};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.pNext = NULL;
	// Some default values
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	return imageMemoryBarrier;
}

VkBufferMemoryBarrier vkUtils::initializers::bufferMemoryBarrier()
{
	VkBufferMemoryBarrier bufferMemoryBarrier = {};
	bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	bufferMemoryBarrier.pNext = NULL;
	return bufferMemoryBarrier;
}

VkMemoryBarrier vkUtils::initializers::memoryBarrier()
{
	VkMemoryBarrier memoryBarrier = {};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memoryBarrier.pNext = NULL;
	return memoryBarrier;
}

VkImageCreateInfo vkUtils::initializers::imageCreateInfo()
{
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.pNext = NULL;
	return imageCreateInfo;
}

VkSamplerCreateInfo vkUtils::initializers::samplerCreateInfo()
{
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.pNext = NULL;
	return samplerCreateInfo;
}

VkImageViewCreateInfo vkUtils::initializers::imageViewCreateInfo()
{
	VkImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.pNext = NULL;
	return imageViewCreateInfo;
}

VkFramebufferCreateInfo vkUtils::initializers::framebufferCreateInfo()
{
	VkFramebufferCreateInfo framebufferCreateInfo = {};
	framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferCreateInfo.pNext = NULL;
	return framebufferCreateInfo;
}

VkSemaphoreCreateInfo vkUtils::initializers::semaphoreCreateInfo()
{
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.pNext = NULL;
	semaphoreCreateInfo.flags = 0;
	return semaphoreCreateInfo;
}

VkFenceCreateInfo vkUtils::initializers::fenceCreateInfo(VkFenceCreateFlags flags)
{
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = flags;
	return fenceCreateInfo;
}

VkEventCreateInfo vkUtils::initializers::eventCreateInfo()
{
	VkEventCreateInfo eventCreateInfo = {};
	eventCreateInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
	return eventCreateInfo;
}

VkSubmitInfo vkUtils::initializers::submitInfo()
{
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = NULL;
	return submitInfo;
}

VkViewport vkUtils::initializers::viewport(
	float width,
	float height,
	float minDepth,
	float maxDepth)
{
	VkViewport viewport = {};
	viewport.width = width;
	viewport.height = height;
	viewport.minDepth = minDepth;
	viewport.maxDepth = maxDepth;
	return viewport;
}

VkRect2D vkUtils::initializers::rect2D(
	int32_t width,
	int32_t height,
	int32_t offsetX,
	int32_t offsetY)
{
	VkRect2D rect2D = {};
	rect2D.extent.width = width;
	rect2D.extent.height = height;
	rect2D.offset.x = offsetX;
	rect2D.offset.y = offsetY;
	return rect2D;
}

VkBufferCreateInfo vkUtils::initializers::bufferCreateInfo()
{
	VkBufferCreateInfo bufCreateInfo = {};
	bufCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	return bufCreateInfo;
}

VkBufferCreateInfo vkUtils::initializers::bufferCreateInfo(
	VkBufferUsageFlags usage,
	VkDeviceSize size)
{
	VkBufferCreateInfo bufCreateInfo = {};
	bufCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufCreateInfo.pNext = NULL;
	bufCreateInfo.usage = usage;
	bufCreateInfo.size = size;
	bufCreateInfo.flags = 0;
	return bufCreateInfo;
}

VkDescriptorPoolCreateInfo vkUtils::initializers::descriptorPoolCreateInfo(
	uint32_t poolSizeCount,
	VkDescriptorPoolSize* pPoolSizes,
	uint32_t maxSets)
{
	VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
	descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolInfo.pNext = NULL;
	descriptorPoolInfo.poolSizeCount = poolSizeCount;
	descriptorPoolInfo.pPoolSizes = pPoolSizes;
	descriptorPoolInfo.maxSets = maxSets;
	return descriptorPoolInfo;
}

VkDescriptorPoolSize vkUtils::initializers::descriptorPoolSize(
	VkDescriptorType type,
	uint32_t descriptorCount)
{
	VkDescriptorPoolSize descriptorPoolSize = {};
	descriptorPoolSize.type = type;
	descriptorPoolSize.descriptorCount = descriptorCount;
	return descriptorPoolSize;
}

VkDescriptorSetLayoutBinding vkUtils::initializers::descriptorSetLayoutBinding(
	VkDescriptorType type,
	VkShaderStageFlags stageFlags,
	uint32_t binding,
	uint32_t count)
{
	VkDescriptorSetLayoutBinding setLayoutBinding = {};
	setLayoutBinding.descriptorType = type;
	setLayoutBinding.stageFlags = stageFlags;
	setLayoutBinding.binding = binding;
	setLayoutBinding.descriptorCount = count;
	return setLayoutBinding;
}

VkDescriptorSetLayoutCreateInfo vkUtils::initializers::descriptorSetLayoutCreateInfo(
	const VkDescriptorSetLayoutBinding* pBindings,
	uint32_t bindingCount)
{
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = NULL;
	descriptorSetLayoutCreateInfo.pBindings = pBindings;
	descriptorSetLayoutCreateInfo.bindingCount = bindingCount;
	return descriptorSetLayoutCreateInfo;
}

VkPipelineLayoutCreateInfo vkUtils::initializers::pipelineLayoutCreateInfo(
	const VkDescriptorSetLayout* pSetLayouts,
	uint32_t setLayoutCount)
{
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = NULL;
	pipelineLayoutCreateInfo.setLayoutCount = setLayoutCount;
	pipelineLayoutCreateInfo.pSetLayouts = pSetLayouts;
	return pipelineLayoutCreateInfo;
}

VkPipelineLayoutCreateInfo vkUtils::initializers::pipelineLayoutCreateInfo(
	uint32_t setLayoutCount)
{
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = setLayoutCount;
	return pipelineLayoutCreateInfo;
}

VkDescriptorSetAllocateInfo vkUtils::initializers::descriptorSetAllocateInfo(
	VkDescriptorPool descriptorPool,
	const VkDescriptorSetLayout* pSetLayouts,
	uint32_t descriptorSetCount)
{
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.pNext = NULL;
	descriptorSetAllocateInfo.descriptorPool = descriptorPool;
	descriptorSetAllocateInfo.pSetLayouts = pSetLayouts;
	descriptorSetAllocateInfo.descriptorSetCount = descriptorSetCount;
	return descriptorSetAllocateInfo;
}

VkDescriptorImageInfo vkUtils::initializers::descriptorImageInfo(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)
{
	VkDescriptorImageInfo descriptorImageInfo = {};
	descriptorImageInfo.sampler = sampler;
	descriptorImageInfo.imageView = imageView;
	descriptorImageInfo.imageLayout = imageLayout;
	return descriptorImageInfo;
}

VkWriteDescriptorSet vkUtils::initializers::writeDescriptorSet(
	VkDescriptorSet dstSet,
	VkDescriptorType type,
	uint32_t binding,
	VkDescriptorBufferInfo* bufferInfo)
{
	VkWriteDescriptorSet writeDescriptorSet = {};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.pNext = NULL;
	writeDescriptorSet.dstSet = dstSet;
	writeDescriptorSet.descriptorType = type;
	writeDescriptorSet.dstBinding = binding;
	writeDescriptorSet.pBufferInfo = bufferInfo;
	// Default value in all examples
	writeDescriptorSet.descriptorCount = 1;
	return writeDescriptorSet;
}

VkWriteDescriptorSet vkUtils::initializers::writeDescriptorSet(
	VkDescriptorSet dstSet,
	VkDescriptorType type,
	uint32_t binding,
	VkDescriptorImageInfo * imageInfo)
{
	VkWriteDescriptorSet writeDescriptorSet = {};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.pNext = NULL;
	writeDescriptorSet.dstSet = dstSet;
	writeDescriptorSet.descriptorType = type;
	writeDescriptorSet.dstBinding = binding;
	writeDescriptorSet.pImageInfo = imageInfo;
	// Default value in all examples
	writeDescriptorSet.descriptorCount = 1;
	return writeDescriptorSet;
}

VkVertexInputBindingDescription vkUtils::initializers::vertexInputBindingDescription(
	uint32_t binding,
	uint32_t stride,
	VkVertexInputRate inputRate)
{
	VkVertexInputBindingDescription vInputBindDescription = {};
	vInputBindDescription.binding = binding;
	vInputBindDescription.stride = stride;
	vInputBindDescription.inputRate = inputRate;
	return vInputBindDescription;
}

VkVertexInputAttributeDescription vkUtils::initializers::vertexInputAttributeDescription(
	uint32_t binding,
	uint32_t location,
	VkFormat format,
	uint32_t offset)
{
	VkVertexInputAttributeDescription vInputAttribDescription = {};
	vInputAttribDescription.location = location;
	vInputAttribDescription.binding = binding;
	vInputAttribDescription.format = format;
	vInputAttribDescription.offset = offset;
	return vInputAttribDescription;
}

VkPipelineVertexInputStateCreateInfo vkUtils::initializers::pipelineVertexInputStateCreateInfo()
{
	VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = {};
	pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	pipelineVertexInputStateCreateInfo.pNext = NULL;
	return pipelineVertexInputStateCreateInfo;
}

VkPipelineInputAssemblyStateCreateInfo vkUtils::initializers::pipelineInputAssemblyStateCreateInfo(
	VkPrimitiveTopology topology,
	VkPipelineInputAssemblyStateCreateFlags flags,
	VkBool32 primitiveRestartEnable)
{
	VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo = {};
	pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	pipelineInputAssemblyStateCreateInfo.topology = topology;
	pipelineInputAssemblyStateCreateInfo.flags = flags;
	pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = primitiveRestartEnable;
	return pipelineInputAssemblyStateCreateInfo;
}

VkPipelineRasterizationStateCreateInfo vkUtils::initializers::pipelineRasterizationStateCreateInfo(
	VkPolygonMode polygonMode,
	VkCullModeFlags cullMode,
	VkFrontFace frontFace,
	VkPipelineRasterizationStateCreateFlags flags)
{
	VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {};
	pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	pipelineRasterizationStateCreateInfo.polygonMode = polygonMode;
	pipelineRasterizationStateCreateInfo.cullMode = cullMode;
	pipelineRasterizationStateCreateInfo.frontFace = frontFace;
	pipelineRasterizationStateCreateInfo.flags = flags;
	pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;
	return pipelineRasterizationStateCreateInfo;
}

VkPipelineColorBlendAttachmentState vkUtils::initializers::pipelineColorBlendAttachmentState(
	VkColorComponentFlags colorWriteMask,
	VkBool32 blendEnable)
{
	VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {};
	pipelineColorBlendAttachmentState.colorWriteMask = colorWriteMask;
	pipelineColorBlendAttachmentState.blendEnable = blendEnable;
	return pipelineColorBlendAttachmentState;
}

VkPipelineColorBlendStateCreateInfo vkUtils::initializers::pipelineColorBlendStateCreateInfo(
	uint32_t attachmentCount,
	const VkPipelineColorBlendAttachmentState * pAttachments)
{
	VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {};
	pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	pipelineColorBlendStateCreateInfo.pNext = NULL;
	pipelineColorBlendStateCreateInfo.attachmentCount = attachmentCount;
	pipelineColorBlendStateCreateInfo.pAttachments = pAttachments;
	return pipelineColorBlendStateCreateInfo;
}

VkPipelineDepthStencilStateCreateInfo vkUtils::initializers::pipelineDepthStencilStateCreateInfo(
	VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp depthCompareOp)
{
	VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo = {};
	pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	pipelineDepthStencilStateCreateInfo.depthTestEnable = depthTestEnable;
	pipelineDepthStencilStateCreateInfo.depthWriteEnable = depthWriteEnable;
	pipelineDepthStencilStateCreateInfo.depthCompareOp = depthCompareOp;
	pipelineDepthStencilStateCreateInfo.front = pipelineDepthStencilStateCreateInfo.back;
	pipelineDepthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
	return pipelineDepthStencilStateCreateInfo;
}

VkPipelineViewportStateCreateInfo vkUtils::initializers::pipelineViewportStateCreateInfo(
	uint32_t viewportCount,
	uint32_t scissorCount,
	VkPipelineViewportStateCreateFlags flags)
{
	VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {};
	pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	pipelineViewportStateCreateInfo.viewportCount = viewportCount;
	pipelineViewportStateCreateInfo.scissorCount = scissorCount;
	pipelineViewportStateCreateInfo.flags = flags;
	return pipelineViewportStateCreateInfo;
}

VkPipelineMultisampleStateCreateInfo vkUtils::initializers::pipelineMultisampleStateCreateInfo(
	VkSampleCountFlagBits rasterizationSamples,
	VkPipelineMultisampleStateCreateFlags flags)
{
	VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo = {};
	pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	pipelineMultisampleStateCreateInfo.rasterizationSamples = rasterizationSamples;
	return pipelineMultisampleStateCreateInfo;
}

VkPipelineDynamicStateCreateInfo vkUtils::initializers::pipelineDynamicStateCreateInfo(
	const VkDynamicState * pDynamicStates,
	uint32_t dynamicStateCount,
	VkPipelineDynamicStateCreateFlags flags)
{
	VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo = {};
	pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	pipelineDynamicStateCreateInfo.pDynamicStates = pDynamicStates;
	pipelineDynamicStateCreateInfo.dynamicStateCount = dynamicStateCount;
	return pipelineDynamicStateCreateInfo;
}

VkPipelineTessellationStateCreateInfo vkUtils::initializers::pipelineTessellationStateCreateInfo(uint32_t patchControlPoints)
{
	VkPipelineTessellationStateCreateInfo pipelineTessellationStateCreateInfo = {};
	pipelineTessellationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
	pipelineTessellationStateCreateInfo.patchControlPoints = patchControlPoints;
	return pipelineTessellationStateCreateInfo;
}

VkGraphicsPipelineCreateInfo vkUtils::initializers::pipelineCreateInfo(
	VkPipelineLayout layout,
	VkRenderPass renderPass,
	VkPipelineCreateFlags flags)
{
	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.pNext = NULL;
	pipelineCreateInfo.layout = layout;
	pipelineCreateInfo.renderPass = renderPass;
	pipelineCreateInfo.flags = flags;
	return pipelineCreateInfo;
}

VkComputePipelineCreateInfo vkUtils::initializers::computePipelineCreateInfo(VkPipelineLayout layout, VkPipelineCreateFlags flags)
{
	VkComputePipelineCreateInfo computePipelineCreateInfo = {};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.layout = layout;
	computePipelineCreateInfo.flags = flags;
	return computePipelineCreateInfo;
}

VkPushConstantRange vkUtils::initializers::pushConstantRange(VkShaderStageFlags stageFlags, uint32_t size, uint32_t offset)
{
	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.stageFlags = stageFlags;
	pushConstantRange.offset = offset;
	pushConstantRange.size = size;
	return pushConstantRange;
}

VkBindSparseInfo vkUtils::initializers::bindSparseInfo()
{
	VkBindSparseInfo bindSparseInfo{};
	bindSparseInfo.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
	return bindSparseInfo;
}

VkSpecializationMapEntry vkUtils::initializers::specializationMapEntry(uint32_t constantID, uint32_t offset, size_t size)
{
	VkSpecializationMapEntry specializationEntry{};
	specializationEntry.constantID = constantID;
	specializationEntry.offset = offset;
	specializationEntry.size = size;
	return specializationEntry;
}

VkSpecializationInfo vkUtils::initializers::specializationInfo(uint32_t mapEntryCount, const VkSpecializationMapEntry* mapEntries, size_t dataSize, const void* data)
{
	VkSpecializationInfo specializationInfo{};
	specializationInfo.mapEntryCount = mapEntryCount;
	specializationInfo.pMapEntries = mapEntries;
	specializationInfo.dataSize = dataSize;
	specializationInfo.pData = data;
	return specializationInfo;
}