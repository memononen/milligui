#include "milli2.h"
#include "nanovg.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include "nanosvg.h"


static int mi__mini(int a, int b) { return a < b ? a : b; }
static int mi__maxi(int a, int b) { return a > b ? a : b; }
static float mi__clampi(int a, int mn, int mx) { return a < mn ? mn : (a > mx ? mx : a); }
static float mi__minf(float a, float b) { return a < b ? a : b; }
static float mi__maxf(float a, float b) { return a > b ? a : b; }
static float mi__clampf(float a, float mn, float mx) { return a < mn ? mn : (a > mx ? mx : a); }
static float mi__absf(float a) { return a < 0.0f ? -a : a; }


MIcolor miRGBA(int r, int g, int b, int a)
{
	MIcolor col;
	col.r = r;
	col.g = g;
	col.b = b;
	col.a = a;
	return col;
}

static struct NVGcolor mi__nvgColMilli(struct MIcolor col)
{
	struct NVGcolor c;
	c.r = col.r / 255.0f;
	c.g = col.g / 255.0f;
	c.b = col.b / 255.0f;
	c.a = col.a / 255.0f;
	return c;
}

static struct NVGcolor mi__nvgColUint(unsigned int col)
{
	struct NVGcolor c;
	c.r = (col & 0xff) / 255.0f;
	c.g = ((col >> 8) & 0xff) / 255.0f;
	c.b = ((col >> 16) & 0xff) / 255.0f;
	c.a = ((col >> 24) & 0xff) / 255.0f;
	return c;
}


static MIrect mi__inflateRect(MIrect r, float size)
{
	r.x -= size;
	r.y -= size;
	r.width = mi__maxf(0, r.width + size*2);
	r.height = mi__maxf(0, r.height + size*2);
	return r;
}

static int mi__pointInRect(float x, float y, MIrect r)
{
   return x >= r.x &&x <= r.x+r.width && y >= r.y && y <= r.y+r.height;
}

static char* mi__codepointToUTF8(int cp, char* str)
{
	int n = 0;
	if (cp < 0x80) n = 1;
	else if (cp < 0x800) n = 2;
	else if (cp < 0x10000) n = 3;
	else if (cp < 0x200000) n = 4;
	else if (cp < 0x4000000) n = 5;
	else if (cp <= 0x7fffffff) n = 6;
	str[n] = '\0';
	switch (n) {
	case 6: str[5] = 0x80 | (cp & 0x3f); cp = cp >> 6; cp |= 0x4000000;
	case 5: str[4] = 0x80 | (cp & 0x3f); cp = cp >> 6; cp |= 0x200000;
	case 4: str[3] = 0x80 | (cp & 0x3f); cp = cp >> 6; cp |= 0x10000;
	case 3: str[2] = 0x80 | (cp & 0x3f); cp = cp >> 6; cp |= 0x800;
	case 2: str[1] = 0x80 | (cp & 0x3f); cp = cp >> 6; cp |= 0xc0;
	case 1: str[0] = cp;
	}
	return str;
}


enum MIdrawCommandType {
	MI_SHAPE_RECT,
	MI_SHAPE_TEXT,
};

struct MIshape {
	int type;
	MIcolor color;
	MIrect rect;
	const char* text;
	int textAlign;
	float fontSize;
	int fontFace;
	struct MIshape* next;
};
typedef struct MIshape MIshape;

#define MAX_LAYOUTS 16
#define MAX_LAYOUT_CELLS 16
struct MIlayout {
	int dir;
	float spacing;
	MIrect rect;
	MIrect space;
	float cellWidths[MAX_LAYOUT_CELLS];
	float cellMin[MAX_LAYOUT_CELLS];
	float cellMax[MAX_LAYOUT_CELLS];
	int cellCount;
	int cellIndex;
	int depth;
	MIhandle handle;

	int pack;
	int align;
	float colWidth;
	float rowHeight;

	float xmin, xmax;
	float ymin, ymax;

	float cxmin, cxmax;
	float cymin, cymax;

};
typedef struct MIlayout MIlayout;

struct MIpanel {
	MIrect rect;
//	MIrect space;
	MIshape* shapesHead;
	MIshape* shapesTail;
	unsigned int id, childCount;
	MIhandle handle;
	int visible;
	int modal;
//	int dir;
//	float spacing;
	MIlayout layoutStack[MAX_LAYOUTS];
	int layoutStackCount;
};
typedef struct MIpanel MIpanel;

struct MIbox {
	MIhandle handle;
	MIrect rect;
};
typedef struct MIbox MIbox;

struct MIstateBlock {
	MIhandle handle;
	int attr;
	int offset, size;
	int touch;
};
typedef struct MIstateBlock MIstateBlock;

struct MIiconImage
{
	char* name;
	struct NSVGimage* image;
};
typedef struct MIiconImage MIiconImage;

#define MAX_PANELS 100
#define MAX_SHAPES 1000
#define MAX_BOXES 1000
#define MAX_TEXT 8000
#define MAX_ICONS 100
#define MAX_STATES 100
#define MAX_STATEMEM 8000

struct MIcontext {
	struct NVGcontext* vg;
	int width, height;
	float dt;
	
	MIinputState input;

	int moved;
	int clickCount;
	float timeSincePress;

	MIhandle hoverPanel;

	MIhandle hover;
	MIhandle active;
	MIhandle focus;

	MIhandle focused;
	MIhandle blurred;

	MIhandle pressed;
	MIhandle released;
	MIhandle clicked;
	MIhandle dragged;
	MIhandle changed;

	float startmx, startmy;

	MIpanel panelPool[MAX_PANELS];
	int panelPoolSize;
	MIpanel* panelStack[MAX_PANELS];
	int panelStackHead;

	MIshape shapePool[MAX_SHAPES];
	int shapePoolSize;

	MIbox boxPool[MAX_BOXES];
	int boxPoolSize;

	char textPool[MAX_TEXT];
	int textPoolSize;

	MIstateBlock statePool[MAX_STATES];
	int statePoolSize;
	char stateMem[MAX_STATEMEM];
	int stateMemSize;

	int fontIds[MI_COUNT_FONTS];

	struct MIiconImage* icons[MAX_ICONS];
	int iconCount;
};
typedef struct MIcontext MIcontext;

static MIcontext g_context;

static void* mi__getState(MIhandle handle, int attr, int size)
{
	int i;
	MIstateBlock* state;

	size = (size+0xf) & ~0xf;	// round up for pointer alignment

	// Return existing state if possible.
	for (i = 0; i < g_context.statePoolSize; i++) {
		state = &g_context.statePool[i];
		if (state->handle == handle && state->attr == attr && state->size == size) {
			state->touch = 1;
			return (void*)&g_context.stateMem[state->offset];
		}
	}
	// Alloc new state.
	if (g_context.statePoolSize+1 > MAX_STATES || g_context.stateMemSize+size > MAX_STATEMEM)
		return NULL;
	state = &g_context.statePool[g_context.statePoolSize];
	g_context.statePoolSize++;
	state->handle = handle;
	state->attr = attr;
	state->size = size;
	state->offset = g_context.stateMemSize;
	state->touch = 1;
	g_context.stateMemSize += size;
	memset(&g_context.stateMem[state->offset], 0, size);

	printf("alloc new %x attr=%d offset=%d size=%d\n", state->handle, attr, state->offset, state->size);

	return (void*)&g_context.stateMem[state->offset];
}

static void mi__garbageCollectState()
{
	int i, n = 0, offset = 0, touch, freed = 0;
	MIstateBlock* state;
	for (i = 0; i < g_context.statePoolSize; i++) {
		state = &g_context.statePool[i];
		touch = state->touch;
		state->touch = 0;
		if (touch) {
			if (offset != state->offset) {
				memmove(&g_context.stateMem[offset], &g_context.stateMem[state->offset], state->size);
				state->offset = offset;
			}
			offset += state->size;
			if (n != i)
				g_context.statePool[n] = g_context.statePool[i];
			n++;
		} else {
			printf("freeing %x size=%d\n", state->handle, state->size);
			freed = 1;
		}
	}
	if (freed) {
		printf("after free: %d->%d  mem: %d->%d\n", g_context.statePoolSize, n, g_context.stateMemSize, offset);
	}
	g_context.statePoolSize = n;
	g_context.stateMemSize = offset;
}

static char* mi__allocText(const char* text, int len)
{
	char* ret;
	if (g_context.textPoolSize+len > MAX_TEXT)
		return NULL;
	ret = &g_context.textPool[g_context.textPoolSize];
	memcpy(ret, text, len);
	g_context.textPoolSize += len;
	return ret;
}

static MIshape* mi__allocShape()
{
	MIshape* ret;
	if (g_context.shapePoolSize+1 > MAX_SHAPES)
		return NULL;
	ret = &g_context.shapePool[g_context.shapePoolSize];
	memset(ret, 0, sizeof(*ret));
	g_context.shapePoolSize++;
	return ret;
}

static MIbox* mi__allocBox()
{
	MIbox* ret;
	if (g_context.boxPoolSize+1 > MAX_BOXES)
		return NULL;
	ret = &g_context.boxPool[g_context.boxPoolSize];
	memset(ret, 0, sizeof(*ret));
	g_context.boxPoolSize++;
	return ret;
}

static MIpanel* mi__allocPanel()
{
	MIpanel* ret;
	if (g_context.panelPoolSize+1 > MAX_PANELS)
		return NULL;
	ret = &g_context.panelPool[g_context.panelPoolSize];
	memset(ret, 0, sizeof(*ret));
	ret->id = (unsigned int)(g_context.panelPoolSize + 1);
	g_context.panelPoolSize++;
	return ret;
}

static void mi__addShape(MIpanel* panel, MIshape* shape)
{
	if (panel->shapesTail == NULL) {
		panel->shapesHead = panel->shapesTail = shape;
	} else {
		panel->shapesTail->next = shape;
		panel->shapesTail = shape;
	}
}

static MIbox* mi__getBoxByHandle(MIhandle handle)
{
	int i;
	for (i = 0; i < g_context.boxPoolSize; i++) {
		if (g_context.boxPool[i].handle == handle)
			return &g_context.boxPool[i];
	}
	return NULL;
}


static void mi__drawRect(MIpanel* panel, float x, float y, float width, float height, MIcolor col)
{
	MIshape* shape = mi__allocShape();
	if (shape == NULL) return;
	shape->type = MI_SHAPE_RECT;
	shape->rect.x = x;
	shape->rect.y = y;
	shape->rect.width = width;
	shape->rect.height = height;
	shape->color = col;
	mi__addShape(panel, shape);
}

static void mi__drawText(MIpanel* panel, float x, float y, float width, float height, const char* text, MIcolor col, int textAlign, int fontFace, int fontSize)
{
	MIshape* shape = mi__allocShape();
	if (shape == NULL) return;
	shape->type = MI_SHAPE_TEXT;
	shape->rect.x = x;
	shape->rect.y = y;
	shape->rect.width = width;
	shape->rect.height = height;
	shape->color = col;
	shape->text = mi__allocText(text, strlen(text)+1);
	shape->textAlign = textAlign;
	shape->fontFace = fontFace;
	shape->fontSize = fontSize;
	mi__addShape(panel, shape);
}

static int mi__measureTextGlyphs(struct NVGglyphPosition* pos, int maxpos,
								 float x, float y, float width, float height,
								 const char* text, int textAlign, int fontFace, int fontSize)
{
	struct NVGcontext* vg = g_context.vg;
	int i, count = 0;
	nvgSave(vg);
	nvgFontFaceId(vg, fontFace);
	nvgFontSize(vg, fontSize);
	nvgTextAlign(vg, textAlign);
	if (textAlign & NVG_ALIGN_CENTER)
		x = x + width/2;
	else if (textAlign & NVG_ALIGN_RIGHT)
		x = x + width;
	if (textAlign & NVG_ALIGN_MIDDLE)
		y = y + height/2;
	else if (textAlign & NVG_ALIGN_BOTTOM)
		y = y + height;
	else if (textAlign & NVG_ALIGN_BASELINE)
		y = y + height/2 - fontSize/2;
	count = nvgTextGlyphPositions(vg, x, y, text, NULL, pos, maxpos);
	nvgRestore(vg);

	// Turn str to indices.
	for (i = 0; i < count; i++)
		pos[i].str -= text;

	return count;
}

MIsize miMeasureText(const char* text, int fontFace, float fontSize)
{
	float bounds[4];
	MIsize size = {0,0};
	struct NVGcontext* vg = g_context.vg;
	if (vg == NULL) return size;
	nvgSave(vg);
	nvgFontFaceId(vg, fontFace);
	nvgFontSize(vg, fontSize);
	nvgTextBounds(vg, 0,0, text, NULL, bounds);
	size.width = bounds[2] - bounds[0];
	size.height = bounds[3] - bounds[1];
	nvgRestore(vg);
	return size;
}

static struct MIiconImage* mi__findIcon(const char* name)
{
	int i = 0;
	for (i = 0; i < g_context.iconCount; i++) {
		if (strcmp(g_context.icons[i]->name, name) == 0)
			return g_context.icons[i];
	}
	printf("Could not find icon '%s'\n", name);
	return NULL;
}

static void mi__scaleIcon(struct NSVGimage* image, float scale)
{
	int i;
	struct NSVGshape* shape = NULL;
	struct NSVGpath* path = NULL;
	image->width *= scale;
	image->height *= scale;
	for (shape = image->shapes; shape != NULL; shape = shape->next) {
		for (path = shape->paths; path != NULL; path = path->next) {
			path->bounds[0] *= scale;
			path->bounds[1] *= scale;
			path->bounds[2] *= scale;
			path->bounds[3] *= scale;
			for (i = 0; i < path->npts; i++) {
				path->pts[i*2+0] *= scale;
				path->pts[i*2+1] *= scale;
			}
		}
	}
	// TODO scale gradients.
}

int miCreateIconImage(const char* name, const char* filename, float scale)
{
	struct MIiconImage* icon = NULL;

	if (g_context.iconCount >= MAX_ICONS)
		return -1;

	icon = (struct MIiconImage*)malloc(sizeof(struct MIiconImage));
	if (icon == NULL) goto error;
	memset(icon, 0, sizeof(struct MIiconImage));

	icon->name = (char*)malloc(strlen(name)+1);
	if (icon->name == NULL) goto error;
	strcpy(icon->name, name);

	icon->image = nsvgParseFromFile(filename, "px", 96.0f);
	if (icon->image == NULL) goto error;

	// Scale
	if (scale > 0.0f)
		mi__scaleIcon(icon->image, scale);

	g_context.icons[g_context.iconCount++] = icon;

	return 0;

error:
	if (icon != NULL) {
		if (icon->name != NULL)
			free(icon->name);
		if (icon->image != NULL)
			nsvgDelete(icon->image);
		free(icon);
	}
	return -1;
}

static void mi__deleteIcons()
{
	int i;
	for (i = 0; i < g_context.iconCount; i++) {
		if (g_context.icons[i]->image != NULL)
			nsvgDelete(g_context.icons[i]->image);
		free(g_context.icons[i]->name);
		free(g_context.icons[i]);
	}
	g_context.iconCount = 0;
}

static void mi__drawIcon(struct NVGcontext* vg, struct MIrect* rect, struct NSVGimage* image, struct MIcolor* color)
{
	int i;
	struct NSVGshape* shape = NULL;
	struct NSVGpath* path;
	float sx, sy, s;

	if (image == NULL) return;

	if (color != NULL) {
		nvgFillColor(vg, mi__nvgColMilli(*color));
		nvgStrokeColor(vg, mi__nvgColMilli(*color));
	}
	sx = rect->width / image->width;
	sy = rect->height / image->height;
	s = mi__minf(sx, sy);

	nvgSave(vg);
	nvgTranslate(vg, rect->x + rect->width/2, rect->y + rect->height/2);
	nvgScale(vg, s, s);
	nvgTranslate(vg, -image->width/2, -image->height/2);

	for (shape = image->shapes; shape != NULL; shape = shape->next) {
		if (shape->fill.type == NSVG_PAINT_NONE && shape->stroke.type == NSVG_PAINT_NONE) continue;

		nvgBeginPath(vg);
		for (path = shape->paths; path != NULL; path = path->next) {
			nvgMoveTo(vg, path->pts[0], path->pts[1]);
			for (i = 1; i < path->npts; i += 3) {
				float* p = &path->pts[i*2];
				nvgBezierTo(vg, p[0],p[1], p[2],p[3], p[4],p[5]);
			}
			if (path->closed)
				nvgLineTo(vg, path->pts[0], path->pts[1]);
			nvgPathWinding(vg, NVG_REVERSE);
		}

		if (shape->fill.type == NSVG_PAINT_COLOR) {
			if (color == NULL)
				nvgFillColor(vg, mi__nvgColUint(shape->fill.color));
			nvgFill(vg);
//			printf("image %s\n", w->icon.icon->name);
//			nvgDebugDumpPathCache(vg);
		}
		if (shape->stroke.type == NSVG_PAINT_COLOR) {
			if (color == NULL)
				nvgStrokeColor(vg, mi__nvgColUint(shape->stroke.color));
			nvgStrokeWidth(vg, shape->strokeWidth);
			nvgStroke(vg);
		}
	}

	nvgRestore(vg);
}

int miCreateFont(int face, const char* filename)
{
	const char* names[] = {
		"sans",
		"sans-italic",
		"sans-bold",
	};
	int idx = -1;
	if (face < 0 || face >= MI_COUNT_FONTS) return -1;
	if (g_context.vg == NULL) return -1;

	idx = nvgCreateFont(g_context.vg, names[face], filename);
	if (idx == -1) return -1;
	g_context.fontIds[face] = idx;

	return 0;
}

int miInit(struct NVGcontext* vg)
{
	memset(&g_context, 0, sizeof(g_context));

	g_context.vg = vg;

	return 1;
}

void miTerminate()
{
	mi__deleteIcons();
}


static void mi__drawPanel(MIpanel* panel)
{
	struct NVGcontext* vg = g_context.vg;
	MIshape* s;

	if (!panel->visible) return;

	for (s = panel->shapesHead; s != NULL; s = s->next) {
		nvgSave(vg);
		if (s->type == MI_SHAPE_RECT) {
			nvgBeginPath(vg);
			nvgRect(vg, s->rect.x,s->rect.y,s->rect.width,s->rect.height);
			nvgFillColor(vg, mi__nvgColMilli(s->color));
			nvgFill(vg);
		} else if (s->type == MI_SHAPE_TEXT) {
			float x = s->rect.x, y = s->rect.y;
			nvgFillColor(vg, mi__nvgColMilli(s->color));
			nvgFontFaceId(vg, s->fontFace);
			nvgFontSize(vg, s->fontSize);
			nvgTextAlign(vg, s->textAlign);
			if (s->textAlign & NVG_ALIGN_LEFT)
				x = s->rect.x;
			else if (s->textAlign & NVG_ALIGN_CENTER)
				x = s->rect.x + s->rect.width/2;
			else if (s->textAlign & NVG_ALIGN_RIGHT)
				x = s->rect.x + s->rect.width;
			if (s->textAlign & NVG_ALIGN_TOP)
				y = s->rect.y;
			else if (s->textAlign & NVG_ALIGN_MIDDLE)
				y = s->rect.y + s->rect.height/2;
			else if (s->textAlign & NVG_ALIGN_BOTTOM)
				y = s->rect.y + s->rect.height;
			else if (s->textAlign & NVG_ALIGN_BASELINE)
				y = s->rect.y + s->rect.height/2 - s->fontSize/2;
			nvgText(vg, x, y, s->text, NULL);
		}
		nvgRestore(vg);
	}
}

void miFrameBegin(int width, int height, MIinputState* input, float dt)
{
	int i;

	g_context.moved = mi__absf(g_context.input.mx - input->mx) > 0.01f || mi__absf(g_context.input.my - input->my) > 0.01f;
	g_context.width = width;
	g_context.height = height;
	g_context.input = *input;
	g_context.dt = dt;
	memset(input, 0, sizeof(*input));

	// Before reseting the pools, check in which panel the mouse is.
	// Only that panel will receive mouse events. Start from top.
	g_context.hoverPanel = 0;
	for (i = g_context.panelPoolSize-1; i >= 0; i--) {
		MIpanel* panel = &g_context.panelPool[i];
		if (panel->visible && (panel->modal || mi__pointInRect(g_context.input.mx, g_context.input.my, panel->rect))) {
			g_context.hoverPanel = panel->handle;
			break;
		}
	}

	if (g_context.input.mbut & MI_MOUSE_PRESSED) {
		if (g_context.timeSincePress < 0.5f)
			g_context.clickCount++;
		else
			g_context.clickCount = 1;
		g_context.timeSincePress = 0;
	} else {
		g_context.timeSincePress += mi__minf(g_context.dt, 0.1f);
	}

	g_context.panelPoolSize = 0;
	g_context.panelStackHead = 0;
	g_context.shapePoolSize = 0;
	g_context.boxPoolSize = 0;
	g_context.textPoolSize = 0;

	g_context.dragged = g_context.moved ? g_context.active : 0;

	g_context.hover = 0;
	g_context.pressed = 0;
	g_context.released = 0;
	g_context.clicked = 0;
	g_context.focused = 0;
	g_context.changed = 0;
}

void miFrameEnd()
{
	int i;
	for (i = 0; i < g_context.panelPoolSize; i++)
		mi__drawPanel(&g_context.panelPool[i]);

	g_context.blurred = 0;
	if (g_context.input.mbut & MI_MOUSE_PRESSED) {
		if (g_context.hover == 0) {
			g_context.blurred = g_context.focus;
			g_context.focus = 0;
		}
	}

	 mi__garbageCollectState();
}

static void mi__pushPanel(MIpanel* panel)
{
	if (g_context.panelStackHead+1 > MAX_PANELS) return;
	g_context.panelStack[g_context.panelStackHead] = panel;
	g_context.panelStackHead++;
}

static MIpanel* mi__popPanel()
{
	if (g_context.panelStackHead <= 0) return NULL;
	g_context.panelStackHead--;
	return g_context.panelStack[g_context.panelStackHead];
}

static MIpanel* mi__curPanel()
{
	if (g_context.panelStackHead <= 0) return NULL;
	return g_context.panelStack[g_context.panelStackHead-1];
}

static MIhandle mi__allocHandle(MIpanel* panel)
{
	MIhandle h = 0;
	h  = (panel->id << 16) | panel->childCount;
	panel->childCount++;
	return h;
}


#define PANEL_PADDING 16
#define LAYOUT_SPACING 8

#define DEFAULT_WIDTH 256
#define DEFAULT_HEIGHT 48

static MIlayout* mi__pushLayout(MIpanel* panel)
{
	MIlayout* ret;
	if (panel->layoutStackCount+1 > MAX_LAYOUTS)
		return NULL;
	ret = &panel->layoutStack[panel->layoutStackCount];
	memset(ret, 0, sizeof(*ret));
	ret->depth = panel->layoutStackCount;
	panel->layoutStackCount++;
	return ret;
} 

static MIlayout* mi__getLayout(MIpanel* panel)
{
	if (panel->layoutStackCount < 1) return NULL;
	return &panel->layoutStack[panel->layoutStackCount-1];
}

static MIlayout* mi__popLayout(MIpanel* panel)
{
	if (panel->layoutStackCount < 1) return NULL;
	panel->layoutStackCount--;
	return &panel->layoutStack[panel->layoutStackCount];
}

static void mi__initLayout(MIpanel* panel, int dir, float x, float y, float width, float height, float spacing)
{
	MIlayout* layout = NULL;

	panel->rect.x = x;
	panel->rect.y = y;
	panel->rect.width = width;
	panel->rect.height = height;
	panel->layoutStackCount = 0;

	layout = mi__pushLayout(panel); 
	if (layout == NULL) return;
	layout->dir = dir;
	layout->spacing = spacing;
	layout->rect = panel->rect;

	layout->xmin = layout->rect.x + PANEL_PADDING;
	layout->ymin = layout->rect.y + PANEL_PADDING;
	layout->xmax = layout->xmin + mi__maxf(0, layout->rect.width - PANEL_PADDING*2);
	layout->ymax = layout->ymin + mi__maxf(0, layout->rect.height - PANEL_PADDING*2);

	layout->cxmin = layout->xmax;
	layout->cymin = layout->ymax;
	layout->cxmax = layout->xmin;
	layout->cymax = layout->ymin;

	layout->pack = MI_TOP_BOTTOM;
	layout->align = MI_TOP_BOTTOM;
	layout->colWidth = 0;
	layout->rowHeight = 0;

/*	layout->space.x = layout->rect.x + PANEL_PADDING;
	layout->space.y = layout->rect.y + PANEL_PADDING;
	layout->space.width = 0;
	layout->space.height = 0;

	if (layout->dir == MI_COL) {
		if (layout->rect.width < 1.0f)
			layout->rect.width = DEFAULT_WIDTH;
		layout->space.width = mi__maxf(0, layout->rect.width - PANEL_PADDING*2);
	} else {
		if (layout->rect.height < 1.0f)
			layout->rect.height = DEFAULT_HEIGHT;
		layout->space.height = mi__maxf(0, layout->rect.height - PANEL_PADDING*2);
	}*/

//	printf("init layout\n");
}

static MIrect mi__layoutRect(MIpanel* panel, MIlayout* layout, MIsize content)
{
	MIrect rect = {0,0,0,0};
	if (layout == NULL) layout = mi__getLayout(panel);
	if (layout == NULL) return rect;

	if (layout->pack == MI_TOP_BOTTOM || layout->pack == MI_BOTTOM_TOP) {
		if (layout->colWidth > 0)
			content.width = layout->colWidth;
		else
			content.width = layout->xmax - layout->xmin;
	}
	if (layout->pack == MI_LEFT_RIGHT || layout->pack == MI_RIGHT_LEFT) {
		if (layout->rowHeight > 0)
			content.height = layout->rowHeight;
		else
			content.height = layout->ymax - layout->ymin;
	}

	if (layout->cellCount > 0) {
		if (layout->pack == MI_TOP_BOTTOM) {

			rect.y = layout->ymin + layout->cellMin[layout->cellIndex];
			rect.height = layout->cellWidths[layout->cellIndex];
			rect.x = layout->align == MI_RIGHT_LEFT ? layout->xmax - content.width : layout->xmin;
			rect.width = content.width; //layout->xmax - layout->xmin;

			layout->cellIndex++;
			if (layout->cellIndex >= layout->cellCount) {
				layout->cellIndex = 0;
				if (layout->align == MI_RIGHT_LEFT)
					layout->xmax = layout->cxmin;
				else
					layout->xmin = layout->cxmax;
			}

		} else if (layout->pack == MI_LEFT_RIGHT) {

			rect.x = layout->xmin + layout->cellMin[layout->cellIndex];
			rect.width = layout->cellWidths[layout->cellIndex];
			rect.y = layout->align == MI_BOTTOM_TOP ? layout->ymax - content.height : layout->ymin;
			rect.height = content.height; //layout->ymax - layout->ymin;

			layout->cellIndex++;
			if (layout->cellIndex >= layout->cellCount) {
				layout->cellIndex = 0;
				if (layout->align == MI_BOTTOM_TOP)
					layout->ymax = layout->cymin;
				else
					layout->ymin = layout->cymax;
			}

		}
	} else {
		if (layout->pack == MI_TOP_BOTTOM) {
			rect.y = layout->ymin;
			rect.height = content.height;
			layout->ymin += content.height;
			rect.x = layout->align == MI_RIGHT_LEFT ? layout->xmax - content.width : layout->xmin;
			rect.width = content.width; //layout->xmax - layout->xmin;
		} else if (layout->pack == MI_BOTTOM_TOP) {
			rect.y = layout->ymax - content.height;
			rect.height = content.height;
			layout->ymax -= content.height;
			rect.x = layout->align == MI_RIGHT_LEFT ? layout->xmax - content.width : layout->xmin;
			rect.width = content.width; //layout->xmax - layout->xmin;
		} else if (layout->pack == MI_LEFT_RIGHT) {
			rect.x = layout->xmin;
			rect.width = content.width;
			layout->xmin += content.width;
			rect.y = layout->align == MI_BOTTOM_TOP ? layout->ymax - content.height : layout->ymin;
			rect.height = content.height; //layout->ymax - layout->ymin;
		} else if (layout->pack == MI_RIGHT_LEFT) {
			rect.x = layout->xmax - content.width;
			rect.width = content.width;
			layout->xmax -= content.width;
			rect.y = layout->align == MI_BOTTOM_TOP ? layout->ymax - content.height : layout->ymin;
			rect.height = content.height; //layout->ymax - layout->ymin;
		} else if (layout->pack == MI_FILLX) {
			rect.x = layout->xmin;
			rect.width = layout->xmax - layout->xmin;
			layout->xmin += rect.width;
			rect.y = layout->align == MI_BOTTOM_TOP ? layout->ymax - content.height : layout->ymin;
			rect.height = content.height; //layout->ymax - layout->ymin;
		} else if (layout->pack == MI_FILLY) {
			rect.y = layout->ymin;
			rect.height = layout->ymax - layout->ymin;
			layout->ymin += rect.height;
			rect.x = layout->align == MI_RIGHT_LEFT ? layout->xmax - content.width : layout->xmin;
			rect.width = content.width; //layout->xmax - layout->xmin;
		}
	}

	layout->cxmin = mi__minf(layout->cxmin, rect.x);
	layout->cymin = mi__minf(layout->cymin, rect.y);
	layout->cxmax = mi__maxf(layout->cxmax, rect.x + content.width);
	layout->cymax = mi__maxf(layout->cymax, rect.y + content.height);

	mi__drawRect(panel, rect.x+2, rect.y+2, rect.width-4, rect.height-4, miRGBA(255,192,0,32));

	return rect;

#if 0
	if (layout->cellCount > 0) {
		// Stacking
//		printf("%*s  - div %d/%d\n", layout->depth*2, "", layout->cellIndex,layout->cellCount);

		if (layout->dir == MI_COL) {
/*			rect.x = layout->space.x;
			rect.y = layout->space.y + layout->space.height;
			rect.width = content.width;
			rect.height = mi__maxf(0, layout->cellWidths[layout->cellIndex] - spacing);
			layout->space.height += layout->cellWidths[layout->cellIndex];
			layout->spacing = spacing;*/

			if (layout->cellIndex >= layout->cellCount) {
				layout->cellIndex = 0;
				layout->space.x += layout->spacing + layout->space.width;
				layout->space.width = 0;
				layout->space.height = 0;
			}

			// Col
			float spaceStart = layout->cellIndex > 0 ? layout->spacing/2 : 0;
			float spaceEnd = layout->cellIndex < layout->cellCount-1 ? layout->spacing/2 : 0;
			rect.x = layout->space.x;
			rect.y = layout->space.y + layout->space.height + spaceStart;
			rect.width = layout->rect.width;
			rect.height = mi__maxf(0, layout->cellWidths[layout->cellIndex] - spaceStart - spaceEnd);
			layout->space.width = mi__maxf(layout->space.width, rect.width);
			layout->space.height += layout->cellWidths[layout->cellIndex];

//			mi__drawRect(panel, rect.x+2, rect.y+2, rect.width-4, rect.height-4, miRGBA(255,192,0,32));

			layout->cellIndex++;

		} else {

			if (layout->cellIndex >= layout->cellCount) {
				layout->cellIndex = 0;
				layout->space.y += layout->spacing + layout->space.height;
				layout->space.width = 0;
				layout->space.height = 0;
			}

			// Row
			float spaceStart = layout->cellIndex > 0 ? layout->spacing/2 : 0;
			float spaceEnd = layout->cellIndex < layout->cellCount-1 ? layout->spacing/2 : 0;
			rect.x = layout->space.x + layout->space.width + spaceStart;
			rect.y = layout->space.y;
			rect.width = mi__maxf(0, layout->cellWidths[layout->cellIndex] - spaceStart - spaceEnd);
			rect.height = layout->rect.height;
			layout->space.height = mi__maxf(layout->space.height, rect.height);
			layout->space.width += layout->cellWidths[layout->cellIndex];

//			mi__drawRect(panel, rect.x+2, rect.y+2, rect.width-4, rect.height-4, miRGBA(255,192,0,32));

			layout->cellIndex++;
		}
	} else {
		// Stacking
//		printf("%*s  - stack\n", layout->depth*2, "");
		if (layout->dir == MI_COL) {
			rect.x = layout->space.x;
			rect.y = layout->space.y + layout->space.height;
			if (layout->space.height > 0) rect.y += layout->spacing;
			rect.width = layout->space.width;
			rect.height = content.height;
			layout->space.height = rect.y + rect.height - layout->space.y;
		} else {
			rect.x = layout->space.x + layout->space.width;
			rect.y = layout->space.y;
			if (layout->space.width > 0) rect.x += layout->spacing;
			rect.width = content.width;
			rect.height = layout->space.height;
			layout->space.width = rect.x + rect.width - layout->space.x;
		}
	}

	return rect;
#endif
}

void miPack(int pack)
{
	MIlayout* layout;
	MIpanel* panel = mi__curPanel();
	if (panel == NULL) return;
	layout = mi__getLayout(panel);
	if (layout == NULL) return;

	layout->pack = pack;
}

void miColWidth(float width)
{
	MIlayout* layout;
	MIpanel* panel = mi__curPanel();
	if (panel == NULL) return;
	layout = mi__getLayout(panel);
	if (layout == NULL) return;

	layout->colWidth = width;
}

void miRowHeight(float height)
{
	MIlayout* layout;
	MIpanel* panel = mi__curPanel();
	if (panel == NULL) return;
	layout = mi__getLayout(panel);
	if (layout == NULL) return;

	layout->rowHeight = height;
}

MIhandle miDivsBegin(int pack, int count, float* divs)
{
	MIbox* box;
	MIlayout* layout;
	MIlayout* prevLayout;
	MIpanel* panel = mi__curPanel();
	if (panel == NULL) return 0;
	prevLayout = mi__getLayout(panel);
	if (prevLayout == NULL) return 0;
	layout = mi__pushLayout(panel);
	if (layout == NULL) return 0;
	box = mi__allocBox();
	if (box == NULL) return 0;

	layout->rect.x = prevLayout->xmin;
	layout->rect.y = prevLayout->ymin;
	layout->rect.width = prevLayout->xmax - prevLayout->xmin;
	layout->rect.height = prevLayout->ymax - prevLayout->ymin;

	float pad = 0;//PANEL_PADDING;
	layout->xmin = layout->rect.x + pad;
	layout->ymin = layout->rect.y + pad;
	layout->xmax = layout->xmin + mi__maxf(0, layout->rect.width - pad*2);
	layout->ymax = layout->ymin + mi__maxf(0, layout->rect.height - pad*2);

	layout->cxmin = layout->xmax;
	layout->cymin = layout->ymax;
	layout->cxmax = layout->xmin;
	layout->cymax = layout->ymin;

	layout->cellCount = mi__clampi(count, 1, MAX_LAYOUT_CELLS);
	layout->cellIndex = 0;
	int i;
	float size;
	float total = 0, avail = 0;

	if (pack == MI_LEFT_RIGHT) {
		size = prevLayout->colWidth > 0 ? prevLayout->colWidth : layout->xmax - layout->xmin;
	} else {
		size = prevLayout->rowHeight > 0 ? prevLayout->rowHeight : layout->ymax - layout->ymin;
	}

	if (divs != NULL) {
		int ngrow = 0;
		for (i = 0; i < layout->cellCount; i++) {
			if (divs[i] < 0) {
				ngrow++;
			} else {
				layout->cellWidths[i] = divs[i];
				total += layout->cellWidths[i];
			}
		}
		if (ngrow > 0) {
			avail = mi__maxf(0, size - total);
			for (i = 0; i < layout->cellCount; i++) {
				if (divs[i] < 0) {
					layout->cellWidths[i] = avail / ngrow;
					total += layout->cellWidths[i];
				}
			}
		}
	} else {
		for (i = 0; i < layout->cellCount; i++) {
			layout->cellWidths[i] = 1;
			total += 1.0f;
		}
	}

	for (i = 0; i < layout->cellCount; i++)
		layout->cellWidths[i] = layout->cellWidths[i]/total * size;

	total = 0;	
	for (i = 0; i < layout->cellCount; i++) {
		layout->cellMin[i] = total;
		layout->cellMax[i] = total + layout->cellWidths[i];
		total += layout->cellWidths[i];
	}

	layout->pack = pack;
	layout->align = prevLayout->pack;

	return layout->handle;
}

MIhandle miDivsEnd()
{
	float w, h;
	MIbox* box;
	MIlayout* prevLayout;
	MIlayout* layout;
	MIpanel* panel = mi__curPanel();
	if (panel == NULL) return 0;
	prevLayout = mi__popLayout(panel);	// this can mismatch, see how to signal if begin failed.
	if (prevLayout == NULL) { printf("no prev\n"); return 0; }
	layout = mi__getLayout(panel);
	if (layout == NULL) { printf("no cur\n"); return 0; }
	box = mi__getBoxByHandle(prevLayout->handle);
	if (box == NULL) return 0;

//	mi__drawRect(panel, prevLayout->cxmin, prevLayout->cymin, (prevLayout->cxmax - prevLayout->cxmin), (prevLayout->cymax - prevLayout->cymin), miRGBA(0,192,255,32));

	if (layout->pack == MI_TOP_BOTTOM) {
		layout->ymin = prevLayout->cymax;
	} else if (layout->pack == MI_BOTTOM_TOP) {
		layout->ymax = prevLayout->cymin;
	} else if (layout->pack == MI_LEFT_RIGHT) {
		layout->xmin = prevLayout->cxmax;
	} else if (layout->pack == MI_RIGHT_LEFT) {
		layout->xmax = prevLayout->cxmin;
	}

	layout->cxmin = mi__minf(layout->cxmin, prevLayout->cxmin);
	layout->cymin = mi__minf(layout->cymin, prevLayout->cymin);
	layout->cxmax = mi__maxf(layout->cxmax, prevLayout->cxmax);
	layout->cymax = mi__maxf(layout->cymax, prevLayout->cymax);

//	mi__drawRect(panel, layout->cxmin, layout->cymin, (layout->cxmax - layout->cxmin), (layout->cymax - layout->cymin), miRGBA(0,192,255,32));
	mi__drawRect(panel, layout->xmin, layout->ymin, (layout->xmax - layout->xmin), (layout->ymax - layout->ymin), miRGBA(0,192,255,32));

	return layout->handle;
}

MIhandle miLayoutBegin(int pack)
{
	MIbox* box;
	MIlayout* layout;
	MIlayout* prevLayout;
	MIpanel* panel = mi__curPanel();
	if (panel == NULL) return 0;
	prevLayout = mi__getLayout(panel);
	if (prevLayout == NULL) return 0;
	layout = mi__pushLayout(panel);
	if (layout == NULL) return 0;
	box = mi__allocBox();
	if (box == NULL) return 0;

	layout->rect.x = prevLayout->xmin;
	layout->rect.y = prevLayout->ymin;
	layout->rect.width = prevLayout->xmax - prevLayout->xmin;
	layout->rect.height = prevLayout->ymax - prevLayout->ymin;

	float pad = 0;//PANEL_PADDING;
	layout->xmin = layout->rect.x + pad;
	layout->ymin = layout->rect.y + pad;
	layout->xmax = layout->xmin + mi__maxf(0, layout->rect.width - pad*2);
	layout->ymax = layout->ymin + mi__maxf(0, layout->rect.height - pad*2);

	layout->cxmin = layout->xmax;
	layout->cymin = layout->ymax;
	layout->cxmax = layout->xmin;
	layout->cymax = layout->ymin;

	layout->pack = pack;
	layout->align = prevLayout->pack;

	return layout->handle;
}

MIhandle miLayoutEnd()
{
	float w, h;
	MIbox* box;
	MIlayout* prevLayout;
	MIlayout* layout;
	MIpanel* panel = mi__curPanel();
	if (panel == NULL) return 0;
	prevLayout = mi__popLayout(panel);	// this can mismatch, see how to signal if begin failed.
	if (prevLayout == NULL) { printf("no prev\n"); return 0; }
	layout = mi__getLayout(panel);
	if (layout == NULL) { printf("no cur\n"); return 0; }
	box = mi__getBoxByHandle(prevLayout->handle);
	if (box == NULL) return 0;

//	mi__drawRect(panel, prevLayout->cxmin, prevLayout->cymin, (prevLayout->cxmax - prevLayout->cxmin), (prevLayout->cymax - prevLayout->cymin), miRGBA(0,192,255,32));

	if (layout->pack == MI_TOP_BOTTOM) {
		layout->ymin = prevLayout->cymax;
	} else if (layout->pack == MI_BOTTOM_TOP) {
		layout->ymax = prevLayout->cymin;
	} else if (layout->pack == MI_LEFT_RIGHT) {
		layout->xmin = prevLayout->cxmax;
	} else if (layout->pack == MI_RIGHT_LEFT) {
		layout->xmax = prevLayout->cxmin;
	}

	layout->cxmin = mi__minf(layout->cxmin, prevLayout->cxmin);
	layout->cymin = mi__minf(layout->cymin, prevLayout->cymin);
	layout->cxmax = mi__maxf(layout->cxmax, prevLayout->cxmax);
	layout->cymax = mi__maxf(layout->cymax, prevLayout->cymax);

//	mi__drawRect(panel, layout->cxmin, layout->cymin, (layout->cxmax - layout->cxmin), (layout->cymax - layout->cymin), miRGBA(0,192,255,32));
	mi__drawRect(panel, layout->xmin, layout->ymin, (layout->xmax - layout->xmin), (layout->ymax - layout->ymin), miRGBA(0,192,255,32));

	return layout->handle;
}

MIhandle miDockBegin(int pack)
{
	miPack(pack);
	if (pack == MI_FILLY) pack = MI_TOP_BOTTOM;
	if (pack == MI_FILLX) pack = MI_LEFT_RIGHT;
	miLayoutBegin(pack);
}

MIhandle miDockEnd()
{
	miLayoutEnd();
}

#if 0
MIhandle miStackBegin(int dir, float height, float spacing)
{
	MIbox* box;
	MIlayout* layout;
	MIlayout* prevLayout;
	MIpanel* panel = mi__curPanel();
	if (panel == NULL) return 0;
	prevLayout = mi__getLayout(panel);
	if (prevLayout == NULL) return 0;
	layout = mi__pushLayout(panel);
	if (layout == NULL) return 0;
	box = mi__allocBox();
	if (box == NULL) return 0;

	MIsize content;
	if (dir == MI_COL) {
		content.width = height;
		content.height = DEFAULT_HEIGHT;
	} else {
		content.width = DEFAULT_WIDTH;
		content.height = height;
	}

	MIrect rect = mi__layoutRect(panel, prevLayout, content);

	layout->dir = dir;
	layout->rect = rect;
	layout->spacing = spacing;

	mi__drawRect(panel, layout->rect.x+2, layout->rect.y+2, layout->rect.width-4, layout->rect.height-4, miRGBA(255,0,192,32));

	layout->space.x = layout->rect.x;
	layout->space.y = layout->rect.y;
	layout->space.width = 0;
	layout->space.height = 0;

	if (layout->dir == MI_COL) {
		layout->space.width = layout->rect.width;
	} else {
		layout->space.height = layout->rect.height;
	}

	box->rect = layout->rect;

	return layout->handle;
}

MIhandle miStackEnd()
{
	float w, h;
	MIbox* box;
	MIlayout* prevLayout;
	MIlayout* layout;
	MIpanel* panel = mi__curPanel();
	if (panel == NULL) return 0;
	prevLayout = mi__popLayout(panel);	// this can mismatch, see how to signal if begin failed.
	if (prevLayout == NULL) { printf("no prev\n"); return 0; }
	layout = mi__getLayout(panel);
	if (layout == NULL) { printf("no cur\n"); return 0; }
	box = mi__getBoxByHandle(prevLayout->handle);
	if (box == NULL) return 0;

//	printf("%*s}\n", prevLayout->depth*2, "");

	// Carry allocations from previous to current.
	if (layout->cellCount > 0) {

		h = (prevLayout->space.y + prevLayout->space.height) - layout->space.y;
		//printf("%*s  - COL%s prev=%f cur=%f %f->%f\n", layout->depth*2, "", layout->cellCount ? "*":"", (prevLayout->space.y+prevLayout->space.height), (layout->space.y+layout->space.height), layout->space.height, h);
		if (h > layout->space.height)
			layout->space.height = h;

		w = (prevLayout->space.x + prevLayout->space.width) - layout->space.x;
		//printf("%*s  - ROW%s prev=%f cur=%f %f->%f\n", layout->depth*2, "", layout->cellCount ? "*":"", (prevLayout->space.x+prevLayout->space.width), (layout->space.x+layout->space.width), layout->space.width, w);
		if (w > layout->space.width)
			layout->space.width = w;

	} else {
		if (layout->dir == MI_COL) {
			h = (prevLayout->space.y + prevLayout->space.height) - layout->space.y;
			//printf("%*s  - COL%s prev=%f cur=%f %f->%f\n", layout->depth*2, "", layout->cellCount ? "*":"", (prevLayout->space.y+prevLayout->space.height), (layout->space.y+layout->space.height), layout->space.height, h);
			if (h > layout->space.height)
				layout->space.height = h;
		} else {
			w = (prevLayout->space.x + prevLayout->space.width) - layout->space.x;
			//printf("%*s  - ROW%s prev=%f cur=%f %f->%f\n", layout->depth*2, "", layout->cellCount ? "*":"", (prevLayout->space.x+prevLayout->space.width), (layout->space.x+layout->space.width), layout->space.width, w);
			if (w > layout->space.width)
				layout->space.width = w;
		}
	}

//	mi__drawRect(panel, prevLayout->space.x, prevLayout->space.y, prevLayout->space.width, prevLayout->space.height, miRGBA(0,255,0,32));
//	mi__drawRect(panel, layout->space.x, layout->space.y, layout->space.width, layout->space.height, miRGBA(0,0,255,32));
		
//	mi__drawRect(panel, layout->space.x, layout->space.y, layout->space.width, layout->space.height, miRGBA(255,255,255,32));

	box->rect.width = mi__maxf(box->rect.width, prevLayout->space.width); 
	box->rect.height = mi__maxf(box->rect.height, prevLayout->space.height); 

	return layout->handle;
}


MIhandle miDivsBegin(int dir, float* widths, int count, float height, float spacing)
{
	int i;
	float total = 0.0f, avail = 1.0f;
	MIbox* box;
	MIlayout* layout;
	MIlayout* prevLayout;
	MIpanel* panel = mi__curPanel();
	if (panel == NULL) return 0;
	prevLayout = mi__getLayout(panel);
	if (prevLayout == NULL) return 0;
	layout = mi__pushLayout(panel);
	if (layout == NULL) return 0;
	box = mi__allocBox();
	if (box == NULL) return 0;

	box->handle = mi__allocHandle(panel);
	layout->handle = box->handle;

	if (widths != NULL) {
		for (i = 0; i < count; i++)
			total += mi__maxf(0.0f, widths[i]);
	} else {
		total = (dir == MI_COL) ? DEFAULT_HEIGHT * count : DEFAULT_WIDTH * count;
	}

	MIsize content;
	if (dir == MI_COL) {
		content.width = height;
		content.height = total;
	} else {
		content.width = total;
		content.height = height;
	}

	MIrect rect = mi__layoutRect(panel, prevLayout, content);

//	printf("layout: (%fx%f) -> (%fx%f)\n", content.width, content.height, rect.width, rect.height);

	layout->dir = dir;
	layout->rect = rect;
	layout->spacing = spacing;

	layout->cellCount = mi__clampi(count, 1, MAX_LAYOUT_CELLS);
	layout->cellIndex = 0;
	total = 0;
	if (widths != NULL) {
		for (i = 0; i < layout->cellCount; i++) {
			layout->cellWidths[i] = widths[i];
			total += mi__maxf(1.0f, layout->cellWidths[i]);
		}
	} else {
		for (i = 0; i < layout->cellCount; i++) {
			layout->cellWidths[i] = 1;
			total += 1.0f;
		}
	}

/*
	if (layout->dir == MI_COL) {
		layout->rect.x = prevLayout->space.x + prevLayout->space.width + prevLayout->spacing;
		layout->rect.y = prevLayout->space.y;
		layout->rect.width = 2;
		layout->rect.height = prevLayout->space.height;
	} else {
		layout->rect.x = prevLayout->space.x;
		layout->rect.y = prevLayout->space.y + prevLayout->space.height + prevLayout->spacing;
		layout->rect.width = prevLayout->space.width;
		layout->rect.height = 2;
	}*/

	mi__drawRect(panel, layout->rect.x+2, layout->rect.y+2, layout->rect.width-4, layout->rect.height-4, miRGBA(255,192,0,32));

	layout->space.x = layout->rect.x;
	layout->space.y = layout->rect.y;
	layout->space.width = 0;
	layout->space.height = 0;
	if (layout->dir == MI_COL) {
		// Column
//		if (layout->rect.height < 1.0f)
//			layout->rect.height = DEFAULT_HEIGHT;
		avail = layout->rect.height;
	} else {
		// Row
//		if (layout->rect.width < 1.0f)
//			layout->rect.width = DEFAULT_WIDTH;
		avail = layout->rect.width;
	}

//	printf("cells: avail=%f: ", avail);
	for (i = 0; i < layout->cellCount; i++) {
		layout->cellWidths[i] = layout->cellWidths[i]/total * avail;
//		printf("%f ", layout->cellWidths[i]);
	}
//	printf("\n");

	box->rect = layout->rect;

	return box->handle;
//	printf("%*sbegin {\n", layout->depth*2, "");
}

MIhandle miDivsEnd()
{
	float w, h;
	MIbox* box;
	MIlayout* prevLayout;
	MIlayout* layout;
	MIpanel* panel = mi__curPanel();
	if (panel == NULL) return 0;
	prevLayout = mi__popLayout(panel);	// this can mismatch, see how to signal if begin failed.
	if (prevLayout == NULL) { printf("no prev\n"); return 0; }
	layout = mi__getLayout(panel);
	if (layout == NULL) { printf("no cur\n"); return 0; }
	box = mi__getBoxByHandle(prevLayout->handle);
	if (box == NULL) return 0;

//	printf("%*s}\n", prevLayout->depth*2, "");

	// Carry allocations from previous to current.
/*	h = (prevLayout->space.y + prevLayout->space.height) - layout->space.y;
	//printf("%*s  - COL%s prev=%f cur=%f %f->%f\n", layout->depth*2, "", layout->cellCount ? "*":"", (prevLayout->space.y+prevLayout->space.height), (layout->space.y+layout->space.height), layout->space.height, h);
	if (h > layout->space.height)
		layout->space.height = h;

	w = (prevLayout->space.x + prevLayout->space.width) - layout->space.x;
	//printf("%*s  - ROW%s prev=%f cur=%f %f->%f\n", layout->depth*2, "", layout->cellCount ? "*":"", (prevLayout->space.x+prevLayout->space.width), (layout->space.x+layout->space.width), layout->space.width, w);
	if (w > layout->space.width)
		layout->space.width = w;*/

	if (layout->cellCount > 0) {

		h = (prevLayout->space.y + prevLayout->space.height) - layout->space.y;
		//printf("%*s  - COL%s prev=%f cur=%f %f->%f\n", layout->depth*2, "", layout->cellCount ? "*":"", (prevLayout->space.y+prevLayout->space.height), (layout->space.y+layout->space.height), layout->space.height, h);
		if (h > layout->space.height)
			layout->space.height = h;

		w = (prevLayout->space.x + prevLayout->space.width) - layout->space.x;
		//printf("%*s  - ROW%s prev=%f cur=%f %f->%f\n", layout->depth*2, "", layout->cellCount ? "*":"", (prevLayout->space.x+prevLayout->space.width), (layout->space.x+layout->space.width), layout->space.width, w);
		if (w > layout->space.width)
			layout->space.width = w;

	} else {
		if (layout->dir == MI_COL) {
			h = (prevLayout->space.y + prevLayout->space.height) - layout->space.y;
			//printf("%*s  - COL%s prev=%f cur=%f %f->%f\n", layout->depth*2, "", layout->cellCount ? "*":"", (prevLayout->space.y+prevLayout->space.height), (layout->space.y+layout->space.height), layout->space.height, h);
			if (h > layout->space.height)
				layout->space.height = h;
		} else {
			w = (prevLayout->space.x + prevLayout->space.width) - layout->space.x;
			//printf("%*s  - ROW%s prev=%f cur=%f %f->%f\n", layout->depth*2, "", layout->cellCount ? "*":"", (prevLayout->space.x+prevLayout->space.width), (layout->space.x+layout->space.width), layout->space.width, w);
			if (w > layout->space.width)
				layout->space.width = w;
		}
	}

	box->rect.width = mi__maxf(box->rect.width, prevLayout->space.width); 
	box->rect.height = mi__maxf(box->rect.height, prevLayout->space.height); 

	return prevLayout->handle;
}
#endif

MIhandle miPanelBegin(float x, float y, float width, float height)
{
	MIpanel* panel = mi__allocPanel();
	if (panel == NULL) return 0;

	panel->visible = 1;
	panel->modal = 0;
	panel->handle = mi__allocHandle(panel);

	mi__pushPanel(panel);

//	mi__drawRect(panel, x, y, width, height, g_context.hoverPanel == panel->handle ? miRGBA(0,0,0,192) : miRGBA(0,0,0,128));
	mi__drawRect(panel, x, y, width, height, miRGBA(0,0,0,128));
	mi__initLayout(panel, MI_COL, x, y, width, height, LAYOUT_SPACING);

	return panel->handle;
}

MIhandle miPanelEnd()
{
	MIpanel* panel;
	panel = mi__popPanel();
	if (panel == NULL) return 0;
	return panel->handle;
}

int miIsHover(MIhandle handle)
{
	return g_context.hover == handle;
}

int miIsActive(MIhandle handle)
{
	return g_context.active == handle;
}

int miIsFocus(MIhandle handle)
{
	return g_context.focus == handle;
}

int miFocused(MIhandle handle)
{
	return g_context.focused == handle;
}

int miBlurred(MIhandle handle)
{
	return g_context.blurred == handle;
}

int miPressed(MIhandle handle)
{
	return g_context.pressed == handle;
}

int miReleased(MIhandle handle)
{
	return g_context.released == handle;
}

int miClicked(MIhandle handle)
{
	return g_context.clicked == handle;
}

int miDragged(MIhandle handle)
{
	return g_context.dragged == handle;
}

int miChanged(MIhandle handle)
{
	return g_context.changed == handle;
}

void miBlur(MIhandle handle)
{
	if (g_context.focus) {
		g_context.blurred = g_context.focus;
		g_context.focus = 0;
	}
}

void miChange(MIhandle handle)
{
	g_context.changed = handle;
}

MIpoint miMousePos()
{
	MIpoint pos;
	pos.x = g_context.input.mx;
	pos.y = g_context.input.my;
	return pos;
}

int miMouseClickCount()
{
	return g_context.clickCount;
}

int miGetKeyPress(MIkeyPress* key)
{
	int i;
	if (g_context.input.nkeys) {
		*key = g_context.input.keys[0];
		for (i = 0; i < g_context.input.nkeys-1; i++)
			g_context.input.keys[i] = g_context.input.keys[i+1];
		g_context.input.nkeys--;
		return 1;
	}
	return 0;
}

#define BUTTON_HEIGHT 32
#define BUTTON_PADDING 16
#define BUTTON_FONT_SIZE 18

static int mi__hitTest(MIpanel* panel, struct MIrect rect)
{
	if (g_context.hoverPanel != panel->handle) return 0;
	return mi__pointInRect(g_context.input.mx, g_context.input.my, rect);
}

static void mi__buttonLogic(int over, MIhandle handle)
{
	if (over) {
		if (g_context.active == 0 || g_context.active == handle)
			g_context.hover = handle;
	}

	if (g_context.input.mbut & MI_MOUSE_PRESSED) {
		if (g_context.active == 0 && g_context.hover == handle) {
			g_context.active = handle;
			g_context.pressed = handle;
			if (g_context.focus != handle) {
				if (g_context.focus != 0)
					g_context.blurred = g_context.focus;
				g_context.focus = handle;
				g_context.focused = handle;
			}
			g_context.input.mbut &= ~MI_MOUSE_PRESSED;
		}
	}

	if (g_context.input.mbut & MI_MOUSE_RELEASED) {
		if (g_context.active == handle) {
			g_context.active = 0;
			g_context.released = handle;
			g_context.input.mbut &= ~MI_MOUSE_RELEASED;
		}
	}

	if (g_context.hover == handle && g_context.released == handle)
		g_context.clicked = handle;
}


MIhandle miButton(const char* label)
{
	MIsize content;
	MIbox* box = NULL;
	MIpanel* panel = mi__curPanel();
	if (panel == NULL) return 0;
	box = mi__allocBox();
	if (box == NULL) return 0;

	box->handle = mi__allocHandle(panel);

	content = miMeasureText(label, MI_FONT_NORMAL, BUTTON_FONT_SIZE);
	content.width += BUTTON_PADDING*2;
	content.height = BUTTON_HEIGHT;

	box->rect = mi__layoutRect(panel, NULL, content);

	mi__buttonLogic(mi__hitTest(panel, box->rect), box->handle);

	if (miIsActive(box->handle))
		mi__drawRect(panel, box->rect.x, box->rect.y, box->rect.width, box->rect.height, miRGBA(255,0,0,192));
	else if (miIsHover(box->handle))
		mi__drawRect(panel, box->rect.x, box->rect.y, box->rect.width, box->rect.height, miRGBA(255,0,0,128));
	else
		mi__drawRect(panel, box->rect.x, box->rect.y, box->rect.width, box->rect.height, miRGBA(255,0,0,64));

	mi__drawText(panel, box->rect.x, box->rect.y, box->rect.width, box->rect.height, label, miRGBA(255,255,255,255), NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE, MI_FONT_NORMAL, BUTTON_FONT_SIZE);

	return box->handle;
}

#define TEXT_FONT_SIZE 18

MIhandle miText(const char* text)
{
	MIsize content;
	MIbox* box = NULL;
	MIpanel* panel = mi__curPanel();
	if (panel == NULL) return 0;
	box = mi__allocBox();
	if (box == NULL) return 0;

	box->handle = mi__allocHandle(panel);

	content = miMeasureText(text, MI_FONT_NORMAL, TEXT_FONT_SIZE);

	box->rect = mi__layoutRect(panel, NULL, content);
	mi__drawRect(panel, box->rect.x, box->rect.y, box->rect.width, box->rect.height, miRGBA(255,0,0,32));
	mi__drawText(panel, box->rect.x, box->rect.y, box->rect.width, box->rect.height, text, miRGBA(255,255,255,255), NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE, MI_FONT_NORMAL, TEXT_FONT_SIZE);

	return box->handle;
}

MIhandle miSpacer()
{
	MIsize content;
	MIbox* box = NULL;
	MIpanel* panel = mi__curPanel();
	if (panel == NULL) return 0;
	box = mi__allocBox();
	if (box == NULL) return 0;

	box->handle = mi__allocHandle(panel);

	content.width = 5;
	content.height = 5;

	box->rect = mi__layoutRect(panel, NULL, content);

	return box->handle;
}

#define INPUT_WIDTH 100
#define INPUT_HEIGHT 28

struct MItextInputState {
	int maxText;
	int caretPos;
	int nglyphs;
	int selPivot;
	int selStart, selEnd;
};
typedef struct MItextInputState MItextInputState;

static int findCaretPos(float x, struct NVGglyphPosition* glyphs, int nglyphs)
{
	float px;
	int i, caret;
	if (nglyphs == 0 || glyphs == NULL) return 0;
	if (x <= glyphs[0].x)
		return 0;
	px = glyphs[0].x;
	caret = nglyphs;
	for (i = 0; i < nglyphs; i++) {
		float x0 = glyphs[i].x;
		float x1 = (i+1 < nglyphs) ? glyphs[i+1].x : glyphs[nglyphs-1].maxx;
		float gx = x0 * 0.3f + x1 * 0.7f;
		if (x >= px && x < gx)
			caret = i;
		px = gx;
	}

	return caret;
}

static void insertText(char* dst, int ndst, int idx, char* str, int nstr)
{
	int i, count;
	if (idx < 0 || idx >= ndst) return;
	// Make space for the new string
	for (i = ndst-1; i >= idx+nstr; i--)
		dst[i] = dst[i-nstr];
	// Insert
	count = mi__mini(idx+nstr, ndst-1) - idx;
	for (i = 0; i < count; i++)
		dst[idx+i] = str[i];
	dst[ndst-1] = '\0';
}

static void deleteText(char* dst, int ndst, int idx, int ndel)
{
	int i;
	if (idx < 0 || idx >= ndst) return;
	for (i = idx; i < ndst-ndel; i++)
		dst[i] = dst[i+ndel];
}

static int isSpace(int c)
{
	switch (c) {
		case 9:			// \t
		case 11:		// \v
		case 12:		// \f
		case 32:		// space
			return 1;
	};
	return 0;
}

MIhandle miInput(char* text, int maxText)
{
	MIsize content;
	MIbox* box = NULL;
	MIrect hrect;
	MIpanel* panel = mi__curPanel();
	if (panel == NULL) return 0;
	box = mi__allocBox();
	if (box == NULL) return 0;

	box->handle = mi__allocHandle(panel);

	content.width = INPUT_WIDTH;
	content.height = INPUT_HEIGHT;

	box->rect = mi__layoutRect(panel, NULL, content);

	mi__buttonLogic(mi__hitTest(panel, box->rect), box->handle);

	if (miFocused(box->handle)) {
		printf("focused\n");
		MItextInputState* state = NULL;
		char* stateText = NULL;
		struct NVGglyphPosition* stateGlyphs = NULL;
		state = (MItextInputState*)mi__getState(box->handle, 0, sizeof(MItextInputState));
		stateText = (char*)mi__getState(box->handle, 0, maxText);
		stateGlyphs = (struct NVGglyphPosition*)mi__getState(box->handle, 0, sizeof(struct NVGglyphPosition)*maxText);
		if (state == NULL  || stateText == NULL || stateGlyphs == NULL) return 0;
		memcpy(stateText, text, maxText);
		state->maxText = maxText;
		state->nglyphs = mi__measureTextGlyphs(stateGlyphs, maxText,
							box->rect.x, box->rect.y, box->rect.width, box->rect.height,
							stateText, NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE, MI_FONT_NORMAL, TEXT_FONT_SIZE);
		state->caretPos = state->nglyphs;
		state->selStart = 0;
		state->selEnd = state->nglyphs;
		state->selPivot = -1;
	}

	if (miIsFocus(box->handle)) {
		int i;
		float caretx = 0;
		MItextInputState* state = NULL;
		char* stateText = NULL;
		struct NVGglyphPosition* stateGlyphs = NULL;
		state = (MItextInputState*)mi__getState(box->handle, 0, sizeof(MItextInputState));
		stateText = (char*)mi__getState(box->handle, 0, maxText);
		stateGlyphs = (struct NVGglyphPosition*)mi__getState(box->handle, 0, sizeof(struct NVGglyphPosition)*maxText);
		if (state == NULL  || stateText == NULL || stateGlyphs == NULL) return 0;

		if (miPressed(box->handle)) {
			MIpoint mouse = miMousePos();
			// Press
			if (miMouseClickCount() > 1) {
				state->selStart = 0;
				state->selEnd = state->selPivot = state->caretPos = state->nglyphs;
			} else {
				state->caretPos = findCaretPos(mouse.x, stateGlyphs, state->nglyphs);
				state->selStart = state->selEnd = state->selPivot = state->caretPos;
			}
		}
		if (miDragged(box->handle)) {
			MIpoint mouse = miMousePos();
			// Drag
			state->caretPos = findCaretPos(mouse.x, stateGlyphs, state->nglyphs);
			state->selStart = mi__mini(state->caretPos, state->selPivot);
			state->selEnd = mi__maxi(state->caretPos, state->selPivot);
		}

		MIkeyPress key;
		while (miGetKeyPress(&key)) {
			if (key.type == MI_KEYPRESSED) {
				if (key.code == 263) {
					// Left
					if (key.mods & 1) { // Shift
						if (state->selPivot == -1)
							state->selPivot = state->caretPos;
					}
					if (key.mods & 4) { // Alt
						// Prev word
						while (state->caretPos > 0 && isSpace(stateText[(int)stateGlyphs[state->caretPos-1].str]))
							state->caretPos--;
						while (state->caretPos > 0 && !isSpace(stateText[(int)stateGlyphs[state->caretPos-1].str]))
							state->caretPos--;
					} else {
						if (state->caretPos > 0)
							state->caretPos--;
					}
					if (key.mods & 1) { // Shift
						state->selStart = mi__mini(state->caretPos, state->selPivot);
						state->selEnd = mi__maxi(state->caretPos, state->selPivot);
					} else {
						if (state->selStart != state->selEnd)
							state->caretPos = state->selStart;
						state->selStart = state->selEnd = 0;
						state->selPivot = -1;
					}
				} else if (key.code == 262) {
					// Right
					if (key.mods & 1) { // Shift
						if (state->selPivot == -1)
							state->selPivot = state->caretPos;
					}
					if (key.mods & 4) { // Alt
						// Next word
						while (state->caretPos < state->nglyphs && isSpace(stateText[(int)stateGlyphs[state->caretPos].str]))
							state->caretPos++;
						while (state->caretPos < state->nglyphs && !isSpace(stateText[(int)stateGlyphs[state->caretPos].str]))
							state->caretPos++;
					} else {
						if (state->caretPos < state->nglyphs)
							state->caretPos++;
					}
					if (key.mods & 1) { // Shift
						state->selStart = mi__mini(state->caretPos, state->selPivot);
						state->selEnd = mi__maxi(state->caretPos, state->selPivot);
					} else {
						if (state->selStart != state->selEnd)
							state->caretPos = state->selEnd;
						state->selStart = state->selEnd = 0;
						state->selPivot = -1;
					}
				} else if (key.code == 259) {
					// Delete
					int del = 0, count = 0;
					if (state->selStart != state->selEnd) {
						del = state->selStart;
						count = state->selEnd - state->selStart;
						state->caretPos = state->selStart;
					} else if (state->caretPos > 0) {
						if (state->caretPos < state->nglyphs) {
							del = (int)stateGlyphs[state->caretPos-1].str;
							count = (int)stateGlyphs[state->caretPos].str - (int)stateGlyphs[state->caretPos-1].str;
							state->caretPos--;
						} else {
							del = (int)stateGlyphs[state->nglyphs-1].str;
							count = (int)strlen(stateText) - (int)stateGlyphs[state->nglyphs-1].str;
							state->caretPos = state->nglyphs-1;
						}
					}
					if (count > 0) {
						deleteText(stateText, state->maxText, del, count);
						state->nglyphs = mi__measureTextGlyphs(stateGlyphs, maxText,
											box->rect.x, box->rect.y, box->rect.width, box->rect.height,
											stateText, NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE, MI_FONT_NORMAL, TEXT_FONT_SIZE);
						// Store result
//						mgSetResultStr(w->id, stateText, state->maxText);
						strncpy(text, stateText, maxText);
						miChange(box->handle);

						state->selStart = state->selEnd = 0;
						state->selPivot = -1;
					}
				} else if (key.code == 258) {
					// Tab
//					mgSetResultStr(w->id, stateText, state->maxText);
					strncpy(text, stateText, maxText);
					miChange(box->handle);
/*					if (key.mods & 1)
						mgFocusPrev(w->id);
					else
						mgFocusNext(w->id);*/
				} else if (key.code == 257) {
					// Enter
//					mgSetResultStr(w->id, stateText, state->maxText);
					strncpy(text, stateText, maxText);
					miChange(box->handle);
					miBlur(box->handle);
				}
			} else if (key.type == MI_CHARTYPED) {
				int ins;
				char str[8];

				// Delete selection
				if (state->selStart != state->selEnd) {
					int del = 0, count = 0;
					del = state->selStart;
					count = state->selEnd - state->selStart;
					state->caretPos = state->selStart;
					if (count > 0) {
						deleteText(stateText, state->maxText, del, count);
						state->nglyphs = mi__measureTextGlyphs(stateGlyphs, maxText,
											box->rect.x, box->rect.y, box->rect.width, box->rect.height,
											stateText, NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE, MI_FONT_NORMAL, TEXT_FONT_SIZE);
						state->selStart = state->selEnd = 0;
						state->selPivot = -1;
					}
				}

				// Append
				mi__codepointToUTF8(key.code, str);
				if (state->caretPos >= 0 && state->caretPos < state->nglyphs)
					ins = (int)stateGlyphs[state->caretPos].str;
				else
					ins = strlen(stateText);
				insertText(stateText, state->maxText, ins, str, strlen(str));

				state->nglyphs = mi__measureTextGlyphs(stateGlyphs, maxText,
									box->rect.x, box->rect.y, box->rect.width, box->rect.height,
									stateText, NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE, MI_FONT_NORMAL, TEXT_FONT_SIZE);
				state->caretPos = mi__mini(state->caretPos + strlen(str), state->nglyphs);

				state->selStart = state->selEnd = 0;
				state->selPivot = -1;

				// Store result
				strncpy(text, stateText, maxText);
			}
		}

/*		for (i = 0; i < state->nglyphs; i++) {
			struct NVGglyphPosition* p = &stateGlyphs[i];
			mi__drawRect(panel, p->minx, box->rect.y, p->maxx - p->minx, box->rect.height, miRGBA(255,0,0,64));
		}*/

		mi__drawRect(panel, box->rect.x, box->rect.y, box->rect.width, box->rect.height, miRGBA(0,0,0,128));

		if (state->selStart != state->selEnd && state->nglyphs > 0) {
			float sx = (state->selStart >= state->nglyphs) ? stateGlyphs[state->nglyphs-1].maxx : stateGlyphs[state->selStart].x;
			float ex = (state->selEnd >= state->nglyphs) ? stateGlyphs[state->nglyphs-1].maxx : stateGlyphs[state->selEnd].x;
			mi__drawRect(panel, sx, box->rect.y, ex - sx, box->rect.height, miRGBA(255,0,0,64));
		}

		mi__drawText(panel, box->rect.x, box->rect.y, box->rect.width, box->rect.height, stateText, miRGBA(255,255,255,255),
			NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE, MI_FONT_NORMAL, TEXT_FONT_SIZE);

/*		nvgFillColor(vg, nvgRGBA(255,0,0,64));
		for (j = 0; j < state->nglyphs; j++) {
			struct NVGglyphPosition* p = &stateGlyphs[j];
			nvgBeginPath(vg);
			nvgRect(vg, p->minx, w->y, p->maxx - p->minx, w->height);
			nvgFill(vg);
		}*/

		if (state->nglyphs == 0) {
/*			if (w->style.textAlign == MG_CENTER)
				caretx = w->x + w->width/2;
			else if (w->style.textAlign == MG_END)
				caretx = w->x + w->width - w->style.paddingx;
			else*/
				caretx = box->rect.x;
		} else if (state->caretPos >= state->nglyphs) {
			caretx = stateGlyphs[state->nglyphs-1].maxx;
		} else {
			caretx = stateGlyphs[state->caretPos].x;
		}
		mi__drawRect(panel, (int)(caretx-0.5f), box->rect.y, 1, box->rect.height, miRGBA(255,0,0,255));
	} else {
		mi__drawRect(panel, box->rect.x, box->rect.y, box->rect.width, box->rect.height, miRGBA(0,0,0,32));
		mi__drawText(panel, box->rect.x, box->rect.y, box->rect.width, box->rect.height, text, miRGBA(255,255,255,255),
			NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE, MI_FONT_NORMAL, TEXT_FONT_SIZE);
	}

	return box->handle;
}


#define SLIDER_WIDTH 100
#define SLIDER_HEIGHT 28
#define SLIDER_HANDLE 16

struct MIsliderState {
	float startValue;
	float origValue;
};
typedef struct MIsliderState MIsliderState;

static MIrect handleRect(MIrect rect, float hsize, float value, float vmin, float vmax)
{
	MIrect res;
	float u = mi__clampf((value - vmin) / (vmax - vmin), 0, 1);
	res.x = rect.x + mi__maxf(0, rect.width - hsize) * u;
	res.y = rect.y + rect.height/2 - hsize/2;
	res.width = hsize;
	res.height = hsize;
	return res;
}

static float mapXToValue(float x, MIrect rect, float hsize, float vmin, float vmax)
{
	float xrange = mi__maxf(0, rect.width - hsize);
	float leftx = rect.x + hsize/2;
	if (xrange < 0.5f) return vmin;
	float u = (x - leftx) / xrange;
	return vmin + (vmax-vmin)*u;
}

MIhandle miSlider(float* value, float vmin, float vmax)
{
	MIsize content;
	MIbox* box = NULL;
	MIrect hrect;
	MIpanel* panel = mi__curPanel();
	if (panel == NULL) return 0;
	box = mi__allocBox();
	if (box == NULL) return 0;

	box->handle = mi__allocHandle(panel);

	content.width = SLIDER_WIDTH;
	content.height = SLIDER_HEIGHT;

	box->rect = mi__layoutRect(panel, NULL, content);

	hrect = handleRect(box->rect, SLIDER_HANDLE, *value, vmin, vmax);

	mi__buttonLogic(mi__hitTest(panel, box->rect), box->handle);

	if (miPressed(box->handle)) {
		MIsliderState* slider = (MIsliderState*)mi__getState(box->handle, 0, sizeof(MIsliderState));
		if (slider != NULL) {
			MIpoint mouse = miMousePos();
			float mval = mi__clampf(mapXToValue(mouse.x, box->rect, SLIDER_HANDLE, vmin, vmax), vmin, vmax);
			if (mouse.x < hrect.x || mouse.x > hrect.x+hrect.width)
				*value = mval;
			slider->startValue = mval;
			slider->origValue = *value;
		}
	}
	if (miIsActive(box->handle)) {
		MIsliderState* slider = (MIsliderState*)mi__getState(box->handle, 0, sizeof(MIsliderState));
		if (slider != NULL) {
			MIpoint mouse = miMousePos();
			float delta = mapXToValue(mouse.x, box->rect, SLIDER_HANDLE, vmin, vmax) - slider->startValue;
			*value = mi__clampf(slider->origValue + delta, vmin, vmax);
		}
	}

//	mi__drawRect(panel, box->rect.x, box->rect.y, box->rect.width, box->rect.height, miRGBA(255,0,0,32));

	mi__drawRect(panel, box->rect.x, hrect.y+SLIDER_HANDLE/2-1, box->rect.width, 2, miRGBA(255,255,255,128));
	if ((hrect.x - box->rect.x) > 0.5f)
		mi__drawRect(panel, box->rect.x, hrect.y+SLIDER_HANDLE/2-1, hrect.x - box->rect.x, 2, miRGBA(0,192,255,255));
	mi__drawRect(panel, hrect.x, hrect.y, hrect.width, hrect.height, miRGBA(255,255,255,255));


//	mi__drawText(panel, box->rect.x, box->rect.y, box->rect.width, box->rect.height, text, miRGBA(255,255,255,255), NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE, MI_FONT_NORMAL, TEXT_FONT_SIZE);

	return box->handle;
}

MIhandle miSliderValue(float* value, float vmin, float vmax)
{
	char num[32];
	float cols[2] = {-1, 50};
	snprintf(num,32,"%.2f", *value);
	miDivsBegin(MI_LEFT_RIGHT, 2, cols);
		miRowHeight(SLIDER_HEIGHT);
		miSlider(value, vmin, vmax);
		miText(num);
	return miDivsEnd();
}

void miPopupShow(MIhandle handle)
{
	MIpopupState* popup = (MIpopupState*)mi__getState(handle, 0, sizeof(MIpopupState));
	if (popup == NULL) return;
	popup->visible = 1;
}

void miPopupHide(MIhandle handle)
{
	MIpopupState* popup = (MIpopupState*)mi__getState(handle, 0, sizeof(MIpopupState));
	if (popup == NULL) return;
	popup->visible = 0;
}

void miPopupToggle(MIhandle handle)
{
	MIpopupState* popup = (MIpopupState*)mi__getState(handle, 0, sizeof(MIpopupState));
	if (popup == NULL) return;
	popup->visible = !popup->visible;
}

#define POPUP_SAFE_ZONE 10

MIhandle miPopupBegin(MIhandle base, int logic, int side)
{
	MIbox* box;
	MIpanel* panel;
	MIpopupState* popup;
	int wentVisible = 0;

	box = mi__getBoxByHandle(base);
	if (box == NULL) return 0;
	panel = mi__allocPanel();
	if (panel == NULL) return 0;
	panel->handle = mi__allocHandle(panel);
	popup = (MIpopupState*)mi__getState(panel->handle, 0, sizeof(MIpopupState));
	if (popup == NULL) return 0;

	popup->logic = logic;

	if (popup->logic == MI_ONCLICK) {
		// on spawn on click
		if (miClicked(base)) {
			popup->visible = !popup->visible;
			popup->visited = 0;
			if (popup->visible)
				wentVisible = 1;
		}
	} else {
		// on spawn on hover
		if (miIsHover(base)) {
			if (!popup->visible) {
				popup->visible = 1;
				popup->visited = 0;
				wentVisible = 1;
			}
		} else {
			if (!popup->visited) {
				popup->visible = 0;
			}
		}
	}

	// Close condition
	if (popup->visible) {
		if (popup->logic == MI_ONCLICK) {
			// on click
			if (g_context.input.mbut & MI_MOUSE_PRESSED) {
				if (g_context.active == 0 && g_context.hoverPanel <= panel->handle && popup->visited)
					popup->visible = 0;
			}
			popup->visited = 1;
		} else {
			// on hover
			if (g_context.input.mbut & MI_MOUSE_PRESSED) {
				if (g_context.active == 0 && g_context.hoverPanel <= panel->handle)
					popup->visible = 0;
			}

			if (mi__pointInRect(g_context.input.mx, g_context.input.my, mi__inflateRect(popup->rect, POPUP_SAFE_ZONE))) {
				popup->visited = 1;
			} else {
				// This prevents closing cascaded popups.
				if (popup->visited && g_context.hoverPanel < panel->handle && !miIsHover(base))
					popup->visible = 0;
			}
		}
	}

	if (wentVisible) {
		if (side == MI_BELOW) {
			// below
			popup->rect = box->rect;
			popup->rect.y += box->rect.height;
			popup->rect.height = 0;
		} else {
			// right
			popup->rect = box->rect;
			popup->rect.x += box->rect.width;
			popup->rect.height = 0;
		}
	}

	panel->visible = popup->visible;
	panel->modal = popup->logic == MI_ONCLICK ? 1 : 0;

	mi__pushPanel(panel);

	mi__drawRect(panel, popup->rect.x, popup->rect.y, popup->rect.width, popup->rect.height, miRGBA(0,0,0,128));
	mi__initLayout(panel, MI_COL, popup->rect.x, popup->rect.y, popup->rect.width, 0, LAYOUT_SPACING);

	return panel->handle;
}
		
MIhandle miPopupEnd()
{
	MIpanel* panel;
	MIpopupState* popup;
	MIlayout* layout;

	panel = mi__popPanel();
	if (panel == NULL) return 0;
	popup = (MIpopupState*)mi__getState(panel->handle, 0, sizeof(MIpopupState));
	if (popup == NULL) return 0;
	layout = mi__getLayout(panel);
	if (layout == NULL) return 0;

	// Update popup size based on content.
	popup->rect.x = layout->space.x - PANEL_PADDING;
	popup->rect.y = layout->space.y - PANEL_PADDING;
	popup->rect.width = layout->space.width + PANEL_PADDING*2;
	popup->rect.height = layout->space.height + PANEL_PADDING*2;

	// Close condition
/*	if (popup->visible) {
		if (popup->logic == MI_ONCLICK) {
			// on click
			if (g_context.input.mbut & MI_MOUSE_PRESSED) {
				if (g_context.active == 0 && g_context.hoverPanel <= panel->handle && popup->visited)
					popup->visible = 0;
			}
			popup->visited = 1;
		} else {
			// on hover
			if (g_context.input.mbut & MI_MOUSE_PRESSED) {
				if (g_context.active == 0 && g_context.hoverPanel <= panel->handle)
					popup->visible = 0;
			}

			if (mi__pointInRect(g_context.input.mx, g_context.input.my, mi__inflateRect(popup->rect, POPUP_SAFE_ZONE))) {
				popup->visited = 1;
			} else {
				// This prevents closing cascaded popups.
				if (popup->visited && g_context.hoverPanel < panel->handle)
					popup->visible = 0;
			}
		}
	}*/

	return panel->handle;
}

/*
MIhandle miButtonRowBegin(int count)
{
	return miDivsBegin(MI_ROW, NULL, count, BUTTON_HEIGHT, 0);
}

MIhandle miButtonRowEnd()
{
	MIpanel* panel = mi__curPanel();
	MIbox* box = NULL;
	MIhandle row = miDivsEnd();
	box = mi__getBoxByHandle(row);

	mi__drawRect(panel, box->rect.x, box->rect.y, box->rect.width, box->rect.height, miRGBA(0,192,255,64));

	return row;
}
*/

MIhandle miCanvasBegin(MIcanvasState* state, float w, float h)
{
	return 0;
}

MIhandle miCanvasEnd()
{
	return 0;
}
