/******************************************************************************/
/*!
\file	VulkanMeshLoader.h
\author David Grosman
\par    email: ToDavidGrosman\@gmail.com
\par    Project: CIS 565: GPU Programming and Architecture - Final Project.
\date   11/24/2016
\brief

Compiled using Microsoft (R) C/C++ Optimizing Compiler Version 18.00.21005.1 for
x86 which is my default VS2013 compiler.

This code is adapted from Sascha Willems's Vulkan Example: https://github.com/SaschaWillems/Vulkan 

This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)

*/
/******************************************************************************/

#ifndef _VULKAN_MESH_LOADER_H_
#define _VULKAN_MESH_LOADER_H_

#include <stdlib.h>
#include <string>
#include <fstream>
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <map>
#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#else
#endif

#include "vulkan/vulkan.h"
#include "VulkanUtilities.h"

#include <assimp/Importer.hpp> 
#include <assimp/scene.h>     
#include <assimp/postprocess.h>
#include <assimp/cimport.h>
#include <assimp/version.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "GfxScene.h"

namespace vkMeshLoader
{
	typedef unsigned char Byte;

	typedef enum VertexLayout {
		VERTEX_LAYOUT_POSITION = 0x0,
		VERTEX_LAYOUT_NORMAL = 0x1,
		VERTEX_LAYOUT_COLOR = 0x2,
		VERTEX_LAYOUT_UV = 0x3,
		VERTEX_LAYOUT_TANGENT = 0x4,
		VERTEX_LAYOUT_MATERIALID_NORMALIZED = 0x5,
		VERTEX_LAYOUT_BITANGENT = 0x6,
		VERTEX_LAYOUT_DUMMY_VEC4 = 0x7,
	};

	/**
	* Get the size of a vertex layout
	*
	* @param layout VertexLayout to get the size for
	*
	* @return Size of the vertex layout in bytes
	*/
	static uint32_t vertexSize(std::vector<vkMeshLoader::VertexLayout> layout)
	{
		uint32_t vSize = 0;
		for (auto& layoutDetail : layout)
		{
			switch (layoutDetail)
			{
				// UV only has two components
			case VERTEX_LAYOUT_UV:
				vSize += 2 * sizeof(float);
				break;
				// Normalized material ID is just float
			case VERTEX_LAYOUT_MATERIALID_NORMALIZED:
				vSize += sizeof(float);
				break;
			default:
				vSize += 3 * sizeof(float);
			}
		}
		return vSize;
	}

	struct MeshBufferInfo
	{
		VkBuffer buf = VK_NULL_HANDLE;
		VkDeviceMemory mem = VK_NULL_HANDLE;
		size_t size = 0;
	};

	/** @brief Stores a mesh's vertex and index descriptions */
	struct MeshDescriptor
	{
		uint32_t vertexCount;
		uint32_t indexBase;
		uint32_t indexCount;
	};

	/** @brief Mesh representation storing all data required to generate buffers */
	struct MeshBuffer
	{
		MeshBufferInfo vertices;
		MeshBufferInfo indices;
		uint32_t indexCount;
		glm::vec3 dim;
		std::vector<MeshDescriptor> meshDescriptors;
	};

	/** @brief Holds parameters for mesh creation */
	struct MeshCreateInfo
	{
		MeshCreateInfo()
		: m_pos(0.0f)
		, m_rotAxisAndAngle(1.0f, 0.0f, 0.0f, 0.0f)
		, m_scale(1.0f)
		, m_uvscale(1.0f)
		{}

		glm::vec3 m_pos;
		glm::vec4 m_rotAxisAndAngle;
		glm::vec3 m_scale;
		glm::vec2 m_uvscale;
	};
}

// Simple mesh class for getting all the necessary stuff from models loaded via ASSIMP
class VulkanMeshLoader
{
private:

	struct MeshEntry;

public:

	VulkanMeshLoader();
	~VulkanMeshLoader();

	bool LoadMesh(const std::string& filename, int flags = defaultFlags);
	bool LoadGLTFMesh(const std::string& filename);
	

private:

	void InitMesh(MeshEntry* meshEntry, const aiMesh* paiMesh, const aiScene* pScene);

	static const int defaultFlags = aiProcess_FlipWindingOrder | aiProcess_Triangulate | aiProcess_PreTransformVertices | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals;

	struct Vertex
	{
		glm::vec3 m_pos;
		glm::vec2 m_tex;
		glm::vec3 m_normal;
		glm::vec3 m_color;
		glm::vec3 m_tangent;
		glm::vec3 m_binormal;

		Vertex() {}

		Vertex(const glm::vec3& pos, const glm::vec2& tex, const glm::vec3& normal, const glm::vec3& tangent, const glm::vec3& bitangent, const glm::vec3& color)
		{
			m_pos = pos;
			m_tex = tex;
			m_normal = normal;
			m_color = color;
			m_tangent = tangent;
			m_binormal = bitangent;
		}
	};

	struct MeshEntry {
		uint32_t NumIndices;
		uint32_t MaterialIndex;
		uint32_t vertexBase;
		std::vector<Vertex> Vertices;
		std::vector<unsigned int> Indices;
	};

	std::vector<MeshEntry> m_Entries;

public:

	struct Dimension
	{
		glm::vec3 min = glm::vec3(FLT_MAX);
		glm::vec3 max = glm::vec3(-FLT_MAX);
		glm::vec3 size;
	} dim;

	uint32_t numVertices = 0;

	Assimp::Importer Importer;
	const aiScene* pScene;

	// For gltf mesh
	SSceneAttributes m_sceneAttributes;

	void createBuffers( vk::VulkanDevice* vkDevice,
		vkMeshLoader::MeshBuffer* meshBuffer,
		std::vector<vkMeshLoader::VertexLayout> layout,
		vkMeshLoader::MeshCreateInfo* createInfo,
		bool useStaging,
		VkCommandBuffer copyCmd,
		VkQueue copyQueue);

	static void destroyBuffers(VkDevice device, vkMeshLoader::MeshBuffer *meshBuffer);

};

#endif // _VULKAN_MESH_LOADER_H_