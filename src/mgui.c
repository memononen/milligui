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


enum MGwidgetType {
	MG_PANEL,
	MG_TEXT,
	MG_ICON,
	MG_SELECT,
	MG_SLIDER,
	MG_NUMBERBOX,
	MG_TEXTBOX,
};


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

#define WIDGET_POOL_SIZE 1000

struct MGwidget {
	int type;
	unsigned int id;
	
	float x, y, width, height;
	float cwidth, cheight;	// Content size
	float spacing;
	float paddingx, paddingy;
	float fontSize;
	unsigned char grow;
	unsigned char textAlign;
	unsigned char align;
	unsigned char dir;
	unsigned char overflow;

	union {
		struct {
			char* text;
		} text;
		struct {
			char* text;
			int maxtext;
		} textbox;
		struct {
			float value;
		} number;
		struct {
			float value;
			float vmin;
			float vmax;
		} slider;
		struct {
			unsigned int panelId;
			unsigned int widgetId;
			struct MGwidget* children;
		} panel;
	};
	struct MGwidget* next;
};
struct MGwidget widgetPool[WIDGET_POOL_SIZE];
static int widgetPoolSize = 0;

#define MG_PANEL_STACK_SIZE 100
#define MG_MAX_PANELS 100

struct MUIstate
{
	int mx, my, mbut;
	unsigned int active;
	unsigned int hot;
	unsigned int hotToBe;
	int isHot;
	int isActive;
	int wentActive;
	int dragX, dragY;
	float dragOrig;
	int widgetX, widgetY, widgetW, widgetH;
	unsigned int panelId;
	unsigned int widgetId;
	struct MGwidget* stack[MG_PANEL_STACK_SIZE];
	int nstack;
	struct NVGcontext* vg;

	int width, height;

	struct MGwidget* panels[MG_MAX_PANELS];
	int npanels;
};

static struct MUIstate state;

static struct MGwidget* getParent()
{
	if (state.nstack == 0) return NULL;
	return state.stack[state.nstack-1];
}

static unsigned int getPanelId(struct MGwidget* w)
{
	return w == NULL ? 0 : w->panel.panelId;
}

static unsigned int getWidgetId(struct MGwidget* w)
{
	return w == NULL ? ++state.widgetId : ++(w->panel.widgetId);
}

static void addChildren(struct MGwidget* parent, struct MGwidget* w)
{
	struct MGwidget** prev = NULL;
	if (parent == NULL) return;
	prev = &parent->panel.children;
	while (*prev != NULL)
		prev = &(*prev)->next;
	*prev = w;
}

static struct MGwidget* allocWidget(int type)
{
	struct MGwidget* parent = getParent();
	struct MGwidget* w = NULL;
	if (widgetPoolSize+1 > WIDGET_POOL_SIZE)
		return NULL;
	w = &widgetPool[widgetPoolSize++];
	memset(w, 0, sizeof(*w));
	w->id = (getPanelId(parent) << 16) | getPanelId(parent);
	w->type = type;
	addChildren(parent, w);
	return w;
}

inline int anyActive()
{
	return state.active != 0;
}

inline int isActive(unsigned int id)
{
	return state.active == id;
}

inline int isHot(unsigned int id)
{
	return state.hot == id;
}

static int inRect(int x, int y, int w, int h)
{
   return state.mx >= x && state.mx <= x+w && state.my >= y && state.my <= y+h;
}

static void clearInput()
{
	state.mbut = 0;
}

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

static void setHot(unsigned int id)
{
   state.hotToBe = id;
}


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

int mgInit()
{
	memset(&state, 0, sizeof(state));

	return 1;
}

void mgTerminate()
{

}

void mgBeginFrame(struct NVGcontext* vg, int width, int height, int mx, int my, int mbut)
{
	state.mx = mx;
	state.my = my;
	state.mbut = mbut;

	state.width = width;
	state.height = height;

	state.hot = state.hotToBe;
	state.hotToBe = 0;

	state.wentActive = 0;
	state.isActive = 0;
	state.isHot = 0;

	state.widgetX = 0;
	state.widgetY = 0;
	state.widgetW = 0;
	state.widgetH = 0;

	state.panelId = 1;
	state.widgetId = 1;

	state.vg = vg;

	state.nstack = 0;
	state.npanels = 0;

	textPoolSize = 0;
	widgetPoolSize = 0;
}

#define LABEL_SIZE 14
#define TEXT_SIZE 18
#define BUTTON_PADX 10
#define BUTTON_PADY 5
#define SPACING 7
#define LABEL_SPACING 2
#define DEFAUL_SLIDERW (TEXT_SIZE*6)
#define DEFAUL_TEXTW (TEXT_SIZE*9)
#define DEFAUL_NUMBERW (TEXT_SIZE*3)
#define PANEL_PAD 5
#define SLIDER_HANDLE ((int)(TEXT_SIZE*0.75f))
#define CHECKBOX_SIZE (TEXT_SIZE)
#define SCROLL_SIZE (TEXT_SIZE/4)
#define SCROLL_PAD (SCROLL_SIZE/2)

static void updateLogic()
{
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

static void drawPanel(struct MGwidget* panel, const float* bounds)
{
	float tw, x;
	char str[32];
	struct MGwidget* w;
	float pbounds[4];
	float wbounds[4];

	nvgFontFace(state.vg, "sans");
	nvgFontSize(state.vg, TEXT_SIZE);

	nvgScissor(state.vg, bounds[0], bounds[1], bounds[2], bounds[3]);

	nvgBeginPath(state.vg);
	nvgRect(state.vg, panel->x, panel->y, panel->width, panel->height);
	nvgFillColor(state.vg, nvgRGBA(255,255,255,32));
	nvgFill(state.vg);

	// calc panel bounds
	isectBounds(pbounds, bounds, panel->x, panel->y, panel->width, panel->height);

	if (pbounds[2] < 0.5f || pbounds[3] < 0.5f)
		return;

	for (w = panel->panel.children; w != NULL; w = w->next) {

		if (!visible(pbounds, w->x, w->y, w->width, w->height))
			continue;

		nvgScissor(state.vg, pbounds[0], pbounds[1], pbounds[2], pbounds[3]);

		switch (w->type) {
		case MG_PANEL:
			drawPanel(w, pbounds);
			break;
/*		case MG_BUTTON:
			nvgBeginPath(state.vg);
			nvgRect(state.vg, w->x+0.5f, w->y+0.5f, w->width-1, w->height-1);
			nvgStrokeColor(state.vg, nvgRGBA(255,255,255,128));
			nvgStrokeWidth(state.vg,1.0f);
			nvgStroke(state.vg);

			isectBounds(wbounds, pbounds, w->x, w->y, w->width, w->height);
			if (wbounds[2] > 0.0f && wbounds[3] > 0.0f) {
				nvgScissor(state.vg, wbounds[0], wbounds[1], wbounds[2], wbounds[3]);
				nvgFillColor(state.vg, nvgRGBA(255,255,255,255));
				nvgFontSize(state.vg, TEXT_SIZE);
				nvgTextAlign(state.vg, NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
				nvgText(state.vg, w->x + w->width/2, w->y + w->height/2, w->button.text, NULL);
			}
			break;*/

		case MG_TEXT:
			isectBounds(wbounds, pbounds, w->x, w->y, w->width, w->height);
			if (wbounds[2] > 0.0f && wbounds[3] > 0.0f) {
				nvgScissor(state.vg, wbounds[0], wbounds[1], wbounds[2], wbounds[3]);
				nvgFillColor(state.vg, nvgRGBA(255,255,255,255));
				nvgFontSize(state.vg, w->fontSize);
				if (w->textAlign == MG_CENTER) {
					nvgTextAlign(state.vg, NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
					nvgText(state.vg, w->x + w->width/2, w->y + w->height/2, w->text.text, NULL);
				} else if (w->textAlign == MG_END) {
					nvgTextAlign(state.vg, NVG_ALIGN_RIGHT|NVG_ALIGN_MIDDLE);
					nvgText(state.vg, w->x + w->width, w->y + w->height/2, w->text.text, NULL);
				} else {
					nvgTextAlign(state.vg, NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);
					nvgText(state.vg, w->x, w->y + w->height/2, w->text.text, NULL);
				}
			}
			break;

		case MG_ICON:
			nvgBeginPath(state.vg);
			nvgRect(state.vg, w->x, w->y, w->width, w->height);
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
			nvgFillColor(state.vg, nvgRGBA(0,0,0,255));
			nvgFill(state.vg);
			nvgStrokeColor(state.vg, nvgRGBA(255,255,255,128));
			nvgStrokeWidth(state.vg,1.0f);
			nvgStroke(state.vg);
			break;

		case MG_NUMBERBOX:
			nvgBeginPath(state.vg);
			nvgRect(state.vg, w->x+0.5f, w->y+0.5f, w->width-1, w->height-1);
			nvgStrokeColor(state.vg, nvgRGBA(255,255,255,128));
			nvgStrokeWidth(state.vg,1.0f);
			nvgStroke(state.vg);

			isectBounds(wbounds, pbounds, w->x, w->y, w->width, w->height);
			if (wbounds[2] > 0.0f && wbounds[3] > 0.0f) {
				nvgScissor(state.vg, wbounds[0], wbounds[1], wbounds[2], wbounds[3]);
				snprintf(str, sizeof(str), "%.2f", w->number.value);
				str[sizeof(str)-1] = '\0';
				nvgFillColor(state.vg, nvgRGBA(255,255,255,255));
				nvgFontSize(state.vg, TEXT_SIZE);
				nvgTextAlign(state.vg, NVG_ALIGN_RIGHT|NVG_ALIGN_MIDDLE);
				nvgText(state.vg, w->x + w->width - BUTTON_PADY, w->y + w->height/2, str, NULL);
			}
			break;

		case MG_TEXTBOX:
			nvgBeginPath(state.vg);
			nvgRect(state.vg, w->x+0.5f, w->y+0.5f, w->width-1, w->height-1);
			nvgStrokeColor(state.vg, nvgRGBA(255,255,255,128));
			nvgStrokeWidth(state.vg,1.0f);
			nvgStroke(state.vg);

			isectBounds(wbounds, pbounds, w->x, w->y, w->width, w->height);
			if (wbounds[2] > 0.0f && wbounds[3] > 0.0f) {
				nvgScissor(state.vg, wbounds[0], wbounds[1], wbounds[2], wbounds[3]);
				nvgFillColor(state.vg, nvgRGBA(255,255,255,255));
				nvgFontSize(state.vg, TEXT_SIZE);
				nvgTextAlign(state.vg, NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);
				nvgText(state.vg, w->x + BUTTON_PADY, w->y + w->height/2, w->textbox.text, NULL);
			}
			break;
		}
	}

	if (panel->overflow == MG_SCROLL) {
		nvgScissor(state.vg, pbounds[0], pbounds[1], pbounds[2], pbounds[3]);
		if (panel->dir == MG_ROW) {
			if (panel->cwidth > 0 && panel->cwidth > panel->width) {
				float x = panel->x + SCROLL_PAD;
				float y = panel->y + panel->height - (SCROLL_SIZE + SCROLL_PAD);
				float w = maxf(0, panel->width - SCROLL_PAD*2);
				float h = SCROLL_SIZE;
				float x2 = x;
				float w2 = (panel->width / panel->cwidth) * w;
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
			if (panel->cheight > 0 && panel->cheight > panel->height) {
				float x = panel->x + panel->width - (SCROLL_SIZE + SCROLL_PAD);
				float y = panel->y + SCROLL_PAD;
				float w = SCROLL_SIZE;
				float h = maxf(0, panel->height - SCROLL_PAD*2);
				float y2 = y;
				float h2 = (panel->height / panel->cheight) * h;
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

void mgEndFrame()
{
	int i;
	float bounds[4] = {0, 0, state.width, state.height};

	updateLogic();

	if (state.vg != NULL) {
		for (i = 0; i < state.npanels; i++)
			drawPanel(state.panels[i], bounds);
	}

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

	root->cwidth = 0;
	root->cheight = 0;

	if (root->dir == MG_COL) {
		for (w = root->panel.children; w != NULL; w = w->next) {
			root->cwidth = maxf(root->cwidth, w->cwidth);
			root->cheight += w->cheight;
			if (w->next != NULL) root->cheight += w->spacing;
		}
	} else {
		for (w = root->panel.children; w != NULL; w = w->next) {
			root->cwidth += w->cwidth;
			root->cheight = maxf(root->cheight, w->cheight);
			if (w->next != NULL) root->cwidth += w->spacing;
		}
	}

	root->cwidth += root->paddingx*2;
	root->cheight += root->paddingy*2;
}

static void layoutWidgets(struct MGwidget* root)
{
	struct MGwidget* w = NULL;
	float x, y, rw, rh;
	float sum = 0, avail = 0;
	int ngrow = 0, nitems = 0;

	if (root->width == MG_AUTO_SIZE)
		root->width = root->cwidth;
	if (root->height == MG_AUTO_SIZE)
		root->height = root->cheight;

	x = root->x + root->paddingx;
	y = root->y + root->paddingy;
	rw = maxf(0, root->width - root->paddingx*2);
	rh = maxf(0, root->height - root->paddingy*2);

	// Allocate space for scrollbar
	if (root->overflow == MG_SCROLL) {
		if (root->dir == MG_ROW) {
			if (root->cwidth > 0 && root->cwidth > root->width)
				rh = maxf(0, rh - (SCROLL_SIZE+SCROLL_PAD*2));
		} else {
			if (root->cheight > 0 && root->cheight > root->height)
				rw = maxf(0, rw - (SCROLL_SIZE+SCROLL_PAD*2));
		}
	}

	if (root->dir == MG_COL) {

		for (w = root->panel.children; w != NULL; w = w->next) {
			sum += w->cheight;
			if (w->next != NULL) sum += w->spacing;
			ngrow += w->grow;
			nitems++;
		}

		avail = rh - sum;
		if (root->overflow != MG_FIT)
			avail = maxf(0, avail); 

		for (w = root->panel.children; w != NULL; w = w->next) {
			w->x = x;
			w->y = y;
			w->height = w->cheight;
			if (ngrow > 0)
				w->height += (float)w->grow/(float)ngrow * avail;
			else if (avail < 0)
				w->height += 1.0f/(float)nitems * avail;

			w->width = minf(rw, w->cwidth);
			switch (root->align) {
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

			y += w->height + w->spacing;

			if (w->type == MG_PANEL)
				layoutWidgets(w);
		}

	} else {

		for (w = root->panel.children; w != NULL; w = w->next) {
			sum += w->cwidth;
			if (w->next != NULL) sum += w->spacing;
			ngrow += w->grow;
			nitems++;
		}

		avail = rw - sum;
		if (root->overflow != MG_FIT)
			avail = maxf(0, avail); 

		for (w = root->panel.children; w != NULL; w = w->next) {
			w->x = x;
			w->y = y;
			w->width = w->cwidth;
			if (ngrow > 0)
				w->width += (float)w->grow/(float)ngrow * avail;
			else if (avail < 0)
				w->width += 1.0f/(float)nitems * avail;

			w->height = minf(rh, w->cheight);
			switch (root->align) {
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

			x += w->width + w->spacing;

			if (w->type == MG_PANEL)
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
	return dst;
}

static void applyArgs(struct MGwidget* w, struct MGargs* args)
{
	if (isArgSet(args, MG_WIDTH_ARG))		w->width = args->width;
	if (isArgSet(args, MG_HEIGHT_ARG))	w->height = args->height;
	if (isArgSet(args, MG_SPACING_ARG))	w->spacing = args->spacing;
	if (isArgSet(args, MG_PADDINGX_ARG))	w->paddingx = args->paddingx;
	if (isArgSet(args, MG_PADDINGY_ARG))	w->paddingy = args->paddingy;
	if (isArgSet(args, MG_GROW_ARG))		w->grow = args->grow;
	if (isArgSet(args, MG_ALIGN_ARG))		w->align = args->align;
	if (isArgSet(args, MG_OVERFLOW_ARG))	w->overflow = args->overflow;
	if (isArgSet(args, MG_FONTSIZE_ARG))	w->fontSize = args->fontSize;
	if (isArgSet(args, MG_TEXTALIGN_ARG))	w->textAlign = args->textAlign;
}

int mgPanelBegin(int dir, float x, float y, float width, float height, struct MGargs args)
{
	struct MGwidget* w = allocWidget(MG_PANEL);

	w->x = x;
	w->y = y;
	w->width = width;
	w->height = height;
	w->dir = dir;
	applyArgs(w, &args);

	state.panelId++;
	w->panel.panelId = state.panelId;

	if (state.nstack < MG_PANEL_STACK_SIZE)
		state.stack[state.nstack++] = w;

	if (state.npanels < MG_MAX_PANELS)
		state.panels[state.npanels++] = w;

	return 0;
}

int mgPanelEnd()
{
	struct MGwidget* panel = NULL;
	if (state.nstack > 0) {
		state.nstack--;
		panel = state.stack[state.nstack];
		fitToContent(panel);
		layoutWidgets(panel);
	}
	return 0;
}

int mgDivBegin(int dir, struct MGargs args)
{
	struct MGwidget* w = allocWidget(MG_PANEL);

	w->width = MG_AUTO_SIZE;
	w->height = MG_AUTO_SIZE;
	w->dir = dir;
	applyArgs(w, &args);

	state.panelId++;
	w->panel.panelId = state.panelId;

	if (state.nstack < MG_PANEL_STACK_SIZE)
		state.stack[state.nstack++] = w;

	return 0;
}

int mgDivEnd()
{
	struct MGwidget* div = NULL;
	if (state.nstack > 0) {
		state.nstack--;
		div = state.stack[state.nstack];
		fitToContent(div);
	}
	return 0;
}

int mgText(const char* text, struct MGargs args)
{
	float tw, th;
	struct MGwidget* w = allocWidget(MG_TEXT);

	w->text.text = allocText(text);

	w->spacing = SPACING;
	w->fontSize = TEXT_SIZE;
	w->textAlign = MG_START;
	applyArgs(w, &args);

	textSize(w->text.text, w->fontSize, &tw, &th);
	w->cwidth = tw;
	w->cheight = th;

	return 0;
}

int mgIcon(int width, int height, struct MGargs args)
{
	struct MGwidget* w = allocWidget(MG_ICON);

	w->cwidth = width;
	w->cheight = height;
	w->spacing = SPACING;
	applyArgs(w, &args);

	return 0;
}

int mgSlider(float* value, float vmin, float vmax, struct MGargs args)
{
	float tw, th;
	struct MGwidget* w = allocWidget(MG_SLIDER);

	w->slider.value = *value;
	w->slider.vmin = vmin;
	w->slider.vmax = vmax;

	w->spacing = SPACING;
	w->fontSize = TEXT_SIZE;
	applyArgs(w, &args);

	textSize(NULL, w->fontSize, &tw,&th);
	w->cwidth = DEFAUL_SLIDERW;
	w->cheight = BUTTON_PADY + th + BUTTON_PADY;

	return 0;
}

int mgNumber(float* value, struct MGargs args)
{
	float tw, th;
	struct MGwidget* w = allocWidget(MG_NUMBERBOX);

	w->number.value = *value;

	w->spacing = SPACING;
	w->fontSize = TEXT_SIZE;
	w->textAlign = MG_END;
	applyArgs(w, &args);

	textSize(NULL, w->fontSize, &tw,&th);
	w->cwidth = DEFAUL_NUMBERW;
	w->cheight = BUTTON_PADY + th + BUTTON_PADY;

	return 0;
}

int mgTextBox(char* text, int maxtext, struct MGargs args)
{
	float tw, th;
	struct MGwidget* w = allocWidget(MG_TEXTBOX);

	w->textbox.text = allocTextLen(text, maxtext);
	w->textbox.maxtext = maxtext;

	w->spacing = SPACING;
	w->fontSize = TEXT_SIZE;
	w->textAlign = MG_START;
	applyArgs(w, &args);

	textSize(NULL, w->fontSize, &tw,&th);
	w->cwidth = DEFAUL_TEXTW;
	w->cheight = BUTTON_PADY + th + BUTTON_PADY;

	return 0;
}

int mgNumber3(float* x, float* y, float* z, const char* units, struct MGargs args)
{
	struct MGargs divArgs = mgMergeArgs(mgArgs(mgAlign(MG_CENTER), mgSpacing(SPACING)), args);
	mgDivBegin(MG_ROW, divArgs);
		mgNumber(x, mgArgs(mgGrow(1)));
		mgNumber(y, mgArgs(mgGrow(1)));
		mgNumber(z, mgArgs(mgGrow(1)));
		if (units != NULL && strlen(units) > 0)
			mgText(units, mgArgs(mgFontSize(LABEL_SIZE), mgSpacing(LABEL_SPACING)));
	return mgDivEnd();
}

int mgColor(float* r, float* g, float* b, float* a, struct MGargs args)
{
	struct MGargs divArgs = mgMergeArgs(mgArgs(mgAlign(MG_CENTER), mgSpacing(SPACING)), args);
	struct MGargs labelArgs = mgArgs(mgFontSize(LABEL_SIZE), mgSpacing(LABEL_SPACING));
	mgDivBegin(MG_ROW, divArgs);
		mgText("R", labelArgs); mgNumber(r, mgArgs(mgGrow(1)));
		mgText("G", labelArgs); mgNumber(g, mgArgs(mgGrow(1)));
		mgText("G", labelArgs); mgNumber(b, mgArgs(mgGrow(1)));
		mgText("A", labelArgs); mgNumber(a, mgArgs(mgGrow(1)));
	return mgDivEnd();
}

int mgCheckBox(const char* text, int* value, struct MGargs args)
{
	struct MGargs divArgs = mgMergeArgs(mgArgs(mgAlign(MG_CENTER), mgSpacing(SPACING), mgPaddingY(BUTTON_PADY)), args);
	mgDivBegin(MG_ROW, divArgs);
		mgText(text, mgArgs(mgFontSize(LABEL_SIZE), mgGrow(1)));
		mgIcon(CHECKBOX_SIZE, CHECKBOX_SIZE, mgArgs(0));
	return mgDivEnd();
}

int mgButton(const char* text, struct MGargs args)
{
	struct MGargs divArgs = mgMergeArgs(mgArgs(mgAlign(MG_CENTER), mgSpacing(SPACING), mgPadding(BUTTON_PADX, BUTTON_PADY)), args);
	mgDivBegin(MG_ROW, divArgs);
		mgText(text, mgArgs(mgTextAlign(MG_CENTER), mgGrow(1)));
	return mgDivEnd();
}

int mgItem(const char* text, struct MGargs args)
{
	struct MGargs divArgs = mgMergeArgs(mgArgs(mgAlign(MG_CENTER), mgPadding(BUTTON_PADX, BUTTON_PADY)), args);
	mgDivBegin(MG_ROW, divArgs);
		mgText(text, mgArgs(mgTextAlign(MG_START), mgGrow(1)));
	return mgDivEnd();
}

int mgLabel(const char* text, struct MGargs args)
{
	struct MGargs textArgs = mgMergeArgs(mgArgs(mgFontSize(LABEL_SIZE), mgSpacing(LABEL_SPACING), mgTextAlign(MG_START)), args);
	return mgText(text, textArgs);
}

int mgSelect(int* value, const char** choices, int nchoises, struct MGargs args)
{
	struct MGargs divArgs = mgMergeArgs(mgArgs(mgAlign(MG_CENTER), mgSpacing(SPACING), mgPadding(BUTTON_PADX, BUTTON_PADY)), args);
	mgDivBegin(MG_ROW, divArgs);
		mgText(choices[*value], mgArgs(mgFontSize(TEXT_SIZE), mgTextAlign(MG_START), mgGrow(1)));
		mgIcon(CHECKBOX_SIZE, CHECKBOX_SIZE, mgArgs(0));
	return mgDivEnd();
}


