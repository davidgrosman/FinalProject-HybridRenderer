#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>

#include "Utilities.h"

#include "VulkanUtilities.h"
#include "VulkanRenderer.h"
#include "GfxScene.h"

#define VERTEX_BUFFER_BIND_ID 0
#define ENABLE_VALIDATION true


class VulkanRaytracer : public VulkanRenderer {

public:
	VulkanRaytracer(const std::string& fileName);
	~VulkanRaytracer();

	virtual void draw(SRendererContext& context) override;
	virtual void shutdownVulkan() override;

	////////////////////////////////////////////////////////////////////////////////////////////////
	////////					VulkanRenderer::VulkanInit() Interface						////////

	void setupUniformBuffers(SRendererContext& context) override;

	// set up the resources (eg: desriptorPool, descriptorSetLayout) that are going to be used by the descriptors.
	void setupDescriptorFramework() override;

	// set up the scene shaders' descriptors.
	void setupDescriptors() override;

	void setupPipelines() override;

	void buildCommandBuffers() override;

	/////////////////////////////////////////////////////////////////////////////////////////////////
	////////					Event-Handler Functions  								     ////////

	void toggleDebugDisplay() override;

	// Called when view change occurs
	// Can be overriden in derived class to e.g. update uniform buffers 
	// Containing view dependant matrices
	void viewChanged(SRendererContext& context) override;

private:
	struct SVkDescriptorSets
	{
		VkDescriptorSet m_compute;
		VkDescriptorSet m_graphics;
	};

	struct SVkDescriptorSetLayouts
	{
		VkDescriptorSetLayout m_compute;
		VkDescriptorSetLayout m_graphics;
	};

	struct SVkPipelinesLayout
	{
		VkPipelineLayout m_graphics;
		VkPipelineLayout m_compute;
	};

	struct SVkPipelines
	{
		VkPipeline m_compute;
		VkPipeline m_graphics;
	};

private:

	void prepareResources();
	void generateQuad();
	void loadMeshes();
	void prepareTextureTarget(vkUtils::VulkanTexture *tex, uint32_t width, uint32_t height, VkFormat format);

	void buildRaytracingCommandBuffer();

private:
	SSceneAttributes		m_sceneAttributes;

	SVkDescriptorSetLayouts	m_descriptorSetLayouts;
	SVkDescriptorSets		m_descriptorSets;

	SVkPipelines			m_pipelines;
	SVkPipelinesLayout		m_pipelineLayouts;

	struct Compute {
		// -- Compute compatible queue
		VkQueue queue;
		VkFence fence;

		// -- Commands
		VkCommandBuffer commandBuffer;

		struct {
			// -- Uniform buffer
			vkUtils::UniformData camera;
			vkUtils::UniformData materials;

			// -- Shapes buffers
			vk::Buffer indices;
			vk::Buffer positions;
			vk::Buffer normals;

		} buffers;

		// -- Output storage image
		vkUtils::VulkanTexture storageRaytraceImage;

		// -- Uniforms
		struct CameraUniform { // Compute shader uniform block object
			glm::vec4 position = glm::vec4(0.0, -2.5f, 5.0f, 1.0f);
			glm::vec4 right = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);;
			glm::vec4 lookat = glm::vec4(0.0, 0.f, 0.0f, 0.0f);
			glm::vec4 forward;
			glm::vec4 up = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
			glm::vec2 pixelLength;
			float fov = 60.0f;
			float aspectRatio = 45.0f;
		} cameraUnif;

		VkSemaphore semaphore;

	} m_compute;

};
