//
// Copyright (c) 2013 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#include <stdio.h>
#include <string.h>
#include <iconv.h>
#include <math.h>
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>
#include "nanovg.h"
#define NANOVG_GL3_IMPLEMENTATION
#include "nanovg_gl3buf.h"
#include "mgui.h"

int mbut = 0;

void errorcb(int error, const char* desc)
{
	printf("GLFW error: %s\n", desc);
}

static void key(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
}

static void mousebutton(GLFWwindow* window, int button, int action, int mods)
{
	(void)window;
	if (button == GLFW_MOUSE_BUTTON_LEFT ) {
		if (action == GLFW_PRESS) {
			mbut |= MG_MOUSE_PRESSED;
		}
		if (action == GLFW_RELEASE) {
			mbut |= MG_MOUSE_RELEASED;
		}
	}
}

int main()
{
	GLFWwindow* window;
	struct NVGcontext* vg = NULL;
	int i;
	int blending = 0;
	float opacity = 0.5f;
	float position[3] = {100.0f, 120.0f, 234.0f};
	float color[4] = {255/255.0f,192/255.0f,0/255.0f,255/255.0f};
	float iterations = 42;
	int cull = 1;
	char name[64] = "Mikko";
	const char* choices[] = { "Normal", "Minimum Color", "Screen Door", "Maximum Velocity" };

	printf("start\n");

	if (!glfwInit()) {
		printf("Failed to init GLFW.");
		return -1;
	}

	glfwSetErrorCallback(errorcb);

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(1000, 600, "mg", NULL, NULL);
	if (!window) {
		glfwTerminate();
		return -1;
	}

	glfwSetKeyCallback(window, key);

	glfwMakeContextCurrent(window);

	vg = nvgCreateGL3(512, 512, NVG_ANTIALIAS);
	if (vg == NULL) {
		printf("Could not init nanovg.\n");
		return -1;
	}
	if (nvgCreateFont(vg, "sans", "../example/Roboto-Regular.ttf") == -1) {
		printf("Could not add font italic.\n");
		return -1;
	}
	if (nvgCreateFont(vg, "sans-bold", "../example/Roboto-Bold.ttf") == -1) {
		printf("Could not add font bold.\n");
		return -1;
	}

	mgInit();


	glfwSetTime(0);

	while (!glfwWindowShouldClose(window))
	{
		double mx, my;
		
		int winWidth, winHeight;
		int fbWidth, fbHeight;
		float pxRatio;

		glfwGetCursorPos(window, &mx, &my);
		glfwGetWindowSize(window, &winWidth, &winHeight);
		glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
		// Calculate pixel ration for hi-dpi devices.
		pxRatio = (float)fbWidth / (float)winWidth;

		// Update and render
		glViewport(0, 0, fbWidth, fbHeight);
		glClearColor(0.3f, 0.3f, 0.32f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_DEPTH_TEST);

/*		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0,width,height,0,-1,1);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glDisable(GL_DEPTH_TEST);
		glColor4ub(255,255,255,255);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);*/

		float t = glfwGetTime();
		float x,y,popy;

		nvgBeginFrame(vg, winWidth, winHeight, pxRatio, NVG_STRAIGHT_ALPHA);

		mgFrameBegin(vg, winWidth, winHeight, mx, my, mbut);
		mbut = 0;

		// Menu bar
//		mgBeginPanel("Menu", 0,0, winWidth, 30, MG_ROW, MG_JUSTIFY, 0, 0,0);
		mgPanelBegin(MG_ROW, 0,0, winWidth, 30, mgArgs(mgAlign(MG_JUSTIFY)));
			mgItem("File", mgArgs(0));
			mgItem("Edit", mgArgs(0));
			mgItem("Tools", mgArgs(0));
			mgItem("View", mgArgs(0));
		mgPanelEnd();

//		mgBeginPanel("NavMesh Options", 20,50, 250, MG_AUTO, MG_COL, MG_JUSTIFY, 0, 5);
//		mgBeginPanel("NavMesh Options", 20,50, 250, winHeight - 50, MG_COL, MG_JUSTIFY, MG_SCROLL, 5,5);
		mgPanelBegin(MG_COL, 20,50, 250, winHeight - 50, mgArgs(mgAlign(MG_JUSTIFY), mgOverflow(MG_SCROLL), mgPadding(5,5)));

//		mgText("NavMesh Options", );

		mgLabel("Blending", mgArgs(0));
		mgSelect(&blending, choices, 4, mgArgs(0));

		mgLabel("Opacity", mgArgs(0));
		mgSlider(&opacity, 0.0f, 1.0f, mgArgs(0));

		mgLabel("Iterations", mgArgs(0));
		mgNumber(&iterations, mgArgs(0));

		mgLabel("Position", mgArgs(0));
		mgNumber3(&position[0], &position[1], &position[3], "mm", mgArgs(0));

		mgLabel("Color", mgArgs(0));
		mgColor(&color[0], &color[1], &color[2], &color[3], mgArgs(0));

		mgCheckBox("Cull Enabled", &cull, mgArgs(0));
		mgLabel("Name", mgArgs(0));

		mgInput(name, 64, mgArgs(0));
		if (mgButton("Build", mgArgs(0))) {
			printf("Build!!");
		}

		mgBoxBegin(MG_ROW, mgArgs(0));
			mgBoxBegin(MG_COL, mgArgs(mgGrow(1), mgAlign(MG_JUSTIFY)));
				mgButton("Build1", mgArgs(0));
				mgButton("Build2", mgArgs(0));
				mgButton("Build3", mgArgs(0));
			mgBoxEnd();
			mgButton("Build4", mgArgs(0));
		mgBoxEnd();

		mgPanelEnd();

		mgFrameEnd();

/*		nvgBeginPath(vg);
		nvgRoundedRect(vg, 20, 20, 200, 30, 5);
		nvgFillColor(vg, nvgRGBA(255,192,0,255));
		nvgFill(vg);

		nvgFontFace(vg, "sans");
		nvgFontSize(vg, 32);
		nvgFillColor(vg, nvgRGBA(255,255,255,255));
		nvgTextAlign(vg, NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
		nvgText(vg, 100, 100, "Mikko", NULL);

		nvgText(vg, 102, 102, "Mikko", NULL);*/

		nvgEndFrame(vg);

		glEnable(GL_DEPTH_TEST);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	nvgDeleteGL3(vg);

	mgTerminate();

	glfwTerminate();
	return 0;
}
