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
#include "mgui.h"
#define NANOSVG_IMPLEMENTATION 1
#include "nanosvg.h"

struct MGinputState input = { 0 };

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
			if (input.nkeys < MG_MAX_INPUTKEYS) {
				input.keys[input.nkeys].type = MG_KEYPRESSED;
				input.keys[input.nkeys].code = key;
				input.keys[input.nkeys].mods = mods;
				input.nkeys++;
			}
		}
	}
	if (action == GLFW_RELEASE) {
		if (!isPrintable(key)) {
			if (input.nkeys < MG_MAX_INPUTKEYS) {
				input.keys[input.nkeys].type = MG_KEYRELEASED;
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
		if (input.nkeys < MG_MAX_INPUTKEYS) {
			input.keys[input.nkeys].type = MG_CHARTYPED;
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
			input.mbut |= MG_MOUSE_PRESSED;
		}
		if (action == GLFW_RELEASE) {
			input.mbut |= MG_MOUSE_RELEASED;
		}
	}
}


static float sqr(float x) { return x*x; }

int main()
{
	GLFWwindow* window;
	struct NVGcontext* vg = NULL;
	int blending = 0;
	float opacity = 0.5f;
	float position[3] = {100.0f, 120.0f, 234.0f};
	float color[4] = {255/255.0f,192/255.0f,0/255.0f,255/255.0f};
	float iterations = 42;
	int cull = 1;
	char name[32] = "Mikko";
	const char* choices[] = { "Normal", "Minimum Color", "Screen Door", "Maximum Velocity" };
	float scroll = 30;
	double t = 0;

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

	mgInit();

	if (mgCreateIcon("check", "../example/icons/check.svg")) {
		printf("Could not create icon 'check'.\n");
		return -1;
	}
	if (mgCreateIcon("arrow-combo", "../example/icons/arrow-combo.svg")) {
		printf("Could not create icon 'arrow-combo'.\n");
		return -1;
	}
	if (mgCreateIcon("tools", "../example/icons/tools.svg")) {
		printf("Could not create icon 'tool'.\n");
		return -1;
	}


	mgCreateStyle("menubar", mgOpts(
		mgFillColor(255,255,255,32)
	), mgOpts(), mgOpts(), mgOpts());

	mgCreateStyle("menu1", mgOpts(
		mgAlign(MG_JUSTIFY),
		mgPropPosition(MG_START,MG_START,0.0f,1.0f),
		mgFillColor(255,255,255,120)
	), mgOpts(), mgOpts(), mgOpts());

	mgCreateStyle("menu2", mgOpts(
		mgAlign(MG_JUSTIFY),
		mgPropPosition(MG_START,MG_START,1.0f,0.0f),
		mgFillColor(255,255,255,120)
	), mgOpts(), mgOpts(), mgOpts());

	mgCreateStyle("dialog", mgOpts(
		mgFillColor(255,255,255,32),
		mgCornerRadius(4)
	), mgOpts(), mgOpts(), mgOpts());

	glfwSetTime(0);

	while (!glfwWindowShouldClose(window))
	{
		double mx, my;
		int winWidth, winHeight;
		int fbWidth, fbHeight;
		float pxRatio;
		double tt, dt;

		unsigned int build = 0;
		unsigned int fileOpen = 0;
		unsigned int file = 0;
		unsigned int edit = 0;
		unsigned int tools = 0, toolsAlign = 0;
		unsigned int view = 0;

		tt = glfwGetTime();
		dt = tt - t;
		t = tt;

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

/*		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0,width,height,0,-1,1);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glDisable(GL_DEPTH_TEST);
		glColor4ub(255,255,255,255);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);*/


		nvgBeginFrame(vg, winWidth, winHeight, pxRatio, NVG_STRAIGHT_ALPHA);

		input.mx = mx;
		input.my = my;
		mgFrameBegin(vg, winWidth, winHeight, &input, dt);
		input.nkeys = 0;
		input.mbut = 0;


/*

- logic

	- popover / lightbox
		- click to activate
		- click outside to close (or close with button)		

	- menu / popup
		- press to activate
		- if keep pressing, click in release
		- click outside to close (or close with button)

	- sub menu
		- hover + wait to activate
		- click outside to close (or close with button)
		- hover outside the hierarchy to close 

	- tooltip
		- hover + wait to activate
		- exit to close


but = mgButton("Popover", mgOpt());
if (mgLogic(but, MG_TOGGLE, MG_CLICKED, MG_MODAL)) {
	mgBeginPopup(but, MG_COL, mgOpt());
		if (mgButton("Press", mgOpt())) {
			mgCloseLogic(but);
		}
	mgEndPopup();
}


but = mgButton("Menu", mgOpt());
if (mgLogic(but, MG_MENU, MG_PRESSED, MG_CLICKTHROUGH)) {
	mgBeginPopup(but, MG_COL, mgOpt());
		if (mgButton("Press", mgOpt())) {
			mgCloseLogic(but);
		}
	mgEndPopup();
}


but = mgButton("Tooltip", mgOpt());
if (mgLogic(but, MG_WAIT, MG_HOVERED, 0)) {
	mgBeginPopup(but, MG_COL, mgOpt());
		mgLabel("Tipping tools.", mgOpt());
	mgEndPopup();
}

*/


		// Menu bar
//		mgBeginPanel("Menu", 0,0, winWidth, 30, MG_ROW, MG_JUSTIFY, 0, 0,0);

		mgPanelBegin(MG_ROW, 0,0, 0, mgOpts(mgWidth(winWidth), mgHeight(30), mgTag("menubar"), mgAlign(MG_JUSTIFY)));
			file = mgItem("File", mgOpts());
			mgPopupBegin(file, MG_ACTIVE, MG_COL, mgOpts(mgTag("menu1")));
				fileOpen = mgItem("Open...", mgOpts());
				mgItem("Save", mgOpts());
				mgItem("Save As...", mgOpts());
				mgItem("Close", mgOpts());
			mgPopupEnd();

			edit = mgItem("Edit", mgOpts());
			mgPopupBegin(edit, MG_ACTIVE, MG_COL, mgOpts(mgTag("menu1")));
				mgItem("Undo", mgOpts());
				mgItem("Redo", mgOpts());
				mgItem("Cut", mgOpts());
				mgItem("Copy", mgOpts());
				mgItem("Paste", mgOpts());
			mgPopupEnd();

			tools = mgItem("Tools", mgOpts());
			mgPopupBegin(tools, MG_ACTIVE, MG_COL, mgOpts(mgTag("menu1")));
				mgItem("Build", mgOpts());
				mgItem("Clear", mgOpts());
				toolsAlign = mgItem("Align", mgOpts());
				mgPopupBegin(toolsAlign, MG_HOVER, MG_COL, mgOpts(mgTag("menu2")));
					mgItem("Left", mgOpts());
					mgItem("Center", mgOpts());
					mgItem("Right", mgOpts());
				mgPopupEnd();
			mgPopupEnd();

			view = mgItem("View", mgOpts());
			mgPopupBegin(view, MG_ACTIVE, MG_COL, mgOpts(mgTag("menu1")));
				mgItem("Sidebar", mgOpts());
				mgItem("Minimap", mgOpts());
				mgItem("Tabs", mgOpts());
			mgPopupEnd();
		mgPanelEnd();

		if (mgClicked(fileOpen)) {
			printf("Open!!\n");
		}

//		mgBeginPanel("NavMesh Options", 20,50, 250, MG_AUTO, MG_COL, MG_JUSTIFY, 0, 5);
//		mgBeginPanel("NavMesh Options", 20,50, 250, winHeight - 50, MG_COL, MG_JUSTIFY, MG_SCROLL, 5,5);
		mgPanelBegin(MG_COL, 20,50, 0, mgOpts(mgWidth(250), /*mgHeight(MG_AUTO_SIZE),*/ mgTag("dialog"), mgAlign(MG_JUSTIFY), mgOverflow(MG_SCROLL), mgPadding(10,10)));

//		mgText("NavMesh Options", );

		mgLabel("Blending", mgOpts());
		mgSelect(&blending, choices, 4, mgOpts());

		mgLabel("Opacity", mgOpts());
		mgBoxBegin(MG_ROW, mgOpts(mgAlign(MG_CENTER)));
			if (mgChanged(mgSlider(&opacity, 0.0f, 1.0f, mgOpts(mgGrow(1))))) {
				printf("opacity = %f\n", opacity);
			}
			mgNumber(&opacity, mgOpts());
		mgBoxEnd();

/*
		// TODO: different in/out?
		float newOpacity = opacity;
		slider = mgSlider(opacity, &newOpacity, 0.0f, 1.0f, mgOpts(mgName("opacity")));
		if (mgChanged(slider))
			opacity = newOpacity;
		if (mgCommited(slider))
			saveUndoState();
*/		

		mgLabel("Iterations", mgOpts());
		mgNumber(&iterations, mgOpts());

		mgLabel("Position", mgOpts());
		mgNumber3(&position[0], &position[1], &position[2], "mm", mgOpts());

		mgLabel("Color", mgOpts());
		mgColor(&color[0], &color[1], &color[2], &color[3], mgOpts());

		mgCheckBox("Cull Enabled", &cull, mgOpts());
		mgLabel("Name", mgOpts());

		mgInput(name, sizeof(name), mgOpts());

		build = mgIconButton("tools", "Build", mgOpts());
		if (mgClicked(build)) {
			printf("Build!!\n");
		}
		mgTooltip(build, "Press Build to build.", mgOpts());

		mgBoxBegin(MG_ROW, mgOpts());
			mgBoxBegin(MG_COL, mgOpts(mgGrow(1), mgSpacing(5), mgAlign(MG_JUSTIFY)));
				mgParagraph("She was busy jumping over the lazy dog with the fox and all the men who came to the aid of the party.", mgOpts());
			mgBoxEnd();
			mgButton("Build4", mgOpts());
		mgBoxEnd();

		mgProgress(sqr(sinf(glfwGetTime()*0.3f)), mgOpts());

//		scroll = (200 - 45) * sqr(sinf(glfwGetTime()*0.3f));
		mgScrollBar(&scroll, 200, 45, mgOpts());

/*		mgBoxBegin(MG_ROW, mgOpts());
			mgBoxBegin(MG_COL, mgOpts(mgGrow(1), mgSpacing(5), mgAlign(MG_JUSTIFY)));
				mgButton("Build1", mgOpts());
				mgButton("Build2", mgOpts());
				mgButton("Build3", mgOpts());
			mgBoxEnd();
			mgButton("Build4", mgOpts());
		mgBoxEnd();*/

		mgPanelEnd();

		mgPanelBegin(MG_COL, winWidth-40-150, 50, 0, mgOpts(mgWidth(150), mgTag("dialog"), mgAlign(MG_JUSTIFY), mgOverflow(MG_SCROLL), mgPadding(10,10)));
		mgParagraph("Headline with verylingtestindeed", mgOpts(mgFontSize(32), mgLineHeight(0.8f)));
		mgParagraph("This is longer chunk of text.\nWould have used lorem ipsum but she was busy jumping over the lazy dog with the fox and all the men who came to the aid of the party.", mgOpts());
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
