/******************************************************************************/
/*!
\file	VulkanUtilities.h
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

#ifndef _VULKAN_UTILITIES_H_
#define _VULKAN_UTILITIES_H_

#include <assert.h>
#include <iostream>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gli/gli.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// Custom define for better code readability
#define VK_FLAGS_NONE 0
// Default fence timeout in nanoseconds
#define DEFAULT_FENCE_TIMEOUT 100000000000

// Macro to check and display Vulkan return results
#define VK_CHECK_RESULT(f)																				\
{																										\
	VkResult res = (f);																					\
	if (res != VK_SUCCESS)																				\
	{																									\
	std::cout << "Fatal : VkResult is \"" << vkUtils::errorString(res).c_str() << "\" in " << __FILE__ << " at line " << __LINE__ << std::endl; \
	assert(res == VK_SUCCESS);																		\
	}																									\
}

// Macro to get a procedure address based on a vulkan instance
#define GET_INSTANCE_PROC_ADDR(inst, entrypoint)                        \
{                                                                       \
	fp##entrypoint = reinterpret_cast<PFN_vk##entrypoint>(vkGetInstanceProcAddr(inst, "vk"#entrypoint)); \
	if (fp##entrypoint == NULL)                                         \
	{																    \
		exit(1);														\
	}                                                                   \
}

// Macro to get a procedure address based on a vulkan device
#define GET_DEVICE_PROC_ADDR(dev, entrypoint)                           \
{                                                                       \
	fp##entrypoint = reinterpret_cast<PFN_vk##entrypoint>(vkGetDeviceProcAddr(dev, "vk"#entrypoint));   \
if (fp##entrypoint == NULL)                                         \
	{																    \
	exit(1);                                                        \
	}                                                                   \
}

namespace vkDebug
{
	static int validationLayerCount = 1;
	static const char *validationLayerNames[] =
	{
		// This is a meta layer that enables all of the standard
		// validation layers in the correct order :
		// threading, parameter_validation, device_limits, object_tracker, image, core_validation, swapchain, and unique_objects
		"VK_LAYER_LUNARG_standard_validation"
	};

	static PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallback = VK_NULL_HANDLE;
	static PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallback = VK_NULL_HANDLE;
	static PFN_vkDebugReportMessageEXT dbgBreakCallback = VK_NULL_HANDLE;

	static VkDebugReportCallbackEXT msgCallback;

	VkBool32 messageCallback(
		VkDebugReportFlagsEXT flags,
		VkDebugReportObjectTypeEXT objType,
		uint64_t srcObject,
		size_t location,
		int32_t msgCode,
		const char* pLayerPrefix,
		const char* pMsg,
		void* pUserData);

	void initDebugCallback(VkInstance instance, VkDebugReportFlagsEXT flags, VkDebugReportCallbackEXT callBack);
	void freeDebugCallback(VkInstance instance);
}

namespace vkUtils
{
	// Check if extension is globally available
	VkBool32 checkGlobalExtensionPresent(const char* extensionName);
	// Check if extension is present on the given device
	VkBool32 checkDeviceExtensionPresent(VkPhysicalDevice physicalDevice, const char* extensionName);

	// Selected a suitable supported depth format starting with 32 bit down to 16 bit
	// Returns false if none of the depth formats in the list is supported by the device
	VkBool32 getSupportedDepthFormat(VkPhysicalDevice physicalDevice, VkFormat *depthFormat);

	// Put an image memory barrier for setting an image layout on the sub resource into the given command buffer
	void setImageLayout( VkCommandBuffer cmdbuffer, VkImage image, VkImageAspectFlags aspectMask,
		VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkImageSubresourceRange subresourceRange);
	// Uses a fixed sub resource layout with first mip level and layer
	void setImageLayout( VkCommandBuffer cmdbuffer, VkImage image, VkImageAspectFlags aspectMask,
		VkImageLayout oldImageLayout, VkImageLayout newImageLayout);

	// Return string representation of a vulkan error string
	std::string errorString(VkResult errorCode);
	// Display error message and exit on fatal error
	void exitFatal(std::string message, std::string caption);
	// Load a text file (e.g. GLGL shader) into a std::string
	std::string readTextFile(const char *fileName);
	// Load a binary file into a buffer (e.g. SPIR-V)
	char *readBinaryFile(const char *filename, size_t *psize);

	// Load a SPIR-V shader
	VkShaderModule loadShader(const char *fileName, VkDevice device, VkShaderStageFlagBits stage);

	// Load a GLSL shader
	// Note : Only for testing purposes, support for directly feeding GLSL shaders into Vulkan
	// may be dropped at some point	
	VkShaderModule loadShaderGLSL(const char *fileName, VkDevice device, VkShaderStageFlagBits stage);

	// Returns a pre-present image memory barrier
	// Transforms the image's layout from color attachment to present khr
	VkImageMemoryBarrier prePresentBarrier(VkImage presentImage);

	// Returns a post-present image memory barrier
	// Transforms the image's layout back from present khr to color attachment
	VkImageMemoryBarrier postPresentBarrier(VkImage presentImage);

	// Contains all vulkan objects
	// required for a uniform data object
	struct UniformData
	{
		VkBuffer buffer;
		VkDeviceMemory memory;
		VkDescriptorBufferInfo descriptor;
		uint32_t allocSize;
		void* mapped = nullptr;
	};

	// Destroy (and free) Vulkan resources used by a uniform data structure
	void destroyUniformData(VkDevice device, vkUtils::UniformData *uniformData);

	// Contains often used vulkan object initializers
	// Save lot of VK_STRUCTURE_TYPE assignments
	// Some initializers are parameterized for convenience
	namespace initializers
	{
		VkMemoryAllocateInfo memoryAllocateInfo();

		VkCommandBufferAllocateInfo commandBufferAllocateInfo(
			VkCommandPool commandPool,
			VkCommandBufferLevel level,
			uint32_t bufferCount);

		VkCommandPoolCreateInfo commandPoolCreateInfo();
		VkCommandBufferBeginInfo commandBufferBeginInfo();
		VkCommandBufferInheritanceInfo commandBufferInheritanceInfo();

		VkRenderPassBeginInfo renderPassBeginInfo();
		VkRenderPassCreateInfo renderPassCreateInfo();

		VkImageMemoryBarrier imageMemoryBarrier();
		VkBufferMemoryBarrier bufferMemoryBarrier();
		VkMemoryBarrier memoryBarrier();

		VkImageCreateInfo imageCreateInfo();
		VkSamplerCreateInfo samplerCreateInfo();
		VkImageViewCreateInfo imageViewCreateInfo();

		VkFramebufferCreateInfo framebufferCreateInfo();

		VkSemaphoreCreateInfo semaphoreCreateInfo();
		VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags = VK_FLAGS_NONE);
		VkEventCreateInfo eventCreateInfo();

		VkSubmitInfo submitInfo();

		VkViewport viewport(
			float width,
			float height,
			float minDepth,
			float maxDepth);

		VkRect2D rect2D(
			int32_t width,
			int32_t height,
			int32_t offsetX,
			int32_t offsetY);

		VkBufferCreateInfo bufferCreateInfo();

		VkBufferCreateInfo bufferCreateInfo(
			VkBufferUsageFlags usage,
			VkDeviceSize size);

		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo(
			uint32_t poolSizeCount,
			VkDescriptorPoolSize* pPoolSizes,
			uint32_t maxSets);

		VkDescriptorPoolSize descriptorPoolSize(
			VkDescriptorType type,
			uint32_t descriptorCount);

		VkDescriptorSetLayoutBinding descriptorSetLayoutBinding(
			VkDescriptorType type,
			VkShaderStageFlags stageFlags,
			uint32_t binding,
			uint32_t count = 1);

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo(
			const VkDescriptorSetLayoutBinding* pBindings,
			uint32_t bindingCount);

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo(
			const VkDescriptorSetLayout* pSetLayouts,
			uint32_t setLayoutCount);
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo(
			uint32_t setLayoutCount = 1);

		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo(
			VkDescriptorPool descriptorPool,
			const VkDescriptorSetLayout* pSetLayouts,
			uint32_t descriptorSetCount);

		VkDescriptorImageInfo descriptorImageInfo(
			VkSampler sampler,
			VkImageView imageView,
			VkImageLayout imageLayout);

		VkWriteDescriptorSet writeDescriptorSet(
			VkDescriptorSet dstSet,
			VkDescriptorType type,
			uint32_t binding,
			VkDescriptorBufferInfo* bufferInfo);

		VkWriteDescriptorSet writeDescriptorSet(
			VkDescriptorSet dstSet,
			VkDescriptorType type,
			uint32_t binding,
			VkDescriptorImageInfo* imageInfo);

		VkVertexInputBindingDescription vertexInputBindingDescription(
			uint32_t binding,
			uint32_t stride,
			VkVertexInputRate inputRate);

		VkVertexInputAttributeDescription vertexInputAttributeDescription(
			uint32_t binding,
			uint32_t location,
			VkFormat format,
			uint32_t offset);

		VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo();

		VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo(
			VkPrimitiveTopology topology,
			VkPipelineInputAssemblyStateCreateFlags flags,
			VkBool32 primitiveRestartEnable);

		VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo(
			VkPolygonMode polygonMode,
			VkCullModeFlags cullMode,
			VkFrontFace frontFace,
			VkPipelineRasterizationStateCreateFlags flags);

		VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState(
			VkColorComponentFlags colorWriteMask,
			VkBool32 blendEnable);

		VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo(
			uint32_t attachmentCount,
			const VkPipelineColorBlendAttachmentState* pAttachments);

		VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo(
			VkBool32 depthTestEnable,
			VkBool32 depthWriteEnable,
			VkCompareOp depthCompareOp);

		VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo(
			uint32_t viewportCount,
			uint32_t scissorCount,
			VkPipelineViewportStateCreateFlags flags);

		VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo(
			VkSampleCountFlagBits rasterizationSamples,
			VkPipelineMultisampleStateCreateFlags flags);

		VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(
			const VkDynamicState *pDynamicStates,
			uint32_t dynamicStateCount,
			VkPipelineDynamicStateCreateFlags flags);

		VkPipelineTessellationStateCreateInfo pipelineTessellationStateCreateInfo(
			uint32_t patchControlPoints);

		VkGraphicsPipelineCreateInfo pipelineCreateInfo(
			VkPipelineLayout layout,
			VkRenderPass renderPass,
			VkPipelineCreateFlags flags);

		VkComputePipelineCreateInfo computePipelineCreateInfo(
			VkPipelineLayout layout,
			VkPipelineCreateFlags flags);

		VkPushConstantRange pushConstantRange(
			VkShaderStageFlags stageFlags,
			uint32_t size,
			uint32_t offset);

		VkBindSparseInfo bindSparseInfo();

		/** @brief Initialize a map entry for a shader specialization constant */
		VkSpecializationMapEntry specializationMapEntry(uint32_t constantID, uint32_t offset, size_t size);

		/** @biref Initialize a specialization constant info structure to pass to a shader stage */
		VkSpecializationInfo specializationInfo(uint32_t mapEntryCount, const VkSpecializationMapEntry* mapEntries, size_t dataSize, const void* data);
	}

	/**
	* @brief Encapsulates a Vulkan texture object (including view, sampler, descriptor, etc.)
	*/
	struct VulkanTexture
	{
		VkSampler sampler;
		VkImage image;
		VkImageLayout imageLayout;
		VkDeviceMemory deviceMemory;
		VkImageView view;
		uint32_t width, height;
		uint32_t mipLevels;
		uint32_t layerCount;
		VkDescriptorImageInfo descriptor;
	};
}

namespace vk
{
	/**
	* @brief Encapsulates access to a Vulkan buffer backed up by device memory
	* @note To be filled by an external source like the VulkanDevice
	*/
	struct Buffer
	{
		VkBuffer buffer;
		VkDevice device;
		VkDeviceMemory memory;
		VkDescriptorBufferInfo descriptor;
		VkDeviceSize size = 0;
		VkDeviceSize alignment = 0;
		void* mapped = nullptr;

		/** @brief Usage flags to be filled by external source at buffer creation (to query at some later point) */
		VkBufferUsageFlags usageFlags;
		/** @brief Memory propertys flags to be filled by external source at buffer creation (to query at some later point) */
		VkMemoryPropertyFlags memoryPropertyFlags;

		/**
		* Map a memory range of this buffer. If successful, mapped points to the specified buffer range.
		*
		* @param size (Optional) Size of the memory range to map. Pass VK_WHOLE_SIZE to map the complete buffer range.
		* @param offset (Optional) Byte offset from beginning
		*
		* @return VkResult of the buffer mapping call
		*/
		VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
		{
			return vkMapMemory(device, memory, offset, size, 0, &mapped);
		}

		/**
		* Unmap a mapped memory range
		*
		* @note Does not return a result as vkUnmapMemory can't fail
		*/
		void unmap()
		{
			if (mapped)
			{
				vkUnmapMemory(device, memory);
				mapped = nullptr;
			}
		}

		/**
		* Attach the allocated memory block to the buffer
		*
		* @param offset (Optional) Byte offset (from the beginning) for the memory region to bind
		*
		* @return VkResult of the bindBufferMemory call
		*/
		VkResult bind(VkDeviceSize offset = 0)
		{
			return vkBindBufferMemory(device, buffer, memory, offset);
		}

		/**
		* Setup the default descriptor for this buffer
		*
		* @param size (Optional) Size of the memory range of the descriptor
		* @param offset (Optional) Byte offset from beginning
		*
		*/
		void setupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
		{
			descriptor.offset = offset;
			descriptor.buffer = buffer;
			descriptor.range = size;
		}

		/**
		* Copies the specified data to the mapped buffer
		*
		* @param data Pointer to the data to copy
		* @param size Size of the data to copy in machine units
		*
		*/
		void copyTo(void* data, VkDeviceSize size)
		{
			assert(mapped);
			memcpy(mapped, data, size);
		}

		/**
		* Flush a memory range of the buffer to make it visible to the device
		*
		* @note Only required for non-coherent memory
		*
		* @param size (Optional) Size of the memory range to flush. Pass VK_WHOLE_SIZE to flush the complete buffer range.
		* @param offset (Optional) Byte offset from beginning
		*
		* @return VkResult of the flush call
		*/
		VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
		{
			VkMappedMemoryRange mappedRange = {};
			mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			mappedRange.memory = memory;
			mappedRange.offset = offset;
			mappedRange.size = size;
			return vkFlushMappedMemoryRanges(device, 1, &mappedRange);
		}

		/**
		* Invalidate a memory range of the buffer to make it visible to the host
		*
		* @note Only required for non-coherent memory
		*
		* @param size (Optional) Size of the memory range to invalidate. Pass VK_WHOLE_SIZE to invalidate the complete buffer range.
		* @param offset (Optional) Byte offset from beginning
		*
		* @return VkResult of the invalidate call
		*/
		VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
		{
			VkMappedMemoryRange mappedRange = {};
			mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			mappedRange.memory = memory;
			mappedRange.offset = offset;
			mappedRange.size = size;
			return vkInvalidateMappedMemoryRanges(device, 1, &mappedRange);
		}

		/**
		* Release all Vulkan resources held by this buffer
		*/
		void destroy()
		{
			if (buffer)
			{
				vkDestroyBuffer(device, buffer, nullptr);
			}
			if (memory)
			{
				vkFreeMemory(device, memory, nullptr);
			}
		}

	};

	struct VulkanDevice
	{
		/** @brief Physical device representation */
		VkPhysicalDevice physicalDevice;
		/** @brief Logical device representation (application's view of the device) */
		VkDevice logicalDevice;
		/** @brief Properties of the physical device including limits that the application can check against */
		VkPhysicalDeviceProperties properties;
		/** @brief Features of the physical device that an application can use to check if a feature is supported */
		VkPhysicalDeviceFeatures features;
		/** @brief Memory types and heaps of the physical device */
		VkPhysicalDeviceMemoryProperties memoryProperties;
		/** @brief Queue family properties of the physical device */
		std::vector<VkQueueFamilyProperties> queueFamilyProperties;

		/** @brief Default command pool for the graphics queue family index */
		VkCommandPool commandPool = VK_NULL_HANDLE;

		/** @brief Set to true when the debug marker extension is detected */
		bool enableDebugMarkers = false;

		/** @brief Contains queue family indices */
		struct
		{
			uint32_t graphics;
			uint32_t compute;
			uint32_t transfer;
		} queueFamilyIndices;

		/**
		* Default constructor
		*
		* @param physicalDevice Phyiscal device that is to be used
		*/
		VulkanDevice(VkPhysicalDevice physicalDevice)
		{
			assert(physicalDevice);
			this->physicalDevice = physicalDevice;

			// Store Properties features, limits and properties of the physical device for later use
			// Device properties also contain limits and sparse properties
			vkGetPhysicalDeviceProperties(physicalDevice, &properties);
			// Features should be checked by the examples before using them
			vkGetPhysicalDeviceFeatures(physicalDevice, &features);
			// Memory properties are used regularly for creating all kinds of buffer
			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
			// Queue family properties, used for setting up requested queues upon device creation
			uint32_t queueFamilyCount;
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
			assert(queueFamilyCount > 0);
			queueFamilyProperties.resize(queueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());
		}

		/**
		* Default destructor
		*
		* @note Frees the logical device
		*/
		~VulkanDevice()
		{
			if (commandPool)
			{
				vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
			}
			if (logicalDevice)
			{
				vkDestroyDevice(logicalDevice, nullptr);
			}
		}

		/**
		* Get the index of a memory type that has all the requested property bits set
		*
		* @param typeBits Bitmask with bits set for each memory type supported by the resource to request for (from VkMemoryRequirements)
		* @param properties Bitmask of properties for the memory type to request
		* @param (Optional) memTypeFound Pointer to a bool that is set to true if a matching memory type has been found
		*
		* @return Index of the requested memory type
		*
		* @throw Throws an exception if memTypeFound is null and no memory type could be found that supports the requested properties
		*/
		uint32_t getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32 *memTypeFound = nullptr)
		{
			for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
			{
				if ((typeBits & 1) == 1)
				{
					if ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
					{
						if (memTypeFound)
						{
							*memTypeFound = true;
						}
						return i;
					}
				}
				typeBits >>= 1;
			}

#if defined(__ANDROID__)
			//todo : Exceptions are disabled by default on Android (need to add LOCAL_CPP_FEATURES += exceptions to Android.mk), so for now just return zero
			if (memTypeFound)
			{
				*memTypeFound = false;
			}
			return 0;
#else
			if (memTypeFound)
			{
				*memTypeFound = false;
				return 0;
			}
			else
			{
				throw std::runtime_error("Could not find a matching memory type");
			}
#endif
		}

		/**
		* Get the index of a queue family that supports the requested queue flags
		*
		* @param queueFlags Queue flags to find a queue family index for
		*
		* @return Index of the queue family index that matches the flags
		*
		* @throw Throws an exception if no queue family index could be found that supports the requested flags
		*/
		uint32_t getQueueFamiliyIndex(VkQueueFlagBits queueFlags)
		{
			// Dedicated queue for compute
			// Try to find a queue family index that supports compute but not graphics
			if (queueFlags & VK_QUEUE_COMPUTE_BIT)
			{
				for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
				{
					if ((queueFamilyProperties[i].queueFlags & queueFlags) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
					{
						return i;
						break;
					}
				}
			}

			// Dedicated queue for transfer
			// Try to find a queue family index that supports transfer but not graphics and compute
			if (queueFlags & VK_QUEUE_TRANSFER_BIT)
			{
				for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
				{
					if ((queueFamilyProperties[i].queueFlags & queueFlags) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
					{
						return i;
						break;
					}
				}
			}

			// For other queue types or if no separate compute queue is present, return the first one to support the requested flags
			for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
			{
				if (queueFamilyProperties[i].queueFlags & queueFlags)
				{
					return i;
					break;
				}
			}

#if defined(__ANDROID__)
			//todo : Exceptions are disabled by default on Android (need to add LOCAL_CPP_FEATURES += exceptions to Android.mk), so for now just return zero
			return 0;
#else
			throw std::runtime_error("Could not find a matching queue family index");
#endif
		}

		/**
		* Create the logical device based on the assigned physical device, also gets default queue family indices
		*
		* @param enabledFeatures Can be used to enable certain features upon device creation
		* @param useSwapChain Set to false for headless rendering to omit the swapchain device extensions
		* @param requestedQueueTypes Bit flags specifying the queue types to be requested from the device
		*
		* @return VkResult of the device creation call
		*/
		VkResult createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, bool useSwapChain = true, VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)
		{
			// Desired queues need to be requested upon logical device creation
			// Due to differing queue family configurations of Vulkan implementations this can be a bit tricky, especially if the application
			// requests different queue types

			std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

			// Get queue family indices for the requested queue family types
			// Note that the indices may overlap depending on the implementation

			const float defaultQueuePriority(0.0f);

			// Graphics queue
			if (requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT)
			{
				queueFamilyIndices.graphics = getQueueFamiliyIndex(VK_QUEUE_GRAPHICS_BIT);
				VkDeviceQueueCreateInfo queueInfo{};
				queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queueInfo.queueFamilyIndex = queueFamilyIndices.graphics;
				queueInfo.queueCount = 1;
				queueInfo.pQueuePriorities = &defaultQueuePriority;
				queueCreateInfos.push_back(queueInfo);
			}
			else
			{
				queueFamilyIndices.graphics = VK_NULL_HANDLE;
			}

			// Dedicated compute queue
			if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT)
			{
				queueFamilyIndices.compute = getQueueFamiliyIndex(VK_QUEUE_COMPUTE_BIT);
				if (queueFamilyIndices.compute != queueFamilyIndices.graphics)
				{
					// If compute family index differs, we need an additional queue create info for the compute queue
					VkDeviceQueueCreateInfo queueInfo{};
					queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
					queueInfo.queueFamilyIndex = queueFamilyIndices.compute;
					queueInfo.queueCount = 1;
					queueInfo.pQueuePriorities = &defaultQueuePriority;
					queueCreateInfos.push_back(queueInfo);
				}
			}
			else
			{
				// Else we use the same queue
				queueFamilyIndices.compute = queueFamilyIndices.graphics;
			}

			// Dedicated transfer queue
			if (requestedQueueTypes & VK_QUEUE_TRANSFER_BIT)
			{
				queueFamilyIndices.transfer = getQueueFamiliyIndex(VK_QUEUE_TRANSFER_BIT);
				if ((queueFamilyIndices.transfer != queueFamilyIndices.graphics) && (queueFamilyIndices.transfer != queueFamilyIndices.compute))
				{
					// If compute family index differs, we need an additional queue create info for the compute queue
					VkDeviceQueueCreateInfo queueInfo{};
					queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
					queueInfo.queueFamilyIndex = queueFamilyIndices.transfer;
					queueInfo.queueCount = 1;
					queueInfo.pQueuePriorities = &defaultQueuePriority;
					queueCreateInfos.push_back(queueInfo);
				}
			}
			else
			{
				// Else we use the same queue
				queueFamilyIndices.transfer = queueFamilyIndices.graphics;
			}

			// Create the logical device representation
			std::vector<const char*> deviceExtensions;
			if (useSwapChain)
			{
				// If the device will be used for presenting to a display via a swapchain we need to request the swapchain extension
				deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
			}

			VkDeviceCreateInfo deviceCreateInfo = {};
			deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());;
			deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
			deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

			// Enable the debug marker extension if it is present (likely meaning a debugging tool is present)
			if (vkUtils::checkDeviceExtensionPresent(physicalDevice, VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
			{
				deviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
				enableDebugMarkers = true;
			}

			if (deviceExtensions.size() > 0)
			{
				deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
				deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
			}

			VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice);

			if (result == VK_SUCCESS)
			{
				// Create a default command pool for graphics command buffers
				commandPool = createCommandPool(queueFamilyIndices.graphics);
			}

			return result;
		}

		/**
		* Create a buffer on the device
		*
		* @param usageFlags Usage flag bitmask for the buffer (i.e. index, vertex, uniform buffer)
		* @param memoryPropertyFlags Memory properties for this buffer (i.e. device local, host visible, coherent)
		* @param size Size of the buffer in byes
		* @param buffer Pointer to the buffer handle acquired by the function
		* @param memory Pointer to the memory handle acquired by the function
		* @param data Pointer to the data that should be copied to the buffer after creation (optional, if not set, no data is copied over)
		*
		* @return VK_SUCCESS if buffer handle and memory have been created and (optionally passed) data has been copied
		*/
		VkResult createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, VkBuffer *buffer, VkDeviceMemory *memory, void *data = nullptr)
		{
			// Create the buffer handle
			VkBufferCreateInfo bufferCreateInfo = vkUtils::initializers::bufferCreateInfo(usageFlags, size);
			bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			VK_CHECK_RESULT(vkCreateBuffer(logicalDevice, &bufferCreateInfo, nullptr, buffer));

			// Create the memory backing up the buffer handle
			VkMemoryRequirements memReqs;
			VkMemoryAllocateInfo memAlloc = vkUtils::initializers::memoryAllocateInfo();
			vkGetBufferMemoryRequirements(logicalDevice, *buffer, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			// Find a memory type index that fits the properties of the buffer
			memAlloc.memoryTypeIndex = getMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags);
			VK_CHECK_RESULT(vkAllocateMemory(logicalDevice, &memAlloc, nullptr, memory));

			// If a pointer to the buffer data has been passed, map the buffer and copy over the data
			if (data != nullptr)
			{
				void *mapped;
				VK_CHECK_RESULT(vkMapMemory(logicalDevice, *memory, 0, size, 0, &mapped));
				memcpy(mapped, data, size);
				vkUnmapMemory(logicalDevice, *memory);
			}

			// Attach the memory to the buffer object
			VK_CHECK_RESULT(vkBindBufferMemory(logicalDevice, *buffer, *memory, 0));

			return VK_SUCCESS;
		}

		/**
		* Create a buffer on the device
		*
		* @param usageFlags Usage flag bitmask for the buffer (i.e. index, vertex, uniform buffer)
		* @param memoryPropertyFlags Memory properties for this buffer (i.e. device local, host visible, coherent)
		* @param buffer Pointer to a vk::Vulkan buffer object
		* @param size Size of the buffer in byes
		* @param data Pointer to the data that should be copied to the buffer after creation (optional, if not set, no data is copied over)
		*
		* @return VK_SUCCESS if buffer handle and memory have been created and (optionally passed) data has been copied
		*/
		VkResult createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, vk::Buffer *buffer, VkDeviceSize size, void *data = nullptr)
		{
			buffer->device = logicalDevice;

			// Create the buffer handle
			VkBufferCreateInfo bufferCreateInfo = vkUtils::initializers::bufferCreateInfo(usageFlags, size);
			VK_CHECK_RESULT(vkCreateBuffer(logicalDevice, &bufferCreateInfo, nullptr, &buffer->buffer));

			// Create the memory backing up the buffer handle
			VkMemoryRequirements memReqs;
			VkMemoryAllocateInfo memAlloc = vkUtils::initializers::memoryAllocateInfo();
			vkGetBufferMemoryRequirements(logicalDevice, buffer->buffer, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			// Find a memory type index that fits the properties of the buffer
			memAlloc.memoryTypeIndex = getMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags);
			VK_CHECK_RESULT(vkAllocateMemory(logicalDevice, &memAlloc, nullptr, &buffer->memory));

			buffer->alignment = memReqs.alignment;
			buffer->size = memAlloc.allocationSize;
			buffer->usageFlags = usageFlags;
			buffer->memoryPropertyFlags = memoryPropertyFlags;

			// If a pointer to the buffer data has been passed, map the buffer and copy over the data
			if (data != nullptr)
			{
				VK_CHECK_RESULT(buffer->map());
				memcpy(buffer->mapped, data, size);
				buffer->unmap();
			}

			// Initialize a default descriptor that covers the whole buffer size
			buffer->setupDescriptor();

			// Attach the memory to the buffer object
			return buffer->bind();
		}

		/**
		* Copy buffer data from src to dst using VkCmdCopyBuffer
		*
		* @param src Pointer to the source buffer to copy from
		* @param dst Pointer to the destination buffer to copy tp
		* @param queue Pointer
		* @param copyRegion (Optional) Pointer to a copy region, if NULL, the whole buffer is copied
		*
		* @note Source and destionation pointers must have the approriate transfer usage flags set (TRANSFER_SRC / TRANSFER_DST)
		*/
		void copyBuffer(vk::Buffer *src, vk::Buffer *dst, VkQueue queue, VkBufferCopy *copyRegion = nullptr)
		{
			assert(dst->size <= src->size);
			assert(src->buffer && src->buffer);
			VkCommandBuffer copyCmd = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			VkBufferCopy bufferCopy{};
			if (copyRegion == nullptr)
			{
				bufferCopy.size = src->size;
			}
			else
			{
				bufferCopy = *copyRegion;
			}

			vkCmdCopyBuffer(copyCmd, src->buffer, dst->buffer, 1, &bufferCopy);

			flushCommandBuffer(copyCmd, queue);
		}

		/**
		* Create a command pool for allocation command buffers from
		*
		* @param queueFamilyIndex Family index of the queue to create the command pool for
		* @param createFlags (Optional) Command pool creation flags (Defaults to VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
		*
		* @note Command buffers allocated from the created pool can only be submitted to a queue with the same family index
		*
		* @return A handle to the created command buffer
		*/
		VkCommandPool createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
		{
			VkCommandPoolCreateInfo cmdPoolInfo = {};
			cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
			cmdPoolInfo.flags = createFlags;
			VkCommandPool cmdPool;
			VK_CHECK_RESULT(vkCreateCommandPool(logicalDevice, &cmdPoolInfo, nullptr, &cmdPool));
			return cmdPool;
		}

		/**
		* Allocate a command buffer from the command pool
		*
		* @param level Level of the new command buffer (primary or secondary)
		* @param (Optional) begin If true, recording on the new command buffer will be started (vkBeginCommandBuffer) (Defaults to false)
		*
		* @return A handle to the allocated command buffer
		*/
		VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin = false)
		{
			VkCommandBufferAllocateInfo cmdBufAllocateInfo = vkUtils::initializers::commandBufferAllocateInfo(commandPool, level, 1);

			VkCommandBuffer cmdBuffer;
			VK_CHECK_RESULT(vkAllocateCommandBuffers(logicalDevice, &cmdBufAllocateInfo, &cmdBuffer));

			// If requested, also start recording for the new command buffer
			if (begin)
			{
				VkCommandBufferBeginInfo cmdBufInfo = vkUtils::initializers::commandBufferBeginInfo();
				VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
			}

			return cmdBuffer;
		}

		/**
		* Finish command buffer recording and submit it to a queue
		*
		* @param commandBuffer Command buffer to flush
		* @param queue Queue to submit the command buffer to
		* @param free (Optional) Free the command buffer once it has been submitted (Defaults to true)
		*
		* @note The queue that the command buffer is submitted to must be from the same family index as the pool it was allocated from
		* @note Uses a fence to ensure command buffer has finished executing
		*/
		void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free = true)
		{
			if (commandBuffer == VK_NULL_HANDLE)
			{
				return;
			}

			VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

			VkSubmitInfo submitInfo = vkUtils::initializers::submitInfo();
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffer;

			// Create fence to ensure that the command buffer has finished executing
			VkFenceCreateInfo fenceInfo = vkUtils::initializers::fenceCreateInfo(VK_FLAGS_NONE);
			VkFence fence;
			VK_CHECK_RESULT(vkCreateFence(logicalDevice, &fenceInfo, nullptr, &fence));

			// Submit to the queue
			VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));
			// Wait for the fence to signal that command buffer has finished executing
			VK_CHECK_RESULT(vkWaitForFences(logicalDevice, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));

			vkDestroyFence(logicalDevice, fence, nullptr);

			if (free)
			{
				vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
			}
		}

	};
}



/**
* @brief A simple Vulkan texture uploader for getting images into GPU memory
*/
class VulkanTextureLoader
{

private:
	vk::VulkanDevice *vulkanDevice;
	VkQueue queue;
	VkCommandBuffer cmdBuffer;
	VkCommandPool cmdPool;

public:

	/**
	* Default constructor
	*
	* @param vulkanDevice Pointer to a valid VulkanDevice
	* @param queue Queue for the copy commands when using staging (queue must support transfers)
	* @param cmdPool Commandpool used to get command buffers for copies and layout transitions
	*/
	VulkanTextureLoader(vk::VulkanDevice *vulkanDevice, VkQueue queue, VkCommandPool cmdPool)
	{
		this->vulkanDevice = vulkanDevice;
		this->queue = queue;
		this->cmdPool = cmdPool;

		// Create command buffer for submitting image barriers
		// and converting tilings
		VkCommandBufferAllocateInfo cmdBufInfo = {};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdBufInfo.commandPool = cmdPool;
		cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdBufInfo.commandBufferCount = 1;

		VK_CHECK_RESULT(vkAllocateCommandBuffers(vulkanDevice->logicalDevice, &cmdBufInfo, &cmdBuffer));
	}

	/**
	* Default destructor
	*
	* @note Does not free texture resources
	*/
	~VulkanTextureLoader()
	{
		vkFreeCommandBuffers(vulkanDevice->logicalDevice, cmdPool, 1, &cmdBuffer);
	}

	/**
	* Load a 2D texture including all mip levels
	*
	* @param filename File to load
	* @param format Vulkan format of the image data stored in the file
	* @param texture Pointer to the texture object to load the image into
	* @param (Optional) forceLinear Force linear tiling (not advised, defaults to false)
	* @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
	*
	* @note Only supports .ktx and .dds
	*/
	void loadTexture(std::string filename, VkFormat format, vkUtils::VulkanTexture *texture, bool forceLinear = false, VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT)
	{
#if defined(__ANDROID__)
		assert(assetManager != nullptr);

		// Textures are stored inside the apk on Android (compressed)
		// So they need to be loaded via the asset manager
		AAsset* asset = AAssetManager_open(assetManager, filename.c_str(), AASSET_MODE_STREAMING);
		assert(asset);
		size_t size = AAsset_getLength(asset);
		assert(size > 0);

		void *textureData = malloc(size);
		AAsset_read(asset, textureData, size);
		AAsset_close(asset);

		gli::texture2D tex2D(gli::load((const char*)textureData, size));

		free(textureData);
#else
		gli::texture2D tex2D(gli::load(filename.c_str()));
#endif		
		assert(!tex2D.empty());

		texture->width = static_cast<uint32_t>(tex2D[0].dimensions().x);
		texture->height = static_cast<uint32_t>(tex2D[0].dimensions().y);
		texture->mipLevels = static_cast<uint32_t>(tex2D.levels());

		// Get device properites for the requested texture format
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(vulkanDevice->physicalDevice, format, &formatProperties);

		// Only use linear tiling if requested (and supported by the device)
		// Support for linear tiling is mostly limited, so prefer to use
		// optimal tiling instead
		// On most implementations linear tiling will only support a very
		// limited amount of formats and features (mip maps, cubemaps, arrays, etc.)
		VkBool32 useStaging = !forceLinear;

		VkMemoryAllocateInfo memAllocInfo = vkUtils::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;

		// Use a separate command buffer for texture loading
		VkCommandBufferBeginInfo cmdBufInfo = vkUtils::initializers::commandBufferBeginInfo();
		VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));

		if (useStaging)
		{
			// Create a host-visible staging buffer that contains the raw image data
			VkBuffer stagingBuffer;
			VkDeviceMemory stagingMemory;

			VkBufferCreateInfo bufferCreateInfo = vkUtils::initializers::bufferCreateInfo();
			bufferCreateInfo.size = tex2D.size();
			// This buffer is used as a transfer source for the buffer copy
			bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VK_CHECK_RESULT(vkCreateBuffer(vulkanDevice->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

			// Get memory requirements for the staging buffer (alignment, memory type bits)
			vkGetBufferMemoryRequirements(vulkanDevice->logicalDevice, stagingBuffer, &memReqs);

			memAllocInfo.allocationSize = memReqs.size;
			// Get memory type index for a host visible buffer
			memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			VK_CHECK_RESULT(vkAllocateMemory(vulkanDevice->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
			VK_CHECK_RESULT(vkBindBufferMemory(vulkanDevice->logicalDevice, stagingBuffer, stagingMemory, 0));

			// Copy texture data into staging buffer
			uint8_t *data;
			VK_CHECK_RESULT(vkMapMemory(vulkanDevice->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
			memcpy(data, tex2D.data(), tex2D.size());
			vkUnmapMemory(vulkanDevice->logicalDevice, stagingMemory);

			// Setup buffer copy regions for each mip level
			std::vector<VkBufferImageCopy> bufferCopyRegions;
			uint32_t offset = 0;

			for (uint32_t i = 0; i < texture->mipLevels; i++)
			{
				VkBufferImageCopy bufferCopyRegion = {};
				bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				bufferCopyRegion.imageSubresource.mipLevel = i;
				bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
				bufferCopyRegion.imageSubresource.layerCount = 1;
				bufferCopyRegion.imageExtent.width = static_cast<uint32_t>(tex2D[i].dimensions().x);
				bufferCopyRegion.imageExtent.height = static_cast<uint32_t>(tex2D[i].dimensions().y);
				bufferCopyRegion.imageExtent.depth = 1;
				bufferCopyRegion.bufferOffset = offset;

				bufferCopyRegions.push_back(bufferCopyRegion);

				offset += static_cast<uint32_t>(tex2D[i].size());
			}

			// Create optimal tiled target image
			VkImageCreateInfo imageCreateInfo = vkUtils::initializers::imageCreateInfo();
			imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
			imageCreateInfo.format = format;
			imageCreateInfo.mipLevels = texture->mipLevels;
			imageCreateInfo.arrayLayers = 1;
			imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageCreateInfo.extent = { texture->width, texture->height, 1 };
			imageCreateInfo.usage = imageUsageFlags;
			// Ensure that the TRANSFER_DST bit is set for staging
			if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
			{
				imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			}
			VK_CHECK_RESULT(vkCreateImage(vulkanDevice->logicalDevice, &imageCreateInfo, nullptr, &texture->image));

			vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, texture->image, &memReqs);

			memAllocInfo.allocationSize = memReqs.size;

			memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(vulkanDevice->logicalDevice, &memAllocInfo, nullptr, &texture->deviceMemory));
			VK_CHECK_RESULT(vkBindImageMemory(vulkanDevice->logicalDevice, texture->image, texture->deviceMemory, 0));

			VkImageSubresourceRange subresourceRange = {};
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.levelCount = texture->mipLevels;
			subresourceRange.layerCount = 1;

			// Image barrier for optimal image (target)
			// Optimal image will be used as destination for the copy
			vkUtils::setImageLayout(
				cmdBuffer,
				texture->image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				subresourceRange);

			// Copy mip levels from staging buffer
			vkCmdCopyBufferToImage(
				cmdBuffer,
				stagingBuffer,
				texture->image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				static_cast<uint32_t>(bufferCopyRegions.size()),
				bufferCopyRegions.data()
				);

			// Change texture image layout to shader read after all mip levels have been copied
			texture->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			vkUtils::setImageLayout(
				cmdBuffer,
				texture->image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				texture->imageLayout,
				subresourceRange);

			// Submit command buffer containing copy and image layout commands
			VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuffer));

			// Create a fence to make sure that the copies have finished before continuing
			VkFence copyFence;
			VkFenceCreateInfo fenceCreateInfo = vkUtils::initializers::fenceCreateInfo(VK_FLAGS_NONE);
			VK_CHECK_RESULT(vkCreateFence(vulkanDevice->logicalDevice, &fenceCreateInfo, nullptr, &copyFence));

			VkSubmitInfo submitInfo = vkUtils::initializers::submitInfo();
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &cmdBuffer;

			VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, copyFence));

			VK_CHECK_RESULT(vkWaitForFences(vulkanDevice->logicalDevice, 1, &copyFence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));

			vkDestroyFence(vulkanDevice->logicalDevice, copyFence, nullptr);

			// Clean up staging resources
			vkFreeMemory(vulkanDevice->logicalDevice, stagingMemory, nullptr);
			vkDestroyBuffer(vulkanDevice->logicalDevice, stagingBuffer, nullptr);
		}
		else
		{
			// Prefer using optimal tiling, as linear tiling 
			// may support only a small set of features 
			// depending on implementation (e.g. no mip maps, only one layer, etc.)

			// Check if this support is supported for linear tiling
			assert(formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

			VkImage mappableImage;
			VkDeviceMemory mappableMemory;

			VkImageCreateInfo imageCreateInfo = vkUtils::initializers::imageCreateInfo();
			imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
			imageCreateInfo.format = format;
			imageCreateInfo.extent = { texture->width, texture->height, 1 };
			imageCreateInfo.mipLevels = 1;
			imageCreateInfo.arrayLayers = 1;
			imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
			imageCreateInfo.usage = imageUsageFlags;
			imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

			// Load mip map level 0 to linear tiling image
			VK_CHECK_RESULT(vkCreateImage(vulkanDevice->logicalDevice, &imageCreateInfo, nullptr, &mappableImage));

			// Get memory requirements for this image 
			// like size and alignment
			vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, mappableImage, &memReqs);
			// Set memory allocation size to required memory size
			memAllocInfo.allocationSize = memReqs.size;

			// Get memory type that can be mapped to host memory
			memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			// Allocate host memory
			VK_CHECK_RESULT(vkAllocateMemory(vulkanDevice->logicalDevice, &memAllocInfo, nullptr, &mappableMemory));

			// Bind allocated image for use
			VK_CHECK_RESULT(vkBindImageMemory(vulkanDevice->logicalDevice, mappableImage, mappableMemory, 0));

			// Get sub resource layout
			// Mip map count, array layer, etc.
			VkImageSubresource subRes = {};
			subRes.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subRes.mipLevel = 0;

			VkSubresourceLayout subResLayout;
			void *data;

			// Get sub resources layout 
			// Includes row pitch, size offsets, etc.
			vkGetImageSubresourceLayout(vulkanDevice->logicalDevice, mappableImage, &subRes, &subResLayout);

			// Map image memory
			VK_CHECK_RESULT(vkMapMemory(vulkanDevice->logicalDevice, mappableMemory, 0, memReqs.size, 0, &data));

			// Copy image data into memory
			memcpy(data, tex2D[subRes.mipLevel].data(), tex2D[subRes.mipLevel].size());

			vkUnmapMemory(vulkanDevice->logicalDevice, mappableMemory);

			// Linear tiled images don't need to be staged
			// and can be directly used as textures
			texture->image = mappableImage;
			texture->deviceMemory = mappableMemory;
			texture->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			// Setup image memory barrier
			vkUtils::setImageLayout(
				cmdBuffer,
				texture->image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_PREINITIALIZED,
				texture->imageLayout);

			// Submit command buffer containing copy and image layout commands
			VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuffer));

			VkFence nullFence = { VK_NULL_HANDLE };

			VkSubmitInfo submitInfo = vkUtils::initializers::submitInfo();
			submitInfo.waitSemaphoreCount = 0;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &cmdBuffer;

			VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, nullFence));
			VK_CHECK_RESULT(vkQueueWaitIdle(queue));
		}

		// Create sampler
		VkSamplerCreateInfo sampler = {};
		sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler.mipLodBias = 0.0f;
		sampler.compareOp = VK_COMPARE_OP_NEVER;
		sampler.minLod = 0.0f;
		// Max level-of-detail should match mip level count
		sampler.maxLod = (useStaging) ? (float)texture->mipLevels : 0.0f;
		// Enable anisotropic filtering
		sampler.maxAnisotropy = 8;
		sampler.anisotropyEnable = VK_TRUE;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(vulkanDevice->logicalDevice, &sampler, nullptr, &texture->sampler));

		// Create image view
		// Textures are not directly accessed by the shaders and
		// are abstracted by image views containing additional
		// information and sub resource ranges
		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.pNext = NULL;
		view.image = VK_NULL_HANDLE;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.format = format;
		view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		// Linear tiling usually won't support mip maps
		// Only set mip map count if optimal tiling is used
		view.subresourceRange.levelCount = (useStaging) ? texture->mipLevels : 1;
		view.image = texture->image;
		VK_CHECK_RESULT(vkCreateImageView(vulkanDevice->logicalDevice, &view, nullptr, &texture->view));

		// Fill descriptor image info that can be used for setting up descriptor sets
		texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		texture->descriptor.imageView = texture->view;
		texture->descriptor.sampler = texture->sampler;
	}

	/**
	* Load a cubemap texture including all mip levels from a single file
	*
	* @param filename File to load
	* @param format Vulkan format of the image data stored in the file
	* @param texture Pointer to the texture object to load the image into
	*
	* @note Only supports .ktx and .dds
	*/
	void loadCubemap(std::string filename, VkFormat format, vkUtils::VulkanTexture *texture, VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT)
	{
#if defined(__ANDROID__)
		assert(assetManager != nullptr);

		// Textures are stored inside the apk on Android (compressed)
		// So they need to be loaded via the asset manager
		AAsset* asset = AAssetManager_open(assetManager, filename.c_str(), AASSET_MODE_STREAMING);
		assert(asset);
		size_t size = AAsset_getLength(asset);
		assert(size > 0);

		void *textureData = malloc(size);
		AAsset_read(asset, textureData, size);
		AAsset_close(asset);

		gli::textureCube texCube(gli::load((const char*)textureData, size));

		free(textureData);
#else
		gli::textureCube texCube(gli::load(filename));
#endif	
		assert(!texCube.empty());

		texture->width = static_cast<uint32_t>(texCube.dimensions().x);
		texture->height = static_cast<uint32_t>(texCube.dimensions().y);
		texture->mipLevels = static_cast<uint32_t>(texCube.levels());

		VkMemoryAllocateInfo memAllocInfo = vkUtils::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;

		// Create a host-visible staging buffer that contains the raw image data
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;

		VkBufferCreateInfo bufferCreateInfo = vkUtils::initializers::bufferCreateInfo();
		bufferCreateInfo.size = texCube.size();
		// This buffer is used as a transfer source for the buffer copy
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK_RESULT(vkCreateBuffer(vulkanDevice->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

		// Get memory requirements for the staging buffer (alignment, memory type bits)
		vkGetBufferMemoryRequirements(vulkanDevice->logicalDevice, stagingBuffer, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		// Get memory type index for a host visible buffer
		memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(vulkanDevice->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
		VK_CHECK_RESULT(vkBindBufferMemory(vulkanDevice->logicalDevice, stagingBuffer, stagingMemory, 0));

		// Copy texture data into staging buffer
		uint8_t *data;
		VK_CHECK_RESULT(vkMapMemory(vulkanDevice->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
		memcpy(data, texCube.data(), texCube.size());
		vkUnmapMemory(vulkanDevice->logicalDevice, stagingMemory);

		// Setup buffer copy regions for each face including all of it's miplevels
		std::vector<VkBufferImageCopy> bufferCopyRegions;
		size_t offset = 0;

		for (uint32_t face = 0; face < 6; face++)
		{
			for (uint32_t level = 0; level < texture->mipLevels; level++)
			{
				VkBufferImageCopy bufferCopyRegion = {};
				bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				bufferCopyRegion.imageSubresource.mipLevel = level;
				bufferCopyRegion.imageSubresource.baseArrayLayer = face;
				bufferCopyRegion.imageSubresource.layerCount = 1;
				bufferCopyRegion.imageExtent.width = static_cast<uint32_t>(texCube[face][level].dimensions().x);
				bufferCopyRegion.imageExtent.height = static_cast<uint32_t>(texCube[face][level].dimensions().y);
				bufferCopyRegion.imageExtent.depth = 1;
				bufferCopyRegion.bufferOffset = offset;

				bufferCopyRegions.push_back(bufferCopyRegion);

				// Increase offset into staging buffer for next level / face
				offset += texCube[face][level].size();
			}
		}

		// Create optimal tiled target image
		VkImageCreateInfo imageCreateInfo = vkUtils::initializers::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = format;
		imageCreateInfo.mipLevels = texture->mipLevels;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.extent = { texture->width, texture->height, 1 };
		imageCreateInfo.usage = imageUsageFlags;
		// Ensure that the TRANSFER_DST bit is set for staging
		if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
		{
			imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}
		// Cube faces count as array layers in Vulkan
		imageCreateInfo.arrayLayers = 6;
		// This flag is required for cube map images
		imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;


		VK_CHECK_RESULT(vkCreateImage(vulkanDevice->logicalDevice, &imageCreateInfo, nullptr, &texture->image));

		vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, texture->image, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(vulkanDevice->logicalDevice, &memAllocInfo, nullptr, &texture->deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(vulkanDevice->logicalDevice, texture->image, texture->deviceMemory, 0));

		VkCommandBufferBeginInfo cmdBufInfo = vkUtils::initializers::commandBufferBeginInfo();
		VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));

		// Image barrier for optimal image (target)
		// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = texture->mipLevels;
		subresourceRange.layerCount = 6;

		vkUtils::setImageLayout(
			cmdBuffer,
			texture->image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		// Copy the cube map faces from the staging buffer to the optimal tiled image
		vkCmdCopyBufferToImage(
			cmdBuffer,
			stagingBuffer,
			texture->image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<uint32_t>(bufferCopyRegions.size()),
			bufferCopyRegions.data());

		// Change texture image layout to shader read after all faces have been copied
		texture->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vkUtils::setImageLayout(
			cmdBuffer,
			texture->image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			texture->imageLayout,
			subresourceRange);

		VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuffer));

		// Create a fence to make sure that the copies have finished before continuing
		VkFence copyFence;
		VkFenceCreateInfo fenceCreateInfo = vkUtils::initializers::fenceCreateInfo(VK_FLAGS_NONE);
		VK_CHECK_RESULT(vkCreateFence(vulkanDevice->logicalDevice, &fenceCreateInfo, nullptr, &copyFence));

		VkSubmitInfo submitInfo = vkUtils::initializers::submitInfo();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuffer;

		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, copyFence));

		VK_CHECK_RESULT(vkWaitForFences(vulkanDevice->logicalDevice, 1, &copyFence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));

		vkDestroyFence(vulkanDevice->logicalDevice, copyFence, nullptr);

		// Create sampler
		VkSamplerCreateInfo sampler = vkUtils::initializers::samplerCreateInfo();
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV = sampler.addressModeU;
		sampler.addressModeW = sampler.addressModeU;
		sampler.mipLodBias = 0.0f;
		sampler.maxAnisotropy = 8;
		sampler.compareOp = VK_COMPARE_OP_NEVER;
		sampler.minLod = 0.0f;
		sampler.maxLod = (float)texture->mipLevels;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(vulkanDevice->logicalDevice, &sampler, nullptr, &texture->sampler));

		// Create image view
		VkImageViewCreateInfo view = vkUtils::initializers::imageViewCreateInfo();
		view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		view.format = format;
		view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		view.subresourceRange.layerCount = 6;
		view.subresourceRange.levelCount = texture->mipLevels;
		view.image = texture->image;
		VK_CHECK_RESULT(vkCreateImageView(vulkanDevice->logicalDevice, &view, nullptr, &texture->view));

		// Clean up staging resources
		vkFreeMemory(vulkanDevice->logicalDevice, stagingMemory, nullptr);
		vkDestroyBuffer(vulkanDevice->logicalDevice, stagingBuffer, nullptr);

		// Fill descriptor image info that can be used for setting up descriptor sets
		texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		texture->descriptor.imageView = texture->view;
		texture->descriptor.sampler = texture->sampler;
	}

	/**
	* Load a texture array including all mip levels from a single file
	*
	* @param filename File to load
	* @param format Vulkan format of the image data stored in the file
	* @param texture Pointer to the texture object to load the image into
	*
	* @note Only supports .ktx and .dds
	*/
	void loadTextureArray(std::string filename, VkFormat format, vkUtils::VulkanTexture *texture, VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT)
	{
#if defined(__ANDROID__)
		assert(assetManager != nullptr);

		// Textures are stored inside the apk on Android (compressed)
		// So they need to be loaded via the asset manager
		AAsset* asset = AAssetManager_open(assetManager, filename.c_str(), AASSET_MODE_STREAMING);
		assert(asset);
		size_t size = AAsset_getLength(asset);
		assert(size > 0);

		void *textureData = malloc(size);
		AAsset_read(asset, textureData, size);
		AAsset_close(asset);

		gli::texture2DArray tex2DArray(gli::load((const char*)textureData, size));

		free(textureData);
#else
		gli::texture2DArray tex2DArray(gli::load(filename));
#endif	

		assert(!tex2DArray.empty());

		texture->width = static_cast<uint32_t>(tex2DArray.dimensions().x);
		texture->height = static_cast<uint32_t>(tex2DArray.dimensions().y);
		texture->layerCount = static_cast<uint32_t>(tex2DArray.layers());
		texture->mipLevels = static_cast<uint32_t>(tex2DArray.levels());

		VkMemoryAllocateInfo memAllocInfo = vkUtils::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;

		// Create a host-visible staging buffer that contains the raw image data
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;

		VkBufferCreateInfo bufferCreateInfo = vkUtils::initializers::bufferCreateInfo();
		bufferCreateInfo.size = tex2DArray.size();
		// This buffer is used as a transfer source for the buffer copy
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK_RESULT(vkCreateBuffer(vulkanDevice->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

		// Get memory requirements for the staging buffer (alignment, memory type bits)
		vkGetBufferMemoryRequirements(vulkanDevice->logicalDevice, stagingBuffer, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		// Get memory type index for a host visible buffer
		memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(vulkanDevice->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
		VK_CHECK_RESULT(vkBindBufferMemory(vulkanDevice->logicalDevice, stagingBuffer, stagingMemory, 0));

		// Copy texture data into staging buffer
		uint8_t *data;
		VK_CHECK_RESULT(vkMapMemory(vulkanDevice->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
		memcpy(data, tex2DArray.data(), static_cast<size_t>(tex2DArray.size()));
		vkUnmapMemory(vulkanDevice->logicalDevice, stagingMemory);

		// Setup buffer copy regions for each layer including all of it's miplevels
		std::vector<VkBufferImageCopy> bufferCopyRegions;
		size_t offset = 0;

		for (uint32_t layer = 0; layer < texture->layerCount; layer++)
		{
			for (uint32_t level = 0; level < texture->mipLevels; level++)
			{
				VkBufferImageCopy bufferCopyRegion = {};
				bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				bufferCopyRegion.imageSubresource.mipLevel = level;
				bufferCopyRegion.imageSubresource.baseArrayLayer = layer;
				bufferCopyRegion.imageSubresource.layerCount = 1;
				bufferCopyRegion.imageExtent.width = static_cast<uint32_t>(tex2DArray[layer][level].dimensions().x);
				bufferCopyRegion.imageExtent.height = static_cast<uint32_t>(tex2DArray[layer][level].dimensions().y);
				bufferCopyRegion.imageExtent.depth = 1;
				bufferCopyRegion.bufferOffset = offset;

				bufferCopyRegions.push_back(bufferCopyRegion);

				// Increase offset into staging buffer for next level / face
				offset += tex2DArray[layer][level].size();
			}
		}

		// Create optimal tiled target image
		VkImageCreateInfo imageCreateInfo = vkUtils::initializers::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = format;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.extent = { texture->width, texture->height, 1 };
		imageCreateInfo.usage = imageUsageFlags;
		// Ensure that the TRANSFER_DST bit is set for staging
		if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
		{
			imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}
		imageCreateInfo.arrayLayers = texture->layerCount;
		imageCreateInfo.mipLevels = texture->mipLevels;

		VK_CHECK_RESULT(vkCreateImage(vulkanDevice->logicalDevice, &imageCreateInfo, nullptr, &texture->image));

		vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, texture->image, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(vulkanDevice->logicalDevice, &memAllocInfo, nullptr, &texture->deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(vulkanDevice->logicalDevice, texture->image, texture->deviceMemory, 0));

		VkCommandBufferBeginInfo cmdBufInfo = vkUtils::initializers::commandBufferBeginInfo();
		VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));

		// Image barrier for optimal image (target)
		// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = texture->mipLevels;
		subresourceRange.layerCount = texture->layerCount;

		vkUtils::setImageLayout(
			cmdBuffer,
			texture->image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		// Copy the layers and mip levels from the staging buffer to the optimal tiled image
		vkCmdCopyBufferToImage(
			cmdBuffer,
			stagingBuffer,
			texture->image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<uint32_t>(bufferCopyRegions.size()),
			bufferCopyRegions.data());

		// Change texture image layout to shader read after all faces have been copied
		texture->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vkUtils::setImageLayout(
			cmdBuffer,
			texture->image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			texture->imageLayout,
			subresourceRange);

		VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuffer));

		// Create a fence to make sure that the copies have finished before continuing
		VkFence copyFence;
		VkFenceCreateInfo fenceCreateInfo = vkUtils::initializers::fenceCreateInfo(VK_FLAGS_NONE);
		VK_CHECK_RESULT(vkCreateFence(vulkanDevice->logicalDevice, &fenceCreateInfo, nullptr, &copyFence));

		VkSubmitInfo submitInfo = vkUtils::initializers::submitInfo();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuffer;

		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, copyFence));

		VK_CHECK_RESULT(vkWaitForFences(vulkanDevice->logicalDevice, 1, &copyFence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));

		vkDestroyFence(vulkanDevice->logicalDevice, copyFence, nullptr);

		// Create sampler
		VkSamplerCreateInfo sampler = vkUtils::initializers::samplerCreateInfo();
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV = sampler.addressModeU;
		sampler.addressModeW = sampler.addressModeU;
		sampler.mipLodBias = 0.0f;
		sampler.maxAnisotropy = 8;
		sampler.compareOp = VK_COMPARE_OP_NEVER;
		sampler.minLod = 0.0f;
		sampler.maxLod = (float)texture->mipLevels;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(vulkanDevice->logicalDevice, &sampler, nullptr, &texture->sampler));

		// Create image view
		VkImageViewCreateInfo view = vkUtils::initializers::imageViewCreateInfo();
		view.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		view.format = format;
		view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		view.subresourceRange.layerCount = texture->layerCount;
		view.subresourceRange.levelCount = texture->mipLevels;
		view.image = texture->image;
		VK_CHECK_RESULT(vkCreateImageView(vulkanDevice->logicalDevice, &view, nullptr, &texture->view));

		// Clean up staging resources
		vkFreeMemory(vulkanDevice->logicalDevice, stagingMemory, nullptr);
		vkDestroyBuffer(vulkanDevice->logicalDevice, stagingBuffer, nullptr);

		// Fill descriptor image info that can be used for setting up descriptor sets
		texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		texture->descriptor.imageView = texture->view;
		texture->descriptor.sampler = texture->sampler;
	}

	/**
	* Creates a 2D texture from a buffer
	*
	* @param buffer Buffer containing texture data to upload
	* @param bufferSize Size of the buffer in machine units
	* @param width Width of the texture to create
	* @param hidth Height of the texture to create
	* @param format Vulkan format of the image data stored in the file
	* @param texture Pointer to the texture object to load the image into
	* @param (Optional) filter Texture filtering for the sampler (defaults to VK_FILTER_LINEAR)
	* @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
	*/
	void createTexture(void* buffer, VkDeviceSize bufferSize, VkFormat format, uint32_t width, uint32_t height, vkUtils::VulkanTexture *texture, VkFilter filter = VK_FILTER_LINEAR, VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT)
	{
		assert(buffer);

		texture->width = width;
		texture->height = height;
		texture->mipLevels = 1;

		VkMemoryAllocateInfo memAllocInfo = vkUtils::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;

		// Use a separate command buffer for texture loading
		VkCommandBufferBeginInfo cmdBufInfo = vkUtils::initializers::commandBufferBeginInfo();
		VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));

		// Create a host-visible staging buffer that contains the raw image data
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;

		VkBufferCreateInfo bufferCreateInfo = vkUtils::initializers::bufferCreateInfo();
		bufferCreateInfo.size = bufferSize;
		// This buffer is used as a transfer source for the buffer copy
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK_RESULT(vkCreateBuffer(vulkanDevice->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

		// Get memory requirements for the staging buffer (alignment, memory type bits)
		vkGetBufferMemoryRequirements(vulkanDevice->logicalDevice, stagingBuffer, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		// Get memory type index for a host visible buffer
		memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(vulkanDevice->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
		VK_CHECK_RESULT(vkBindBufferMemory(vulkanDevice->logicalDevice, stagingBuffer, stagingMemory, 0));

		// Copy texture data into staging buffer
		uint8_t *data;
		VK_CHECK_RESULT(vkMapMemory(vulkanDevice->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
		memcpy(data, buffer, bufferSize);
		vkUnmapMemory(vulkanDevice->logicalDevice, stagingMemory);

		VkBufferImageCopy bufferCopyRegion = {};
		bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		bufferCopyRegion.imageSubresource.mipLevel = 0;
		bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
		bufferCopyRegion.imageSubresource.layerCount = 1;
		bufferCopyRegion.imageExtent.width = width;
		bufferCopyRegion.imageExtent.height = height;
		bufferCopyRegion.imageExtent.depth = 1;
		bufferCopyRegion.bufferOffset = 0;

		// Create optimal tiled target image
		VkImageCreateInfo imageCreateInfo = vkUtils::initializers::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = format;
		imageCreateInfo.mipLevels = texture->mipLevels;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.extent = { texture->width, texture->height, 1 };
		imageCreateInfo.usage = imageUsageFlags;
		// Ensure that the TRANSFER_DST bit is set for staging
		if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
		{
			imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}
		VK_CHECK_RESULT(vkCreateImage(vulkanDevice->logicalDevice, &imageCreateInfo, nullptr, &texture->image));

		vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, texture->image, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;

		memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(vulkanDevice->logicalDevice, &memAllocInfo, nullptr, &texture->deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(vulkanDevice->logicalDevice, texture->image, texture->deviceMemory, 0));

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = texture->mipLevels;
		subresourceRange.layerCount = 1;

		// Image barrier for optimal image (target)
		// Optimal image will be used as destination for the copy
		vkUtils::setImageLayout(
			cmdBuffer,
			texture->image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		// Copy mip levels from staging buffer
		vkCmdCopyBufferToImage(
			cmdBuffer,
			stagingBuffer,
			texture->image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&bufferCopyRegion
			);

		// Change texture image layout to shader read after all mip levels have been copied
		texture->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vkUtils::setImageLayout(
			cmdBuffer,
			texture->image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			texture->imageLayout,
			subresourceRange);

		// Submit command buffer containing copy and image layout commands
		VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuffer));

		// Create a fence to make sure that the copies have finished before continuing
		VkFence copyFence;
		VkFenceCreateInfo fenceCreateInfo = vkUtils::initializers::fenceCreateInfo(VK_FLAGS_NONE);
		VK_CHECK_RESULT(vkCreateFence(vulkanDevice->logicalDevice, &fenceCreateInfo, nullptr, &copyFence));

		VkSubmitInfo submitInfo = vkUtils::initializers::submitInfo();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuffer;

		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, copyFence));

		VK_CHECK_RESULT(vkWaitForFences(vulkanDevice->logicalDevice, 1, &copyFence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));

		vkDestroyFence(vulkanDevice->logicalDevice, copyFence, nullptr);

		// Clean up staging resources
		vkFreeMemory(vulkanDevice->logicalDevice, stagingMemory, nullptr);
		vkDestroyBuffer(vulkanDevice->logicalDevice, stagingBuffer, nullptr);

		// Create sampler
		VkSamplerCreateInfo sampler = {};
		sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler.magFilter = filter;
		sampler.minFilter = filter;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler.mipLodBias = 0.0f;
		sampler.compareOp = VK_COMPARE_OP_NEVER;
		sampler.minLod = 0.0f;
		sampler.maxLod = 0.0f;
		VK_CHECK_RESULT(vkCreateSampler(vulkanDevice->logicalDevice, &sampler, nullptr, &texture->sampler));

		// Create image view
		VkImageViewCreateInfo view = {};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.pNext = NULL;
		view.image = VK_NULL_HANDLE;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.format = format;
		view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		view.subresourceRange.levelCount = 1;
		view.image = texture->image;
		VK_CHECK_RESULT(vkCreateImageView(vulkanDevice->logicalDevice, &view, nullptr, &texture->view));

		// Fill descriptor image info that can be used for setting up descriptor sets
		texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		texture->descriptor.imageView = texture->view;
		texture->descriptor.sampler = texture->sampler;
	}

	/**
	* Free all Vulkan resources used by a texture object
	*
	* @param texture Texture object whose resources are to be freed
	*/
	void destroyTexture(vkUtils::VulkanTexture texture)
	{
		vkDestroyImageView(vulkanDevice->logicalDevice, texture.view, nullptr);
		vkDestroyImage(vulkanDevice->logicalDevice, texture.image, nullptr);
		vkDestroySampler(vulkanDevice->logicalDevice, texture.sampler, nullptr);
		vkFreeMemory(vulkanDevice->logicalDevice, texture.deviceMemory, nullptr);
	}
};

typedef struct _SwapChainBuffers {
	VkImage image;
	VkImageView view;
} SwapChainBuffer;

struct GLFWwindow;

class VulkanSwapChain
{
private:
	VkInstance instance;
	VkDevice device;
	VkPhysicalDevice physicalDevice;
	VkSurfaceKHR surface;

	// Function pointers
	PFN_vkGetPhysicalDeviceSurfaceSupportKHR fpGetPhysicalDeviceSurfaceSupportKHR;
	PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fpGetPhysicalDeviceSurfaceCapabilitiesKHR;
	PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fpGetPhysicalDeviceSurfaceFormatsKHR;
	PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fpGetPhysicalDeviceSurfacePresentModesKHR;
	PFN_vkCreateSwapchainKHR fpCreateSwapchainKHR;
	PFN_vkDestroySwapchainKHR fpDestroySwapchainKHR;
	PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR;
	PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR;
	PFN_vkQueuePresentKHR fpQueuePresentKHR;
public:
	VkFormat colorFormat;
	VkColorSpaceKHR colorSpace;
	/** @brief Handle to the current swap chain, required for recreation */
	VkSwapchainKHR swapChain = VK_NULL_HANDLE;
	uint32_t imageCount;
	std::vector<VkImage> images;
	std::vector<SwapChainBuffer> buffers;
	// Index of the deteced graphics and presenting device queue
	/** @brief Queue family index of the deteced graphics and presenting device queue */
	uint32_t queueNodeIndex = UINT32_MAX;

	// Creates an os specific surface
	/**
	* Create the surface object, an abstraction for the native platform window
	*
	* @pre Windows
	* @param platformHandle HINSTANCE of the window to create the surface for
	* @param platformWindow HWND of the window to create the surface for
	*
	* @pre Android
	* @param window A native platform window
	*
	* @pre Linux (XCB)
	* @param connection xcb connection to the X Server
	* @param window The xcb window to create the surface for
	* @note Targets other than XCB ar not yet supported
	*/
	void initSurface(GLFWwindow* window);

	/**
	* Set instance, physical and logical device to use for the swpachain and get all required function pointers
	*
	* @param instance Vulkan instance to use
	* @param physicalDevice Physical device used to query properties and formats relevant to the swapchain
	* @param device Logical representation of the device to create the swapchain for
	*
	*/
	void connect(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device)
	{
		this->instance = instance;
		this->physicalDevice = physicalDevice;
		this->device = device;

		GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceSurfaceSupportKHR);
		GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceSurfaceCapabilitiesKHR);
		GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceSurfaceFormatsKHR);
		GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceSurfacePresentModesKHR);
		GET_DEVICE_PROC_ADDR(device, CreateSwapchainKHR);
		GET_DEVICE_PROC_ADDR(device, DestroySwapchainKHR);
		GET_DEVICE_PROC_ADDR(device, GetSwapchainImagesKHR);
		GET_DEVICE_PROC_ADDR(device, AcquireNextImageKHR);
		GET_DEVICE_PROC_ADDR(device, QueuePresentKHR);
	}

	/**
	* Create the swapchain and get it's images with given width and height
	*
	* @param width Pointer to the width of the swapchain (may be adjusted to fit the requirements of the swapchain)
	* @param height Pointer to the height of the swapchain (may be adjusted to fit the requirements of the swapchain)
	* @param vsync (Optional) Can be used to force vsync'd rendering (by using VK_PRESENT_MODE_FIFO_KHR as presentation mode)
	*/
	void create(uint32_t *width, uint32_t *height, bool vsync = false)
	{
		VkResult err;
		VkSwapchainKHR oldSwapchain = swapChain;

		// Get physical device surface properties and formats
		VkSurfaceCapabilitiesKHR surfCaps;
		err = fpGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfCaps);
		assert(!err);

		// Get available present modes
		uint32_t presentModeCount;
		err = fpGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, NULL);
		assert(!err);
		assert(presentModeCount > 0);

		std::vector<VkPresentModeKHR> presentModes(presentModeCount);

		err = fpGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());
		assert(!err);

		VkExtent2D swapchainExtent = {};
		// If width (and height) equals the special value 0xFFFFFFFF, the size of the surface will be set by the swapchain
		if (surfCaps.currentExtent.width == (uint32_t)-1)
		{
			// If the surface size is undefined, the size is set to
			// the size of the images requested.
			swapchainExtent.width = *width;
			swapchainExtent.height = *height;
		}
		else
		{
			// If the surface size is defined, the swap chain size must match
			swapchainExtent = surfCaps.currentExtent;
			*width = surfCaps.currentExtent.width;
			*height = surfCaps.currentExtent.height;
		}


		// Select a present mode for the swapchain

		// The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
		// This mode waits for the vertical blank ("v-sync")
		VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

		// If v-sync is not requested, try to find a mailbox mode
		// It's the lowest latency non-tearing present mode available
		if (!vsync)
		{
			for (size_t i = 0; i < presentModeCount; i++)
			{
				if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
				{
					swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
					break;
				}
				if ((swapchainPresentMode != VK_PRESENT_MODE_MAILBOX_KHR) && (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR))
				{
					swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
				}
			}
		}

		// Determine the number of images
		uint32_t desiredNumberOfSwapchainImages = surfCaps.minImageCount + 1;
		if ((surfCaps.maxImageCount > 0) && (desiredNumberOfSwapchainImages > surfCaps.maxImageCount))
		{
			desiredNumberOfSwapchainImages = surfCaps.maxImageCount;
		}

		// Find the transformation of the surface
		VkSurfaceTransformFlagsKHR preTransform;
		if (surfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
		{
			// We prefer a non-rotated transform
			preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		}
		else
		{
			preTransform = surfCaps.currentTransform;
		}

		VkSwapchainCreateInfoKHR swapchainCI = {};
		swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchainCI.pNext = NULL;
		swapchainCI.surface = surface;
		swapchainCI.minImageCount = desiredNumberOfSwapchainImages;
		swapchainCI.imageFormat = colorFormat;
		swapchainCI.imageColorSpace = colorSpace;
		swapchainCI.imageExtent = { swapchainExtent.width, swapchainExtent.height };
		swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swapchainCI.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;
		swapchainCI.imageArrayLayers = 1;
		swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCI.queueFamilyIndexCount = 0;
		swapchainCI.pQueueFamilyIndices = NULL;
		swapchainCI.presentMode = swapchainPresentMode;
		swapchainCI.oldSwapchain = oldSwapchain;
		// Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
		swapchainCI.clipped = VK_TRUE;
		swapchainCI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

		err = fpCreateSwapchainKHR(device, &swapchainCI, nullptr, &swapChain);
		assert(!err);

		// If an existing sawp chain is re-created, destroy the old swap chain
		// This also cleans up all the presentable images
		if (oldSwapchain != VK_NULL_HANDLE)
		{
			for (uint32_t i = 0; i < imageCount; i++)
			{
				vkDestroyImageView(device, buffers[i].view, nullptr);
			}
			fpDestroySwapchainKHR(device, oldSwapchain, nullptr);
		}

		err = fpGetSwapchainImagesKHR(device, swapChain, &imageCount, NULL);
		assert(!err);

		// Get the swap chain images
		images.resize(imageCount);
		err = fpGetSwapchainImagesKHR(device, swapChain, &imageCount, images.data());
		assert(!err);

		// Get the swap chain buffers containing the image and imageview
		buffers.resize(imageCount);
		for (uint32_t i = 0; i < imageCount; i++)
		{
			VkImageViewCreateInfo colorAttachmentView = {};
			colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			colorAttachmentView.pNext = NULL;
			colorAttachmentView.format = colorFormat;
			colorAttachmentView.components = {
				VK_COMPONENT_SWIZZLE_R,
				VK_COMPONENT_SWIZZLE_G,
				VK_COMPONENT_SWIZZLE_B,
				VK_COMPONENT_SWIZZLE_A
			};
			colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			colorAttachmentView.subresourceRange.baseMipLevel = 0;
			colorAttachmentView.subresourceRange.levelCount = 1;
			colorAttachmentView.subresourceRange.baseArrayLayer = 0;
			colorAttachmentView.subresourceRange.layerCount = 1;
			colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
			colorAttachmentView.flags = 0;

			buffers[i].image = images[i];

			colorAttachmentView.image = buffers[i].image;

			err = vkCreateImageView(device, &colorAttachmentView, nullptr, &buffers[i].view);
			assert(!err);
		}
	}

	/**
	* Acquires the next image in the swap chain
	*
	* @param presentCompleteSemaphore (Optional) Semaphore that is signaled when the image is ready for use
	* @param imageIndex Pointer to the image index that will be increased if the next image could be acquired
	*
	* @note The function will always wait until the next image has been acquired by setting timeout to UINT64_MAX
	*
	* @return VkResult of the image acquisition
	*/
	VkResult acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t *imageIndex)
	{
		// By setting timeout to UINT64_MAX we will always wait until the next image has been acquired or an actual error is thrown
		// With that we don't have to handle VK_NOT_READY
		return fpAcquireNextImageKHR(device, swapChain, UINT64_MAX, presentCompleteSemaphore, (VkFence)nullptr, imageIndex);
	}

	/**
	* Queue an image for presentation
	*
	* @param queue Presentation queue for presenting the image
	* @param imageIndex Index of the swapchain image to queue for presentation
	* @param waitSemaphore (Optional) Semaphore that is waited on before the image is presented (only used if != VK_NULL_HANDLE)
	*
	* @return VkResult of the queue presentation
	*/
	VkResult queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore = VK_NULL_HANDLE)
	{
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pNext = NULL;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapChain;
		presentInfo.pImageIndices = &imageIndex;
		// Check if a wait semaphore has been specified to wait for before presenting the image
		if (waitSemaphore != VK_NULL_HANDLE)
		{
			presentInfo.pWaitSemaphores = &waitSemaphore;
			presentInfo.waitSemaphoreCount = 1;
		}
		return fpQueuePresentKHR(queue, &presentInfo);
	}


	/**
	* Destroy and free Vulkan resources used for the swapchain
	*/
	void cleanup()
	{
		if (swapChain != VK_NULL_HANDLE)
		{
			for (uint32_t i = 0; i < imageCount; i++)
			{
				vkDestroyImageView(device, buffers[i].view, nullptr);
			}
		}
		if (surface != VK_NULL_HANDLE)
		{
			fpDestroySwapchainKHR(device, swapChain, nullptr);
			vkDestroySurfaceKHR(instance, surface, nullptr);
		}
		surface = VK_NULL_HANDLE;
		swapChain = VK_NULL_HANDLE;
	}
};

struct SSceneLight
{
	glm::vec4 position;
	glm::vec3 color;
	float radius;
};

#endif //_VULKAN_UTILITIES_H_