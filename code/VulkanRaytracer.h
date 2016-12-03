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
	VulkanRaytracer();
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
	struct SVkVertices
	{
		VkPipelineVertexInputStateCreateInfo			m_inputState;
		std::vector<VkVertexInputBindingDescription>	m_bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription>	m_attributeDescriptions;
	};

	struct SSceneMeshes
	{
		vkMeshLoader::MeshBuffer m_model;
		vkMeshLoader::MeshBuffer m_quad;
	};

	struct SVkDescriptorSets
	{
		VkDescriptorSet m_compute;
		VkDescriptorSet m_graphics;
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

	void generateQuad();
	void loadMeshes();

	void buildRaytracingCommandBuffer();

private:
	SVkVertices				m_vertices;
	SSceneMeshes			m_sceneMeshes;

	VkDescriptorSetLayout	m_descriptorSetLayout;
	SVkDescriptorSets		m_descriptorSets;

	SVkPipelines			m_pipelines;
	SVkPipelinesLayout		m_pipelineLayouts;



	struct Compute {
		// -- Compute compatible queue
		VkQueue queue;
		VkFence fence;

		// -- Pipeline
		VkPipelineLayout pipelineLayout;

		// -- Commands
		VkCommandPool commandPool;
		VkCommandBuffer commandBuffer;

		struct {
			// -- Uniform buffer
			vkUtils::UniformData camera;
			vkUtils::UniformData materials;

			// -- Shapes buffers
			vk::Buffer meshes;

		} buffers;

		// -- Output storage image
		vkUtils::VulkanTexture storageRaytraceImage;

		// -- Uniforms
		struct CameraUniform { // Compute shader uniform block object
			glm::vec4 position = glm::vec4(0.0, 2.5f, 15.0f, 1.0f);
			glm::vec4 right = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);;
			glm::vec4 lookat = glm::vec4(0.0, 2.5f, 0.0f, 0.0f);
			glm::vec4 forward;
			glm::vec4 up = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
			glm::vec2 pixelLength;
			float fov = 40.0f;
			float aspectRatio = 45.0f;
		} cameraUnif;

		VkSemaphore semaphore;

	} m_compute;

};
