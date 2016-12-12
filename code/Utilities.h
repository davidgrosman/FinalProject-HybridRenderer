/******************************************************************************/
/*!
\file	Utilities.h
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

#ifndef _UTILITIES_H_
#define _UTILITIES_H_

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gli/gli.hpp>

#include <GLFW/glfw3.h>

namespace nUtils
{
	static bool hasFileExt(const std::string& s, const char* ext) {

		size_t i = s.rfind('.', s.length());
		if (i != std::string::npos) {
			return (s.compare(i + 1, std::string::npos, ext) == 0);
		}

		return false;
	}
};

class Camera
{
public:

	struct InputKeys
	{
		bool m_left;
		bool m_right;
		bool m_up;
		bool m_down;
	};

	struct SCamMatrices
	{
		glm::mat4 m_viewMtx;
		glm::mat4 m_projMtx;
	};

public:

	Camera();

	void updateAspectRatio(float aspect);
	void setPerspective(float fov, float aspect, float znear, float zfar);
	

	void setRotation(glm::vec3 rotation);
	void rotate(glm::vec3 delta);

	void setTranslation(glm::vec3 translation);
	void translate(glm::vec3 delta);

	bool update(float deltaTime);

public:
	
	InputKeys m_pressedKeys;

	SCamMatrices m_matrices;
	glm::vec3 m_rotation = glm::vec3();
	glm::vec3 m_position = glm::vec3();

private:

	void updateViewMatrix();

	float	m_fov;
	float	m_znear, m_zfar;
	bool	m_isUpdated;
};

struct SRendererContext
{
	SRendererContext() : m_window(NULL), m_debugDraw(false), m_enableBVH(false)
	{}
	void getWindowSize(uint32_t& width, uint32_t& height);

	GLFWwindow* m_window;
	Camera		m_camera;

	bool		m_debugDraw;
	bool		m_enableBVH;
};

#endif // _UTILITIES_H_