/******************************************************************************/
/*!
\file	Application.cpp
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
#include <time.h>  

//#include <AntTweakBar/AntTweakBar.h>
#include <GLFW/glfw3.h>
#include "Utilities.h"

#include "Application.h"


#include "GfxScene.h"
#include "VulkanDeferredRenderer.h"
#include "VulkanRaytracer.h"
#include "VulkanHybridRenderer.h"


//------------------------------
//-------GLFW CALLBACKS---------
//------------------------------
void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void mouseMotionCallback(GLFWwindow* window, double xpos, double ypos);
void mouseWheelCallback(GLFWwindow* window, double xoffset, double yoffset);

class CSceneRenderApp : public CApplication
{
public:

	CSceneRenderApp(int width, int height/*, const std::string& sceneFilename*/);
	virtual ~CSceneRenderApp();

	virtual void Update(float dt);

	Camera& getCamera() { return m_context.m_camera; }

public:

	SRendererContext m_context;
	VulkanRenderer* m_renderer;
};
static CSceneRenderApp* pScene = NULL;


CApplication::CApplication(int width, int height) :
	m_width(width),
	m_height(height),
	m_window(NULL),
	m_frame(0),
	m_fps(0),
	m_fpstracker(0)
{
	// Initialize glfw
	glfwInit();

	// Create window
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	m_window = glfwCreateWindow(m_width, m_height, "Hybrid Renderer", NULL, NULL);
	if (!m_window) {
		glfwTerminate();
		return;
	}
	glfwMakeContextCurrent(m_window);
	glfwSetKeyCallback(m_window, keyCallback);
	glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
	glfwSetCursorPosCallback(m_window, mouseMotionCallback);
	glfwSetScrollCallback(m_window, mouseWheelCallback);
}

CApplication::~CApplication()
{
	glfwDestroyWindow(m_window);
	glfwTerminate();
}

void CApplication::LaunchApplication(int argc, char **argv, int width, int height)
{
	// Extra filename
	//std::string inputFilename(argv[1]);
	CSceneRenderApp renderApp(width, height);
	renderApp.Run();
}


void CApplication::Run() {

	while (!glfwWindowShouldClose(m_window))
	{
		glfwPollEvents();
		static int samples = 0;
		static int fpsSamples = 0;
		static auto start = std::chrono::system_clock::now();
		auto now = std::chrono::system_clock::now();
		int64_t timeElapsedInMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
		if (timeElapsedInMs >= 1000)
		{
			m_fps = (int) ( m_fpstracker / (timeElapsedInMs / 1000) );
			m_fpstracker = 0;
			start = now;
			fpsSamples += m_fps;
			samples++;
		}

		std::string title = m_title + " | " + std::to_string(m_fps) + " FPS | " + std::to_string(timeElapsedInMs) + " ms";
		glfwSetWindowTitle(m_window, title.c_str());

		float timeElapsedInS = timeElapsedInMs / 1000.0f;
		Update(timeElapsedInS);

		const int NUM_SAMPLES = 100;
		if (samples == NUM_SAMPLES) {
			fpsSamples /= NUM_SAMPLES;
			std::cout << fpsSamples << std::endl;
			samples = 0;
			fpsSamples = 0;
		}

		m_frame++;
		m_fpstracker++;
		
	}
}

CSceneRenderApp::CSceneRenderApp(int width, int height/*, const std::string& sceneFilename*/) : CApplication(width, height)
{
	Camera& cam = m_context.m_camera;
	{
		cam.m_position = {0.f, 2.5f, -10.f };
		cam.setRotation(glm::vec3(0.f, 0.f, 0.0f));
		cam.setPerspective(60.0f, width / (float)height, 0.1f, 1000.0f);
	}
	m_context.m_window = m_window;

	const std::string fileName = "models/astronaut/astronauts.dae";
	m_renderer = new VulkanHybridRenderer(fileName);
	m_renderer->initVulkan(m_context, true);
	m_title = m_renderer->m_appName;

	pScene = this;
}

CSceneRenderApp::~CSceneRenderApp()
{
	m_renderer->shutdownVulkan();
	delete m_renderer;
}

void CSceneRenderApp::Update(float dt)
{
	bool updatedCam = m_context.m_camera.update(dt);
	if (updatedCam)
		m_renderer->viewChanged(m_context);
	m_renderer->render(m_context);
}

//------------------------------
//-------GLFW CALLBACKS---------
//------------------------------

void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	if (!pScene)
		return;

	if (action == GLFW_PRESS)
	{
		if (key == GLFW_KEY_ESCAPE)
			glfwSetWindowShouldClose(window, GL_TRUE);
	}
	else if (action == GLFW_RELEASE)
	{
		if (key == GLFW_KEY_F)
			pScene->m_context.m_debugDraw = !pScene->m_context.m_debugDraw;
	}

	Camera& cam = pScene->getCamera();
	{
		bool keyIsPressed = (action == GLFW_PRESS);
		switch (key)
		{
		case GLFW_KEY_W:
			cam.m_pressedKeys.m_up = keyIsPressed;
			break;
		case GLFW_KEY_S:
			cam.m_pressedKeys.m_down = keyIsPressed;
			break;
		case GLFW_KEY_A:
			cam.m_pressedKeys.m_left = keyIsPressed;
			break;
		case GLFW_KEY_D:
			cam.m_pressedKeys.m_right = keyIsPressed;
			break;
		}
	}

}


//-----------------------------
//---- Mouse control ----------
//-----------------------------

namespace
{
	enum ControlState { NONE = 0, ROTATE, TRANSLATE, ZOOM };
	static ControlState mouseState = NONE;
	static glm::vec2 curMousePos(0.0f, 0.0f);
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	if (action == GLFW_PRESS)
	{
		if (button == GLFW_MOUSE_BUTTON_LEFT)
		{
			mouseState = ROTATE;
		}
		else if (button == GLFW_MOUSE_BUTTON_MIDDLE)
		{
			mouseState = TRANSLATE;
		}
		else if (action == GLFW_MOUSE_BUTTON_RIGHT)
		{
			mouseState = ZOOM;
		}
	}
	else
	{
		mouseState = NONE;
	}
}

void mouseMotionCallback(GLFWwindow* window, double xpos, double ypos)
{
	if (!pScene)
		return;
	
	Camera& cam = pScene->getCamera();

	const double s_r = 0.01;
	const double s_t = 0.01;

	glm::vec2 nextMousePos(xpos, ypos);
	glm::vec2 diffMousePos = nextMousePos - curMousePos;

	if (mouseState == ROTATE)
	{
		cam.m_rotation.x -= (diffMousePos.y)* 1.25f;
		cam.m_rotation.y += (diffMousePos.x)* 1.25f;
		cam.rotate(glm::vec3(-diffMousePos.y, diffMousePos.x, 0.0f));
	}
	else if (mouseState == TRANSLATE)
	{
		cam.m_position.x += (diffMousePos.x)* 0.01f;
		cam.m_position.y += (diffMousePos.y)* 0.01f;
		cam.translate(glm::vec3(diffMousePos.x * 0.01f, diffMousePos.y * 0.01f, 0.0f));
	}

	curMousePos = nextMousePos;
}

void mouseWheelCallback(GLFWwindow* window, double wheelDeltaX, double wheelDeltaY)
{
	if (!pScene)
		return;

	Camera& cam = pScene->getCamera();

	cam.translate(glm::vec3(0.0f, 0.0f, (float)wheelDeltaY * 0.1f));
}
