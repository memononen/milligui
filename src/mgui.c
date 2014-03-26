#include "mgui.h"
#include "nanovg.h"
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>


static int mini(int a, int b) { return a < b ? a : b; }
static int maxi(int a, int b) { return a > b ? a : b; }
static float minf(float a, float b) { return a < b ? a : b; }
static float maxf(float a, float b) { return a > b ? a : b; }

#define LABEL_SIZE 14
#define TEXT_SIZE 18
#define BUTTON_PADX 10
#define BUTTON_PADY 5
#define SPACING 7
#define LABEL_SPACING 2
#define DEFAULT_SLIDERW (TEXT_SIZE*6)
#define DEFAULT_TEXTW (TEXT_SIZE*9)
#define DEFAULT_NUMBERW (TEXT_SIZE*3)
#define PANEL_PAD 5
#define SLIDER_HANDLE ((int)(TEXT_SIZE*0.75f))
#define CHECKBOX_SIZE (TEXT_SIZE)
#define SCROLL_SIZE (TEXT_SIZE/4)
#define SCROLL_PAD (SCROLL_SIZE/2)

#define TEXT_POOL_SIZE 8000

static char textPool[TEXT_POOL_SIZE];
static int textPoolSize = 0;

static char* allocText(const char* text)
{
	unsigned len = strlen(text)+1;
	if (textPoolSize + len >= TEXT_POOL_SIZE)
		return 0;
	char* dst = &textPool[textPoolSize]; 
	memcpy(dst, text, len);
	textPoolSize += len;
	return dst;
}

static char* allocTextLen(const char* text, int len)
{
	if (textPoolSize + len >= TEXT_POOL_SIZE)
		return 0;
	char* dst = &textPool[textPoolSize]; 
	memcpy(dst, text, len);
	textPoolSize += len;
	return dst;
}

#define MG_WIDGET_POOL_SIZE 1000

struct MGwidget widgetPool[MG_WIDGET_POOL_SIZE];
static int widgetPoolSize = 0;

#define MG_BOX_STACK_SIZE 100
#define MG_ID_STACK_SIZE 100
#define MG_MAX_PANELS 100

struct MUIstate
{
	int mx, my, mbut;
	unsigned int active;
	unsigned int hover;
	unsigned int focus;
	unsigned int clicked;

	int dragX, dragY;
	float dragOrig;

	struct MGwidget* boxStack[MG_BOX_STACK_SIZE];
	int boxStackCount;

	int idStack[MG_ID_STACK_SIZE];
	int idStackCount;

	struct MGwidget* panels[MG_MAX_PANELS];
	int panelCount;

	struct NVGcontext* vg;

	int width, height;
};

static struct MUIstate state;

static void addPanel(struct MGwidget* w)
{
	if (state.panelCount < MG_MAX_PANELS)
		state.panels[state.panelCount++] = w;
}

static void pushBox(struct MGwidget* w)
{
	if (state.boxStackCount < MG_BOX_STACK_SIZE)
		state.boxStack[state.boxStackCount++] = w;
}

static struct MGwidget* popBox()
{
	if (state.boxStackCount > 0)
		return state.boxStack[--state.boxStackCount];
	return NULL;
}

static void pushId()
{
	if (state.idStackCount < MG_ID_STACK_SIZE)
		state.idStack[state.idStackCount++] = 0;
}

static void popId()
{
	if (state.idStackCount > 0)
		state.idStackCount--;
}


static struct MGwidget* getParent()
{
	if (state.boxStackCount == 0) return NULL;
	return state.boxStack[state.boxStackCount-1];
}

static int genId()
{
	unsigned int id = 0;
	if (state.idStackCount > 0) {
		state.idStack[state.idStackCount-1]++;
		id |= (state.idStack[0] << 16);
		if (state.idStackCount > 1)
			id |= state.idStack[state.idStackCount-1];
	}
	return id;
}

static void addChildren(struct MGwidget* parent, struct MGwidget* w)
{
	struct MGwidget** prev = NULL;
	if (parent == NULL) return;
	prev = &parent->box.children;
	while (*prev != NULL)
		prev = &(*prev)->next;
	*prev = w;
}

static struct MGwidget* allocWidget(int type)
{
	struct MGwidget* parent = getParent();
	struct MGwidget* w = NULL;
	if (widgetPoolSize+1 > MG_WIDGET_POOL_SIZE)
		return NULL;
	w = &widgetPool[widgetPoolSize++];
	memset(w, 0, sizeof(*w));
	w->id = genId();
	w->type = type;
	addChildren(parent, w);
	return w;
}

/*
inline int anyActive()
{
	return state.active != 0;
}

inline int isActive(unsigned int id)
{
	return state.active == id;
}

inline int isHover(unsigned int id)
{
	return state.hover == id;
}*/

static int inRect(float x, float y, float w, float h)
{
   return state.mx >= x && state.mx <= x+w && state.my >= y && state.my <= y+h;
}

static void clearInput()
{
	state.mbut = 0;
}

/*
static void clearActive()
{
	state.active = 0;
	// mark all UI for this frame as processed
	clearInput();
}

static void setActive(unsigned int id)
{
	state.active = id;
	state.wentActive = 1;
}

static void setHover(unsigned int id)
{
   state.hoverToBe = id;
}
*/
/*
static int buttonLogic(unsigned int id, int over)
{
	int res = 0;
	// process down
	if (!anyActive())
	{
		if (over)
			setHot(id);
		if (isHot(id) && (state.mbut & MG_MOUSE_PRESSED))
			setActive(id);
	}

	// if button is active, then react on left up
	if (isActive(id))
	{
		state.isActive = 0;
		if (over)
			setHot(id);
		if (state.mbut & MG_MOUSE_RELEASED)
		{
			if (isHot(id))
				res = 1;
			clearActive();
		}
	}

	if (isHot(id))
		state.isHot = 1;

	return res;
}
*/

int mgInit()
{
	memset(&state, 0, sizeof(state));

	return 1;
}

void mgTerminate()
{

}

void mgFrameBegin(struct NVGcontext* vg, int width, int height, int mx, int my, int mbut)
{
	state.mx = mx;
	state.my = my;
	state.mbut = mbut;

	state.width = width;
	state.height = height;

	state.vg = vg;

	state.boxStackCount = 0;
	state.panelCount = 0;

	state.idStackCount = 1;
	state.idStack[0] = 0;

	textPoolSize = 0;
	widgetPoolSize = 0;
}

static void isectBounds(float* dst, const float* src, float x, float y, float w, float h)
{
	float minx = maxf(src[0], x);
	float miny = maxf(src[1], y);
	float maxx = minf(src[0]+src[2], x+w);
	float maxy = minf(src[1]+src[3], y+h);
	dst[0] = minx;
	dst[1] = miny;
	dst[2] = maxf(0, maxx - minx);
	dst[3] = maxf(0, maxy - miny);
}

static int visible(const float* bounds, float x, float y, float w, float h)
{
	float minx = maxf(bounds[0], x);
	float miny = maxf(bounds[1], y);
	float maxx = minf(bounds[0]+bounds[2], x+w);
	float maxy = minf(bounds[1]+bounds[3], y+h);
	return (maxx - minx) >= 0.0f && (maxy - miny) >= 0.0f;
}

static struct MGwidget* hitTest(struct MGwidget* box, const float* bounds)
{
	struct MGwidget* w;
	float bbounds[4];
	float wbounds[4];
	struct MGwidget* hit = NULL;
	struct MGwidget* child = NULL;

	// calc box bounds
	isectBounds(bbounds, bounds, box->x, box->y, box->width, box->height);

	// Skip if invisible
	if (bbounds[2] < 0.1f || bbounds[3] < 0.1f)
		return NULL;

	if (inRect(bbounds[0], bbounds[1], bbounds[2], bbounds[3]))
		hit = box;

	for (w = box->box.children; w != NULL; w = w->next) {

		if (!visible(bbounds, w->x, w->y, w->width, w->height))
			continue;

		// calc widget bounds
		isectBounds(wbounds, bbounds, w->x, w->y, w->width, w->height);

		if (inRect(wbounds[0], wbounds[1], wbounds[2], wbounds[3]))
			hit = w;

		switch (w->type) {
		case MG_BOX:
			child = hitTest(w, bbounds);
			if (child != NULL)
				hit = child;
			break;
		case MG_TEXT:
			break;
		case MG_ICON:
			break;
		case MG_SLIDER:
			// TODO hit knob separately?
			break;
		case MG_INPUT:
			break;
		}
	}

	// TODO: hit scrollbars?

	return hit;
} 

static void updateState(struct MGwidget* box, unsigned int hover, unsigned int active, unsigned int focus)
{
	struct MGwidget* w;

	box->state = 0;
	if (box->id == hover)
		box->state |= MG_HOVER;
	if (box->id == active)
		box->state |= MG_ACTIVE;
	if (box->id == focus)
		box->state |= MG_FOCUS;

	for (w = box->box.children; w != NULL; w = w->next) {
		w->state = 0;
		if (w->id == hover)
			w->state |= MG_HOVER;
		if (w->id == active)
			w->state |= MG_ACTIVE;
		if (w->id == focus)
			w->state |= MG_FOCUS;

		if  (w->type == MG_BOX)
			updateState(w, hover, active, focus);
	}
} 

/*
static void dumpId(struct MGwidget* box, int indent)
{
	struct MGwidget* w;
	printf("%*sbox %d\n", indent, "", box->id);
	for (w = box->box.children; w != NULL; w = w->next) {
		if  (w->type == MG_BOX)
			dumpId(w, indent+2);
		else
			printf("%*swid %d\n", indent+2, "", w->id);
	}
} 
*/

static void updateLogic(const float* bounds)
{
	int i;
	struct MGwidget* hit = NULL;

//	for (i = 0; i < state.panelCount; i++)
//		dumpId(state.panels[i], 0);

	for (i = 0; i < state.panelCount; i++) {
		struct MGwidget* child = hitTest(state.panels[i], bounds);
		if (child != NULL)
			hit = child;
	}

	state.clicked = 0;
	state.hover = 0;

	if (state.active == 0) {
		if (hit != NULL) {
			state.hover = hit->id;
			if (state.mbut & MG_MOUSE_PRESSED) {
				state.active = hit->id;
				state.focus = hit->id;
			}
		}
	} else {
		if (hit != NULL) {
			state.hover = hit->id;
		}
		if (state.mbut & MG_MOUSE_RELEASED) {
			state.clicked = state.hover;
			state.active = 0;
		}
	}

	for (i = 0; i < state.panelCount; i++)
		updateState(state.panels[i], state.hover, state.active, state.focus);


/*	bool res = false;
	// process down
	if (!anyActive())
	{
		if (over)
			setHover(id);
		if (isHot(id) && g_state.leftPressed)
			setActive(id);
	}

	// if button is active, then react on left up
	if (isActive(id))
	{
		g_state.isActive = true;
		if (over)
			setHot(id);
		if (g_state.leftReleased)
		{
			if (isHot(id))
				res = true;
			clearActive();
		}
	}

	if (isHot(id))
		g_state.isHot = true;*/

}


static void drawBox(struct MGwidget* box, const float* bounds)
{
	float tw, x;
	char str[32];
	struct MGwidget* w;
	float bbounds[4];
	float wbounds[4];

	nvgFontFace(state.vg, "sans");
	nvgFontSize(state.vg, TEXT_SIZE);

	nvgScissor(state.vg, bounds[0], bounds[1], bounds[2], bounds[3]);

	nvgBeginPath(state.vg);
	nvgRect(state.vg, box->x, box->y, box->width, box->height);

	if (box->state & MG_ACTIVE)
		nvgFillColor(state.vg, nvgRGBA(255,64,32,32));
	else if (box->state & MG_HOVER)
		nvgFillColor(state.vg, nvgRGBA(255,192,0,32));
	else if (box->state & MG_FOCUS)
		nvgFillColor(state.vg, nvgRGBA(0,192,255,32));
	else
		nvgFillColor(state.vg, nvgRGBA(255,255,255,32));

	nvgFill(state.vg);

	// calc panel bounds
	isectBounds(bbounds, bounds, box->x, box->y, box->width, box->height);

	// Skip if invisible
	if (bbounds[2] < 0.1f || bbounds[3] < 0.1f)
		return;

	for (w = box->box.children; w != NULL; w = w->next) {

		if (!visible(bbounds, w->x, w->y, w->width, w->height))
			continue;

		nvgScissor(state.vg, bbounds[0], bbounds[1], bbounds[2], bbounds[3]);

		switch (w->type) {
		case MG_BOX:
			drawBox(w, bbounds);
			break;

		case MG_TEXT:
			isectBounds(wbounds, bbounds, w->x, w->y, w->width, w->height);
			if (wbounds[2] > 0.0f && wbounds[3] > 0.0f) {
				nvgScissor(state.vg, wbounds[0], wbounds[1], wbounds[2], wbounds[3]);

				if (w->state & MG_ACTIVE)
					nvgFillColor(state.vg, nvgRGBA(255,64,32,255));
				else if (w->state & MG_HOVER)
					nvgFillColor(state.vg, nvgRGBA(255,192,0,255));
				else if (w->state & MG_FOCUS)
					nvgFillColor(state.vg, nvgRGBA(0,192,255,255));
				else
					nvgFillColor(state.vg, nvgRGBA(255,255,255,255));

				nvgFontSize(state.vg, w->args.fontSize);
				if (w->args.textAlign == MG_CENTER) {
					nvgTextAlign(state.vg, NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
					nvgText(state.vg, w->x + w->width/2, w->y + w->height/2, w->text.text, NULL);
				} else if (w->args.textAlign == MG_END) {
					nvgTextAlign(state.vg, NVG_ALIGN_RIGHT|NVG_ALIGN_MIDDLE);
					nvgText(state.vg, w->x + w->width - w->args.paddingx, w->y + w->height/2, w->text.text, NULL);
				} else {
					nvgTextAlign(state.vg, NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);
					nvgText(state.vg, w->x + w->args.paddingx, w->y + w->height/2, w->text.text, NULL);
				}
			}
			break;

		case MG_ICON:
			nvgBeginPath(state.vg);
			nvgRect(state.vg, w->x, w->y, w->width, w->height);

			if (w->state & MG_ACTIVE)
				nvgFillColor(state.vg, nvgRGBA(255,64,32,64));
			else if (w->state & MG_HOVER)
				nvgFillColor(state.vg, nvgRGBA(255,192,0,64));
			else if (w->state & MG_FOCUS)
				nvgFillColor(state.vg, nvgRGBA(0,192,255,64));
			else
				nvgFillColor(state.vg, nvgRGBA(255,255,255,64));

			nvgFill(state.vg);
			break;

		case MG_SLIDER:

			tw = SLIDER_HANDLE/2;

			nvgBeginPath(state.vg);
			nvgMoveTo(state.vg, w->x+tw, w->y+w->height/2+0.5f);
			nvgLineTo(state.vg, w->x-tw + w->width, w->y+w->height/2+0.5f);
			nvgStrokeColor(state.vg, nvgRGBA(255,255,255,128));
			nvgStrokeWidth(state.vg,1.0f);
			nvgStroke(state.vg);

			x = w->x + tw + (w->slider.value - w->slider.vmin) / (w->slider.vmax - w->slider.vmin) * (w->width - tw*2);

			nvgBeginPath(state.vg);
			nvgCircle(state.vg, x, w->y+w->height/2, tw);

			if (w->state & MG_ACTIVE)
				nvgFillColor(state.vg, nvgRGBA(255,64,32,255));
			if (w->state & MG_HOVER)
				nvgFillColor(state.vg, nvgRGBA(255,192,0,255));
			else if (w->state & MG_FOCUS)
				nvgFillColor(state.vg, nvgRGBA(0,192,255,255));
			else
				nvgFillColor(state.vg, nvgRGBA(255,255,255,255));

			nvgFill(state.vg);
			break;

		case MG_INPUT:
			nvgBeginPath(state.vg);
			nvgRect(state.vg, w->x+0.5f, w->y+0.5f, w->width-1, w->height-1);
			nvgStrokeColor(state.vg, nvgRGBA(255,255,255,128));
			nvgStrokeWidth(state.vg,1.0f);
			nvgStroke(state.vg);

			isectBounds(wbounds, bbounds, w->x, w->y, w->width, w->height);
			if (wbounds[2] > 0.0f && wbounds[3] > 0.0f) {
				nvgScissor(state.vg, wbounds[0], wbounds[1], wbounds[2], wbounds[3]);

				if (w->state & MG_ACTIVE)
					nvgFillColor(state.vg, nvgRGBA(255,64,32,255));
				if (w->state & MG_HOVER)
					nvgFillColor(state.vg, nvgRGBA(255,192,0,255));
				else if (w->state & MG_FOCUS)
					nvgFillColor(state.vg, nvgRGBA(0,192,255,255));
				else
					nvgFillColor(state.vg, nvgRGBA(255,255,255,255));

				nvgFontSize(state.vg, w->args.fontSize);
				if (w->args.textAlign == MG_CENTER) {
					nvgTextAlign(state.vg, NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
					nvgText(state.vg, w->x + w->width/2, w->y + w->height/2, w->input.text, NULL);
				} else if (w->args.textAlign == MG_END) {
					nvgTextAlign(state.vg, NVG_ALIGN_RIGHT|NVG_ALIGN_MIDDLE);
					nvgText(state.vg, w->x + w->width - w->args.paddingx, w->y + w->height/2, w->input.text, NULL);
				} else {
					nvgTextAlign(state.vg, NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);
					nvgText(state.vg, w->x + w->args.paddingx, w->y + w->height/2, w->input.text, NULL);
				}
			}
			break;
		}
	}

	if (box->args.overflow == MG_SCROLL) {
		nvgScissor(state.vg, bbounds[0], bbounds[1], bbounds[2], bbounds[3]);
		if (box->dir == MG_ROW) {
			float contentSize = box->args.width;
			float containerSize = maxf(0.0f, box->width - box->args.paddingx*2);
			if (contentSize > 0 && contentSize > containerSize) {
				float x = box->x + SCROLL_PAD;
				float y = box->y + box->height - (SCROLL_SIZE + SCROLL_PAD);
				float w = maxf(0, box->width - SCROLL_PAD*2);
				float h = SCROLL_SIZE;
				float x2 = x;
				float w2 = (containerSize / contentSize) * w;
				nvgBeginPath(state.vg);
				nvgRect(state.vg, x, y, w, h);
				nvgFillColor(state.vg, nvgRGBA(0,0,0,64));
				nvgFill(state.vg);
				nvgBeginPath(state.vg);
				nvgRect(state.vg, x2, y, w2, h);
				nvgFillColor(state.vg, nvgRGBA(0,0,0,255));
				nvgFill(state.vg);
			}
		} else {
			float contentSize = box->args.height;
			float containerSize = maxf(0.0f, box->height - box->args.paddingy*2);
			if (contentSize > 0 && contentSize > containerSize) {
				float x = box->x + box->width - (SCROLL_SIZE + SCROLL_PAD);
				float y = box->y + SCROLL_PAD;
				float w = SCROLL_SIZE;
				float h = maxf(0, box->height - SCROLL_PAD*2);
				float y2 = y;
				float h2 = (containerSize / contentSize) * h;
				nvgBeginPath(state.vg);
				nvgRect(state.vg, x, y, w, h);
				nvgFillColor(state.vg, nvgRGBA(0,0,0,64));
				nvgFill(state.vg);
				nvgBeginPath(state.vg);
				nvgRect(state.vg, x, y2, w, h2);
				nvgFillColor(state.vg, nvgRGBA(0,0,0,255));
				nvgFill(state.vg);
			}
		}
	}
} 

static void drawPanels(const float* bounds)
{
	int i;
	if (state.vg == NULL) return;	
	for (i = 0; i < state.panelCount; i++)
		drawBox(state.panels[i], bounds);
}

void mgFrameEnd()
{
	float bounds[4] = {0, 0, state.width, state.height};
	updateLogic(bounds);
	drawPanels(bounds);
	clearInput();
}

static void textSize(const char* str, float size, float* w, float* h)
{
	float tw, th;
	if (state.vg == NULL) {
		*w = *h = 0;
		return;
	}
	nvgFontFace(state.vg, "sans");
	nvgFontSize(state.vg, size);
	tw = str != NULL ? nvgTextBounds(state.vg, str, NULL, NULL) : 0;
	nvgVertMetrics(state.vg, NULL, NULL, &th);
	if (w) *w = tw;
	if (h) *h = th;
}

static void fitToContent(struct MGwidget* root)
{
	struct MGwidget* w = NULL;

	root->args.width = 0;
	root->args.height = 0;

	if (root->dir == MG_COL) {
		for (w = root->box.children; w != NULL; w = w->next) {
			root->args.width = maxf(root->args.width, w->args.width);
			root->args.height += w->args.height;
			if (w->next != NULL) root->args.height += w->args.spacing;
		}
	} else {
		for (w = root->box.children; w != NULL; w = w->next) {
			root->args.width += w->args.width;
			root->args.height = maxf(root->args.height, w->args.height);
			if (w->next != NULL) root->args.width += w->args.spacing;
		}
	}

	root->args.width += root->args.paddingx*2;
	root->args.height += root->args.paddingy*2;
}

static void layoutWidgets(struct MGwidget* root)
{
	struct MGwidget* w = NULL;
	float x, y, rw, rh;
	float sum = 0, avail = 0;
	int ngrow = 0, nitems = 0;

	if (root->width == MG_AUTO_SIZE)
		root->width = root->args.width;
	if (root->height == MG_AUTO_SIZE)
		root->height = root->args.height;

	x = root->x + root->args.paddingx;
	y = root->y + root->args.paddingy;
	rw = maxf(0, root->width - root->args.paddingx*2);
	rh = maxf(0, root->height - root->args.paddingy*2);

	// Allocate space for scrollbar
	if (root->args.overflow == MG_SCROLL) {
		if (root->dir == MG_ROW) {
			if (root->args.width > 0 && root->args.width > root->width)
				rh = maxf(0, rh - (SCROLL_SIZE+SCROLL_PAD*2));
		} else {
			if (root->args.height > 0 && root->args.height > root->height)
				rw = maxf(0, rw - (SCROLL_SIZE+SCROLL_PAD*2));
		}
	}

	if (root->dir == MG_COL) {

		for (w = root->box.children; w != NULL; w = w->next) {
			sum += w->args.height;
			if (w->next != NULL) sum += w->args.spacing;
			ngrow += w->args.grow;
			nitems++;
		}

		avail = rh - sum;
		if (root->args.overflow != MG_FIT)
			avail = maxf(0, avail); 

		for (w = root->box.children; w != NULL; w = w->next) {
			w->x = x;
			w->y = y;
			w->height = w->args.height;
			if (ngrow > 0)
				w->height += (float)w->args.grow/(float)ngrow * avail;
			else if (avail < 0)
				w->height += 1.0f/(float)nitems * avail;

			w->width = minf(rw, w->args.width);
			switch (root->args.align) {
			case MG_END:
				w->x += rw - w->width;
				break;
			case MG_CENTER:
				w->x += rw/2 - w->width/2;
				break;
			case MG_JUSTIFY:
				w->width = rw;
				break;
			default: // MG_START
				break;
			}

			y += w->height + w->args.spacing;

			if (w->type == MG_BOX)
				layoutWidgets(w);
		}

	} else {

		for (w = root->box.children; w != NULL; w = w->next) {
			sum += w->args.width;
			if (w->next != NULL) sum += w->args.spacing;
			ngrow += w->args.grow;
			nitems++;
		}

		avail = rw - sum;
		if (root->args.overflow != MG_FIT)
			avail = maxf(0, avail); 

		for (w = root->box.children; w != NULL; w = w->next) {
			w->x = x;
			w->y = y;
			w->width = w->args.width;
			if (ngrow > 0)
				w->width += (float)w->args.grow/(float)ngrow * avail;
			else if (avail < 0)
				w->width += 1.0f/(float)nitems * avail;

			w->height = minf(rh, w->args.height);
			switch (root->args.align) {
			case MG_END:
				w->y += rh - w->height;
				break;
			case MG_CENTER:
				w->y += rh/2 - w->height/2;
				break;
			case MG_JUSTIFY:
				w->height = rh;
				break;
			default: // MG_START
				break;
			}

			x += w->width + w->args.spacing;

			if (w->type == MG_BOX)
				layoutWidgets(w);
		}

	}
}

unsigned int mgPackArg(unsigned char arg, int x)
{
	return (unsigned int)arg | ((unsigned int)(x & 0x00ffffff) << 8);
}

struct MGargs mgArgs_(unsigned int first, ...)
{
	va_list list;
	struct MGargs args;
	int v = MG_NONE;

	memset(&args, 0, sizeof(first));

	va_start(list, first);
	for (v = first; v != MG_NONE; v = va_arg(list, unsigned int)) {
		switch(v & 0xff) {
			case MG_OVERFLOW_ARG:		args.overflow = v >> 8; break;
			case MG_ALIGN_ARG:			args.align = v >> 8; break;
			case MG_GROW_ARG:			args.grow = v >> 8; break;
			case MG_WIDTH_ARG:			args.width = v >> 8; break;
			case MG_HEIGHT_ARG:			args.height = v >> 8; break;
			case MG_PADDINGX_ARG:		args.paddingx = v >> 8; break;
			case MG_PADDINGY_ARG:		args.paddingy = v >> 8; break;
			case MG_SPACING_ARG:		args.spacing = v >> 8; break;
			case MG_FONTSIZE_ARG:		args.fontSize = v >> 8; break;
			case MG_TEXTALIGN_ARG:		args.textAlign = v >> 8; break;
		}
		// Mark which properties has been set.
		args.set |= 1 << (v & 0xff);
	}
	va_end(list);

	return args;
}

static int isArgSet(struct MGargs* args, unsigned int f)
{
	return args->set & (1 << f);
}

struct MGargs mgMergeArgs(struct MGargs dst, struct MGargs src)
{
	if (isArgSet(&src, MG_WIDTH_ARG))		dst.width = src.width;
	if (isArgSet(&src, MG_HEIGHT_ARG))		dst.height = src.height;
	if (isArgSet(&src, MG_SPACING_ARG))		dst.spacing = src.spacing;
	if (isArgSet(&src, MG_PADDINGX_ARG))	dst.paddingx = src.paddingx;
	if (isArgSet(&src, MG_PADDINGY_ARG))	dst.paddingy = src.paddingy;
	if (isArgSet(&src, MG_GROW_ARG))		dst.grow = src.grow;
	if (isArgSet(&src, MG_ALIGN_ARG))		dst.align = src.align;
	if (isArgSet(&src, MG_OVERFLOW_ARG))	dst.overflow = src.overflow;
	if (isArgSet(&src, MG_FONTSIZE_ARG))	dst.fontSize = src.fontSize;
	if (isArgSet(&src, MG_TEXTALIGN_ARG))	dst.textAlign = src.textAlign;
	dst.set |= src.set;
	return dst;
}

int mgPanelBegin(int dir, float x, float y, float width, float height, struct MGargs args)
{
	struct MGwidget* w = allocWidget(MG_BOX);

	w->x = x;
	w->y = y;
	w->width = width;
	w->height = height;
	w->dir = dir;
	w->args = mgMergeArgs(w->args, args);

	addPanel(w);
	pushId();
	pushBox(w);

	return 0;
}

int mgPanelEnd()
{
	struct MGwidget* panel = popBox();
	if (panel != NULL) {
		fitToContent(panel);
		layoutWidgets(panel);
	}
	popId();
	return 0;
}

int mgBoxBegin(int dir, struct MGargs args)
{
	struct MGwidget* w = allocWidget(MG_BOX);

	w->width = MG_AUTO_SIZE;
	w->height = MG_AUTO_SIZE;
	w->dir = dir;
	w->args = mgMergeArgs(w->args, args);

	pushBox(w);

	return 0;
}

int mgBoxEnd()
{
	struct MGwidget* box = popBox();
	if (box != NULL)
		fitToContent(box);
	return 0;
}

int mgText(const char* text, struct MGargs args)
{
	float tw, th;
	struct MGwidget* w = allocWidget(MG_TEXT);

	w->text.text = allocText(text);

	w->args.spacing = SPACING;
	w->args.fontSize = TEXT_SIZE;
	w->args.textAlign = MG_START;

	textSize(w->text.text, w->args.fontSize, &tw, &th);
	w->args.width = tw;
	w->args.height = th;

	w->args = mgMergeArgs(w->args, args);

	w->args.width += w->args.paddingx*2;
	w->args.height += w->args.paddingy*2;

	return 0;
}

int mgIcon(int width, int height, struct MGargs args)
{
	struct MGwidget* w = allocWidget(MG_ICON);

	w->args.width = width;
	w->args.height = height;
	w->args.spacing = SPACING;

	w->args = mgMergeArgs(w->args, args);

	w->args.width += w->args.paddingx*2;
	w->args.height += w->args.paddingy*2;

	return 0;
}

int mgSlider(float* value, float vmin, float vmax, struct MGargs args)
{
	float tw, th;
	struct MGwidget* w = allocWidget(MG_SLIDER);

	w->slider.value = *value;
	w->slider.vmin = vmin;
	w->slider.vmax = vmax;

	w->args.spacing = SPACING;
	w->args.fontSize = TEXT_SIZE;
	w->args.paddingx = 0;
	w->args.paddingy = BUTTON_PADY;

	textSize(NULL, w->args.fontSize, &tw,&th);
	w->args.width = DEFAULT_SLIDERW;
	w->args.height = th;

	w->args = mgMergeArgs(w->args, args);

	w->args.width += w->args.paddingx*2;
	w->args.height += w->args.paddingy*2;

	return 0;
}

int mgInput(char* text, int maxtext, struct MGargs args)
{
	float tw, th;
	struct MGwidget* w = allocWidget(MG_INPUT);

	w->input.text = allocTextLen(text, maxtext);
	w->input.maxtext = maxtext;

	w->args.spacing = SPACING;
	w->args.fontSize = TEXT_SIZE;
	w->args.textAlign = MG_START;
	w->args.paddingx = BUTTON_PADX/2;
	w->args.paddingy = BUTTON_PADY;

	textSize(NULL, w->args.fontSize, &tw,&th);
	w->args.width = DEFAULT_TEXTW;
	w->args.height = th;

	w->args = mgMergeArgs(w->args, args);

	w->args.width += w->args.paddingx*2;
	w->args.height += w->args.paddingy*2;

	return 0;
}

int mgNumber(float* value, struct MGargs args)
{
	char str[32];

	snprintf(str, sizeof(str), "%.2f", *value);
	str[sizeof(str)-1] = '\0';

	args = mgMergeArgs(args, mgArgs(mgTextAlign(MG_END), mgWidth(DEFAULT_NUMBERW)));

	return mgInput(str, sizeof(str), args);
/*
	float tw, th;
	struct MGwidget* w = allocWidget(MG_NUMBERBOX);

	w->number.value = *value;

	w->args.spacing = SPACING;
	w->args.fontSize = TEXT_SIZE;
	w->args.textAlign = MG_END;
	applyArgs(w, &args);

	textSize(NULL, w->args.fontSize, &tw,&th);
	w->args.width = DEFAULT_NUMBERW;
	w->args.height = BUTTON_PADY + th + BUTTON_PADY;
	return 0;*/
}

int mgNumber3(float* x, float* y, float* z, const char* units, struct MGargs args)
{
	struct MGargs boxArgs = mgMergeArgs(mgArgs(mgAlign(MG_CENTER), mgSpacing(SPACING)), args);
	mgBoxBegin(MG_ROW, boxArgs);
		mgNumber(x, mgArgs(mgGrow(1)));
		mgNumber(y, mgArgs(mgGrow(1)));
		mgNumber(z, mgArgs(mgGrow(1)));
		if (units != NULL && strlen(units) > 0)
			mgText(units, mgArgs(mgFontSize(LABEL_SIZE), mgSpacing(LABEL_SPACING)));
	return mgBoxEnd();
}

int mgColor(float* r, float* g, float* b, float* a, struct MGargs args)
{
	struct MGargs boxArgs = mgMergeArgs(mgArgs(mgAlign(MG_CENTER), mgSpacing(SPACING)), args);
	struct MGargs labelArgs = mgArgs(mgFontSize(LABEL_SIZE), mgSpacing(LABEL_SPACING));
	mgBoxBegin(MG_ROW, boxArgs);
		mgText("R", labelArgs); mgNumber(r, mgArgs(mgGrow(1)));
		mgText("G", labelArgs); mgNumber(g, mgArgs(mgGrow(1)));
		mgText("G", labelArgs); mgNumber(b, mgArgs(mgGrow(1)));
		mgText("A", labelArgs); mgNumber(a, mgArgs(mgGrow(1)));
	return mgBoxEnd();
}

int mgCheckBox(const char* text, int* value, struct MGargs args)
{
	struct MGargs boxArgs = mgMergeArgs(mgArgs(mgAlign(MG_CENTER), mgSpacing(SPACING), mgPaddingY(BUTTON_PADY)), args);
	mgBoxBegin(MG_ROW, boxArgs);
		mgText(text, mgArgs(mgFontSize(LABEL_SIZE), mgGrow(1)));
		mgIcon(CHECKBOX_SIZE, CHECKBOX_SIZE, mgArgs(0));
	return mgBoxEnd();
}

int mgButton(const char* text, struct MGargs args)
{
	struct MGargs boxArgs = mgMergeArgs(mgArgs(mgAlign(MG_CENTER), mgSpacing(SPACING), mgPadding(BUTTON_PADX, BUTTON_PADY)), args);
	mgBoxBegin(MG_ROW, boxArgs);
		mgText(text, mgArgs(mgTextAlign(MG_CENTER), mgGrow(1)));
	return mgBoxEnd();
}

int mgItem(const char* text, struct MGargs args)
{
	struct MGargs boxArgs = mgMergeArgs(mgArgs(mgAlign(MG_CENTER), mgPadding(BUTTON_PADX, BUTTON_PADY)), args);
	mgBoxBegin(MG_ROW, boxArgs);
		mgText(text, mgArgs(mgTextAlign(MG_START), mgGrow(1)));
	return mgBoxEnd();
}

int mgLabel(const char* text, struct MGargs args)
{
	struct MGargs textArgs = mgMergeArgs(mgArgs(mgFontSize(LABEL_SIZE), mgSpacing(LABEL_SPACING), mgTextAlign(MG_START)), args);
	return mgText(text, textArgs);
}

int mgSelect(int* value, const char** choices, int nchoises, struct MGargs args)
{
	struct MGargs boxArgs = mgMergeArgs(mgArgs(mgAlign(MG_CENTER), mgSpacing(SPACING), mgPadding(BUTTON_PADX, BUTTON_PADY)), args);
	mgBoxBegin(MG_ROW, boxArgs);
		mgText(choices[*value], mgArgs(mgFontSize(TEXT_SIZE), mgTextAlign(MG_START), mgGrow(1)));
		mgIcon(CHECKBOX_SIZE, CHECKBOX_SIZE, mgArgs(0));
	return mgBoxEnd();
}


