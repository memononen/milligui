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
#include "milli2.h"
#define NANOSVG_IMPLEMENTATION 1
#include "nanosvg.h"

MIinputState input;

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
				input.keys[input.nkeys].mods = mods;
				input.nkeys++;
			}
		}
	}
	if (action == GLFW_RELEASE) {
		if (!isPrintable(key)) {
			if (input.nkeys < MI_MAX_INPUTKEYS) {
				input.keys[input.nkeys].type = MI_KEYRELEASED;
				input.keys[input.nkeys].code = key;
				input.keys[input.nkeys].mods = mods;
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
			input.keys[input.nkeys].mods = 0;
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

/*


- row
	- left
	- right
	- pack n
- col
- grid
	- regular w * h
	- pack n * h

- widget style



*/


int main()
{
	GLFWwindow* window;
	struct NVGcontext* vg = NULL;

	char search[64] = "Foob-foob";
	double t = 0;

/*	int blending = 0;
	float opacity = 0.5f;
	float position[3] = {100.0f, 120.0f, 234.0f};
	float color[4] = {255/255.0f,192/255.0f,0/255.0f,255/255.0f};
	float iterations = 42;
	int cull = 1;
	char name[64] = "Mikko";
	const char* choices[] = { "Normal", "Minimum Color", "Screen Door", "Maximum Velocity" };
	float scroll = 30;*/

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

	miInit(vg);

	if (miCreateFont(MI_FONT_NORMAL, "../example/fonts/Roboto-Regular.ttf")) {
		printf("Could not add font italic.\n");
		return -1;
	}
	if (miCreateFont(MI_FONT_ITALIC, "../example/fonts/Roboto-Italic.ttf")) {
		printf("Could not add font italic.\n");
		return -1;
	}
	if (miCreateFont(MI_FONT_BOLD, "../example/fonts/Roboto-Bold.ttf")) {
		printf("Could not add font bold.\n");
		return -1;
	}

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

	glfwSetTime(0);

	MIcanvasState canvas = {0};
	float value = 0.15f;

	while (!glfwWindowShouldClose(window))
	{
		double mx, my;
		int winWidth, winHeight;
		int fbWidth, fbHeight;
		float pxRatio;
		double dt;
		dt = glfwGetTime() - t;
		t += dt;

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
		miFrameBegin(winWidth, winHeight, &input, (float)dt);

//		input.nkeys = 0;
//		input.mbut = 0;

//		miInput(panel, &input);
		
/*		float s = slider;
		if (miSync(panel, "slider", &s)) {
			slider = s;
		}

		if (miValue(panel, "slider", &s)) {
			slider = s;
		}

		if (miClicked(panel, "footer-add")) {			
		}
*/
		
//		miRender(panel, vg);

		miPanelBegin(50,50, 250,450);

		miDockBegin(MI_TOP_BOTTOM);
			miText("Materials");
			float cols[3] = {25, -1, 25};
			miDivsBegin(MI_LEFT_RIGHT, 3, cols);
				miRowHeight(25);
				miText("S");
				miInput(search, sizeof(search));
				miText("X");
				miText("Q");
			miDivsEnd();
			miSliderValue(&value, -1.0f, 1.0f);
/*			miLayoutBegin();
				miRowHeight(25);
				miPack(MI_LEFT_RIGHT);
				miText("S");
				miPack(MI_RIGHT_LEFT);
				miText("X");
				miPack(MI_FILLX);
				miInput(search, sizeof(search));
			miLayoutEnd();*/
		miDockEnd();

		miDockBegin(MI_BOTTOM_TOP);
			float cols2[3] = {-1, 60, 40};
			miDivsBegin(MI_LEFT_RIGHT, 3, cols2);
				miRowHeight(20);
				miSpacer();
				miButton("Add");
				miButton("Delete");
			miDivsEnd();
		miDockEnd();

//		miLayoutBegin();
//				miRowHeight(20);
/*			miPack(MI_LEFT_RIGHT);
			miText("Ins");
			miPack(MI_RIGHT_LEFT);
			miButton("Delete");
			miButton("Add");*/

//		miLayoutEnd();

		miDockBegin(MI_FILLY);


			float cols3[2] = {50, -1};
			miDivsBegin(MI_LEFT_RIGHT, 2, cols3);
				miRowHeight(50);
				miText("IMG");
				float rows[4] = {-1, 20, 15, -1};
				miDivsBegin(MI_TOP_BOTTOM, 4, rows);
					miSpacer();
					miText("Plastic");
					miLayoutBegin(MI_LEFT_RIGHT);
						miPack(MI_LEFT_RIGHT);
						miText("very shiny");
						miPack(MI_RIGHT_LEFT);
						miText("7kB");
					miLayoutEnd();
				miDivsEnd();
			miDivsEnd();

			miLayoutBegin(MI_LEFT_RIGHT);
				miRowHeight(50);
				miText("IMG");
				miLayoutBegin(MI_TOP_BOTTOM);
					miText("Plastic");
					miText("very shiny");
				miLayoutEnd();
			miLayoutEnd();

		miDockEnd();


/*		miText("Text 1");
		float cols[3] = {25, -1, 25};
		miDivsBegin(MI_LEFT_RIGHT, 3, cols);
//		miLayoutBegin(MI_LEFT_RIGHT);
			miPack(MI_LEFT_RIGHT);
			miText("Text 2.1");
			miButton("Text 2.2");
			miText("Text 2.3");
		miDivsEnd();
//		miLayoutEnd();
		miText("Text 3");*/

/*		miPack(MI_BOTTOM_TOP);
			miText("BOTTOM");

		miPack(MI_LEFT_RIGHT);
			miText("LEFT");

		miPack(MI_RIGHT_LEFT);
			miText("RIGHT");*/


/*		MIhandle button = miButton("Popup");
		MIhandle popup = miPopupBegin(button, MI_ONCLICK, MI_BELOW);
			miText("Popup...");
			miCanvasBegin(&canvas, MI_FIT, 50);
			miCanvasEnd();

			if (miClicked(miButton("Close"))) {
				printf("Close popup\n");
				miPopupHide(popup);
			}

			MIhandle button2 = miButton("Popup 2");
			miPopupBegin(button2, MI_ONHOVER, MI_RIGHT);
				miText("Popup 2");
			miPopupEnd();

			MIhandle button3 = miButton("Popup 3");
			miPopupBegin(button3, MI_ONHOVER, MI_RIGHT);
				miText("Popup 3");
			miPopupEnd();

		miPopupEnd();

		miSlider(&value, -1.0f, 1.0f);
		miSliderValue(&value, -1.0f, 1.0f);
		miText("Foobar");

		float divs[] = {50,100};
		miDivsBegin(MI_ROW, divs, 2, 30, 5);
			miButton("Tab 1");
			miButton("Tab 2");

			miDivsBegin(MI_COL, divs, 2, 30, 0);
				miButton("Tab 4.1");
				miDivsBegin(MI_ROW, divs, 2, 30, 0);
					miButton("Tab 4.2.1");
					miButton("Tab 4.2.2");
					miButton("Tab 4.2.3");
				miDivsEnd();
			miDivsEnd();

			miButton("Tab 3");

			miButton("Tab 5");
			miStackBegin(MI_COL, 30, 5);
				miText("Tab 6.1");
				miStackBegin(MI_ROW, 30, 5);
					miText("Tab 6.2.1");
					miText("Tab 6.2.2");
					miText("Tab 6.2.3");
				miStackEnd();
				miText("Tab 6.3");
			miStackEnd();

		miDivsEnd();

		miText("Foofoo");

		miButtonRowBegin(4);
			miButton("A");
			miButton("B");
			miButton("C");
			miButton("D");
		miButtonRowEnd();

		miInput(search, sizeof(search));

		miPanelBegin(250,250, 250,40);
			miText("Another one...");
		miPanelEnd();*/

		miPanelEnd();


		miFrameEnd();

		nvgEndFrame(vg);

		glEnable(GL_DEPTH_TEST);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	nvgDeleteGL3(vg);

	miTerminate();

	glfwTerminate();
	return 0;
}
