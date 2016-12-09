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

This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)

*/
/******************************************************************************/


#include "VulkanMeshLoader.h"
#define TINYGLTF_LOADER_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <tinygltfloader/tiny_gltf_loader.h>

#include "Utilities.h"

typedef unsigned char Byte;

VulkanMeshLoader::VulkanMeshLoader()
{
}

VulkanMeshLoader::~VulkanMeshLoader()
{
	m_Entries.clear();
}

/**
* Load a scene from a supported 3D file format
*
* @param filename Name of the file (or asset) to load
* @param flags (Optional) Set of ASSIMP processing flags
*
* @return Returns true if the scene has been loaded
*/
bool VulkanMeshLoader::LoadMesh(const std::string& filename, int flags)
{
	bool loadedMesh = false;
	if (!nUtils::hasFileExt(filename.c_str(), "gltf") && !nUtils::hasFileExt(filename.c_str(), "glb"))
	{
		pScene = Importer.ReadFile(filename.c_str(), flags);
		unsigned int versMjr = aiGetVersionMajor();
		unsigned int versMnr = aiGetVersionMinor();
		if (pScene)
		{
			m_Entries.clear();
			m_Entries.resize(pScene->mNumMeshes);
			// Read in all meshes in the scene
			for (auto i = 0; i < m_Entries.size(); i++)
			{
				m_Entries[i].vertexBase = numVertices;
				numVertices += pScene->mMeshes[i]->mNumVertices;
				const aiMesh* paiMesh = pScene->mMeshes[i];
				InitMesh(&m_Entries[i], paiMesh, pScene);

				// Add to indices
				for (auto iCount = 0; iCount < m_Entries[i].Indices.size(); iCount += 3) {
					m_sceneAttributes.m_indices.push_back(
						glm::ivec4(
						m_Entries[i].Indices[iCount] + m_Entries[i].vertexBase,
						m_Entries[i].Indices[iCount + 1] + m_Entries[i].vertexBase,
						m_Entries[i].Indices[iCount + 2] + m_Entries[i].vertexBase,
							// Packed 4th element as material index
							m_Entries[i].MaterialIndex));
				}

				// Add to vertice positions and vertice normals
				for (auto v : m_Entries[i].Vertices) {
					m_sceneAttributes.m_verticePositions.push_back(glm::vec4(v.m_pos, 1));
					m_sceneAttributes.m_verticeNormals.push_back(glm::vec4(v.m_normal, 1));
				}
			}

			m_sceneAttributes.m_materials.resize(pScene->mNumMaterials);
			for (auto m = 0; m < pScene->mNumMaterials; ++m) {
				// Add to materials list
				SMaterial material;
				aiColor3D color;
				pScene->mMaterials[m]->Get(AI_MATKEY_COLOR_DIFFUSE, color);
				material.m_colorDiffuse = glm::vec4(color.r, color.g, color.b, 1.0f);
				pScene->mMaterials[m]->Get(AI_MATKEY_COLOR_SPECULAR, color);
				material.m_colorSpecular = glm::vec4(color.r, color.g, color.b, 1.0f);
				pScene->mMaterials[m]->Get(AI_MATKEY_COLOR_EMISSIVE, color);
				material.m_colorEmission = glm::vec4(color.r, color.g, color.b, 1.0f);
				pScene->mMaterials[m]->Get(AI_MATKEY_COLOR_AMBIENT, color);
				material.m_colorAmbient = glm::vec4(color.r, color.g, color.b, 1.0f);
				pScene->mMaterials[m]->Get(AI_MATKEY_COLOR_TRANSPARENT, color);
				material.m_colorTransparent = glm::vec4(color.r, color.g, color.b, 1.0f);
				pScene->mMaterials[m]->Get(AI_MATKEY_COLOR_REFLECTIVE, color);
				material.m_colorReflective = glm::vec4(color.r, color.g, color.b, 1.0f);

				pScene->mMaterials[m]->Get(AI_MATKEY_REFLECTIVITY, material.m_reflectivity);
				pScene->mMaterials[m]->Get(AI_MATKEY_REFRACTI, material.m_refracti);
				if (material.m_colorTransparent.x > 0.0) {
					material.m_refracti = 1.60;
				}
				pScene->mMaterials[m]->Get(AI_MATKEY_SHININESS_STRENGTH, material.m_shininess);

				m_sceneAttributes.m_materials[m] = material;
			}
	
			loadedMesh = true;
		}
	}
	else
	{
		loadedMesh = LoadGLTFMesh(filename);
	}
	
	if (!loadedMesh)
	{
		printf("Error parsing '%s': '%s'\n", filename.c_str(), Importer.GetErrorString());
		assert(false);
	}
	return loadedMesh;
}

/**
* Read mesh data from ASSIMP mesh to an internal mesh representation that can be used to generate Vulkan buffers
*
* @param meshEntry Pointer to the target MeshEntry strucutre for the mesh data
* @param paiMesh ASSIMP mesh to get the data from
* @param pScene Scene file of the ASSIMP mesh
*/
void VulkanMeshLoader::InitMesh(MeshEntry *meshEntry, const aiMesh* paiMesh, const aiScene* pScene)
{
	meshEntry->MaterialIndex = paiMesh->mMaterialIndex;

	aiColor3D pColor(0.f, 0.f, 0.f);
	pScene->mMaterials[paiMesh->mMaterialIndex]->Get(AI_MATKEY_COLOR_DIFFUSE, pColor);

	aiVector3D Zero3D(0.0f, 0.0f, 0.0f);

	for (unsigned int i = 0; i < paiMesh->mNumVertices; i++)
	{
		aiVector3D* pPos = &(paiMesh->mVertices[i]);
		aiVector3D* pNormal = &(paiMesh->mNormals[i]);
		aiVector3D* pTexCoord = (paiMesh->HasTextureCoords(0)) ? &(paiMesh->mTextureCoords[0][i]) : &Zero3D;
		aiVector3D* pTangent = (paiMesh->HasTangentsAndBitangents()) ? &(paiMesh->mTangents[i]) : &Zero3D;
		aiVector3D* pBiTangent = (paiMesh->HasTangentsAndBitangents()) ? &(paiMesh->mBitangents[i]) : &Zero3D;

		Vertex v(
			glm::vec3(pPos->x, -pPos->y, pPos->z),
			glm::vec2(pTexCoord->x, pTexCoord->y),
			glm::vec3(pNormal->x, pNormal->y, pNormal->z),
			glm::vec3(pTangent->x, pTangent->y, pTangent->z),
			glm::vec3(pBiTangent->x, pBiTangent->y, pBiTangent->z),
			glm::vec3(pColor.r, pColor.g, pColor.b)
			);

		dim.max.x = fmax(pPos->x, dim.max.x);
		dim.max.y = fmax(pPos->y, dim.max.y);
		dim.max.z = fmax(pPos->z, dim.max.z);

		dim.min.x = fmin(pPos->x, dim.min.x);
		dim.min.y = fmin(pPos->y, dim.min.y);
		dim.min.z = fmin(pPos->z, dim.min.z);

		meshEntry->Vertices.push_back(v);
	}

	dim.size = dim.max - dim.min;

	uint32_t indexBase = static_cast<uint32_t>(meshEntry->Indices.size());
	for (unsigned int i = 0; i < paiMesh->mNumFaces; i++)
	{
		const aiFace& Face = paiMesh->mFaces[i];
		if (Face.mNumIndices != 3)
			continue;
		meshEntry->Indices.push_back(indexBase + Face.mIndices[0]);
		meshEntry->Indices.push_back(indexBase + Face.mIndices[1]);
		meshEntry->Indices.push_back(indexBase + Face.mIndices[2]);
	}
}

// ==== Load glTF mesh ====

static std::map<int, int> GLTF_COMPONENT_LENGTH_LOOKUP = {
	{ TINYGLTF_TYPE_SCALAR, 1 },
	{ TINYGLTF_TYPE_VEC2, 2 },
	{ TINYGLTF_TYPE_VEC3, 3 },
	{ TINYGLTF_TYPE_VEC4, 4 },
	{ TINYGLTF_TYPE_MAT2, 4 },
	{ TINYGLTF_TYPE_MAT3, 9 },
	{ TINYGLTF_TYPE_MAT4, 16 }
};

static std::map<int, int> GLTF_COMPONENT_BYTE_SIZE_LOOKUP = {
	{ TINYGLTF_COMPONENT_TYPE_BYTE, 1 },
	{ TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE, 1 },
	{ TINYGLTF_COMPONENT_TYPE_SHORT, 2 },
	{ TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT, 2 },
	{ TINYGLTF_COMPONENT_TYPE_FLOAT, 4 }
};

static glm::mat4 GetMatrixFromGLTFNode(const tinygltf::Node & node) {

	glm::mat4 curMatrix(1.0);

	const std::vector<double> &matrix = node.matrix;
	if (matrix.size() > 0)
	{
		// matrix, copy it

		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				curMatrix[i][j] = (float)matrix.at(4 * i + j);
			}
		}
	}
	else
	{
		// no matrix, use rotation, scale, translation

		if (node.translation.size() > 0)
		{
			curMatrix[3][0] = node.translation[0];
			curMatrix[3][1] = node.translation[1];
			curMatrix[3][2] = node.translation[2];
		}

		if (node.rotation.size() > 0)
		{
			glm::mat4 R;
			glm::quat q;
			q[0] = node.rotation[0];
			q[1] = node.rotation[1];
			q[2] = node.rotation[2];

			R = glm::mat4_cast(q);
			curMatrix = curMatrix * R;
		}

		if (node.scale.size() > 0)
		{
			curMatrix = curMatrix * glm::scale(glm::mat4(1.0f), glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
		}
	}

	return curMatrix;
}


static void TraverseGLTFNode(
	std::map<std::string, glm::mat4> & n2m,
	const tinygltf::Scene & scene,
	const std::string & nodeString,
	const glm::mat4 & parentMatrix
	)
{
	const tinygltf::Node & node = scene.nodes.at(nodeString);
	glm::mat4 worldTransformationMatrix = parentMatrix * GetMatrixFromGLTFNode(node);
	n2m.insert(std::pair<std::string, glm::mat4>(nodeString, worldTransformationMatrix));

	for (auto& child : node.children)
	{
		TraverseGLTFNode(n2m, scene, child, worldTransformationMatrix);
	}
}

static std::string GetFilePathExtension(const std::string &FileName) {
	if (FileName.find_last_of(".") != std::string::npos)
		return FileName.substr(FileName.find_last_of(".") + 1);
	return "";
}

bool VulkanMeshLoader::LoadGLTFMesh(const std::string& fileName) 
{
	tinygltf::Scene scene;
	tinygltf::TinyGLTFLoader loader;
	std::string err;
	std::string ext = GetFilePathExtension(fileName);

	bool ret = false;
	if (ext.compare("glb") == 0) {
		// binary glTF.
		ret = loader.LoadBinaryFromFile(&scene, &err, fileName.c_str());
	}
	else {
		// ascii glTF.
		ret = loader.LoadASCIIFromFile(&scene, &err, fileName.c_str());
	}

	if (!err.empty())
	{
		printf("Err: %s\n", err.c_str());
	}

	if (!ret)
	{
		printf("Failed to parse glTF\n");
		return false;
	}

	// ----------- Transformation matrix --------- 
	std::map<std::string, glm::mat4> nodeString2Matrix;
	auto rootNodeNamesList = scene.scenes.at(scene.defaultScene);
	for (auto& sceneNode : rootNodeNamesList)
	{
		TraverseGLTFNode(nodeString2Matrix, scene, sceneNode, glm::mat4(1.0f));
	}

	// -------- For each mesh -----------

	for (auto& nodeString : nodeString2Matrix)
	{

		const tinygltf::Node& node = scene.nodes.at(nodeString.first);
		const glm::mat4 & matrix = nodeString.second;
		const glm::mat3 & matrixNormal = glm::transpose(glm::inverse(glm::mat3(matrix)));

		int materialId = 0;
		if (node.meshes.size() == 0) {
			continue;
		}
		m_Entries.clear();
		m_Entries.resize(node.meshes.size());
		for (auto e = 0; e < m_Entries.size(); e++)
		{
			auto meshName = node.meshes.at(e);
			auto& mesh = scene.meshes.at(meshName);

			for (size_t i = 0; i < mesh.primitives.size(); i++)
			{
				auto primitive = mesh.primitives[i];
				if (primitive.indices.empty())
				{
					return true;
				}

				// -------- Indices ----------
				{
					// Get accessor info
					auto indexAccessor = scene.accessors.at(primitive.indices);
					auto indexBufferView = scene.bufferViews.at(indexAccessor.bufferView);
					auto indexBuffer = scene.buffers.at(indexBufferView.buffer);

					int componentLength = GLTF_COMPONENT_LENGTH_LOOKUP.at(indexAccessor.type);
					int componentTypeByteSize = GLTF_COMPONENT_BYTE_SIZE_LOOKUP.at(indexAccessor.componentType);

					// Extra index data
					int bufferOffset = indexBufferView.byteOffset + indexAccessor.byteOffset;
					int bufferLength = indexAccessor.count * componentLength * componentTypeByteSize;
					auto first = indexBuffer.data.begin() + bufferOffset;
					auto last = indexBuffer.data.begin() + bufferOffset + bufferLength;
					std::vector<Byte> data = std::vector<Byte>(first, last);

					int indicesCount = indexAccessor.count;
					uint16_t* in = reinterpret_cast<uint16_t*>(data.data());
					
					uint32_t indexBase = static_cast<uint32_t>(m_Entries[e].Indices.size());
					for (auto iCount = 0; iCount < indicesCount; iCount += 3)
					{
						m_sceneAttributes.m_indices.push_back(glm::ivec4(in[iCount], in[iCount + 1], in[iCount + 2], materialId));
						m_Entries[e].Indices.push_back(indexBase + in[iCount]);
						m_Entries[e].Indices.push_back(indexBase + in[iCount + 1]);
						m_Entries[e].Indices.push_back(indexBase + in[iCount + 2]);
					}
					m_Entries[e].NumIndices = indicesCount;
				}

				// -------- Attributes -----------

				m_Entries[e].Vertices.clear();	

				for (auto& attribute : primitive.attributes)
				{

					// Get accessor info
					auto& accessor = scene.accessors.at(attribute.second);
					auto& bufferView = scene.bufferViews.at(accessor.bufferView);
					auto& buffer = scene.buffers.at(bufferView.buffer);
					int componentLength = GLTF_COMPONENT_LENGTH_LOOKUP.at(accessor.type);
					int componentTypeByteSize = GLTF_COMPONENT_BYTE_SIZE_LOOKUP.at(accessor.componentType);

					// Extra vertex data from buffer
					int bufferOffset = bufferView.byteOffset + accessor.byteOffset;
					int bufferLength = accessor.count * componentLength * componentTypeByteSize;
					auto first = buffer.data.begin() + bufferOffset;
					auto last = buffer.data.begin() + bufferOffset + bufferLength;
					std::vector<Byte> data = std::vector<Byte>(first, last);

					// -------- Position attribute -----------

					if (attribute.first.compare("POSITION") == 0)
					{
						int positionCount = accessor.count;
						// Update mesh entry
						m_Entries[e].vertexBase = numVertices;
						numVertices += positionCount;

						if (m_Entries[e].Vertices.size() == 0) {
							m_Entries[e].Vertices.resize(positionCount);
						}

						glm::vec3* positions = reinterpret_cast<glm::vec3*>(data.data());
						for (auto p = 0; p < positionCount; ++p)
						{
							positions[p] = glm::vec3(matrix * glm::vec4(positions[p], 1.0f));
							m_sceneAttributes.m_verticePositions.push_back(glm::vec4(positions[p], 1.0f));
							m_Entries[e].Vertices[p].m_pos = positions[p];

							dim.max.x = fmax(positions[p].x, dim.max.x);
							dim.max.y = fmax(positions[p].y, dim.max.y);
							dim.max.z = fmax(positions[p].z, dim.max.z);

							dim.min.x = fmin(positions[p].x, dim.min.x);
							dim.min.y = fmin(positions[p].y, dim.min.y);
							dim.min.z = fmin(positions[p].z, dim.min.z);
						}
						
						dim.size = dim.max - dim.min;

					}

					// -------- Normal attribute -----------

					else if (attribute.first.compare("NORMAL") == 0)
					{
						int normalCount = accessor.count;
						if (m_Entries[e].Vertices.size() == 0) {
							m_Entries[e].Vertices.resize(normalCount);
						}

						glm::vec3* normals = reinterpret_cast<glm::vec3*>(data.data());
						for (auto p = 0; p < normalCount; ++p)
						{
							normals[p] = glm::normalize(matrixNormal * glm::vec4(normals[p], 1.0f));
							m_sceneAttributes.m_verticeNormals.push_back(glm::vec4(normals[p], 0.0f));
							m_Entries[e].Vertices[p].m_normal = normals[p];

						}
					}

					// -------- Texcoord attribute -----------

					else if (attribute.first.compare("TEXCOORD_0") == 0)
					{
						int texcoordCount = accessor.count;
						if (m_Entries[e].Vertices.size() == 0) {
							m_Entries[e].Vertices.resize(texcoordCount);
						}

						glm::vec2* texcoords = reinterpret_cast<glm::vec2*>(data.data());
						for (auto p = 0; p < texcoordCount; ++p)
						{
							m_Entries[e].Vertices[p].m_tex = texcoords[p];
						}
					}

					// ----------Materials-------------

					//TextureData* dev_diffuseTex = NULL;
					int diffuseTexWidth = 0;
					int diffuseTexHeight = 0;
					SMaterial material;
					if (!primitive.material.empty())
					{
						const tinygltf::Material &mat = scene.materials.at(primitive.material);
						printf("material.name = %s\n", mat.name.c_str());

						if (mat.values.find("diffuse") != mat.values.end())
						{
							std::string diffuseTexName = mat.values.at("diffuse").string_value;
							if (scene.textures.find(diffuseTexName) != scene.textures.end())
							{
								const tinygltf::Texture &tex = scene.textures.at(diffuseTexName);
								if (scene.images.find(tex.source) != scene.images.end())
								{
									const tinygltf::Image &image = scene.images.at(tex.source);

									// Texture bytes
									size_t s = image.image.size() * sizeof(Byte);
									diffuseTexWidth = image.width;
									diffuseTexHeight = image.height;
								}
							}
							else
							{
								auto diff = mat.values.at("diffuse").number_array;
								material.m_colorDiffuse = glm::vec4(diff.at(0), diff.at(1), diff.at(2), diff.at(3));
							}
						}

						if (mat.values.find("ambient") != mat.values.end())
						{
							auto amb = mat.values.at("ambient").number_array;
							material.m_colorAmbient = glm::vec4(amb.at(0), amb.at(1), amb.at(2), amb.at(3));
						}
						if (mat.values.find("emission") != mat.values.end())
						{
							auto em = mat.values.at("emission").number_array;
							material.m_colorEmission = glm::vec4(em.at(0), em.at(1), em.at(2), em.at(3));

						}
						if (mat.values.find("specular") != mat.values.end())
						{
							auto spec = mat.values.at("specular").number_array;
							material.m_colorSpecular = glm::vec4(spec.at(0), spec.at(1), spec.at(2), spec.at(3));

						}
						if (mat.values.find("shininess") != mat.values.end())
						{
							material.m_shininess = mat.values.at("shininess").number_array.at(0);
						}

						if (mat.values.find("transparency") != mat.values.end())
						{
							material.m_refracti = mat.values.at("transparency").number_array.at(0);
						}
						else
						{
							material.m_refracti = 1.0f;
						}

						m_sceneAttributes.m_materials.push_back(material);
						++materialId;
					}
				}
			}
		}
	}
	return true;
}

/**
* Create Vulkan buffers for the index and vertex buffer using a vertex layout
*
* @note Only does staging if a valid command buffer and transfer queue are passed
*
* @param meshBuffer Pointer to the mesh buffer containing buffer handles and memory
* @param layout Vertex layout for the vertex buffer
* @param createInfo Structure containing information for mesh creation time (center, scaling, etc.)
* @param useStaging If true, buffers are staged to device local memory
* @param copyCmd (Required for staging) Command buffer to put the copy commands into
* @param copyQueue (Required for staging) Queue to put copys into
*/
void VulkanMeshLoader::createBuffers(
	vk::VulkanDevice* vkDevice,
	vkMeshLoader::MeshBuffer* meshBuffer,
	std::vector<vkMeshLoader::VertexLayout> layout,
	vkMeshLoader::MeshCreateInfo* createInfo,
	bool useStaging,
	VkCommandBuffer copyCmd,
	VkQueue copyQueue)
{
	vkMeshLoader::MeshCreateInfo meshInfo;
	if (createInfo != nullptr)
	{
		meshInfo.m_pos = createInfo->m_pos;
		meshInfo.m_rotAxisAndAngle = createInfo->m_rotAxisAndAngle;
		meshInfo.m_scale = createInfo->m_scale;
		meshInfo.m_uvscale = createInfo->m_uvscale;
	}

	std::vector<float> vertexBuffer;
	for (int m = 0; m < m_Entries.size(); m++)
	{
		for (int i = 0; i < m_Entries[m].Vertices.size(); i++)
		{
			// Push vertex data depending on layout
			for (auto& layoutDetail : layout)
			{
				// Position
				if (layoutDetail == vkMeshLoader::VERTEX_LAYOUT_POSITION)
				{
					glm::mat4 modelWorldMtx;
					{
						glm::mat4 scaleMtx = glm::scale(meshInfo.m_scale);
						glm::mat4 transMtx = glm::translate(meshInfo.m_pos);						

						glm::vec3 rotAxis = glm::vec3(meshInfo.m_rotAxisAndAngle);
						float rotAngle = meshInfo.m_rotAxisAndAngle.w;
						glm::mat4 rotMtx = glm::rotate(rotAngle, rotAxis);

						modelWorldMtx = transMtx * rotMtx * scaleMtx;
					}


					glm::vec4 outVtx = modelWorldMtx * glm::vec4(m_Entries[m].Vertices[i].m_pos, 1.0f);
					vertexBuffer.push_back(outVtx.x);
					vertexBuffer.push_back(outVtx.y);
					vertexBuffer.push_back(outVtx.z);
				}
				// Normal
				if (layoutDetail == vkMeshLoader::VERTEX_LAYOUT_NORMAL)
				{
					vertexBuffer.push_back(m_Entries[m].Vertices[i].m_normal.x);
					vertexBuffer.push_back(-m_Entries[m].Vertices[i].m_normal.y);
					vertexBuffer.push_back(m_Entries[m].Vertices[i].m_normal.z);
				}
				// Texture coordinates
				if (layoutDetail == vkMeshLoader::VERTEX_LAYOUT_UV)
				{
					vertexBuffer.push_back(m_Entries[m].Vertices[i].m_tex.s * meshInfo.m_uvscale.s);
					vertexBuffer.push_back(m_Entries[m].Vertices[i].m_tex.t * meshInfo.m_uvscale.t);
				}
				// Color
				if (layoutDetail == vkMeshLoader::VERTEX_LAYOUT_COLOR)
				{
					vertexBuffer.push_back(m_Entries[m].Vertices[i].m_color.r);
					vertexBuffer.push_back(m_Entries[m].Vertices[i].m_color.g);
					vertexBuffer.push_back(m_Entries[m].Vertices[i].m_color.b);
				}
				// Tangent
				if (layoutDetail == vkMeshLoader::VERTEX_LAYOUT_TANGENT)
				{
					vertexBuffer.push_back(m_Entries[m].Vertices[i].m_tangent.x);
					vertexBuffer.push_back(m_Entries[m].Vertices[i].m_tangent.y);
					vertexBuffer.push_back(m_Entries[m].Vertices[i].m_tangent.z);
				}

				// Material ID normalized
				if (layoutDetail == vkMeshLoader::VERTEX_LAYOUT_MATERIALID_NORMALIZED)
				{
					// Store material Id into the position vector to save space
					float materialIdNormalized = m_Entries[m].MaterialIndex / (float)pScene->mNumMaterials;
					vertexBuffer.push_back(materialIdNormalized);
				}

				// Bitangent
				if (layoutDetail == vkMeshLoader::VERTEX_LAYOUT_BITANGENT)
				{
					vertexBuffer.push_back(m_Entries[m].Vertices[i].m_binormal.x);
					vertexBuffer.push_back(m_Entries[m].Vertices[i].m_binormal.y);
					vertexBuffer.push_back(m_Entries[m].Vertices[i].m_binormal.z);
				}

				if (layoutDetail == vkMeshLoader::VERTEX_LAYOUT_DUMMY_VEC4)
				{
					vertexBuffer.push_back(0.0f);
					vertexBuffer.push_back(0.0f);
					vertexBuffer.push_back(0.0f);
					vertexBuffer.push_back(0.0f);
				}
			}
		}
	}
	meshBuffer->vertices.size = vertexBuffer.size() * sizeof(float);

	dim.min *= meshInfo.m_scale;
	dim.max *= meshInfo.m_scale;
	dim.size *= meshInfo.m_scale;

	std::vector<uint32_t> indexBuffer;
	for (uint32_t m = 0; m < m_Entries.size(); m++)
	{
		uint32_t indexBase = static_cast<uint32_t>(indexBuffer.size());
		for (uint32_t i = 0; i < m_Entries[m].Indices.size(); i++)
		{
			indexBuffer.push_back(m_Entries[m].Indices[i] + indexBase);
		}
		vkMeshLoader::MeshDescriptor descriptor{};
		descriptor.indexBase = indexBase;
		descriptor.indexCount = static_cast<uint32_t>(m_Entries[m].Indices.size());
		descriptor.vertexCount = static_cast<uint32_t>(m_Entries[m].Vertices.size());
		meshBuffer->meshDescriptors.push_back(descriptor);
	}
	meshBuffer->indices.size = indexBuffer.size() * sizeof(uint32_t);
	meshBuffer->indexCount = static_cast<uint32_t>(indexBuffer.size());

	// Use staging buffer to move vertex and index buffer to device local memory
	if (useStaging && copyQueue != VK_NULL_HANDLE && copyCmd != VK_NULL_HANDLE)
	{
		// Create staging buffers
		struct {
			VkBuffer buffer;
			VkDeviceMemory memory;
		} vertexStaging, indexStaging;

		// Vertex buffer
		vkDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			meshBuffer->vertices.size,
			&vertexStaging.buffer,
			&vertexStaging.memory,
			vertexBuffer.data());

		// Index buffer
		vkDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			meshBuffer->indices.size,
			&indexStaging.buffer,
			&indexStaging.memory,
			indexBuffer.data());

		// Create device local target buffers
		// Vertex buffer
		vkDevice->createBuffer(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			meshBuffer->vertices.size,
			&meshBuffer->vertices.buf,
			&meshBuffer->vertices.mem);

		// Index buffer
		vkDevice->createBuffer(
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			meshBuffer->indices.size,
			&meshBuffer->indices.buf,
			&meshBuffer->indices.mem);

		// Copy from staging buffers
		VkCommandBufferBeginInfo cmdBufInfo = vkUtils::initializers::commandBufferBeginInfo();
		VK_CHECK_RESULT(vkBeginCommandBuffer(copyCmd, &cmdBufInfo));

		VkBufferCopy copyRegion = {};

		copyRegion.size = meshBuffer->vertices.size;
		vkCmdCopyBuffer(
			copyCmd,
			vertexStaging.buffer,
			meshBuffer->vertices.buf,
			1,
			&copyRegion);

		copyRegion.size = meshBuffer->indices.size;
		vkCmdCopyBuffer(
			copyCmd,
			indexStaging.buffer,
			meshBuffer->indices.buf,
			1,
			&copyRegion);

		VK_CHECK_RESULT(vkEndCommandBuffer(copyCmd));

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &copyCmd;

		VK_CHECK_RESULT(vkQueueSubmit(copyQueue, 1, &submitInfo, VK_NULL_HANDLE));
		VK_CHECK_RESULT(vkQueueWaitIdle(copyQueue));

		vkDestroyBuffer(vkDevice->logicalDevice, vertexStaging.buffer, nullptr);
		vkFreeMemory(vkDevice->logicalDevice, vertexStaging.memory, nullptr);
		vkDestroyBuffer(vkDevice->logicalDevice, indexStaging.buffer, nullptr);
		vkFreeMemory(vkDevice->logicalDevice, indexStaging.memory, nullptr);
	}
	else
	{
		// Generate vertex buffer
		vkDevice->createBuffer(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			meshBuffer->vertices.size,
			&meshBuffer->vertices.buf,
			&meshBuffer->vertices.mem,
			vertexBuffer.data());

		// Generate index buffer
		vkDevice->createBuffer(
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			meshBuffer->indices.size,
			&meshBuffer->indices.buf,
			&meshBuffer->indices.mem,
			indexBuffer.data());
	}
}

void VulkanMeshLoader::destroyBuffers(VkDevice device, vkMeshLoader::MeshBuffer *meshBuffer)
{
	vkDestroyBuffer(device, meshBuffer->vertices.buf, nullptr);
	vkFreeMemory(device, meshBuffer->vertices.mem, nullptr);
	if (meshBuffer->indices.buf != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(device, meshBuffer->indices.buf, nullptr);
		vkFreeMemory(device, meshBuffer->indices.mem, nullptr);
	}
}

