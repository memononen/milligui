#include "mgui.h"
#include "nanovg.h"
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#define NANOSVG_IMPLEMENTATION 1
#include "nanosvg.h"

static int mini(int a, int b) { return a < b ? a : b; }
static int maxi(int a, int b) { return a > b ? a : b; }
static float minf(float a, float b) { return a < b ? a : b; }
static float maxf(float a, float b) { return a > b ? a : b; }
static float clampf(float a, float mn, float mx) { return a < mn ? mn : (a > mx ? mx : a); }
static float absf(float a) { return a < 0.0f ? -a : a; }

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
#define SLIDER_HANDLE ((int)(TEXT_SIZE*0.85f) & ~1)
#define CHECKBOX_SIZE (TEXT_SIZE)
#define SCROLL_SIZE (TEXT_SIZE/4)
#define SCROLL_PAD (SCROLL_SIZE/2)


#define OPT_POOL_SIZE 1000
static struct MGopt optPool[OPT_POOL_SIZE];
static int optPoolSize = 0;
struct MGopt* allocOpt()
{
	struct MGopt* opt;
	if (optPoolSize >= OPT_POOL_SIZE)
		return NULL;
	opt = &optPool[optPoolSize++];
	memset(opt, 0, sizeof(*opt));
	return opt;
}


#define TEMP_ALLOC_SIZE 16000
static unsigned char tempPool[TEMP_ALLOC_SIZE];
static int tempPoolSize = 0;

void* mgTempMalloc(int size)
{
	if (tempPoolSize+size >= TEMP_ALLOC_SIZE)
		return NULL;
	tempPoolSize += size;
	return &tempPool[tempPoolSize - size];
}


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

#define FRAMETEMP_POOL_SIZE 8000

static unsigned char frameTempPool[FRAMETEMP_POOL_SIZE];
static int frameTempPoolSize = 0;

static unsigned char* allocFrameTemp(int size)
{
	if (frameTempPoolSize + size >= FRAMETEMP_POOL_SIZE)
		return NULL;
	unsigned char* dst = &frameTempPool[frameTempPoolSize]; 
	frameTempPoolSize += size;
	return dst;
}


#define MG_WIDGET_POOL_SIZE 1000
struct MGwidget widgetPool[MG_WIDGET_POOL_SIZE];
static int widgetPoolSize = 0;

#define MG_STYLE_POOL_SIZE 300
struct MGnamedStyle
{
	char* selector;
	char** path;
	int npath;
	struct MGstyle normal;	// 0=normal, 1=hover, 2=active, 3=focus
	struct MGstyle hover;
	struct MGstyle active;
	struct MGstyle focus;
};
struct MGnamedStyle stylePool[MG_STYLE_POOL_SIZE];
static int stylePoolSize = 0;

static int isStyleSet(struct MGstyle* style, unsigned int f)
{
	return style->set & (1 << f);
}


// TODO move resource handling to render abstraction.
#define MG_MAX_ICONS 100
struct MGicon
{
	char* name;
	struct NSVGimage* image;
};
static struct MGicon* icons[MG_MAX_ICONS];
static int iconCount = 0;


static struct MGicon* findIcon(const char* name)
{
	int i = 0;
	for (i = 0; i < iconCount; i++) {
		if (strcmp(icons[i]->name, name) == 0)
			return icons[i];
	}
	printf("Could not find icon '%s'\n", name);
	return NULL;
}

int mgCreateIcon(const char* name, const char* filename)
{
	struct MGicon* icon = NULL;

	if (iconCount >= MG_MAX_ICONS)
		return -1;

	icon = (struct MGicon*)malloc(sizeof(struct MGicon));
	if (icon == NULL) goto error;
	memset(icon, 0, sizeof(struct MGicon));

	icon->name = (char*)malloc(strlen(name)+1);
	if (icon->name == NULL) goto error;
	strcpy(icon->name, name);

	icon->image = nsvgParseFromFile(filename, "px", 96.0f);;
	if (icon->image == NULL) goto error;

	icons[iconCount++] = icon;

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

static void deleteIcons()
{
	int i;
	for (i = 0; i < iconCount; i++) {
		if (icons[i]->image != NULL)
			nsvgDelete(icons[i]->image);
		free(icons[i]->name);
		free(icons[i]);
	}
	iconCount = 0;
}


#define MG_BOX_STACK_SIZE 100
#define MG_ID_STACK_SIZE 100
#define MG_MAX_PANELS 100
#define MG_MAX_TAGS 100
#define MG_TRANSIENT_POOL_SIZE 4096

struct MGtransientHeader {
	unsigned int id;
	int size;
	short counter;
	short num;
};

struct MGidRange {
	unsigned int base, count;
};

struct MUIstate
{
	float mx, my;
	float startmx, startmy;
	float localmx, localmy;
	float deltamx, deltamy;
	int moved;
	int drag;
	int mbut;

	unsigned int active;
	unsigned int hover;
	unsigned int focus;

	unsigned int clicked;
	unsigned int pressed;
	unsigned int dragged;
	unsigned int released;

	struct MGwidget* boxStack[MG_BOX_STACK_SIZE];
	int boxStackCount;

	struct MGidRange idStack[MG_ID_STACK_SIZE];
	int idStackCount;

	struct MGwidget* panels[MG_MAX_PANELS];
	int panelsz[MG_MAX_PANELS];
	int panelCount;

	const char* tags[MG_MAX_TAGS];
	int tagCount;

	unsigned char transientMem[MG_TRANSIENT_POOL_SIZE];
	int transientMemSize;
	int transientCount;

	struct NVGcontext* vg;

	int width, height;

	struct MGhit result;
};

static struct MUIstate state;

static unsigned char* findTransient(unsigned int id, unsigned short num, int size)
{	
	int i;
	unsigned char* ptr = state.transientMem;
	for (i = 0; i < state.transientCount; i++) {
		struct MGtransientHeader* trans = (struct MGtransientHeader*)ptr;
		if (trans->id == id && trans->num == num) {
			if (trans->size == size) {
				trans->counter = 1; // touch
				return ptr + sizeof(struct MGtransientHeader);
			}
		}
		ptr += sizeof(struct MGtransientHeader) + trans->size; 
	}
	return NULL;
}

static void freeTransient(unsigned int id, short num)
{	
	int i;
	unsigned char* ptr = state.transientMem;
	for (i = 0; i < state.transientCount; i++) {
		struct MGtransientHeader* trans = (struct MGtransientHeader*)ptr;
		if (trans->id == id && trans->num == num) {
			// Mark for delete
			printf("mark free %d/%d\n", id, num);
			trans->counter = 0;
			trans->id = 0;
			return;
		}
		ptr += sizeof(struct MGtransientHeader) + trans->size; 
	}
}

static unsigned char* allocTransient(unsigned int id, unsigned short num, int size)
{
	int i;
	struct MGtransientHeader* trans;
	unsigned char* ptr;
	unsigned char* prev = NULL;
	int prevSize = 0;
	int allocSize = sizeof(struct MGtransientHeader) + size;

	// Check if the transient exists.
	ptr = state.transientMem;
	for (i = 0; i < state.transientCount; i++) {
		trans = (struct MGtransientHeader*)ptr;
		if (trans->id == id && trans->num == num) {
			if (trans->size != size) {
				allocSize = sizeof(struct MGtransientHeader) + maxi(trans->size, size);
				prev = ptr;
				break;
			}
			trans->counter = 1; // touch
			return ptr + sizeof(struct MGtransientHeader);
		}
		ptr += sizeof(struct MGtransientHeader) + trans->size; 
	}

	if ((state.transientMemSize + allocSize) > MG_TRANSIENT_POOL_SIZE) {
		printf("transient pool exhausted!\n");
		return NULL;
	}

	// Allocate new block
	ptr = &state.transientMem[state.transientMemSize];
	state.transientMemSize += allocSize; 
	if (prev != NULL)
		memcpy(ptr, prev, allocSize);
	else
		memset(ptr, 0, allocSize);
	trans = (struct MGtransientHeader*)ptr;
	trans->id = id;
	trans->num = num;
	trans->counter = 1;
	trans->size = size;

	state.transientCount++;

	return ptr + sizeof(struct MGtransientHeader);
}

static void cleanUpTransients()
{
	int i, n = 0, size = 0;
	struct MGtransientHeader* trans;
	unsigned char* ptr;
	unsigned char* tail;

	// Check if the transient exists.
	ptr = tail = state.transientMem;

	for (i = 0; i < state.transientCount; i++) {
		trans = (struct MGtransientHeader*)ptr;
		size = sizeof(struct MGtransientHeader) + trans->size;
		trans->counter--;
		if (trans->counter >= 0) {
			memmove(tail, ptr, size);
			tail += size;
			n++;
		}
		ptr += size;
	}

	state.transientMemSize = (int)(tail - state.transientMem);
	state.transientCount = n;
}



static void addPanel(struct MGwidget* w, int zidx)
{
	if (state.panelCount < MG_MAX_PANELS) {
		int i, z = zidx + state.panelCount, idx = 0;
		while (idx < state.panelCount && state.panelsz[idx] < z)
			idx++;
		for (i = state.panelCount; i >= idx; i--) {
			state.panels[i] = state.panels[i-1];
			state.panelsz[i] = state.panelsz[i-1];
		}
		state.panels[idx] = w;
		state.panelsz[idx] = z;
		state.panelCount++;
	}
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

static void pushId(int base)
{
	if (state.idStackCount+1 < MG_ID_STACK_SIZE) {
		state.idStack[state.idStackCount].base = base;
		state.idStack[state.idStackCount].count = 0;
		state.idStackCount++;
	}
}

static void popId()
{
	if (state.idStackCount > 0) {
		state.idStackCount--;
	}
}

static void pushTag(const char* tag)
{
	if (state.tagCount < MG_MAX_TAGS)
		state.tags[state.tagCount++] = tag;
}

static void popTag()
{
	if (state.tagCount > 0)
		state.tagCount--;
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
		int idx = state.idStackCount-1;
		id |= state.idStack[idx].base << 16;
		id |= state.idStack[idx].count;
		state.idStack[idx].count++;
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
	w->parent = parent;
}

static struct MGwidget* allocWidget(int type)
{
	struct MGwidget* w = NULL;
	if (widgetPoolSize+1 > MG_WIDGET_POOL_SIZE)
		return NULL;
	w = &widgetPool[widgetPoolSize++];
	memset(w, 0, sizeof(*w));
	w->id = genId();
	w->type = type;
	w->active = 1;
	w->bubble = 1;
	return w;
}

static struct MGwidget* findWidget(unsigned int id)
{
	int i;
	for (i = 0; i < widgetPoolSize; i++)
		if (widgetPool[i].id == id)
			return &widgetPool[i];
	return NULL;
}


static int inRect(float x, float y, float w, float h)
{
   return state.mx >= x && state.mx <= x+w && state.my >= y && state.my <= y+h;
}

int mgInit()
{
	memset(&state, 0, sizeof(state));

	textPoolSize = 0;
	widgetPoolSize = 0;
	stylePoolSize = 0;
	optPoolSize = 0;
	frameTempPoolSize = 0;

	// Default style
	mgCreateStyle("text", mgOpts(
		mgFontSize(TEXT_SIZE),
		mgAlign(MG_START),
		mgSpacing(SPACING),
		mgContentColor(255,255,255,255)
	), mgOpts(), mgOpts(), mgOpts());

	mgCreateStyle("icon", mgOpts(
		mgSpacing(SPACING)
	), mgOpts(), mgOpts(), mgOpts());



	mgCreateStyle("slider-canvas",
		// Normal
		mgOpts(
			mgSpacing(SPACING),
			mgFillColor(220,220,220,255),
			mgBorderSize(2.0f),
			mgContentColor(220,220,220,255)
		),
		// Hover
		mgOpts(
			mgFillColor(255,255,255,255)
		),
		// Active
		mgOpts(
			mgFillColor(32,32,32,255),
			mgBorderColor(255,255,255,255)
		),
		// Focus
		mgOpts(
			mgBorderColor(0,192,255,255)
		)
	);


	mgCreateStyle("slider",
		// Normal
		mgOpts(
			mgSpacing(SPACING)
//			mgFillColor(255,0,0,64)
//			mgCornerRadius(3)
		),
		// Hover
		mgOpts(
//			mgFillColor(255,255,255,64)
		),
		// Active
		mgOpts(
//			mgFillColor(255,255,255,64)
		),
		// Focus
		mgOpts(
//			mgFillColor(0,192,255,64)
		)
	);

	mgCreateStyle("slider.slot", mgOpts(
		mgHeight(2), //SLIDER_HANDLE),
		mgFillColor(0,0,0,128)
//		mgPaddingX(SLIDER_HANDLE/2),
//		mgCornerRadius(3)
	), mgOpts(), mgOpts(), mgOpts());

	mgCreateStyle("slider.bar",
		// Normal
		mgOpts(
			mgPaddingX(SLIDER_HANDLE/2),
			mgHeight(2), //SLIDER_HANDLE),
			mgOverflow(MG_VISIBLE),
			mgFillColor(220,220,220,255)
		),
		// Hover
		mgOpts(
			mgFillColor(255,255,255,255)
		),
		// Active
		mgOpts(
			mgFillColor(255,255,255,255)
		),
		// Focus
		mgOpts(
		)
	);

	mgCreateStyle("slider.handle",
		// Normal
		mgOpts(
			mgWidth(SLIDER_HANDLE),
			mgHeight(SLIDER_HANDLE),
			mgFillColor(255,255,255,255),
			mgBorderColor(255,255,255,0),
			mgBorderSize(2),
			mgCornerRadius(SLIDER_HANDLE/2)
		),
		// Hover
		mgOpts(
//			mgFillColor(220,220,220,255)
		),
		// Active
		mgOpts(
			mgBorderColor(255,255,255,255),
			mgFillColor(32,32,32,255)
		),
		// Focus
		mgOpts(
			mgBorderColor(0,192,255,128)
		)
	);


	mgCreateStyle("progress",
		// Normal
		mgOpts(
			mgWidth(DEFAULT_SLIDERW),
			mgHeight(SLIDER_HANDLE+2),
			mgSpacing(SPACING),
			mgFillColor(0,0,0,128),
			mgPadding(2,2),
			mgCornerRadius(4)
		),
		mgOpts(), mgOpts(), mgOpts()
	);
	mgCreateStyle("progress.bar", mgOpts(
		mgPropHeight(1.0f),
		mgFillColor(0,192,255,255),
		mgPaddingX(2),
		mgCornerRadius(2)
	), mgOpts(), mgOpts(), mgOpts());

	mgCreateStyle("scroll",
		// Normal
		mgOpts(
			mgWidth(DEFAULT_SLIDERW),
			mgHeight(10),
			mgSpacing(SPACING),
			mgFillColor(255,255,255,24),
			mgCornerRadius(4)
		),
		mgOpts(), mgOpts(), mgOpts()
	);
	mgCreateStyle("scroll.bar",
		mgOpts(
			mgPropHeight(1.0f),
			mgFillColor(0,0,0,64),
			mgPaddingX(4),
			mgCornerRadius(4)
		),
		mgOpts(
			mgFillColor(0,0,0,128)
		),
		mgOpts(
			mgFillColor(0,0,0,192)
		),
		mgOpts()
	);

	mgCreateStyle("button",
		// Normal
		mgOpts(
			mgPack(MG_CENTER),
			mgAlign(MG_CENTER),
			mgSpacing(SPACING),
			mgPadding(BUTTON_PADX, BUTTON_PADY),
			mgLogic(MG_CLICK),
			mgFillColor(255,255,255,16),
			mgBorderColor(255,255,255,128),
			mgBorderSize(1),
			mgCornerRadius(4)
		),
		// Hover
		mgOpts(
			mgFillColor(255,255,255,64),
			mgBorderColor(255,255,255,192)
		),
		// Active
		mgOpts(
			mgFillColor(255,255,255,192),
			mgBorderColor(255,255,255,255)
		),
		// Focus
		mgOpts(
			mgBorderColor(0,192,255,128)
		)
	);
	mgCreateStyle("button.text",
		// Normal
		mgOpts(
			mgTextAlign(MG_CENTER),
			mgFontSize(TEXT_SIZE),
			mgContentColor(255,255,255,255)
		),
		// Hover
		mgOpts(
			mgContentColor(255,255,255,255)
		),
		// Active
		mgOpts(
			mgContentColor(0,0,0,255)
		),
		// Focus
		mgOpts()
	);

	mgCreateStyle("button.icon",
		// Normal
		mgOpts(
			mgHeight(TEXT_SIZE),
			mgContentColor(255,255,255,192),
			mgSpacing(SPACING)
		),
		// Hover
		mgOpts(
			mgContentColor(255,255,255,192)
		),
		// Active
		mgOpts(
			mgContentColor(0,0,0,192)
		),
		// Focus
		mgOpts()
	);


	mgCreateStyle("select",
		// Normal
		mgOpts(
			mgAlign(MG_CENTER),
			mgSpacing(SPACING),
			mgPadding(BUTTON_PADX, BUTTON_PADY),
			mgLogic(MG_CLICK),
			mgFillColor(255,255,255,16),
			mgBorderColor(255,255,255,128),
			mgBorderSize(1),
			mgCornerRadius(4)
		),
		// Hover
		mgOpts(
			mgFillColor(255,255,255,64),
			mgBorderColor(255,255,255,192)
		),
		// Active
		mgOpts(
			mgFillColor(255,255,255,192),
			mgBorderColor(255,255,255,255)
		),
		// Focus
		mgOpts(
			mgBorderColor(0,192,255,128)
		)
	);
	mgCreateStyle("select.text",
		// Normal
		mgOpts(
			mgTextAlign(MG_START),
			mgFontSize(TEXT_SIZE),
			mgContentColor(255,255,255,255)
		),
		// Hover
		mgOpts(
			mgContentColor(255,255,255,255)
		),
		// Active
		mgOpts(
			mgContentColor(0,0,0,255)
		),
		// Focus
		mgOpts()
	);
	mgCreateStyle("select.arrow",
		// Normal
		mgOpts(
			mgHeight(TEXT_SIZE*0.8f),
			mgContentColor(255,255,255,255)
		),
		// Hover
		mgOpts(
			mgContentColor(255,255,255,255)
		),
		// Active
		mgOpts(
			mgContentColor(0,0,0,255)
		),
		// Focus
		mgOpts()
	);

	mgCreateStyle("checkbox",
		// Normal
		mgOpts(
			mgAlign(MG_CENTER),
			mgSpacing(SPACING),
			mgPaddingY(BUTTON_PADY),
			mgLogic(MG_CLICK)
		),
		mgOpts(), mgOpts(), mgOpts()
	);

	mgCreateStyle("checkbox.box",
		// Normal
		mgOpts(
			mgAlign(MG_CENTER),
			mgFillColor(255,255,255,16),
			mgBorderColor(255,255,255,128),
			mgBorderSize(1),
			mgCornerRadius(4),
			mgWidth(CHECKBOX_SIZE),
			mgHeight(CHECKBOX_SIZE)
		),
		// Hover
		mgOpts(
			mgFillColor(255,255,255,64),
			mgBorderColor(255,255,255,192)
		),
		// Active
		mgOpts(
			mgFillColor(255,255,255,192),
			mgBorderColor(255,255,255,255)
		),
		// Focus
		mgOpts(
			mgBorderColor(0,192,255,128)
		)
	);

	mgCreateStyle("checkbox.box.tick",
		// Normal
		mgOpts(
			mgContentColor(255,255,255,255)
		),
		// Hover
		mgOpts(
			mgContentColor(255,255,255,255)
		),
		// Active
		mgOpts(
			mgContentColor(0,0,0,255)
		),
		// Focus
		mgOpts()
	);

	mgCreateStyle("item",
		// Normal
		mgOpts(
			mgAlign(MG_CENTER),
			mgPadding(BUTTON_PADX, BUTTON_PADY),
			mgLogic(MG_CLICK)
		),
		// Hover
		mgOpts(
			mgFillColor(255,255,255,64)
		),
		// Active
		mgOpts(
			mgFillColor(255,255,255,192)
		),
		// Focus
		mgOpts(
			mgFillColor(0,192,255,16)
		)
	);
	mgCreateStyle("item.text",
		// Normal
		mgOpts(
			mgTextAlign(MG_START),
			mgFontSize(TEXT_SIZE),
			mgContentColor(255,255,255,192)
		),
		// Hover
		mgOpts(
			mgContentColor(255,255,255,255)
		),
		// Active
		mgOpts(
			mgContentColor(0,0,0,255)
		),
		// Focus
		mgOpts()
	);


	mgCreateStyle("input",
		// Normal
		mgOpts(
			mgFontSize(TEXT_SIZE),
			mgAlign(MG_START),
			mgPadding(BUTTON_PADX/2, BUTTON_PADY),
			mgLogic(MG_TYPE),
			mgSpacing(SPACING),
			mgContentColor(255,255,255,255),
			mgBorderColor(255,255,255,128),
			mgBorderSize(1)
		),
		// Hover
		mgOpts(
			mgFillColor(255,255,255,32)
		),
		// Active
		mgOpts(
			mgFillColor(255,255,255,32),
			mgBorderColor(255,255,255,192)
		),
		// Focus
		mgOpts(
			mgBorderColor(0,192,255,192)
		)
	);

	mgCreateStyle("number",
		// Normal
		mgOpts(
			mgTextAlign(MG_END),
			mgFontSize(TEXT_SIZE),
			mgAlign(MG_START),
			mgPadding(BUTTON_PADX/2, BUTTON_PADY),
			mgLogic(MG_TYPE),
			mgSpacing(SPACING),
			mgContentColor(255,255,255,255),
			mgBorderColor(255,255,255,128),
			mgBorderSize(1)
		),
		// Hover
		mgOpts(
			mgFillColor(255,255,255,32)
		),
		// Active
		mgOpts(
			mgFillColor(255,255,255,32),
			mgBorderColor(255,255,255,192)
		),
		// Focus
		mgOpts(
			mgBorderColor(0,192,255,192)
		)
	);


	mgCreateStyle("label",
		// Normal
		mgOpts(
			mgFontSize(LABEL_SIZE),
			mgAlign(MG_START),
			mgSpacing(LABEL_SPACING),
			mgContentColor(255,255,255,192)
		),
		// Hover, active, focus
		mgOpts(), mgOpts(), mgOpts()
	);

	mgCreateStyle("color",
		// Normal
		mgOpts(
			mgAlign(MG_CENTER),
			mgSpacing(SPACING)
		),
		// Hover, active, focus
		mgOpts(), mgOpts(), mgOpts()
	);

	mgCreateStyle("number3",
		// Normal
		mgOpts(
			mgAlign(MG_CENTER),
			mgSpacing(SPACING)
		),
		// Hover, active, focus
		mgOpts(), mgOpts(), mgOpts()
	);

	mgCreateStyle("popup",
		// Normal
		mgOpts(
			mgLogic(MG_CLICK),
			mgFillColor(32,32,32,192)
		),
		// Hover, active, focus
		mgOpts(), mgOpts(), mgOpts()
	);

	return 1;
}

void mgTerminate()
{
	int i, j;
	// Free styles
	for (i = 0; i < stylePoolSize; i++) {
		free(stylePool[i].selector);
		for (j = 0; j < stylePool[i].npath; j++)
			free(stylePool[i].path[j]);
		free(stylePool[i].path);
	}
	// Free resources
	deleteIcons();
}

void mgFrameBegin(struct NVGcontext* vg, int width, int height, float mx, float my, int mbut)
{
	state.moved = absf(state.mx - mx) > 0.01f || absf(state.my - my) > 0.01f;
	state.mx = mx;
	state.my = my;
	state.mbut = mbut;

	state.width = width;
	state.height = height;

	state.vg = vg;

	state.boxStackCount = 0;
	state.panelCount = 0;
	state.tagCount = 0;

	state.idStackCount = 1;
	state.idStack[0].base = 0;
	state.idStack[0].count = 0;

	tempPoolSize = 0;
	textPoolSize = 0;
	widgetPoolSize = 0;
	optPoolSize = 0;
	frameTempPoolSize = 0;
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
	struct MGwidget* w = NULL;
	float bbounds[4];
	float wbounds[4];
	struct MGwidget* hit = NULL;
	struct MGwidget* child = NULL;

	// TODO: something not quite right with MG_VISIBLE, we should clip some bit of the bounds, but
	// not immediate parent.

	// calc box bounds
	if (box->style.overflow == MG_VISIBLE) {
//		bbounds[0] = bounds[0]; bbounds[1] = bounds[1]; bbounds[2] = bounds[2]; bbounds[3] = bounds[3];
		bbounds[0] = box->x; bbounds[1] = box->y; bbounds[2] = box->width; bbounds[3] = box->height;
	} else {
		isectBounds(bbounds, bounds, box->x, box->y, box->width, box->height);
	}
//	isectBounds(bbounds, bounds, box->x, box->y, box->width, box->height);

	// Skip if invisible
	if (bbounds[2] < 0.1f || bbounds[3] < 0.1f)
		return NULL;

	if (box->style.logic != 0 && inRect(bbounds[0], bbounds[1], bbounds[2], bbounds[3]))
		hit = box;

	for (w = box->box.children; w != NULL; w = w->next) {

		if (!visible(bbounds, w->x, w->y, w->width, w->height))
			continue;

		// calc widget bounds
		if (w->style.overflow == MG_VISIBLE) {
			wbounds[0] = bbounds[0]; wbounds[1] = bbounds[1]; wbounds[2] = bbounds[2]; wbounds[3] = bbounds[3];
		} else {
			isectBounds(wbounds, bbounds, w->x, w->y, w->width, w->height);
		}
//		isectBounds(wbounds, bbounds, w->x, w->y, w->width, w->height);

		if (w->style.logic != 0 && inRect(wbounds[0], wbounds[1], wbounds[2], wbounds[3]))
			hit = w;

		switch (w->type) {
		case MG_BOX:
			child = hitTest(w, bbounds);
			if (child != NULL)
				hit = child;
			break;
		case MG_TEXT:
			break;
		case MG_PARAGRAPH:
			break;
		case MG_ICON:
			break;
		case MG_INPUT:
			break;
		}
	}

	// TODO: hit scrollbars?

	return hit;
} 

static struct MGwidget* updateState(struct MGwidget* box, unsigned int hover, unsigned int active, unsigned int focus, unsigned char state)
{
	struct MGwidget* ret = NULL;
	struct MGwidget* w = NULL;

	if (box->id == hover)
		box->state |= MG_HOVER;
	if (box->id == focus)
		box->state |= MG_FOCUS;
	if (box->id == active) {
		box->state |= MG_ACTIVE;
		ret = box;
	}

	for (w = box->box.children; w != NULL; w = w->next) {
		w->state = box->state;
		if (w->id == hover)
			w->state |= MG_HOVER;
		if (w->id == focus)
			w->state |= MG_FOCUS;
		if (w->id == active) {
			w->state |= MG_ACTIVE;
			ret = w;
		}

		if  (w->type == MG_BOX) {
			struct MGwidget* child = updateState(w, hover, active, focus, w->state);
			if (child != NULL)
				ret = child;
		}
	}

	return ret;
} 

/*
static void dumpId(struct MGwidget* box, int indent)
{
	struct MGwidget* w = NULL;
	printf("%*sbox %d\n", indent, "", box->id);
	for (w = box->box.children; w != NULL; w = w->next) {
		if  (w->type == MG_BOX)
			dumpId(w, indent+2);
		else
			printf("%*swid %d\n", indent+2, "", w->id);
	}
} 
*/

static void fireLogic(unsigned int id, int event, struct MGhit* hit)
{
	struct MGwidget* w = NULL;
	if (id == 0) return;
	w = findWidget(id);
	if (w == NULL) return;
	if (w->logic == NULL) return;
	state.localmx = state.mx - w->x;
	state.localmy = state.my - w->y;
	w->logic(w->uptr, w, event, hit);
}

static void updateLogic(const float* bounds)
{
	int i;
	struct MGwidget* hit = NULL;
//	struct MGwidget* active = NULL;
//	struct MGwidget* w = NULL;
	int deactivate = 0;

	unsigned int focused = 0;
	unsigned int blurred = 0;
	unsigned int entered = 0;
	unsigned int exited = 0;

/*	printf("---\n");
	for (i = 0; i < state.panelCount; i++)
		dumpId(state.panels[i], 0);*/

	for (i = 0; i < state.panelCount; i++) {
		if (state.panels[i]->active) {
			struct MGwidget* child = hitTest(state.panels[i], bounds);
			if (child != NULL)
				hit = child;
		}
	}

//	state.hover = 0;
	state.clicked = 0;
	state.pressed = 0;
	state.dragged = 0;
	state.released = 0;

	if (state.active == 0) {
		unsigned int id = hit != NULL ? hit->id : 0;
		if (state.hover != id) {
			exited = state.hover;
			entered = id;
			state.hover = id;
		}
		if (state.mbut & MG_MOUSE_PRESSED) {
			if (state.focus != id) {
				blurred = state.focus;
				focused = id;
			}
			state.focus = id;
			state.active = id;
			state.pressed = id;
		}
	}
	// Press and release can happen in same frame.
	if (state.active != 0) {
		unsigned int id = hit != NULL ? hit->id : 0;
		if (id == 0 || id == state.active) {
			if (state.hover != id) {
				exited = state.hover;
				entered = id;
				state.hover = id;
			}
//			state.hover = hit->id;
		}
		if (state.mbut & MG_MOUSE_RELEASED) {
			if (state.hover == state.active)
				state.clicked = state.hover;
			state.released = state.active;
			deactivate = 1;
		} else {
			if (state.moved)
				state.dragged = state.active;
		}
	}

/*	for (i = 0; i < state.panelCount; i++) {
		struct MGwidget* child = updateState(state.panels[i], state.hover, state.active, state.focus, 0);
		if (child != NULL)
			active = child;
	}*/

	// Post pone deactivation so that we get atleast one frame of active state if mouse press/release during one frame.
	if (deactivate)
		state.active = 0;

	// Update mouse positions.
	if (state.mbut & MG_MOUSE_PRESSED) {
		state.startmx = state.mx;
		state.startmy = state.my;
		state.drag = 1;
	}
	if (state.mbut & MG_MOUSE_RELEASED) {
		state.startmx = state.startmy = 0;
		state.drag = 0;
	}
	if (state.drag) {
		state.deltamx = state.mx - state.startmx;
		state.deltamy = state.my - state.startmy;
	} else {
		state.deltamx = state.deltamy = 0;
	}

	state.result.mx = state.mx;
	state.result.my = state.my;
	state.result.deltamx = state.deltamx;
	state.result.deltamy = state.deltamy;
	state.result.localmx = state.localmx;
	state.result.localmy = state.localmy;

	fireLogic(blurred, MG_BLURRED, &state.result);
	fireLogic(focused, MG_FOCUSED, &state.result);
	fireLogic(state.pressed, MG_PRESSED, &state.result);
	fireLogic(state.dragged, MG_DRAGGED, &state.result);
	fireLogic(state.released, MG_RELEASED, &state.result);
	fireLogic(state.clicked, MG_CLICKED, &state.result);
	fireLogic(exited, MG_EXITED, &state.result);
	fireLogic(entered, MG_ENTERED, &state.result);


/*	state.result.clicked = state.clicked != 0;
	state.result.pressed = state.pressed != 0;
	state.result.dragged = state.dragged != 0;
	state.result.released = state.released != 0;

	if (active != NULL) {
		state.result.bounds[0] = active->x;
		state.result.bounds[1] = active->y;
		state.result.bounds[2] = active->width;
		state.result.bounds[3] = active->height;
		if (active->parent != NULL) {
			state.result.pbounds[0] = active->parent->x;
			state.result.pbounds[1] = active->parent->y;
			state.result.pbounds[2] = active->parent->width;
			state.result.pbounds[3] = active->parent->height;
		} else {
			state.result.pbounds[0] = state.result.pbounds[1] = state.result.pbounds[2] = state.result.pbounds[3] = 0;
		}
	} else {
		state.result.bounds[0] = state.result.bounds[1] = state.result.bounds[2] = state.result.bounds[3] = 0;
		state.result.pbounds[0] = state.result.pbounds[1] = state.result.pbounds[2] = state.result.pbounds[3] = 0;
	}

	if (active != NULL && active->logic != NULL)
		active->logic(active->uptr, active, &state.result);*/
}

static struct NVGcolor nvgCol(unsigned int col)
{
	struct NVGcolor c;
	c.r = (col & 0xff) / 255.0f;
	c.g = ((col >> 8) & 0xff) / 255.0f;
	c.b = ((col >> 16) & 0xff) / 255.0f;
	c.a = ((col >> 24) & 0xff) / 255.0f;
	return c;
}

static void drawDebugRect(struct MGwidget* w)
{
	// round
/*	nvgBeginPath(state.vg);
	nvgRect(state.vg, w->x + w->style.paddingx+0.5f, w->y + w->style.paddingy+0.5f, w->width - w->style.paddingx*2, w->height - w->style.paddingy*2);
	nvgStrokeWidth(state.vg, 1.0f);
	nvgStrokeColor(state.vg, nvgRGBA(255,0,0,128));
	nvgStroke(state.vg);*/

	nvgBeginPath(state.vg);
	nvgRect(state.vg, w->x + w->style.paddingx+0.5f, w->y + w->style.paddingy+0.5f, w->cwidth, w->cheight);
	nvgStrokeWidth(state.vg, 1.0f);
	nvgStrokeColor(state.vg, nvgRGBA(255,255,0,128));
	nvgStroke(state.vg);
}


static void drawIcon(struct MGwidget* w)
{
	int i;
	struct NSVGimage* image = NULL;
	struct NSVGshape* shape = NULL;
	int override = isStyleSet(&w->style, MG_CONTENTCOLOR_ARG);
	float sx, sy, s;

	if (w->icon.icon == NULL) return;
	image = w->icon.icon->image;
	if (image == NULL) return;

	if (override) {
		nvgFillColor(state.vg, nvgCol(w->style.contentColor));
		nvgStrokeColor(state.vg, nvgCol(w->style.contentColor));
	}
	sx = w->width / image->width;
	sy = w->height / image->height;
	s = minf(sx, sy);

	nvgSave(state.vg);
	nvgTranslate(state.vg, w->x + w->width/2, w->y + w->height/2);
	nvgScale(state.vg, s, s);
	nvgTranslate(state.vg, -image->width/2, -image->height/2);

	for (shape = image->shapes; shape != NULL; shape = shape->next) {
		struct NSVGpath* path;

		if (shape->fill.type == NSVG_PAINT_NONE && shape->stroke.type == NSVG_PAINT_NONE)
			continue;

		nvgBeginPath(state.vg);
		for (path = shape->paths; path != NULL; path = path->next) {
			nvgMoveTo(state.vg, path->pts[0], path->pts[1]);
			for (i = 1; i < path->npts; i += 3) {
				float* p = &path->pts[i*2];
				nvgBezierTo(state.vg, p[0],p[1], p[2],p[3], p[4],p[5]);
			}
			if (path->closed)
				nvgLineTo(state.vg, path->pts[0], path->pts[1]);
		}

		if (shape->fill.type == NSVG_PAINT_COLOR) {
			if (!override)
				nvgFillColor(state.vg, nvgCol(shape->fill.color));
			nvgFill(state.vg);
//			printf("image %s\n", w->icon.icon->name);
//			nvgDebugDumpPathCache(state.vg);
		}
		if (shape->stroke.type == NSVG_PAINT_COLOR) {
			if (!override)
				nvgStrokeColor(state.vg, nvgCol(shape->stroke.color));
			nvgStrokeWidth(state.vg, shape->strokeWidth);
			nvgStroke(state.vg);
		}
	}

	nvgRestore(state.vg);
}

static void drawRect(struct MGwidget* w)
{
	// round
	int x = w->x;
	int y = w->y;
	int width = w->width;
	int height = w->height;

	if (isStyleSet(&w->style, MG_FILLCOLOR_ARG)) {
		nvgBeginPath(state.vg);
		if (isStyleSet(&w->style, MG_CORNERRADIUS_ARG)) {
			nvgRoundedRect(state.vg, x, y, width, height, w->style.cornerRadius);
		} else {
			nvgRect(state.vg, x, y, width, height);
		}
		nvgFillColor(state.vg, nvgCol(w->style.fillColor));
		nvgFill(state.vg);
	}

	if (isStyleSet(&w->style, MG_BORDERCOLOR_ARG)) {
		float s = w->style.borderSize * 0.5f;
		nvgBeginPath(state.vg);
		if (isStyleSet(&w->style, MG_CORNERRADIUS_ARG)) {
			nvgRoundedRect(state.vg, x+s, y+s, width-s*2, height-s*2, w->style.cornerRadius - s);
		} else {
			nvgRect(state.vg, x+s, y+s, width-s*2, height-s*2);
		}
		nvgStrokeWidth(state.vg, w->style.borderSize);
		nvgStrokeColor(state.vg, nvgCol(w->style.borderColor));
		nvgStroke(state.vg);
	}
}

static int measureTextGlyphs(struct MGwidget* w, const char* text, struct NVGglyphPosition* pos, int maxpos)
{
	nvgFontSize(state.vg, w->style.fontSize);
	if (w->style.textAlign == MG_CENTER) {
		nvgTextAlign(state.vg, NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
		return nvgTextGlyphPositions(state.vg, w->x + w->width/2, w->y + w->height/2, text, NULL, pos, maxpos);
	} else if (w->style.textAlign == MG_END) {
		nvgTextAlign(state.vg, NVG_ALIGN_RIGHT|NVG_ALIGN_MIDDLE);
		return nvgTextGlyphPositions(state.vg, w->x + w->width - w->style.paddingx, w->y + w->height/2, text, NULL, pos, maxpos);
	} else {
		nvgTextAlign(state.vg, NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);
		return nvgTextGlyphPositions(state.vg, w->x + w->style.paddingx, w->y + w->height/2, text, NULL, pos, maxpos);
	}
	return 0;
}

static void drawText(struct MGwidget* w, const char* text)
{
//	float bounds[4];
//	struct NVGglyphPosition pos[100];
//	int npos = 0, i;

	nvgFillColor(state.vg, nvgCol(w->style.contentColor));
	nvgFontSize(state.vg, w->style.fontSize);
	if (w->style.textAlign == MG_CENTER) {
		nvgTextAlign(state.vg, NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
//		npos = nvgTextGlyphPositions(state.vg, w->x + w->width/2, w->y + w->height/2, w->text, NULL, bounds, pos, 100);
		nvgText(state.vg, w->x + w->width/2, w->y + w->height/2, text, NULL);
	} else if (w->style.textAlign == MG_END) {
		nvgTextAlign(state.vg, NVG_ALIGN_RIGHT|NVG_ALIGN_MIDDLE);
//		npos = nvgTextGlyphPositions(state.vg, w->x + w->width - w->style.paddingx, w->y + w->height/2, w->text, NULL, bounds, pos, 100);
		nvgText(state.vg, w->x + w->width - w->style.paddingx, w->y + w->height/2, text, NULL);
	} else {
		nvgTextAlign(state.vg, NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);
//		npos = nvgTextGlyphPositions(state.vg, w->x + w->style.paddingx, w->y + w->height/2, w->text, NULL, bounds, pos, 100);
		nvgText(state.vg, w->x + w->style.paddingx, w->y + w->height/2, text, NULL);
	}

/*	nvgFillColor(state.vg, nvgRGBA(255,0,0,64));
	for (i = 0; i < npos; i++) {
		struct NVGglyphPosition* p = &pos[i];
		nvgBeginPath(state.vg);
		nvgRect(state.vg, p->x, bounds[1], p->width, bounds[3]-bounds[1]);
		nvgFill(state.vg);
	}*/
}

static void drawParagraph(struct MGwidget* w)
{
	float x = w->x + w->style.paddingx;
	float y = w->y + w->style.paddingy;
	float width = maxf(0.0f, w->width - w->style.paddingx*2);
	if (width < 1.0f) return;
	nvgFillColor(state.vg, nvgCol(w->style.contentColor));
	nvgFontSize(state.vg, w->style.fontSize);
	nvgTextLineHeight(state.vg, w->style.lineHeight > 0 ? w->style.lineHeight : 1);
	if (w->style.textAlign == MG_CENTER)
		nvgTextAlign(state.vg, NVG_ALIGN_CENTER|NVG_ALIGN_TOP);
	else if (w->style.textAlign == MG_END)
		nvgTextAlign(state.vg, NVG_ALIGN_RIGHT|NVG_ALIGN_TOP);
	else
		nvgTextAlign(state.vg, NVG_ALIGN_LEFT|NVG_ALIGN_TOP);
	nvgTextBox(state.vg, x, y, width, w->text, NULL);
}

static void drawBox(struct MGwidget* box, const float* bounds)
{
	int i;
	struct MGwidget* w;
	float bbounds[4];
	float wbounds[4];
	int debug = 0;

	nvgFontFace(state.vg, "sans");
	nvgFontSize(state.vg, TEXT_SIZE);

	nvgScissor(state.vg, (int)bounds[0], (int)bounds[1], (int)bounds[2], (int)bounds[3]);

	drawRect(box);

	if (debug) drawDebugRect(box);

	// calc panel bounds
	if (box->style.overflow == MG_VISIBLE) {
		bbounds[0] = bounds[0]; bbounds[1] = bounds[1]; bbounds[2] = bounds[2]; bbounds[3] = bounds[3];
	} else {
		isectBounds(bbounds, bounds, box->x, box->y, box->width, box->height);
	}

	// Skip if invisible
	if (bbounds[2] < 0.1f || bbounds[3] < 0.1f)
		return;

	for (i = 0; i < 2; i++) {
		for (w = box->box.children; w != NULL; w = w->next) {
			unsigned char anchor = isStyleSet(&w->style, MG_ANCHOR_ARG);
			// draw anchored last
			if ((i == 0 && anchor) || (i == 1 && !anchor))
				continue;

			if (!visible(bbounds, w->x, w->y, w->width, w->height))
				continue;

			nvgScissor(state.vg, (int)bbounds[0], (int)bbounds[1], (int)bbounds[2], (int)bbounds[3]);

			switch (w->type) {
			case MG_BOX:
				drawBox(w, bbounds);
				break;

			case MG_TEXT:
				drawRect(w);
				if (debug) drawDebugRect(w);
				isectBounds(wbounds, bbounds, w->x, w->y, w->width, w->height);
				if (wbounds[2] > 0.0f && wbounds[3] > 0.0f) {
					nvgScissor(state.vg, (int)wbounds[0], (int)wbounds[1], (int)wbounds[2], (int)wbounds[3]);
					drawText(w, w->text);
				}
				break;

			case MG_PARAGRAPH:
				drawRect(w);
				if (debug) drawDebugRect(w);
				isectBounds(wbounds, bbounds, w->x, w->y, w->width, w->height);
				if (wbounds[2] > 0.0f && wbounds[3] > 0.0f) {
					nvgScissor(state.vg, (int)wbounds[0], (int)wbounds[1], (int)wbounds[2], (int)wbounds[3]);
					drawParagraph(w);
				}
				break;

			case MG_ICON:
//				drawRect(w);
				drawIcon(w);
				if (debug) drawDebugRect(w);
				break;

			case MG_INPUT:
/*				drawRect(w);
				if (debug) drawDebugRect(w);
				isectBounds(wbounds, bbounds, w->x, w->y, w->width, w->height);
				if (wbounds[2] > 0.0f && wbounds[3] > 0.0f) {
					nvgScissor(state.vg, (int)wbounds[0], (int)wbounds[1], (int)wbounds[2], (int)wbounds[3]);
					drawText(w);
					if (w->uptr != NULL) {
						struct MGinputState* input = allocTransientInput(w, 0);
						int j;
						if (input != NULL && input->maxText > 0) {
							input->npos = measureTextGlyphs(w, input->pos, input->maxText);
//							printf("input  max=%d i='%s' w='%s' pos=%p npos=%d\n", input->maxText, input->buf, w->text, input->pos, input->npos);
							nvgFillColor(state.vg, nvgRGBA(255,0,0,64));
							for (j = 0; j < input->npos; j++) {
								struct NVGglyphPosition* p = &input->pos[j];
								nvgBeginPath(state.vg);
								nvgRect(state.vg, p->x, w->y, p->width, w->height);
								nvgFill(state.vg);
							}
						}
					}
				}*/

				isectBounds(wbounds, bbounds, w->x, w->y, w->width, w->height);
				if (w->render != NULL && wbounds[2] > 0.0f && wbounds[3] > 0.0f) {
					w->render(w->uptr, w, state.vg, wbounds);
				}
				if (debug) drawDebugRect(w);
				break;

			case MG_CANVAS:
				isectBounds(wbounds, bbounds, w->x, w->y, w->width, w->height);
				if (w->render != NULL && wbounds[2] > 0.0f && wbounds[3] > 0.0f) {
					w->render(w->uptr, w, state.vg, wbounds);
				}
				if (debug) drawDebugRect(w);
				break;
			}
		}
	}

	if (box->style.overflow == MG_SCROLL) {
		nvgScissor(state.vg, bbounds[0], bbounds[1], bbounds[2], bbounds[3]);
		if (box->dir == MG_ROW) {
			float contentSize = box->cwidth;
			float containerSize = box->width;
			if (contentSize > containerSize+0.5f) {
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
			float contentSize = box->cheight;
			float containerSize = box->height;
			if (contentSize > containerSize+0.5f) {
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
	for (i = 0; i < state.panelCount; i++) {
		if (state.panels[i]->active)
			drawBox(state.panels[i], bounds);
	}
}

static void offsetWidget(struct MGwidget* w, float dx, float dy)
{
	w->x += dx;
	w->y += dy;
	if  (w->type == MG_BOX) {
		struct MGwidget* c = NULL;
		for (c = w->box.children; c != NULL; c = c->next)
			offsetWidget(c, dx, dy);
	}
} 

static void offsetPopups()
{	
	int i;
	if (state.vg == NULL) return;	
	for (i = 0; i < state.panelCount; i++) {
		struct MGwidget* w = state.panels[i];
		if (!w->active) continue;
		if (w->parent) {
			float dx = w->parent->x;
			float dy = w->parent->y + w->parent->height;
			offsetWidget(w, dx,dy);
		}
	}
}


void mgFrameEnd()
{
	float bounds[4] = {0, 0, state.width, state.height};

	offsetPopups();

	updateLogic(bounds);
	drawPanels(bounds);

	// clean up transients
	cleanUpTransients();
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
	tw = str != NULL ? nvgTextBounds(state.vg, 0,0, str, NULL, NULL) : 0;
	nvgTextMetrics(state.vg, NULL, NULL, &th);
	if (w) *w = tw;
	if (h) *h = th;
}


static void paragraphSize(const char* str, float size, float lineh, float maxw, float* w, float* h)
{
	float bounds[4];
	if (state.vg == NULL) {
		*w = *h = 0;
		return;
	}
	nvgFontFace(state.vg, "sans");
	nvgFontSize(state.vg, size);
	nvgTextLineHeight(state.vg, lineh > 0 ? lineh : 1);
	nvgTextBoxBounds(state.vg, 0,0, maxw, str, NULL, bounds);
	*w = bounds[2] - bounds[0];
	*h = bounds[3] - bounds[1];
}

static void applySize(struct MGwidget* w)
{
	if (isStyleSet(&w->style, MG_WIDTH_ARG)) // && w->style.width != MG_AUTO_SIZE)
		w->cwidth = w->style.width;
	if (isStyleSet(&w->style, MG_HEIGHT_ARG)) // && w->style.height != MG_AUTO_SIZE)
		w->cheight = w->style.height;
}

static void applyPanelSize(struct MGwidget* w)
{
	w->width = w->cwidth;
	w->height = w->cheight;

	if (isStyleSet(&w->style, MG_WIDTH_ARG) && w->style.width != MG_AUTO_SIZE)
		w->width = w->style.width;
	if (isStyleSet(&w->style, MG_HEIGHT_ARG) && w->style.height != MG_AUTO_SIZE)
		w->height = w->style.height;

	w->width += w->style.paddingx*2;
	w->height += w->style.paddingy*2;
	// Prop size is not used.
}

static void fitToContent(struct MGwidget* root)
{
	struct MGwidget* w = NULL;
	float width = 0;
	float height = 0;

	for (w = root->box.children; w != NULL; w = w->next) {
		if (isStyleSet(&w->style, MG_ANCHOR_ARG)) continue;
		if (w->type == MG_BOX) {
			fitToContent(w);
			applySize(w);
		}
	}

	if (root->dir == MG_COL) {
		for (w = root->box.children; w != NULL; w = w->next) {
			if (isStyleSet(&w->style, MG_ANCHOR_ARG)) continue;
			width = maxf(width, w->cwidth + w->style.paddingx*2);
			height += w->cheight + w->style.paddingy*2;
			if (w->next != NULL) height += w->style.spacing;
		}
	} else {
		for (w = root->box.children; w != NULL; w = w->next) {
			if (isStyleSet(&w->style, MG_ANCHOR_ARG)) continue;
			width += w->cwidth + w->style.paddingx*2;
			height = maxf(height, w->cheight + w->style.paddingy*2);
			if (w->next != NULL) width += w->style.spacing;
		}
	}

	root->cwidth = width;
	root->cheight = height;
}

static float calcPropDelta(unsigned char align, float u, float psize, float wsize)
{
	switch (align) {
	case MG_START: return psize*u;
	case MG_END: return psize*u - wsize;
	case MG_CENTER: return psize*u - wsize/2;
	case MG_JUSTIFY: return maxf(0, wsize/2 + (psize - wsize)*u) - wsize/2;
	}
	return 0;
}

static float calcPosDelta(unsigned char align, float x, float psize, float wsize)
{
	switch (align) {
	case MG_START: return x;
	case MG_END: return psize - wsize + x;
	case MG_CENTER: return psize/2 - wsize/2 + x;
	case MG_JUSTIFY: return x;
	}
	return 0;
}

static int layoutWidgets(struct MGwidget* root)
{
	struct MGwidget* w = NULL;
	float x, y, rw, rh;
	float sum = 0, avail = 0;
	int ngrow = 0, nitems = 0;
	int reflow = 0;

	x = root->x + root->style.paddingx;
	y = root->y + root->style.paddingy;
	rw = maxf(0, root->width - root->style.paddingx*2);
	rh = maxf(0, root->height - root->style.paddingy*2);

	// Allocate space for scrollbar
	if (root->style.overflow == MG_SCROLL) {
		if (root->dir == MG_ROW) {
			if (root->cwidth > rw+0.5f)
				rh = maxf(0, rh - (SCROLL_SIZE+SCROLL_PAD*2));
		} else {
			if (root->cheight > rh+0.5f)
				rw = maxf(0, rw - (SCROLL_SIZE+SCROLL_PAD*2));
		}
	}

	// Calculate desired sizes of the boxes.
	for (w = root->box.children; w != NULL; w = w->next) {

		if (isStyleSet(&w->style, MG_PROPWIDTH_ARG)) {
			w->width = clampf(maxf(0, rw - w->style.paddingx*2) * w->style.width + w->style.paddingx*2, 0, rw);
		} else if (isStyleSet(&w->style, MG_WIDTH_ARG)) {
			w->width = w->style.width + w->style.paddingx*2;
		} else {
			w->width = w->cwidth + w->style.paddingx*2;
		}

		if (isStyleSet(&w->style, MG_PROPHEIGHT_ARG)) {
			w->height = clampf(maxf(0, rh - w->style.paddingy*2) * w->style.height + w->style.paddingy*2, 0, rh);
		} else if (isStyleSet(&w->style, MG_HEIGHT_ARG)) {
			w->height = w->style.height + w->style.paddingy*2;
		} else {
			w->height = w->cheight + w->style.paddingy*2;
		}

		if (!isStyleSet(&w->style, MG_ANCHOR_ARG)) {
			// Handle justify align already here.
			if (root->style.align == MG_JUSTIFY) {
				if (root->dir == MG_COL)
					w->width = rw;
				else
					w->height = rh;
			}
			w->width = minf(rw, w->width);
			w->height = minf(rh, w->height);
		}
	}

	// Reflow multi-line text if needed
	for (w = root->box.children; w != NULL; w = w->next) {
		float tw, th;
		if (w->type != MG_PARAGRAPH) continue;
		// Apply to columns only
		if (w->parent != NULL && w->parent->dir != MG_COL) continue;
		// Apply only if height is not set.
		if (isStyleSet(&w->style, MG_PROPHEIGHT_ARG) || isStyleSet(&w->style, MG_HEIGHT_ARG)) continue;
		// Recalc paragraph size based on new width.
		paragraphSize(w->text, w->style.fontSize, w->style.lineHeight, w->width, &tw, &th);
		w->cwidth = w->width;
		w->cheight = th;
		w->height = w->cheight + w->style.paddingy*2;
		reflow |= 1;
	}

	// Layout anchored widgets
	for (w = root->box.children; w != NULL; w = w->next) {
		if (isStyleSet(&w->style, MG_ANCHOR_ARG)) {
			unsigned char ax = w->style.anchor & 0xf;
			unsigned char ay = (w->style.anchor >> 4) & 0xf;

			if (isStyleSet(&w->style, MG_PROPX_ARG))
				w->x = root->x + root->style.paddingx + calcPropDelta(ax, w->style.x, rw, w->width);
			else
				w->x = root->x + root->style.paddingx + calcPosDelta(ax, w->style.x, rw, w->width);

			if (isStyleSet(&w->style, MG_PROPY_ARG))
				w->y = root->y + root->style.paddingy + calcPropDelta(ay, w->style.y, rh, w->height);
			else
				w->y = root->y + root->style.paddingy + calcPosDelta(ay, w->style.y, rh, w->height);

			if (w->type == MG_BOX)
				reflow |= layoutWidgets(w);
		}
	}

	// Layout box model widgets
	if (root->dir == MG_COL) {
		float packSpacing = 0;

		for (w = root->box.children; w != NULL; w = w->next) {
			if (isStyleSet(&w->style, MG_ANCHOR_ARG)) continue;
			sum += w->height;
			if (w->next != NULL) sum += w->style.spacing;
			ngrow += w->style.grow;
			nitems++;
		}

		avail = rh - sum;
		if (root->style.overflow != MG_FIT)
			avail = maxf(0, avail); 

		if (ngrow == 0 && avail > 0) {
			if (root->style.pack == MG_START)
				y += 0;
			else if (root->style.pack == MG_CENTER)
				y += avail/2;
			else if (root->style.pack == MG_END)
				y += avail;
			else if (root->style.pack == MG_JUSTIFY) {
				packSpacing = avail / nitems;
				y += packSpacing/2;
			}
			avail = 0;
		}

		for (w = root->box.children; w != NULL; w = w->next) {
			if (isStyleSet(&w->style, MG_ANCHOR_ARG)) continue;
			w->x = x;
			w->y = y;
			if (ngrow > 0)
				w->height += (float)w->style.grow/(float)ngrow * avail;
			else if (avail < 0)
				w->height += 1.0f/(float)nitems * avail;

			switch (root->style.align) {
			case MG_END:
				w->x += rw - w->width;
				break;
			case MG_CENTER:
				w->x += rw/2 - w->width/2;
				break;
			default: // MG_START and MG_JUSTIFY
				break;
			}
			y += w->height + w->style.spacing + packSpacing;

			if (w->type == MG_BOX)
				reflow |= layoutWidgets(w);
		}

	} else {
		float packSpacing = 0;

		for (w = root->box.children; w != NULL; w = w->next) {
			if (isStyleSet(&w->style, MG_ANCHOR_ARG)) continue;
			sum += w->width;
			if (w->next != NULL) sum += w->style.spacing;
			ngrow += w->style.grow;
			nitems++;
		}

		avail = rw - sum;
		if (root->style.overflow != MG_FIT)
			avail = maxf(0, avail); 

		if (ngrow == 0 && avail > 0) {
			if (root->style.pack == MG_START)
				x += 0;
			else if (root->style.pack == MG_CENTER)
				x += avail/2;
			else if (root->style.pack == MG_END)
				x += avail;
			else if (root->style.pack == MG_JUSTIFY) {
				packSpacing = avail / nitems;
				x += packSpacing/2;
			}
			avail = 0;
		}

		for (w = root->box.children; w != NULL; w = w->next) {
			if (isStyleSet(&w->style, MG_ANCHOR_ARG)) continue;

			w->x = x;
			w->y = y;

			if (ngrow > 0)
				w->width += (float)w->style.grow/(float)ngrow * avail;
			else if (avail < 0)
				w->width += 1.0f/(float)nitems * avail;

			switch (root->style.align) {
			case MG_END:
				w->y += rh - w->height;
				break;
			case MG_CENTER:
				w->y += rh/2 - w->height/2;
				break;
			default: // MG_START and MG_JUSTIFY
				break;
			}

			x += w->width + w->style.spacing + packSpacing;

			if (w->type == MG_BOX)
				reflow |= layoutWidgets(w);
		}
	}

	return reflow;
}

static void layoutPanel(struct MGwidget* w)
{
	int reflow = 0;
	// Do first pass on layout, most things anre handled here.
	fitToContent(w);
	applyPanelSize(w);
	reflow = layoutWidgets(w);
	// If the layout had paragraphs, we'll need to reflow because the height may have changed.
	if (reflow) {
		fitToContent(w);
		applyPanelSize(w);
		layoutWidgets(w);
	}
}

struct MGopt* mgPackOpt(unsigned char a, int v)
{
	struct MGopt* opt = allocOpt();
	if (opt == NULL) return NULL;
	opt->type = a;
	opt->ival = v;
	return opt;
}

struct MGopt* mgPackOptf(unsigned char a, float v)
{
	struct MGopt* opt = allocOpt();
	if (opt == NULL) return NULL;
	opt->type = a;
	opt->fval = v;
	return opt;
}

struct MGopt* mgPackOpt2(unsigned char a, int x, int y)
{
	struct MGopt* opt = allocOpt();
	if (opt == NULL) return NULL;
	opt->type = a;
	opt->ival = (x & 0x0f) | ((y & 0x0f) << 4);
	return opt;
}

struct MGopt* mgPackOptStr(unsigned char a, const char* s)
{
	struct MGopt* opt = allocOpt();
	if (opt == NULL) return NULL;
	opt->type = a;
	opt->sval = allocText(s);
	return opt;
}

static void dumpOpts(struct MGopt* opts)
{
	printf("opts = ");
	for (; opts != NULL; opts = opts->next) {
		switch (opts->type) {
			case MG_OVERFLOW_ARG:		printf("overflow=%d ", opts->ival); break;
			case MG_ALIGN_ARG:			printf("align=%d ", opts->ival); break;
			case MG_PACK_ARG:			printf("pack=%d ", opts->ival); break;
			case MG_GROW_ARG:			printf("grow=%d ", opts->ival); break;
			case MG_WIDTH_ARG:			printf("width=%f ", opts->fval); break;
			case MG_HEIGHT_ARG:			printf("height=%f ", opts->fval); break;
			case MG_PADDINGX_ARG:		printf("paddingx=%d ", opts->ival); break;
			case MG_PADDINGY_ARG:		printf("paddingy=%d ", opts->ival); break;
			case MG_SPACING_ARG:		printf("spacing=%d ", opts->ival); break;
			case MG_FONTSIZE_ARG:		printf("fontSize=%d ", opts->ival); break;
			case MG_TEXTALIGN_ARG:		printf("textAlign=%d ", opts->ival); break;
			case MG_LINEHEIGHT_ARG:		printf("lineHeight=%f ", opts->fval); break;
			case MG_LOGIC_ARG:			printf("logic=%d ", opts->ival); break;
			case MG_CONTENTCOLOR_ARG:	printf("contentColor=%08x ", opts->ival); break;
			case MG_FILLCOLOR_ARG:		printf("fillColor=%08x ", opts->ival); break;
			case MG_BORDERCOLOR_ARG:	printf("borderColor=%08x ", opts->ival); break;
			case MG_BORDERSIZE_ARG:		printf("borderSize=%d ", opts->ival); break;
			case MG_CORNERRADIUS_ARG:	printf("cornerRadius=%d ", opts->ival); break;
			case MG_TAG_ARG:			printf("tag=%s ", opts->sval); break;
			case MG_PROPWIDTH_ARG:		printf("pwidth=%f ", opts->fval); break;
			case MG_PROPHEIGHT_ARG:		printf("pheight=%f ", opts->fval); break;
			case MG_ANCHOR_ARG:			printf("anchor=%d ", opts->ival); break;
			case MG_PROPX_ARG:			printf("px=%f ", opts->fval); break;
			case MG_PROPY_ARG:			printf("py=%f ", opts->fval); break;
			case MG_X_ARG:				printf("x=%f ", opts->fval); break;
			case MG_Y_ARG:				printf("y=%f ", opts->fval); break;
		}
	}
	printf("\n");
}

struct MGopt* mgOpts_(unsigned int dummy, ...)
{
	va_list list;
	struct MGopt* opt = NULL;
	struct MGopt* ret = NULL;
	struct MGopt* tail = NULL;

	va_start(list, dummy);
	for (opt = va_arg(list, struct MGopt*); opt != NULL; opt = va_arg(list, struct MGopt*)) {

		if (ret == NULL)
			ret = opt;

		if (tail == NULL) {
			tail = opt;
		} else {
			tail->next = opt;
		}
		// Find the tail of the added arg.
		while (tail->next != NULL) {
			tail = tail->next;
		}
	}
	va_end(list);

	// Always return something.
	if (ret == NULL) {
		ret = allocOpt();
		if (ret == NULL) return NULL;
		ret->type = MG_NONE;
		ret->ival = 0;
		ret->next = NULL;
	}

	return ret;
}

static void flattenStyle(struct MGstyle* style, struct MGopt* opts)
{

//	dumpOpts(opts);

	for (; opts != NULL; opts = opts->next) {
		unsigned int unset = 0;
		switch (opts->type) {
			case MG_OVERFLOW_ARG:		style->overflow = opts->ival; break;
			case MG_ALIGN_ARG:			style->align = opts->ival; break;
			case MG_PACK_ARG:			style->pack = opts->ival; break;
			case MG_GROW_ARG:			style->grow = opts->ival; break;

			case MG_WIDTH_ARG:			style->width = opts->fval; unset = 1 << MG_PROPWIDTH_ARG; break;
			case MG_HEIGHT_ARG:			style->height = opts->fval; unset = 1 << MG_PROPHEIGHT_ARG; break;
			case MG_PROPWIDTH_ARG:		style->width = opts->fval; unset = 1 << MG_WIDTH_ARG; break;
			case MG_PROPHEIGHT_ARG:		style->height = opts->fval; unset = 1 << MG_HEIGHT_ARG; break;

			case MG_X_ARG:				style->x = opts->fval; unset = 1 << MG_PROPX_ARG; break;
			case MG_Y_ARG:				style->y = opts->fval; unset = 1 << MG_PROPY_ARG; break;
			case MG_PROPX_ARG:			style->x = opts->fval; unset = 1 << MG_X_ARG; break;
			case MG_PROPY_ARG:			style->y = opts->fval; unset = 1 << MG_Y_ARG; break;

			case MG_PADDINGX_ARG:		style->paddingx = opts->ival; break;
			case MG_PADDINGY_ARG:		style->paddingy = opts->ival; break;
			case MG_SPACING_ARG:		style->spacing = opts->ival; break;

			case MG_FONTSIZE_ARG:		style->fontSize = opts->ival; break;
			case MG_TEXTALIGN_ARG:		style->textAlign = opts->ival; break;
			case MG_LINEHEIGHT_ARG:		style->lineHeight = opts->fval; break;

			case MG_LOGIC_ARG:			style->logic = opts->ival; break;

			case MG_CONTENTCOLOR_ARG:	style->contentColor = opts->ival; break;
			case MG_FILLCOLOR_ARG:		style->fillColor = opts->ival; break;
			case MG_BORDERCOLOR_ARG:	style->borderColor = opts->ival; break;
			case MG_BORDERSIZE_ARG:		style->borderSize = opts->ival; break;
			case MG_CORNERRADIUS_ARG:	style->cornerRadius = opts->ival; break;

			case MG_ANCHOR_ARG:			style->anchor = opts->ival; break;
		}
		// Mark which properties has been set.
		style->set &= ~unset;
		style->set |= 1 << opts->type;
	}
}

unsigned int mgRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	return (r) | (g << 8) | (b << 16) | (a << 24);	
}

static struct MGnamedStyle* findStyle(const char* selector)
{
	// TODO: optimize
	int i;
	for (i = 0; i < stylePoolSize; i++) {
		if (strcmp(selector, stylePool[i].selector) == 0)
			return &stylePool[i];
	}
	return NULL;
}

static void dumpPath(char** path, int npath)
{
	int i;
	for (i = 0; i < npath; i++)
		printf("%s%s", (i > 0 ? "." : ""), path[i]);
}

static void parseSelector(struct MGnamedStyle* style, const char* sel)
{
	const char* str;
	const char* start;
	int n = 0;
	style->path = NULL;
	style->npath = 1;
	for (str = sel; *str; str++) {
		if (*str == '.')
			style->npath++;
	}
	style->path = (char**)malloc(sizeof(char*)*style->npath);
	for (str = sel, start = sel; *str; str++) {
		if (str[1] == '.' || str[1] == '\0') {
			int len = (int)(str+1 - start);
			if (len > 0) {
				style->path[n] = (char*)malloc(len+1);
				memcpy(style->path[n], start, len);
				style->path[n][len] = '\0';
				start = str+2;
				n++;
			}
		}
	}

	printf("selector: ");
	dumpPath(style->path, style->npath);
	printf("\n");
}

unsigned int mgCreateStyle(const char* selector, struct MGopt* normal, struct MGopt* hover, struct MGopt* active, struct MGopt* focus)
{
	struct MGnamedStyle* style = findStyle(selector);
	if (style == NULL) {
		if (stylePoolSize+1 > MG_STYLE_POOL_SIZE)
			return 0;
		style = &stylePool[stylePoolSize++];
		memset(style, 0, sizeof(*style));
		style->selector = malloc(strlen(selector)+1);
		strcpy(style->selector, selector);
		// Parse and store selector
		parseSelector(style, selector);
	}

	flattenStyle(&style->normal, normal);

	style->hover = style->normal;
	flattenStyle(&style->hover, hover);

	style->active = style->normal;
	flattenStyle(&style->active, active);

	style->focus = style->normal;
	flattenStyle(&style->focus, focus);

	return 1;
}

static struct MGhit* hitResult(struct MGwidget* w)
{
	if (w == NULL) return NULL;
	if (w->style.logic == MG_CLICK && state.clicked == w->id)
		return &state.result;
	if (w->style.logic == MG_DRAG && (state.pressed == w->id || state.dragged == w->id || state.released == w->id))
		return &state.result;
	return NULL;
}

static struct MGhit* hitResult2(unsigned int id)
{
	if (state.clicked == id || state.pressed == id || state.dragged == id || state.released == id)
		return &state.result;
	return NULL;
}

static unsigned char getState(struct MGwidget* w)
{
	unsigned char ret = MG_NORMAL;
	if (w == NULL) return ret;
	if (w->parent != NULL && w->bubble)
		ret = getState(w->parent);
	if (state.active == w->id) ret |= MG_ACTIVE;
	if (state.hover == w->id) ret |= MG_HOVER;
	if (state.focus == w->id) ret |= MG_FOCUS;
	return ret;
}

static unsigned char getState2(unsigned int id)
{
	unsigned char ret = MG_NORMAL;
	if (state.active == id) ret |= MG_ACTIVE;
	if (state.hover == id) ret |= MG_HOVER;
	if (state.focus == id) ret |= MG_FOCUS;
	return ret;
}

static unsigned char getChildState(struct MGwidget* w)
{
	unsigned char ret = MG_NORMAL;
	if (w == NULL) return ret;
	if (state.active == w->id) ret |= MG_ACTIVE;
	if (state.hover == w->id) ret |= MG_HOVER;
	if (state.focus == w->id) ret |= MG_FOCUS;
	if (w->type == MG_BOX) {
		struct MGwidget* c;
		for (c = w->box.children; c != NULL; c = c->next)
			ret |= getChildState(c);
	}
	return ret;
}

unsigned int murmur3(const void * key, int len, unsigned int seed)
{
	const unsigned char* data = (const unsigned char*)key;
	const int nblocks = len / 4;
	const unsigned int* blocks = (const unsigned int*)(data + nblocks*4);
	const unsigned char* tail = data + nblocks*4;
	unsigned int h = seed, k;
	int i;

	// Body
	for (i = -nblocks; i; i++) {
		k = blocks[i];
		k *= 0xcc9e2d51;
		k = (k << 15) | (k >> (32-15));	// rotl
	    k *= 0x1b873593;
		h ^= k;
		h = (h << 13) | (h >> (32-13)); // rotl
		h = h * 5 + 0xe6546b64;
	}

	// tail
	k = 0;
	switch(len & 3)
	{
	case 3: k ^= tail[2] << 16;
	case 2: k ^= tail[1] << 8;
	case 1: k ^= tail[0];
		k *= 0xcc9e2d51;
		k = (k << 15) | (k >> (32-15));	// rotl
		k *= 0x1b873593;
		h ^= k;
	};

	// finalization
	h ^= (unsigned int)len;
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
} 

static int matchStyle(struct MGnamedStyle* style, const char* path[], int npath)
{
	int i, a = style->npath-1, b = npath-1;
	int n = mini(npath, style->npath);

/*	printf(" - match n=%d sel=", n);
	dumpPath(style->path, style->npath);
	printf("  path=");
	dumpPath(path, npath);*/

	for (i = 0; i < n; i++) {
		if (strcmp(style->path[a], path[b]) != 0) {
//			printf(" - fail\n");
			return 0;
		}
		a--;
		b--;
	}

/*	printf(" - match n=%d sel=", n);
	dumpPath(style->path, style->npath);
	printf("  path=");
	dumpPath(path, npath);
	printf(" - succeed\n");*/

	return n;
}

static struct MGnamedStyle* selectStyle(const char* path[], int npath)
{
	int i, nmax = 0;
	struct MGnamedStyle* smax = NULL;

	if (npath == 0)
		return NULL;

/*	printf("selectStyle(");
	dumpPath(path, npath);
	printf(")\n");*/

	// Returns longest match
	for (i = 0; i < stylePoolSize; i++) {
		int n = matchStyle(&stylePool[i], path, npath);
		if (n > nmax) {
			nmax = n;
			smax = &stylePool[i];
		}
	}

/*	if (smax != NULL) {
		printf("  - found: ");
		dumpPath(smax->path, smax->npath);
		printf("\n");
	}*/

	return smax;
}

static const char* getTag(struct MGopt* opts)
{
	char* tag = NULL;
	for (; opts != NULL; opts = opts->next) {
		if (opts->type == MG_TAG_ARG)
			tag = opts->sval;
	}
	return tag;
}

static struct MGstyle getStyle(unsigned char wstate, struct MGopt* opts, const char* subtag)
{
	int i = 0;
	const char* path[100];
	int npath = 0;
	struct MGnamedStyle* match = NULL;
	struct MGstyle style;
	const char* tag = getTag(opts);

	// Find current path to be used with selector.
	for (i = 0; i < state.tagCount; i++) {
		if (state.tags[i] != NULL)
			path[npath++] = (char*)state.tags[i];
	}
	if (tag != NULL)
		path[npath++] = tag;
	if (subtag != NULL)
		path[npath++] = subtag;

	match = selectStyle(path, npath);
	if (match != NULL) {
		if (wstate & MG_ACTIVE)
			return match->active;
		else if (wstate & MG_HOVER)
			return match->hover;
		else if (wstate & MG_FOCUS)
			return match->focus;
		else
			return match->normal;
	}
	memset(&style, 0, sizeof(style));
	return style;
}

static struct MGstyle computeStyle(unsigned char wstate, struct MGopt* opts, const char* subtag)
{
	int i = 0;
	const char* path[100];
	int npath = 0;
	struct MGnamedStyle* match = NULL;
	struct MGstyle style;
	const char* tag = getTag(opts);
	memset(&style, 0, sizeof(style));

	// Find current path to be used with selector.
	for (i = 0; i < state.tagCount; i++) {
		if (state.tags[i] != NULL)
			path[npath++] = (char*)state.tags[i];
	}
	if (tag != NULL)
		path[npath++] = tag;
	if (subtag != NULL)
		path[npath++] = subtag;

	match = selectStyle(path, npath);
	if (match != NULL) {
		if (wstate & MG_ACTIVE)
			style = match->active;
		else if (wstate & MG_HOVER)
			style = match->hover;
		else if (wstate & MG_FOCUS)
			style = match->focus;
		else
			style = match->normal;
	}
	flattenStyle(&style, opts);
	return style;
}

unsigned int mgPanelBegin(int dir, float x, float y, int zidx, struct MGopt* opts)
{
	struct MGwidget* w = NULL;

	pushId(state.panelCount+1);

	opts = mgOpts(mgTag("panel"), opts);

	w = allocWidget(MG_BOX);
	w->x = x;
	w->y = y;
	w->dir = dir;
	w->style = computeStyle(getState(w), opts, NULL);

	pushBox(w);
	pushTag(getTag(opts));

	addPanel(w, zidx);

	return w->id;
}

unsigned int mgPanelEnd()
{
	popId();
	popTag();
	struct MGwidget* w = popBox();
	if (w != NULL) {
		layoutPanel(w);
		return w->id;
	}

	return 0;
}

unsigned int mgBoxBegin(int dir, struct MGopt* opts)
{
	struct MGwidget* parent = getParent();
	struct MGwidget* w = allocWidget(MG_BOX);
	if (parent != NULL)
		addChildren(parent, w);

	opts = mgOpts(mgTag("box"), opts);

	w->dir = dir;
	w->style = computeStyle(getState(w), opts, NULL);

	pushBox(w);
	pushTag(getTag(opts));

	return w->id;
}

unsigned int mgBoxEnd()
{
	struct MGwidget* w = popBox();
	popTag();
	if (w != NULL) {
//		fitToContent(w);
//		applySize(w);
		return w->id;
	}
	return 0;
}

unsigned int mgBox(struct MGopt* opts)
{
	mgBoxBegin(MG_ROW, opts);
	return mgBoxEnd();
}

unsigned int mgParagraph(const char* text, struct MGopt* opts)
{
	float tw, th;
	struct MGwidget* parent = getParent();
	struct MGwidget* w = allocWidget(MG_PARAGRAPH);
	if (parent != NULL)
		addChildren(parent, w);

	w->text = allocText(text);

	w->style = computeStyle(getState(w), mgOpts(mgTag("text"), opts), NULL);

	textSize(NULL, w->style.fontSize, NULL, &th);
	paragraphSize(w->text, w->style.fontSize, w->style.lineHeight, th*20, &tw, &th);
	w->cwidth = tw;
	w->cheight = th;
	applySize(w);

	return w->id;
}

unsigned int mgText(const char* text, struct MGopt* opts)
{
	float tw, th;
	struct MGwidget* parent = getParent();
	struct MGwidget* w = allocWidget(MG_TEXT);
	if (parent != NULL)
		addChildren(parent, w);

	w->text = allocText(text);

	w->style = computeStyle(getState(w), mgOpts(mgTag("text"), opts), NULL);
	textSize(w->text, w->style.fontSize, &tw, &th);
	w->cwidth = tw;
	w->cheight = th;
	applySize(w);

	return w->id;
}

unsigned int mgIcon(const char* name, struct MGopt* opts)
{
	struct MGwidget* parent = getParent();
	struct MGwidget* w = allocWidget(MG_ICON);
	float aspect = 1.0f;
	if (parent != NULL)
		addChildren(parent, w);

	if (name != NULL)
		w->icon.icon = findIcon(name);

	w->style = computeStyle(getState(w), mgOpts(mgTag("icon"), opts), NULL);

	if (w->icon.icon != NULL) {
		w->cwidth = w->icon.icon->image->width;
		w->cheight = w->icon.icon->image->height;		
	} else {
		w->cwidth = 1;
		w->cheight = 1;
	}
	aspect = w->cwidth / w->cheight;

//	applySize(w);
	// Maintain aspect
	if (isStyleSet(&w->style, MG_WIDTH_ARG)) {
		w->cwidth = w->style.width;
		if (!isStyleSet(&w->style, MG_HEIGHT_ARG))
			w->cheight = w->cwidth / aspect;
	}
	if (isStyleSet(&w->style, MG_HEIGHT_ARG)) {
		w->cheight = w->style.height;
		if (!isStyleSet(&w->style, MG_WIDTH_ARG))
			w->cwidth = w->cheight * aspect;
	}

//	printf("icon %f %f\n", w->cwidth, w->cheight);

	return w->id;
}

unsigned int mgCanvas(float width, float height, MGcanvasLogicFun logic, MGcanvasRenderFun render, void* uptr, struct MGopt* opts)
{
	struct MGwidget* parent = getParent();
	struct MGwidget* w = allocWidget(MG_CANVAS);
	if (parent != NULL)
		addChildren(parent, w);

	w->logic = logic;
	w->render = render;
	w->uptr = uptr;
	w->cwidth = width;
	w->cheight = height;

	w->style = computeStyle(getState(w), mgOpts(mgTag("canvas"), opts), NULL);

	return w->id;
}

struct MGsliderState {
	float value;
	float vstart;
	float vmin, vmax;
/*	struct MGstyle slotStyle;
	struct MGstyle barStyle;
	struct MGstyle handleStyle;*/
};

static void innerBounds(struct MGwidget* w, struct MGrect* b)
{
	float padx = minf(w->style.paddingx, maxf(0.0f, w->width));
	float pady = minf(w->style.paddingy, maxf(0.0f, w->height));
	b->x = w->x + padx;
	b->y = w->y + pady;
	b->width = maxf(0.0f, w->width - padx*2);
	b->height = maxf(0.0f, w->height - pady*2);
}

static void sliderDraw(void* uptr, struct MGwidget* w, struct NVGcontext* vg, const float* view)
{
	struct MGsliderState* input = (struct MGsliderState*)findTransient(w->id, 0, sizeof(struct MGsliderState));
	float x, hr;
	if (input == NULL) return;

	hr = maxf(2.0f, w->height - w->style.paddingy*2) / 2.0f;

	// Slot
	nvgBeginPath(vg);
	nvgMoveTo(vg, w->x+hr, (int)(w->y+w->height/2));
	nvgLineTo(vg, w->x-hr + w->width, (int)(w->y+w->height/2));
	nvgStrokeColor(vg, nvgRGBA(0,0,0,64));
	nvgStrokeWidth(vg,2.0f);
	nvgStroke(vg);

	// Handle position
	x = w->x + hr + (input->value - input->vmin) / (input->vmax - input->vmin) * (w->width - hr*2);

	// Bar
	if (isStyleSet(&w->style, MG_CONTENTCOLOR_ARG)) {
		nvgBeginPath(vg);
		nvgMoveTo(vg, w->x+hr, (int)(w->y+w->height/2));
		nvgLineTo(vg, x, (int)(w->y+w->height/2));
		nvgStrokeColor(vg, nvgCol(w->style.contentColor));
		nvgStrokeWidth(vg,2.0f);
		nvgStroke(vg);
	}

	// Handle
	if (isStyleSet(&w->style, MG_FILLCOLOR_ARG)) {
		nvgBeginPath(vg);
		nvgCircle(vg, x, w->y+w->height/2, hr);
		nvgFillColor(vg, nvgCol(w->style.fillColor));
		nvgFill(vg);
	}
	if (isStyleSet(&w->style, MG_BORDERCOLOR_ARG)) {
		float s = w->style.borderSize * 0.5f;
		nvgBeginPath(vg);
		nvgCircle(vg, x, w->y+w->height/2, hr-s);
		nvgStrokeWidth(vg, w->style.borderSize);
		nvgStrokeColor(vg, nvgCol(w->style.borderColor));
		nvgStroke(vg);
	}
}

static void sliderLogic(void* uptr, struct MGwidget* w, int event, struct MGhit* hit)
{
	struct MGsliderState* input = (struct MGsliderState*)findTransient(w->id, 0, sizeof(struct MGsliderState));
	struct MGsliderState* output = (struct MGsliderState*)hit->storage;
	float hr, xmin, xmax, xrange;

	if (input == NULL) return;

	hr = maxf(2.0f, w->height - w->style.paddingy*2) / 2.0f;

	xmin = w->x + hr;
	xmax = w->x + w->width - hr;
	xrange = maxf(1.0f, xmax - xmin);

	if (hit->pressed) {
		float u = (input->value - input->vmin) / (input->vmax - input->vmin);
		float x = xmin + u * (xmax - xmin);
		if (hit->mx < (x-hr) || hit->mx > (x+hr)) {
			// If hit outside the handle, skip there directly.
			float v = clampf((hit->mx - xmin) / xrange, 0.0f, 1.0f);
			input->value = clampf(input->vmin + v * (input->vmax - input->vmin), input->vmin, input->vmax);
		}
		output->value = input->value;
		output->vstart = input->value;
	}
	if (hit->dragged) {
		float delta = (hit->deltamx / xrange) * (input->vmax - input->vmin);
		input->value = clampf(output->vstart + delta, input->vmin, input->vmax);
		output->value = input->value;
	}

}


unsigned int mgSlider(float* value, float vmin, float vmax, struct MGopt* opts)
{
	struct MGhit* res = NULL;
	//struct MGstyle comp;
	struct MGsliderState* input = NULL;
	unsigned int canvas;

/*
	mgCreateStyle("slider.slot", mgOpts(
		mgHeight(2), //SLIDER_HANDLE),
		mgFillColor(0,0,0,128)
//		mgPaddingX(SLIDER_HANDLE/2),
//		mgCornerRadius(3)
	), mgOpts(), mgOpts(), mgOpts());

	mgCreateStyle("slider.bar",
		// Normal
		mgOpts(
			mgPaddingX(SLIDER_HANDLE/2),
			mgHeight(2), //SLIDER_HANDLE),
			mgOverflow(MG_VISIBLE),
			mgFillColor(220,220,220,255)
		),
		// Hover
		mgOpts(
			mgFillColor(255,255,255,255)
		),
		// Active
		mgOpts(
			mgFillColor(255,255,255,255)
		),
		// Focus
		mgOpts(
		)
	);

	mgCreateStyle("slider.handle",
		// Normal
		mgOpts(
			mgWidth(SLIDER_HANDLE),
			mgHeight(SLIDER_HANDLE),
			mgFillColor(255,255,255,255),
			mgBorderColor(255,255,255,0),
			mgBorderSize(2),
			mgCornerRadius(SLIDER_HANDLE/2)
		),
		// Hover
		mgOpts(
//			mgFillColor(220,220,220,255)
		),
		// Active
		mgOpts(
			mgBorderColor(255,255,255,255),
			mgFillColor(32,32,32,255)
		),
		// Focus
		mgOpts(
			mgBorderColor(0,192,255,128)
		)
	);
*/
	opts = mgOpts(mgLogic(MG_DRAG), mgTag("slider-canvas"), opts);
	canvas = mgCanvas(DEFAULT_SLIDERW, SLIDER_HANDLE, sliderLogic, sliderDraw, NULL, opts);

	input = (struct MGsliderState*)allocTransient(canvas, 0, sizeof(struct MGsliderState));
	if (input != NULL) {
		unsigned char s = getState2(canvas);
		input->value = *value;
		input->vmin = vmin;
		input->vmax = vmax;

/*		input->slotStyle = getStyle(s, opts, "slot");
		input->barStyle = getStyle(s, opts, "bar");
		input->handleStyle = getStyle(s, opts, "handle");*/
	}

	res = hitResult2(canvas);
	if (res != NULL) {
		struct MGsliderState* output = (struct MGsliderState*)res->storage;
		*value = output->value;
	}

	return canvas;
}


unsigned int mgSlider2(float* value, float vmin, float vmax, struct MGopt* opts)
{
	struct MGhit* res = NULL;
	struct MGhit* hres = NULL;
	float pc = (*value - vmin) / (vmax - vmin);
	struct MGopt* hopts;	
	struct MGstyle hstyle;
	unsigned int slider, handle;

	slider = mgBoxBegin(MG_ROW, mgOpts(mgLogic(MG_DRAG), mgWidth(DEFAULT_SLIDERW), mgHeight(SLIDER_HANDLE+1), mgTag("slider"), opts));
		// Slot
		mgBox(mgOpts(mgPropPosition(MG_JUSTIFY,MG_CENTER,0,0.5f), mgTag("slot"), mgPropWidth(1.0f)));
		// Bar
		mgBox(mgOpts(mgPropPosition(MG_START,MG_CENTER,0,0.5f), mgAlign(MG_CENTER), mgOverflow(MG_VISIBLE), mgTag("bar"), mgPropWidth(pc)));

//		hopts = mgOpts(mgLogic(MG_DRAG), mgPropPosition(MG_JUSTIFY,MG_CENTER,pc,0.5f), mgTag("handle"));
//		hstyle = computeStyle(MG_NORMAL, hopts);
//		hres = mgIcon("handle", hopts);
		// Handle
//		handle = mgBox(mgOpts(mgLogic(MG_DRAG), mgPropPosition(MG_JUSTIFY,MG_CENTER,pc,0.5f), mgTag("handle")));
//				mgIcon(icon, mgOpts());

		handle = mgIcon("check", mgOpts(mgLogic(MG_DRAG), mgPropPosition(MG_JUSTIFY,MG_CENTER,pc,0.5f), mgTag("handle")));
	mgBoxEnd();

	hres = hitResult2(handle);
	res = hitResult2(slider);

	// TODO: reconsider pbounds.

	if (hres != NULL) {
		struct MGsliderState* state = (struct MGsliderState*)hres->storage;
		float xmin = hres->pbounds[0] + hstyle.width/2;
		float xmax = hres->pbounds[0] + hres->pbounds[2] - hstyle.width/2;
		float xrange = maxf(1.0f, xmax - xmin);
		if (hres->pressed) {
			state->value = *value;
		}
		if (hres->dragged) {
			float delta = (hres->deltamx / xrange) * (vmax - vmin);
			*value = clampf(state->value + delta, vmin, vmax);
		}
	}

	if (res != NULL) {
		struct MGsliderState* state = (struct MGsliderState*)res->storage;
		float xmin = res->bounds[0] + hstyle.width/2;
		float xmax = res->bounds[0] + res->bounds[2] - hstyle.width/2;
		float xrange = maxf(1.0f, xmax - xmin);
		if (res->pressed) {
			float v = clampf((res->mx - xmin) / xrange, 0.0f, 1.0f);
			*value = clampf(vmin + v * (vmax - vmin), vmin, vmax);
			state->value = *value;
		}
		if (res->dragged) {
			float delta = (res->deltamx / xrange) * (vmax - vmin);
			*value = clampf(state->value + delta, vmin, vmax);
		}
	}

	return slider;
}

unsigned int mgProgress(float progress, struct MGopt* opts)
{
	float pc = clampf(progress, 0.0f, 1.0f);
	mgBoxBegin(MG_ROW, mgOpts(mgTag("progress"), opts));
		mgBoxBegin(MG_ROW, mgOpts(mgPropPosition(MG_START,MG_JUSTIFY,0,0.5f), mgAlign(MG_CENTER), mgOverflow(MG_VISIBLE), mgTag("bar"), mgPropWidth(pc)));
		mgBoxEnd();
	return mgBoxEnd();
}

unsigned int mgScrollBar(float* offset, float contentSize, float viewSize, struct MGopt* opts)
{
	struct MGhit* res = NULL;
	struct MGhit* hres = NULL;
	float slack = maxf(0, contentSize - viewSize);
	float oc = minf(1.0f, *offset / maxf(1.0f, slack));
	float pc = minf(1.0f, viewSize / maxf(1.0f, contentSize));
	unsigned int scroll, handle;

	scroll = mgBoxBegin(MG_ROW, mgOpts(mgLogic(MG_DRAG), mgTag("scroll"), opts));
		handle = mgBoxBegin(MG_ROW, mgOpts(mgLogic(MG_DRAG), mgPropPosition(MG_JUSTIFY,MG_JUSTIFY,oc,0.5f), mgTag("bar"), mgPropWidth(pc)));
		mgBoxEnd();
	mgBoxEnd();

	res = hitResult2(scroll);
	hres = hitResult2(handle);

	if (hres != NULL) {
		// Drag slider to scroll
		struct MGsliderState* state = (struct MGsliderState*)hres->storage;
		float xmin = hres->pbounds[0];
		float xmax = hres->pbounds[0] + hres->pbounds[2];
		float xrange = maxf(1.0f, xmax - xmin);
		if (hres->pressed) {
			state->value = *offset;
		}
		if (hres->dragged && pc < 1.0f) {
			float delta = (hres->deltamx / xrange) * slack / (1-pc);
			*offset = clampf(state->value + delta, 0, slack);
		}
	} else if (res != NULL) {
		// Click on the BG will jump a page worth forw/back
		struct MGsliderState* state = (struct MGsliderState*)res->storage;
		float delay = 0.75f;
		float delay2 = 0.05f;
		float xmin = res->bounds[0];
		float xmax = res->bounds[0] + res->bounds[2];
		float xrange = maxf(1.0f, xmax - xmin);
		float hmin = xmin + xrange * oc * (1-pc);
		float hmax = hmin + xrange * pc;
		if (res->pressed) {
			state->value = 0;
		}
		state->value += 1.0f/ 60.0f; // todo, make this timer.
		if (res->pressed || (res->dragged && state->value > delay)) {
			if (res->mx < hmin)
				*offset = clampf(*offset - pc * contentSize, 0, slack);
			else if (res->mx > hmax)
				*offset = clampf(*offset + pc * contentSize, 0, slack);
			if (!res->pressed)
				state->value = delay-delay2;
		}
	}

	return scroll;
}


struct MGinputState {
	int maxText;
	int caretPos;
	int npos;
	int counter;
	struct NVGglyphPosition* pos;
	char* text;
};

static struct MGinputState* allocTransientInput(struct MGwidget* w, int maxText)
{
	struct MGinputState* input = (struct MGinputState*)allocTransient(w->id, 0, sizeof(struct MGinputState));
	if (input == NULL) return NULL;
	input->pos = (struct NVGglyphPosition*)allocTransient(w->id, 1, sizeof(struct NVGglyphPosition)*maxText);
	if (input->pos == NULL) return NULL;
	input->text = (char*)allocTransient(w->id, 2, maxText);
	if (input->text == NULL) return NULL;
	return input;
}

static struct MGinputState* findTransientInput(struct MGwidget* w)
{
	struct MGinputState* input = (struct MGinputState*)findTransient(w->id, 0, sizeof(struct MGinputState));
	if (input == NULL) return NULL;
	input->pos = (struct NVGglyphPosition*)findTransient(w->id, 1, sizeof(struct NVGglyphPosition)*input->maxText);
	if (input->pos == NULL) return NULL;
	input->text = (char*)findTransient(w->id, 2, input->maxText);
	if (input->text == NULL) return NULL;
	return input;
}

static void freeTransientInput(struct MGwidget* w)
{
	freeTransient(w->id, 0);
	freeTransient(w->id, 1);
	freeTransient(w->id, 2);
}

struct MGinputState2
{
	char* text;
	int maxText;
};

static void inputDraw(void* uptr, struct MGwidget* w, struct NVGcontext* vg, const float* view)
{
	struct MGinputState2* state = (struct MGinputState2*)uptr;
	struct MGinputState* input = NULL;
	char* text;
	int j;

	drawRect(w);
//	if (debug) drawDebugRect(w);

	if (state == NULL) return;
	if (state->text == NULL) return;

	nvgScissor(vg, (int)view[0], (int)view[1], (int)view[2], (int)view[3]);
	drawText(w, state->text);

	input = findTransientInput(w);

	if (input != NULL && input->maxText > 0) {
//							printf("input  max=%d i='%s' w='%s' pos=%p npos=%d\n", input->maxText, input->buf, w->text, input->pos, input->npos);
		nvgFillColor(vg, nvgRGBA(255,0,0,64));
		for (j = 0; j < input->npos; j++) {
			struct NVGglyphPosition* p = &input->pos[j];
			nvgBeginPath(vg);
			nvgRect(vg, p->minx, w->y, p->maxx - p->minx, w->height);
			nvgFill(vg);
		}

		if (input->pos != NULL) {
			float caretx = 0;
			if (input->caretPos >= input->npos)
				caretx = input->pos[input->npos-1].maxx;
			else
				caretx = input->pos[input->caretPos].x;
			nvgFillColor(vg, nvgRGBA(255,0,0,255));
			nvgBeginPath(vg);
			nvgRect(vg, (int)(caretx-0.5f), w->y, 1, w->height);
			nvgFill(vg);
		}
	}
}

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

static void inputLogic(void* uptr, struct MGwidget* w, int event, struct MGhit* hit)
{
	struct MGinputState2* state = (struct MGinputState2*)uptr;
	struct MGinputState* input = NULL;
	if (state == NULL || state->text == NULL) return;

	if (event == MG_FOCUSED) {
		input = (struct MGinputState*)allocTransientInput(w, state->maxText);
		if (input == NULL) return;
		memcpy(input->text, state->text, state->maxText);
		input->maxText = state->maxText;
		input->npos = measureTextGlyphs(w, input->text, input->pos, input->maxText);
		input->caretPos = 0;
		printf("%d focused\n", w->id);
	}
	if (event == MG_BLURRED) {
		freeTransientInput(w);
		printf("%d blurred\n", w->id);
	}
	if (event == MG_PRESSED) {
		input = findTransientInput(w);
		if (input == NULL) return;
		input->caretPos = findCaretPos(hit->mx, input->pos, input->npos);
	}
	if (event == MG_DRAGGED) {
		input = findTransientInput(w);
		if (input == NULL) return;
		input->caretPos = findCaretPos(hit->mx, input->pos, input->npos);
	}

/*	switch (event) {
	case MG_FOCUSED:	printf("%08x focused\n", w->id); break;
	case MG_BLURRED:	printf("%08x blurred\n", w->id); break;
	case MG_CLICKED:	printf("%08x clicked\n", w->id); break;
	case MG_PRESSED:	printf("%08x pressed\n", w->id); break;
	case MG_RELEASED:	printf("%08x released\n", w->id); break;
	case MG_DRAGGED:	printf("%08x dragged\n", w->id); break;
	case MG_ENTERED:	printf("%08x entered\n", w->id); break;
	case MG_EXITED:		printf("%08x exited\n", w->id); break;
	}*/
}

unsigned int mgInput(char* text, int maxText, struct MGopt* opts)
{
//	struct MGhit* res = NULL;
	float th;
	struct MGinputState2* state = NULL;
	struct MGwidget* parent = getParent();
	struct MGwidget* w = allocWidget(MG_INPUT);
	if (parent != NULL)
		addChildren(parent, w);

	w->style = computeStyle(getState(w), mgOpts(mgTag("input"), opts), NULL);

	textSize(NULL, w->style.fontSize, NULL, &th);
	w->cwidth = DEFAULT_TEXTW;
	w->cheight = th;
	applySize(w);

	w->render = inputDraw;
	w->logic = inputLogic;

	state = (struct MGinputState2*)allocFrameTemp(sizeof(struct MGinputState2));
	if (state != NULL) {
		memset(state, 0, sizeof(struct MGinputState2));
		state->text = (char*)allocFrameTemp(maxText);
		if (state->text != NULL) {
			state->maxText = maxText;
			memcpy(state->text, text, maxText);
		}
	}

	w->uptr = state;

/*	res = hitResult(w);
	if (getState(w) & MG_FOCUS) {
		struct MGinputState* input = allocTransientInput(w, maxi(20, maxText));
		if (input == NULL) return res;

		if (input->maxText == 0) {
			// The state is not set up yet.
			memcpy(input->buf, text, maxText);
			input->maxText = maxText;
			input->caretPos = 0;
		}
		w->text = input->buf;

//		input->counter++;
//		snprintf(input->buf, input->maxText, "Count %d", input->counter);

		w->uptr = input;

	} else {*/
//		w->text = allocTextLen(text, maxText);
//	}

	return w->id;
}

unsigned int mgNumber(float* value, struct MGopt* opts)
{
	char str[32];
	snprintf(str, sizeof(str), "%.2f", *value);
	str[sizeof(str)-1] = '\0';

	return mgInput(str, sizeof(str), mgOpts(mgTag("number"), mgWidth(DEFAULT_NUMBERW), opts));
}

unsigned int mgNumber3(float* x, float* y, float* z, const char* units, struct MGopt* opts)
{
	mgBoxBegin(MG_ROW, mgOpts(mgTag("number3"), opts));
		mgNumber(x, mgOpts(mgGrow(1)));
		mgNumber(y, mgOpts(mgGrow(1)));
		mgNumber(z, mgOpts(mgGrow(1)));
		if (units != NULL && strlen(units) > 0)
			mgLabel(units, mgOpts());
	return mgBoxEnd();
}

unsigned int mgColor(float* r, float* g, float* b, float* a, struct MGopt* opts)
{
	mgBoxBegin(MG_ROW, mgOpts(mgTag("color"), opts));
		mgLabel("R", mgOpts()); mgNumber(r, mgOpts(mgGrow(1)));
		mgLabel("G", mgOpts()); mgNumber(g, mgOpts(mgGrow(1)));
		mgLabel("B", mgOpts()); mgNumber(b, mgOpts(mgGrow(1)));
		mgLabel("A", mgOpts()); mgNumber(a, mgOpts(mgGrow(1)));
	return mgBoxEnd();
}

unsigned int mgCheckBox(const char* text, int* value, struct MGopt* opts)
{
	unsigned int check = mgBoxBegin(MG_ROW, mgOpts(mgTag("checkbox"), mgAlign(MG_CENTER), mgSpacing(SPACING), mgPaddingY(BUTTON_PADY), mgLogic(MG_CLICK), opts));
		mgText(text, mgOpts(mgTag("label"), mgGrow(1)));
		mgBoxBegin(MG_ROW, mgOpts(mgTag("box"), mgWidth(CHECKBOX_SIZE), mgHeight(CHECKBOX_SIZE)));
			mgIcon(*value ? "check" : NULL, mgOpts(mgTag("tick"), mgPropWidth(1.0f), mgPropHeight(1.0f)));
		mgBoxEnd();
	mgBoxEnd();
	
	if (mgClicked(check))
		*value = !*value;

	return check;
}

unsigned int mgButton(const char* text, struct MGopt* opts)
{
	mgBoxBegin(MG_ROW, mgOpts(mgTag("button"), opts));
		mgText(text, mgOpts());
	return mgBoxEnd();
}

unsigned int mgIconButton(const char* icon, const char* text, struct MGopt* opts)
{
	mgBoxBegin(MG_ROW, mgOpts(mgTag("button"), opts));
		mgIcon(icon, mgOpts());
		mgText(text, mgOpts());
	return mgBoxEnd();
}

unsigned int mgItem(const char* text, struct MGopt* opts)
{
	mgBoxBegin(MG_ROW, mgOpts(mgTag("item"), opts));
		mgText(text, mgOpts(mgGrow(1)));
	return mgBoxEnd();
}

unsigned int mgLabel(const char* text, struct MGopt* opts)
{
	return mgText(text, mgOpts(mgTag("label"), opts));
}

unsigned int mgSelect(int* value, const char** choices, int nchoises, struct MGopt* opts)
{
	int i;
	unsigned int button = 0;

	button = mgBoxBegin(MG_ROW, mgOpts(mgTag("select"), opts));
		mgText(choices[*value], mgOpts(mgGrow(1)));

//		mgIcon(CHECKBOX_SIZE, CHECKBOX_SIZE, mgOpts());
		mgIcon("arrow-combo", mgOpts(mgTag("arrow")));

	mgBoxEnd();
	mgPopupBegin(button, MG_ACTIVE, MG_COL, mgOpts(mgAlign(MG_JUSTIFY)));
		for (i = 0; i < nchoises; i++)
			mgItem(choices[i], mgOpts());
	mgPopupEnd();

	return button;
}

struct MGpopupState {
	int show;
	int counter;
	int hover;
	int trigger;
	float x, y;
};

unsigned int mgPopupBegin(unsigned int target, int trigger, int dir, struct MGopt* opts)
{
	struct MGwidget* w = NULL;
	struct MGpopupState* popup = NULL;
	struct MGwidget* tgt = findWidget(target);
	int show = 0;

	if (tgt == NULL) return 0;

	pushId(state.panelCount+1);

	w = allocWidget(MG_BOX);
	w->parent = tgt;

	popup = (struct MGpopupState*)allocTransient(w->id, 0, sizeof(struct MGpopupState));
	if (popup != NULL) {
//		if (hit != NULL) {
//			if (hit->clicked) {
		popup->trigger = trigger;
		if (tgt != NULL) {
			if (trigger == MG_HOVER) {
				if ((getState(tgt) & MG_HOVER) || popup->hover) {
					popup->counter++;
				} else {
					popup->counter--;
//					if (popup->show & popup->closeCounter == 0)
//						popup->closeCounter = 2;
				}
			}			
			if (trigger == MG_ACTIVE) {
				if ((getState(tgt) & MG_ACTIVE)) {
					popup->counter = 2;
				}
			}
		}

		if (popup->counter > 2) popup->counter = 2;
		if (popup->counter < 0) popup->counter = 0;

		if (popup->show) {
			if (trigger == MG_HOVER) {
				if (state.released) {
					popup->counter = 0;
				}
			}
			if (trigger == MG_ACTIVE) {
				// close on second release, the first will come from the activation
				if (state.released) {
					popup->counter--;
				}
			}
		}

		popup->hover = 0;

		if (popup->show == 0) {
			if (popup->counter == 2)
				popup->show = 1;
		} else {
			if (popup->counter == 0)
				popup->show = 0;
		}

		popup->x = 0;
		popup->y = 0;

		show = popup->show;
		w->x = popup->x;
		w->y = popup->y;
	}

	opts = mgOpts(mgTag("popup"), opts);

	w->active = show;
	w->bubble = 0;
	w->dir = dir;
	w->style = computeStyle(getState(w), opts, NULL);

	pushBox(w);
	pushTag(getTag(opts));

	addPanel(w, 10000);

	return w->id;
}

unsigned int mgPopupEnd()
{
	struct MGwidget* w = NULL;
	popId();
	popTag();
	w = popBox();
	if (w != NULL) {
		struct MGpopupState* popup = (struct MGpopupState*)allocTransient(w->id, 0, sizeof(struct MGpopupState));
		if (popup != NULL) {
			if (popup->show) {
				if (popup->trigger == MG_HOVER) {
					if (getChildState(w) & MG_HOVER) {
						popup->hover = 1;
					}
				}
			}
		}
		layoutPanel(w);
		return w->id;
	}

	return 0;
}

int mgClicked(unsigned int id)
{
	return state.clicked == id ? 1 : 0;
}

int mgPressed(unsigned int id)
{
	return state.pressed == id ? 1 : 0;
}

int mgReleased(unsigned int id)
{
	return state.released == id ? 1 : 0;
}

int mgActive(unsigned int id)
{
	return state.active == id ? 1 : 0;
}

int mgHover(unsigned int id)
{
	return state.hover == id ? 1 : 0;
}
