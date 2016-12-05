/******************************************************************************/
/*!
\file	GfxScene.h
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

#ifndef _GFX_SCENE_H_
#define _GFX_SCENE_H_

#include <glm/glm.hpp>
#include <vector>
#include <map>

typedef unsigned char Byte;


enum EVertexAttributeType
{
	INDEX,
	POSITION,
	NORMAL,
	TEXCOORD
};

struct SVertexAttributes
{
	size_t m_byteStride;
	size_t m_count;
	int m_componentLength;
	int m_componentTypeByteSize;
};

struct SGeometryAttributes
{
	SGeometryAttributes() : m_vertexData(), m_vertexAttributes()
	{}

	std::map<EVertexAttributeType, std::vector<Byte>> m_vertexData;
	std::map<EVertexAttributeType, SVertexAttributes> m_vertexAttributes;
};

struct SMaterial
{
	glm::vec4	m_diffuse;
	glm::vec4	m_ambient;
	glm::vec4	m_emission;
	glm::vec4	m_specular;
	float		m_shininess;
	float		m_transparency;
	glm::ivec2  _pad;
};

struct SSceneAttributes
{
	std::vector<SGeometryAttributes> m_geometriesData;
	std::vector<SMaterial> m_materials;
	std::vector<glm::ivec4> m_indices; // The fourth element of indices is used as material IDs
	std::vector<glm::vec4> m_verticePositions;
	std::vector<glm::vec4> m_verticeNormals;
};

extern void generateSceneAttributes(const std::string& fileName, SSceneAttributes& outScene);

#endif // _GFX_SCENE_H_