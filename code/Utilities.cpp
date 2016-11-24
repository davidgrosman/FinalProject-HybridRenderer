/******************************************************************************/
/*!
\file	Utilities.cpp
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



#include "Utilities.h"

void SRendererContext::getWindowSize(uint32_t& width, uint32_t& height)
{
	//http://www.glfw.org/docs/latest/group__window.html
	int iWidth = 0; int iHeight = 0;
	glfwGetWindowSize(m_window, &iWidth, &iHeight);
	width = iWidth; height = iHeight;
}

Camera::Camera()
{
	m_pressedKeys.m_left = false; m_pressedKeys.m_right = false;
	m_pressedKeys.m_down = false; m_pressedKeys.m_up = false;

	m_isUpdated = true;
}

void Camera::updateAspectRatio(float aspect)
{
	m_matrices.m_projMtx = glm::perspective(glm::radians(m_fov), aspect, m_znear, m_zfar);
}

void Camera::setPerspective(float fov, float aspect, float znear, float zfar)
{
	m_fov = fov;
	m_znear = znear;
	m_zfar = zfar;
	m_matrices.m_projMtx = glm::perspective(glm::radians(m_fov), aspect, znear, zfar);
}

void Camera::setRotation(glm::vec3 rotation)
{
	m_rotation = rotation;
	updateViewMatrix();
}

void Camera::rotate(glm::vec3 delta)
{
	m_rotation += delta;
	updateViewMatrix();
}

void Camera::setTranslation(glm::vec3 translation)
{
	m_position = translation;
	updateViewMatrix();
}

void Camera::translate(glm::vec3 delta)
{
	m_position += delta;
	updateViewMatrix();
}

bool Camera::update(float deltaTime)
{
	if (m_pressedKeys.m_left || m_pressedKeys.m_right || m_pressedKeys.m_up || m_pressedKeys.m_down)
	{
		glm::vec3 camFront;
		camFront.x = -cos(glm::radians(m_rotation.x)) * sin(glm::radians(m_rotation.y));
		camFront.y = sin(glm::radians(m_rotation.x));
		camFront.z = cos(glm::radians(m_rotation.x)) * cos(glm::radians(m_rotation.y));
		camFront = glm::normalize(camFront);

		float moveSpeed = deltaTime;

		if (m_pressedKeys.m_up)
			m_position += camFront * moveSpeed;
		if (m_pressedKeys.m_down)
			m_position -= camFront * moveSpeed;
		if (m_pressedKeys.m_left)
			m_position -= glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;
		if (m_pressedKeys.m_right)
			m_position += glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;

		updateViewMatrix();
	}

	bool retVal = !m_isUpdated;
	m_isUpdated = true;
	return retVal;
}

void Camera::updateViewMatrix()
{
	glm::mat4 rotM = glm::mat4();
	glm::mat4 transM;

	rotM = glm::rotate(rotM, glm::radians(m_rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	rotM = glm::rotate(rotM, glm::radians(m_rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	rotM = glm::rotate(rotM, glm::radians(m_rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	transM = glm::translate(glm::mat4(), m_position);

	m_matrices.m_viewMtx = rotM * transM;
	m_isUpdated = false;
}








