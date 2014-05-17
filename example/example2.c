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
#include "nanovg_gl.h"
#include "milli.h"
#define NANOSVG_IMPLEMENTATION 1
#include "nanosvg.h"

struct MIinputState input = { 0 };

void errorcb(int error, const char* desc)
{
	printf("GLFW error %d: %s\n", error, desc);
}


static int isPrintable(int key)
{
	switch (key) {
	case GLFW_KEY_ESCAPE:
	case GLFW_KEY_ENTER:
	case GLFW_KEY_TAB:
	case GLFW_KEY_BACKSPACE:
	case GLFW_KEY_INSERT:
	case GLFW_KEY_DELETE:
	case GLFW_KEY_RIGHT:
	case GLFW_KEY_LEFT:
	case GLFW_KEY_DOWN:
	case GLFW_KEY_UP:
	case GLFW_KEY_PAGE_UP:
	case GLFW_KEY_PAGE_DOWN:
	case GLFW_KEY_HOME:
	case GLFW_KEY_END:
	case GLFW_KEY_CAPS_LOCK:
	case GLFW_KEY_SCROLL_LOCK:
	case GLFW_KEY_NUM_LOCK:
	case GLFW_KEY_PRINT_SCREEN:
	case GLFW_KEY_PAUSE:
	case GLFW_KEY_F1:
	case GLFW_KEY_F2:
	case GLFW_KEY_F3:
	case GLFW_KEY_F4:
	case GLFW_KEY_F5:
	case GLFW_KEY_F6:
	case GLFW_KEY_F7:
	case GLFW_KEY_F8:
	case GLFW_KEY_F9:
	case GLFW_KEY_F10:
	case GLFW_KEY_F11:
	case GLFW_KEY_F12:
	case GLFW_KEY_F13:
	case GLFW_KEY_F14:
	case GLFW_KEY_F15:
	case GLFW_KEY_F16:
	case GLFW_KEY_F17:
	case GLFW_KEY_F18:
	case GLFW_KEY_F19:
	case GLFW_KEY_F20:
	case GLFW_KEY_F21:
	case GLFW_KEY_F22:
	case GLFW_KEY_F23:
	case GLFW_KEY_F24:
	case GLFW_KEY_F25:
	case GLFW_KEY_KP_ENTER:
	case GLFW_KEY_LEFT_SHIFT:
	case GLFW_KEY_LEFT_CONTROL:
	case GLFW_KEY_LEFT_ALT:
	case GLFW_KEY_LEFT_SUPER:
	case GLFW_KEY_RIGHT_SHIFT:
	case GLFW_KEY_RIGHT_CONTROL:
	case GLFW_KEY_RIGHT_ALT:
	case GLFW_KEY_RIGHT_SUPER:
	case GLFW_KEY_MENU:
		return 0;
	}
	return 1;
}

int printable = 0;

static void keycb(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	(void)scancode;
	(void)mods;
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
	if (action == GLFW_PRESS)
		printable = isPrintable(key);

	if (action == GLFW_PRESS) {
		if (!isPrintable(key)) {
			if (input.nkeys < MI_MAX_INPUTKEYS) {
				input.keys[input.nkeys].type = MI_KEYPRESSED;
				input.keys[input.nkeys].code = key;
				input.nkeys++;
			}
		}
	}
	if (action == GLFW_RELEASE) {
		if (!isPrintable(key)) {
			if (input.nkeys < MI_MAX_INPUTKEYS) {
				input.keys[input.nkeys].type = MI_KEYRELEASED;
				input.keys[input.nkeys].code = key;
				input.nkeys++;
			}
		}
	}
}

static void charcb(GLFWwindow* window, unsigned int codepoint)
{
	(void)window;

	if (printable) {
		if (input.nkeys < MI_MAX_INPUTKEYS) {
			input.keys[input.nkeys].type = MI_CHARTYPED;
			input.keys[input.nkeys].code = codepoint;
			input.nkeys++;
		}
	}
}

static void buttoncb(GLFWwindow* window, int button, int action, int mods)
{
	(void)window;
	(void)mods;
	if (button == GLFW_MOUSE_BUTTON_LEFT ) {
		if (action == GLFW_PRESS) {
			input.mbut |= MI_MOUSE_PRESSED;
		}
		if (action == GLFW_RELEASE) {
			input.mbut |= MI_MOUSE_RELEASED;
		}
	}
}


struct MIcell* createPanel()
{
	struct MIcell* panel = NULL;
	struct MIcell* search = NULL;
	struct MIcell* footer = NULL;
	struct MIcell* button = NULL;

	panel = miCreateBox("id=panel dir=col align=justify padding='5 5' width=250 height=400");

	miAddChild(panel, miCreateText("id=header label='Materials' font-size=24 spacing=10"));

	search = miCreateBox("id=search dir=row align=justify padding='2 2' spacing=5");
		miAddChild(search, miCreateIcon("id=search-icon icon=search"));
		miAddChild(search, miCreateText("id=search-input label='Search' grow=1 paddingx=5"));
		miAddChild(search, miCreateIcon("id=search-clear icon=cancel-circled"));
	miAddChild(panel, search);

	miAddChild(panel, miCreateBox("id=list dir=col grow=1 spacing=5"));

	footer = miCreateBox("id=footer dir=row pack=end");
		miAddChild(footer, miCreateIconButton("id=footer-add icon=plus label=Add spacing=5"));
		miAddChild(footer, miCreateButton("id=footer-remove label=Remove spacing=5"));
	miAddChild(panel, footer);

	return panel;
}

int main()
{
	GLFWwindow* window;
	struct NVGcontext* vg = NULL;

/*	int blending = 0;
	float opacity = 0.5f;
	float position[3] = {100.0f, 120.0f, 234.0f};
	float color[4] = {255/255.0f,192/255.0f,0/255.0f,255/255.0f};
	float iterations = 42;
	int cull = 1;
	char name[64] = "Mikko";
	const char* choices[] = { "Normal", "Minimum Color", "Screen Door", "Maximum Velocity" };
	float scroll = 30;*/

	struct MIcell* panel = NULL;
	float iconScale = 21.0f/1000.0f;

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

	glfwSetKeyCallback(window, keycb);
	glfwSetCharCallback(window, charcb);
    glfwSetMouseButtonCallback(window, buttoncb);

	glfwMakeContextCurrent(window);

	vg = nvgCreateGL3(512, 512, NVG_ANTIALIAS);
	if (vg == NULL) {
		printf("Could not init nanovg.\n");
		return -1;
	}
	if (nvgCreateFont(vg, "sans", "../example/fonts/Roboto-Regular.ttf") == -1) {
		printf("Could not add font italic.\n");
		return -1;
	}
	if (nvgCreateFont(vg, "sans-bold", "../example/fonts/Roboto-Bold.ttf") == -1) {
		printf("Could not add font bold.\n");
		return -1;
	}

	miInit();

	if (miCreateIconImage("check", "../example/icons/check.svg", iconScale)) {
		printf("Could not create icon 'check'.\n");
		return -1;
	}
	if (miCreateIconImage("arrow-combo", "../example/icons/arrow-combo.svg", iconScale)) {
		printf("Could not create icon 'arrow-combo'.\n");
		return -1;
	}
	if (miCreateIconImage("tools", "../example/icons/tools.svg", iconScale)) {
		printf("Could not create icon 'tool'.\n");
		return -1;
	}
	if (miCreateIconImage("cancel-circled", "../example/icons/cancel-circled.svg", iconScale)) {
		printf("Could not create icon 'cancel-circled'.\n");
		return -1;
	}
	if (miCreateIconImage("plus", "../example/icons/plus.svg", iconScale)) {
		printf("Could not create icon 'plus'.\n");
		return -1;
	}
	if (miCreateIconImage("search", "../example/icons/search.svg", iconScale)) {
		printf("Could not create icon 'search'.\n");
		return -1;
	}

	panel = createPanel();

	miLayout(panel, vg);

	glfwSetTime(0);

	while (!glfwWindowShouldClose(window))
	{
		double mx, my;
		int winWidth, winHeight;
		int fbWidth, fbHeight;
		float pxRatio;

//		float t = glfwGetTime();
//		float x,y,popy;
//		struct MGhit* hit;

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

		nvgBeginFrame(vg, winWidth, winHeight, pxRatio, NVG_STRAIGHT_ALPHA);

		input.mx = mx;
		input.my = my;
		miFrameBegin(vg, winWidth, winHeight, &input);
		input.nkeys = 0;
		input.mbut = 0;
		
		miRender(panel, vg);

		miFrameEnd();

		nvgEndFrame(vg);

		glEnable(GL_DEPTH_TEST);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	nvgDeleteGL3(vg);

	miFreeCell(panel);

	miTerminate();

	glfwTerminate();
	return 0;
}
