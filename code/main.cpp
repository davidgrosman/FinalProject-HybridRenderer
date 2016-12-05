/******************************************************************************/
/*!
\file	main.cpp
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
#include "Application.h"

int main(int argc, char **argv) 
{
	// Launch our application using the Vulkan API
	CApplication::LaunchApplication(argc, argv, 800, 800);
}