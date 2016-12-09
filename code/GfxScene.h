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

struct SMaterial
{
	glm::vec4	m_colorDiffuse;
	glm::vec4	m_colorAmbient;
	glm::vec4	m_colorEmission;
	glm::vec4	m_colorSpecular;
	glm::vec4   m_colorReflective;
	glm::vec4   m_colorTransparent;
	float		m_shininess;
	float		m_refracti;
	float	    m_reflectivity;
	float       _pad;
};

struct SSceneAttributes
{
	std::vector<glm::ivec4> m_indices; // The fourth element of indices is used as material IDs
	std::vector<glm::vec4> m_verticePositions;
	std::vector<glm::vec4> m_verticeNormals;
	std::vector<SMaterial> m_materials;
};

#endif // _GFX_SCENE_H_