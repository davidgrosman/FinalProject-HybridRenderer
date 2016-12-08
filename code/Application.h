/******************************************************************************/
/*!
\file	Application.h
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

#ifndef _APPLICATION_H_
#define _APPLICATION_H_

struct GLFWwindow;

class CApplication
{

public:
	CApplication(int width, int height);
	~CApplication();

	static void LaunchApplication(int argc, char **argv, int width = 1280, int height = 720);

	/**
	* \brief This is the main loop of Application
	*/
	virtual void Run();

	// Update specific-application stuff.
	virtual void Update(float dt) = 0;

protected:
	int m_width;
	int m_height;
	std::string m_title;

	GLFWwindow* m_window;

	int		m_frame;
	int		m_fps;
	int		m_fpstracker;
};

#endif // _APPLICATION_H_