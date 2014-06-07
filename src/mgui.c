#include "mgui.h"
#include "nanovg.h"
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include "nanosvg.h"

static int mini(int a, int b) { return a < b ? a : b; }
static int maxi(int a, int b) { return a > b ? a : b; }
static int clampi(int a, int mn, int mx) { return a < mn ? mn : (a > mx ? mx : a); }
static float minf(float a, float b) { return a < b ? a : b; }
static float maxf(float a, float b) { return a > b ? a : b; }
static float clampf(float a, float mn, float mx) { return a < mn ? mn : (a > mx ? mx : a); }
static float absf(float a) { return a < 0.0f ? -a : a; }


static struct MGwidget* findWidget(unsigned int id);


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


#define INPUTTEMP_POOL_SIZE 8000
static unsigned char inputTempPool[INPUTTEMP_POOL_SIZE];
static int inputTempPoolSize = 0;

static void* allocInputTemp(int size)
{
	if (inputTempPoolSize + size >= INPUTTEMP_POOL_SIZE)
		return NULL;
	void* dst = &inputTempPool[inputTempPoolSize]; 
	inputTempPoolSize += size;
	return dst;
}

#define OUTPUTTEMP_POOL_SIZE 8000
static unsigned char outputTempPool[OUTPUTTEMP_POOL_SIZE*2];
static int outputTempPoolSize = 0;

static void* allocOutputTemp(int size)
{
	if (outputTempPoolSize + size >= OUTPUTTEMP_POOL_SIZE)
		return NULL;
	void* dst = &outputTempPool[outputTempPoolSize]; 
	outputTempPoolSize += size;
	return dst;
}

struct MGoutputResult {
	unsigned int id;
	union {
		void* ptrval;
		int ival;
		float fval;
	};
	int size;
};

#define OUTPUTRES_POOL_SIZE 64
struct MGoutputResult outputResPool[OUTPUTRES_POOL_SIZE];
static int outputResPoolSize = 0;

static void mgSetResultBlock(unsigned int id, void* data, int size)
{
	void* buf;
	if (outputResPoolSize + 1 >= OUTPUTRES_POOL_SIZE) return;
	buf = (char*)allocOutputTemp(size);
	if (buf == NULL) return;
	memcpy(buf, data, size);
	outputResPool[outputResPoolSize].id = id;
	outputResPool[outputResPoolSize].ptrval = buf;
	outputResPool[outputResPoolSize].size = size;
	outputResPoolSize++;
}

static void mgSetResultStr(unsigned int id, char* str, int size)
{
	mgSetResultBlock(id, (void*)str, size);
}

static void mgSetResultInt(unsigned int id, int data)
{
	if (outputResPoolSize + 1 >= OUTPUTRES_POOL_SIZE) return;
	outputResPool[outputResPoolSize].id = id;
	outputResPool[outputResPoolSize].ival = data;
	outputResPoolSize++;
}

static void mgSetResultFloat(unsigned int id, float data)
{
	if (outputResPoolSize + 1 >= OUTPUTRES_POOL_SIZE) return;
	outputResPool[outputResPoolSize].id = id;
	outputResPool[outputResPoolSize].fval = data;
	outputResPoolSize++;
}

static int mgGetResultBlock(unsigned int id, void** val, int* size)
{
	int i;
	for (i = 0; i < outputResPoolSize; i++) {
		if (outputResPool[i].id == id) {
			if (val != NULL) *val = outputResPool[i].ptrval;
			if (size != NULL) *size = outputResPool[i].size;
			return 1;
		}
	}
	return 0;
}

static int mgGetResultStr(unsigned int id, char** text, int* size)
{
	return mgGetResultBlock(id, (void**)text, size);
}

static int mgGetResultInt(unsigned int id, int* val)
{
	int i;
	for (i = 0; i < outputResPoolSize; i++) {
		if (outputResPool[i].id == id) {
			*val = outputResPool[i].ival;
			return 1;
		}
	}
	return 0;
}

static int mgGetResultFloat(unsigned int id, float* val)
{
	int i;
	for (i = 0; i < outputResPoolSize; i++) {
		if (outputResPool[i].id == id) {
			*val = outputResPool[i].fval;
			return 1;
		}
	}
	return 0;
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
	struct MGstyle normal;
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


static char* cpToUTF8(int cp, char* str)
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
#define MG_STATE_POOL_SIZE 4096

struct MGstateHeader {
	unsigned int id;
	int size;
	short num;
};

struct MGidRange {
	unsigned int base, count;
};

struct MGcontext
{
	struct MGinputState input;
	float startmx, startmy;
	float deltamx, deltamy;
	int moved;
	int drag;
	int clickCount;

	float timeSincePress;
	float dt;

	unsigned int forceFocus;
	unsigned int focusNext;
	unsigned int focusPrev;
	unsigned int forceBlur;

	unsigned int active;
	unsigned int hover;
	unsigned int focus;

	unsigned int clicked;
	unsigned int pressed;
	unsigned int dragged;
	unsigned int released;
	unsigned int blurred;
	unsigned int focused;
	unsigned int entered;
	unsigned int exited;

	struct MGwidget* boxStack[MG_BOX_STACK_SIZE];
	int boxStackCount;

	struct MGidRange idStack[MG_ID_STACK_SIZE];
	int idStackCount;

	struct MGwidget* panels[MG_MAX_PANELS];
	int panelsz[MG_MAX_PANELS];
	int panelCount;

	const char* tags[MG_MAX_TAGS];
	int tagCount;

	unsigned char stateMem[MG_STATE_POOL_SIZE];
	int stateMemSize;
	int stateCount;

	struct NVGcontext* vg;

	int width, height;

	struct MGhit hoverHit;
	struct MGhit activeHit;
};

static struct MGcontext context;

static int mgGetStateBlock(unsigned int id, int num, void** ptr, int* size)
{	
	int i;
	unsigned char* mem = context.stateMem;
	for (i = 0; i < context.stateCount; i++) {
		struct MGstateHeader* state = (struct MGstateHeader*)mem;
		int stateSize = sizeof(struct MGstateHeader) + state->size;
		if (state->id == id && state->num == num) {
//			printf("mgGetStateBlock %d/%d %d==%d\n", id, num, state->size, size);
			if (ptr != NULL) *ptr = mem + sizeof(struct MGstateHeader);
			if (size != NULL) *size = state->size;
			return 1;
		}
		mem += stateSize;
	}
	return 0;
}

static void mgFreeStateBlock(unsigned int id, short num)
{	
	int i;
	unsigned char* mem = context.stateMem;
	for (i = 0; i < context.stateCount; i++) {
		struct MGstateHeader* state = (struct MGstateHeader*)mem;
		int stateSize = sizeof(struct MGstateHeader) + state->size;
		if (state->id == id && state->num == num) {
			// Mark for delete
//			printf("mgFreeStateBlock free %d/%d\n", id, num);
			state->id = 0;
			return;
		}
		mem += stateSize;
	}
}

static int mgAllocStateBlock(unsigned int id, int num, void** ptr, int size)
{
	int i;
	struct MGstateHeader* state;
	unsigned char* mem;
	int allocSize = sizeof(struct MGstateHeader) + size;

	mem = context.stateMem;
	for (i = 0; i < context.stateCount; i++) {
		struct MGstateHeader* state = (struct MGstateHeader*)mem;
		int stateSize = sizeof(struct MGstateHeader) + state->size;
		if (state->id == id && state->num == num) {
			if (state->size == size) {
				// Return if same size
				if (ptr != NULL) *ptr = mem + sizeof(struct MGstateHeader);
				return 1;
			} else {
				// Mark for delete and allow new if wrong size.
				state->id = 0;
			}
			break;
		}
		mem += stateSize;
	}

	if ((context.stateMemSize + allocSize) > MG_STATE_POOL_SIZE) {
		printf("state pool exhausted!\n");
		return 0;
	}

	// Allocate new block
	mem = &context.stateMem[context.stateMemSize];
	context.stateMemSize += allocSize; 
	memset(mem, 0, allocSize);
	state = (struct MGstateHeader*)mem;
	state->id = id;
	state->num = num;
	state->size = size;

	context.stateCount++;

	if (ptr != NULL) *ptr = mem + sizeof(struct MGstateHeader);

	return 1;
}


static void garbageCollectStates()
{
	int i, n = 0, size = 0;
	struct MGstateHeader* state;
	unsigned char* ptr;
	unsigned char* tail;

	// Check if the transient exists.
	ptr = tail = context.stateMem;

	for (i = 0; i < context.stateCount; i++) {
		state = (struct MGstateHeader*)ptr;
		size = sizeof(struct MGstateHeader) + state->size;
		if (state->id != 0 && findWidget(state->id) != NULL) {
			memmove(tail, ptr, size);
			tail += size;
			n++;
		}
		ptr += size;
	}

	context.stateMemSize = (int)(tail - context.stateMem);
	context.stateCount = n;
}


static int mgGetValueBlock(unsigned int id, void** val, int* size)
{
	struct MGwidget* w = findWidget(id);
	if (w == NULL) { return 0; }
	if (w->uptr == NULL) return 0;
	if (w->uptrsize == 0) return 0;
	if (val != NULL) *val = w->uptr;
	if (size != NULL) *size = w->uptrsize;

//	printf("get value block %d: %p size=%d\n", id, w->uptr, w->uptrsize);

	return 1;
}

static int mgSetValueBlock(unsigned int id, void* val, int size)
{
	void* buf;
	struct MGwidget* w = findWidget(id);
	if (w == NULL) return 0;
	buf = allocInputTemp(size);
	if (buf == NULL) return 0;
	memcpy(buf, val, size);
	w->uptr = buf;
	w->uptrsize = size;

//	printf("set value block %d: %p size=%d\n", id, w->uptr, w->uptrsize);

	return 1;
}

static int mgGetValueStr(unsigned int id, char** text, int* maxText)
{
	return mgGetValueBlock(id, (void**)text, maxText);
}

static int mgSetValueStr(unsigned int id, char* text, int maxText)
{
	return mgSetValueBlock(id, (void*)text, maxText);
}

static int mgGetValueInt(unsigned int id, int* val)
{	
	int* ptr;
	if (!mgGetValueBlock(id, (void**)&ptr, NULL)) return 0;
	*val = *ptr;
	return 1;
}

static int mgSetValueInt(unsigned int id, int val)
{
	return mgSetValueBlock(id, &val, sizeof(int));
}

static int mgGetValueFloat(unsigned int id, float* val)
{
	float* ptr;
	if (!mgGetValueBlock(id, (void**)&ptr, NULL)) return 0;
	*val = *ptr;
	return 1;
}

static int mgSetValueFloat(unsigned int id, float val)
{
	return mgSetValueBlock(id, &val, sizeof(float));
}



static void addPanel(struct MGwidget* w, int zidx)
{
	if (context.panelCount < MG_MAX_PANELS) {
		int i, z = zidx + context.panelCount, idx = 0;
		while (idx < context.panelCount && context.panelsz[idx] < z)
			idx++;
		for (i = context.panelCount; i >= idx; i--) {
			context.panels[i] = context.panels[i-1];
			context.panelsz[i] = context.panelsz[i-1];
		}
		context.panels[idx] = w;
		context.panelsz[idx] = z;
		context.panelCount++;
	}
}

static void pushBox(struct MGwidget* w)
{
	if (context.boxStackCount < MG_BOX_STACK_SIZE)
		context.boxStack[context.boxStackCount++] = w;
}

static struct MGwidget* popBox()
{
	if (context.boxStackCount > 0)
		return context.boxStack[--context.boxStackCount];
	return NULL;
}

static void pushId(int base)
{
	if (context.idStackCount+1 < MG_ID_STACK_SIZE) {
		context.idStack[context.idStackCount].base = base;
		context.idStack[context.idStackCount].count = 0;
		context.idStackCount++;
	}
}

static void popId()
{
	if (context.idStackCount > 0) {
		context.idStackCount--;
	}
}

static void pushTag(const char* tag)
{
	if (context.tagCount < MG_MAX_TAGS)
		context.tags[context.tagCount++] = tag;
}

static void popTag()
{
	if (context.tagCount > 0)
		context.tagCount--;
}

static struct MGwidget* getParent()
{
	if (context.boxStackCount == 0) return NULL;
	return context.boxStack[context.boxStackCount-1];
}

static int genId()
{
	unsigned int id = 0;
	if (context.idStackCount > 0) {
		int idx = context.idStackCount-1;
		id |= context.idStack[idx].base << 16;
		id |= context.idStack[idx].count;
		context.idStack[idx].count++;
	}
	return id;
}

static void addChildren(struct MGwidget* parent, struct MGwidget* w)
{
	struct MGwidget** prev = NULL;
	if (parent == NULL) return;
	prev = &parent->children;
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
   return context.input.mx >= x && context.input.mx <= x+w && context.input.my >= y && context.input.my <= y+h;
}

int mgInit()
{
	memset(&context, 0, sizeof(context));

	widgetPoolSize = 0;
	stylePoolSize = 0;
	optPoolSize = 0;
	inputTempPoolSize = 0;

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
			mgHeight(SLIDER_HANDLE)
/*			mgFillColor(220,220,220,255),
			mgBorderSize(2.0f),
			mgContentColor(220,220,220,255)*/
		),
		// Hover
		mgOpts(
//			mgFillColor(255,255,255,255)
		),
		// Active
		mgOpts(
/*			mgFillColor(32,32,32,255),
			mgBorderColor(255,255,255,255)*/
		),
		// Focus
		mgOpts(
//			mgBorderColor(0,192,255,255)
		)
	);

	mgCreateStyle("slider-canvas.slot",
		mgOpts(
			mgHeight(2),
			mgFillColor(0,0,0,128)
		),
		mgOpts(
			mgHeight(4)
		),
		mgOpts(
			mgHeight(4)
		),
		mgOpts()
	);

	mgCreateStyle("slider-canvas.bar",
		// Normal
		mgOpts(
//			mgPaddingX(SLIDER_HANDLE/2),
			mgHeight(2), //SLIDER_HANDLE),
//			mgOverflow(MG_VISIBLE),
			mgFillColor(220,220,220,255)
		),
		// Hover
		mgOpts(
			mgHeight(4), //SLIDER_HANDLE),
			mgFillColor(255,255,255,255)
		),
		// Active
		mgOpts(
			mgHeight(4), //SLIDER_HANDLE),
			mgFillColor(255,192,0,255)
		),
		// Focus
		mgOpts(
		)
	);

	mgCreateStyle("slider-canvas.handle",
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
			mgFillColor(32,32,32,255),
			mgBorderColor(255,255,255,255)
//			mgBorderColor(255,255,255,255),
//			mgFillColor(32,32,32,255)
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
//			mgOverflow(MG_VISIBLE),
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

	mgCreateStyle("tooltip",
		// Normal
		mgOpts(
			mgPropPosition(MG_CENTER,MG_END,0.5f,0.0f),
			mgLogic(MG_CLICK),
			mgFillColor(32,32,32,220),
			mgPadding(5,5)
		),
		// Hover, active, focus
		mgOpts(), mgOpts(), mgOpts()
	);
	mgCreateStyle("tooltip.label",
		// Normal
		mgOpts(
			mgFontSize(LABEL_SIZE),
			mgAlign(MG_START),
			mgSpacing(LABEL_SPACING),
			mgContentColor(220,220,220,220)
		),
		// Hover, active, focus
		mgOpts(), mgOpts(), mgOpts()
	);

//	mgBox(mgOpts(mgPropPosition(MG_JUSTIFY,MG_CENTER,0,0.5f), mgTag("slot"), mgPropWidth(1.0f)));

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

void mgFrameBegin(struct NVGcontext* vg, int width, int height, struct MGinputState* input, float dt)
{
	context.moved = absf(context.input.mx - input->mx) > 0.01f || absf(context.input.my - input->my) > 0.01f;
	memcpy(&context.input, input, sizeof(*input));

	context.width = width;
	context.height = height;

	context.dt = dt;

	context.vg = vg;

	context.boxStackCount = 0;
	context.panelCount = 0;
	context.tagCount = 0;

	context.idStackCount = 1;
	context.idStack[0].base = 0;
	context.idStack[0].count = 0;

	widgetPoolSize = 0;
	optPoolSize = 0;
	inputTempPoolSize = 0;
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

	for (w = box->children; w != NULL; w = w->next) {

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
		case MG_POPUP:
		case MG_PANEL:
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

/*
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

	for (w = box->children; w != NULL; w = w->next) {
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
*/

/*
static void dumpId(struct MGwidget* box, int indent)
{
	struct MGwidget* w = NULL;
	printf("%*sbox %d\n", indent, "", box->id);
	for (w = box->children; w != NULL; w = w->next) {
		if  (w->type == MG_BOX)
			dumpId(w, indent+2);
		else
			printf("%*swid %d\n", indent+2, "", w->id);
	}
} 
*/

static void getAdjacentStops(struct MGwidget* box, unsigned int id, unsigned int* prev, unsigned int* next, int* state, int indent)
{
	int i;
	struct MGwidget* w = NULL;
	for (w = box->children; w != NULL; w = w->next)
		getAdjacentStops(w, id, prev, next, state, indent+1);
	if (box->stop) {
		if (box->id == id) {
			(*state)++;
		} else if ((*state) == 0) {
			*prev = box->id;
		} else if ((*state) == 1) {
			*next = box->id;
			(*state)++;
			return;
		}
	}
} 

static void fireLogic(unsigned int id, int event, struct MGhit* hit)
{
	struct MGwidget* w = NULL;
	if (id == 0) return;
	w = findWidget(id);
	if (w == NULL) return;
	if (w->logic == NULL) return;
	hit->localmx = context.input.mx - w->x;
	hit->localmy = context.input.my - w->y;
	w->logic(w->uptr, w, event, hit);
}

static void innerBounds(struct MGwidget* w, struct MGrect* b)
{
	float padx = minf(w->style.paddingx, maxf(0.0f, w->width));
	float pady = minf(w->style.paddingy, maxf(0.0f, w->height));
	b->x = w->x + padx;
	b->y = w->y + pady;
	b->width = maxf(0.0f, w->width - padx*2);
	b->height = maxf(0.0f, w->height - pady*2);
}

static void setHit(unsigned int id, struct MGhit* hit)
{
	struct MGwidget* w = findWidget(id);
	if (w == NULL) return;

	hit->rect.x = w->x;
	hit->rect.y = w->y;
	hit->rect.width = w->width;
	hit->rect.height = w->height;

	if (w->parent != NULL) {
		innerBounds(w->parent, &hit->view);
	} else {
		hit->rect.x = 0;
		hit->rect.y = 0;
		hit->rect.width =  context.width;
		hit->rect.height = context.height;
	}

	hit->style = w->style;
}

static void updateLogic(const float* bounds)
{
	int i;
	struct MGwidget* hit = NULL;
//	struct MGwidget* active = NULL;
//	struct MGwidget* w = NULL;
	int deactivate = 0;

/*	printf("---\n");
	for (i = 0; i < context.panelCount; i++)
		dumpId(context.panels[i], 0);*/

	for (i = 0; i < context.panelCount; i++) {
		if (context.panels[i]->active) {
			struct MGwidget* child = hitTest(context.panels[i], bounds);
			if (child != NULL)
				hit = child;
		}
	}

	if (context.input.mbut & MG_MOUSE_PRESSED) {
		if (context.timeSincePress < 0.5f)
			context.clickCount++;
		else
			context.clickCount = 1;
		context.timeSincePress = 0;
	} else {
		context.timeSincePress += minf(context.dt, 0.1f);
	}

//	context.hover = 0;
	context.focused = 0;
	context.blurred = 0;
	context.entered = 0;
	context.exited = 0;
	context.clicked = 0;
	context.pressed = 0;
	context.dragged = 0;
	context.released = 0;

	if (context.active == 0) {
		unsigned int id = hit != NULL ? hit->id : 0;		
		if (context.hover != id) {
			context.exited = context.hover;
			context.entered = id;
			context.hover = id;
		}
		if (context.input.mbut & MG_MOUSE_PRESSED) {
			if (context.focus != id) {
				context.blurred = context.focus;
				context.focused = id;
			}
			context.focus = id;
			context.active = id;
			context.pressed = id;
		} else {

			if (context.focusNext != 0) {
				int state = 0;
				unsigned int next = 0, prev = 0;
				for (i = 0; i < context.panelCount; i++) {
					if (context.panels[i]->active)
						getAdjacentStops(context.panels[i], context.focusNext, &prev, &next, &state, 0);
				}
				if (next != 0)
					context.forceFocus = next;
				context.focusNext = 0;
			}

			if (context.focusPrev != 0) {
				int state = 0;
				unsigned int next = 0, prev = 0;
				for (i = 0; i < context.panelCount; i++) {
					if (context.panels[i]->active)
						getAdjacentStops(context.panels[i], context.focusPrev, &prev, &next, &state, 0);
				}
				if (prev != 0)
					context.forceFocus = prev;
				context.focusPrev = 0;
			}

			if (context.forceFocus != 0) {
				if (context.focus != context.forceFocus) {
					context.blurred = context.focus;
					context.focused = context.forceFocus;
				}
				context.focus = context.forceFocus;
				context.forceFocus = 0;
			}
			if (context.forceBlur != 0) {
				if (context.focus != 0) {
					context.blurred = context.focus;
					context.focused = 0;
				}
				context.focus = 0;
				context.forceBlur = 0;
			}
		}
	}
	// Press and release can happen in same frame.
	if (context.active != 0) {
		unsigned int id = hit != NULL ? hit->id : 0;
		if (id == 0 || id == context.active) {
			if (context.hover != id) {
				context.exited = context.hover;
				context.entered = id;
				context.hover = id;
			}
//			context.hover = hit->id;
		}
		if (context.input.mbut & MG_MOUSE_RELEASED) {
			if (context.hover == context.active)
				context.clicked = context.hover;
			context.released = context.active;
			deactivate = 1;
		} else {
			if (context.moved)
				context.dragged = context.active;
		}
	}

/*	for (i = 0; i < context.panelCount; i++) {
		struct MGwidget* child = updateState(context.panels[i], context.hover, context.active, context.focus, 0);
		if (child != NULL)
			active = child;
	}*/

	// Post pone deactivation so that we get atleast one frame of active state if mouse press/release during one frame.
	if (deactivate)
		context.active = 0;

	// Update mouse positions.
	if (context.input.mbut & MG_MOUSE_PRESSED) {
		context.startmx = context.input.mx;
		context.startmy = context.input.my;
		context.drag = 1;
	}
	if (context.drag) {
		context.deltamx = context.input.mx - context.startmx;
		context.deltamy = context.input.my - context.startmy;
	} else {
		context.deltamx = context.deltamy = 0;
	}
	if (context.input.mbut & MG_MOUSE_RELEASED) {
		context.startmx = context.startmy = 0;
		context.drag = 0;
	}

	memset(&context.hoverHit, 0, sizeof(context.hoverHit));
	memset(&context.activeHit, 0, sizeof(context.hoverHit));

	context.hoverHit.code = 0;
	context.hoverHit.mx = context.input.mx;
	context.hoverHit.my = context.input.my;
	context.hoverHit.deltamx = context.deltamx;
	context.hoverHit.deltamy = context.deltamy;

	context.activeHit.code = 0;
	context.activeHit.mx = context.input.mx;
	context.activeHit.my = context.input.my;
	context.activeHit.deltamx = context.deltamx;
	context.activeHit.deltamy = context.deltamy;
	context.activeHit.clickCount = context.clickCount;

	setHit(context.hover, &context.hoverHit);
	setHit(context.active, &context.activeHit);

	fireLogic(context.blurred, MG_BLURRED, &context.activeHit);
	fireLogic(context.focused, MG_FOCUSED, &context.activeHit);
	fireLogic(context.pressed, MG_PRESSED, &context.activeHit);
	fireLogic(context.dragged, MG_DRAGGED, &context.activeHit);
	fireLogic(context.released, MG_RELEASED, &context.activeHit);
	fireLogic(context.clicked, MG_CLICKED, &context.activeHit);
	fireLogic(context.exited, MG_EXITED, &context.activeHit);
	fireLogic(context.entered, MG_ENTERED, &context.activeHit);

	if (context.focus != 0) {
		for (i = 0; i < context.input.nkeys; i++) {
			context.activeHit.code = context.input.keys[i].code;
			context.activeHit.mods = context.input.keys[i].mods;
			fireLogic(context.focus, context.input.keys[i].type, &context.activeHit);
		}
		context.activeHit.code = 0;
		context.activeHit.mods = 0;
	}
	context.input.nkeys = 0;


/*	context.result.clicked = context.clicked != 0;
	context.result.pressed = context.pressed != 0;
	context.result.dragged = context.dragged != 0;
	context.result.released = context.released != 0;

	if (active != NULL) {
		context.result.bounds[0] = active->x;
		context.result.bounds[1] = active->y;
		context.result.bounds[2] = active->width;
		context.result.bounds[3] = active->height;
		if (active->parent != NULL) {
			context.result.pbounds[0] = active->parent->x;
			context.result.pbounds[1] = active->parent->y;
			context.result.pbounds[2] = active->parent->width;
			context.result.pbounds[3] = active->parent->height;
		} else {
			context.result.pbounds[0] = context.result.pbounds[1] = context.result.pbounds[2] = context.result.pbounds[3] = 0;
		}
	} else {
		context.result.bounds[0] = context.result.bounds[1] = context.result.bounds[2] = context.result.bounds[3] = 0;
		context.result.pbounds[0] = context.result.pbounds[1] = context.result.pbounds[2] = context.result.pbounds[3] = 0;
	}

	if (active != NULL && active->logic != NULL)
		active->logic(active->uptr, active, &context.result);*/
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
/*	nvgBeginPath(context.vg);
	nvgRect(context.vg, w->x + w->style.paddingx+0.5f, w->y + w->style.paddingy+0.5f, w->width - w->style.paddingx*2, w->height - w->style.paddingy*2);
	nvgStrokeWidth(context.vg, 1.0f);
	nvgStrokeColor(context.vg, nvgRGBA(255,0,0,128));
	nvgStroke(context.vg);*/

	nvgBeginPath(context.vg);
	nvgRect(context.vg, w->x + w->style.paddingx+0.5f, w->y + w->style.paddingy+0.5f, w->cwidth, w->cheight);
	nvgStrokeWidth(context.vg, 1.0f);
	nvgStrokeColor(context.vg, nvgRGBA(255,255,0,128));
	nvgStroke(context.vg);
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
		nvgFillColor(context.vg, nvgCol(w->style.contentColor));
		nvgStrokeColor(context.vg, nvgCol(w->style.contentColor));
	}
	sx = w->width / image->width;
	sy = w->height / image->height;
	s = minf(sx, sy);

	nvgSave(context.vg);
	nvgTranslate(context.vg, w->x + w->width/2, w->y + w->height/2);
	nvgScale(context.vg, s, s);
	nvgTranslate(context.vg, -image->width/2, -image->height/2);

	for (shape = image->shapes; shape != NULL; shape = shape->next) {
		struct NSVGpath* path;

		if (shape->fill.type == NSVG_PAINT_NONE && shape->stroke.type == NSVG_PAINT_NONE)
			continue;

		nvgBeginPath(context.vg);
		for (path = shape->paths; path != NULL; path = path->next) {
			nvgMoveTo(context.vg, path->pts[0], path->pts[1]);
			for (i = 1; i < path->npts; i += 3) {
				float* p = &path->pts[i*2];
				nvgBezierTo(context.vg, p[0],p[1], p[2],p[3], p[4],p[5]);
			}
			if (path->closed)
				nvgLineTo(context.vg, path->pts[0], path->pts[1]);
		}

		if (shape->fill.type == NSVG_PAINT_COLOR) {
			if (!override)
				nvgFillColor(context.vg, nvgCol(shape->fill.color));
			nvgFill(context.vg);
//			printf("image %s\n", w->icon.icon->name);
//			nvgDebugDumpPathCache(context.vg);
		}
		if (shape->stroke.type == NSVG_PAINT_COLOR) {
			if (!override)
				nvgStrokeColor(context.vg, nvgCol(shape->stroke.color));
			nvgStrokeWidth(context.vg, shape->strokeWidth);
			nvgStroke(context.vg);
		}
	}

	nvgRestore(context.vg);
}

static void drawRect(float x, float y, float width, float height, struct MGstyle* style)
{
	if (isStyleSet(style, MG_FILLCOLOR_ARG)) {
		nvgBeginPath(context.vg);
		if (isStyleSet(style, MG_CORNERRADIUS_ARG)) {
			float w = maxf(0, width);
			float h = maxf(0, height);
			float r = minf(style->cornerRadius, minf(w, h));
			nvgRoundedRect(context.vg, x, y, w, h, r);
		} else {
			nvgRect(context.vg, x, y, width, height);
		}
		nvgFillColor(context.vg, nvgCol(style->fillColor));
		nvgFill(context.vg);
	}

	if (isStyleSet(style, MG_BORDERCOLOR_ARG)) {
		float s = style->borderSize * 0.5f;
		nvgBeginPath(context.vg);
		if (isStyleSet(style, MG_CORNERRADIUS_ARG)) {
			float w = maxf(0, width-s*2);
			float h = maxf(0, height-s*2);
			float r = minf(style->cornerRadius - s, minf(w, h));
			nvgRoundedRect(context.vg, x+s, y+s, w, h, r);
		} else {
			nvgRect(context.vg, x+s, y+s, width-s*2, height-s*2);
		}
		nvgStrokeWidth(context.vg, style->borderSize);
		nvgStrokeColor(context.vg, nvgCol(style->borderColor));
		nvgStroke(context.vg);
	}
}

static int measureTextGlyphs(struct MGwidget* w, const char* text, struct NVGglyphPosition* pos, int maxpos)
{
	int i, count = 0;
	nvgFontSize(context.vg, w->style.fontSize);
	if (w->style.textAlign == MG_CENTER) {
		nvgTextAlign(context.vg, NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
		count = nvgTextGlyphPositions(context.vg, w->x + w->width/2, w->y + w->height/2, text, NULL, pos, maxpos);
	} else if (w->style.textAlign == MG_END) {
		nvgTextAlign(context.vg, NVG_ALIGN_RIGHT|NVG_ALIGN_MIDDLE);
		count = nvgTextGlyphPositions(context.vg, w->x + w->width - w->style.paddingx, w->y + w->height/2, text, NULL, pos, maxpos);
	} else {
		nvgTextAlign(context.vg, NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);
		count = nvgTextGlyphPositions(context.vg, w->x + w->style.paddingx, w->y + w->height/2, text, NULL, pos, maxpos);
	}
	// Turn str to indices.
	for (i = 0; i < count; i++)
		pos[i].str -= text;

	return count;
}

static void drawText(struct MGwidget* w, const char* text)
{
//	float bounds[4];
//	struct NVGglyphPosition pos[100];
//	int npos = 0, i;

	nvgFillColor(context.vg, nvgCol(w->style.contentColor));
	nvgFontSize(context.vg, w->style.fontSize);
	if (w->style.textAlign == MG_CENTER) {
		nvgTextAlign(context.vg, NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
//		npos = nvgTextGlyphPositions(context.vg, w->x + w->width/2, w->y + w->height/2, w->text, NULL, bounds, pos, 100);
		nvgText(context.vg, w->x + w->width/2, w->y + w->height/2, text, NULL);
	} else if (w->style.textAlign == MG_END) {
		nvgTextAlign(context.vg, NVG_ALIGN_RIGHT|NVG_ALIGN_MIDDLE);
//		npos = nvgTextGlyphPositions(context.vg, w->x + w->width - w->style.paddingx, w->y + w->height/2, w->text, NULL, bounds, pos, 100);
		nvgText(context.vg, w->x + w->width - w->style.paddingx, w->y + w->height/2, text, NULL);
	} else {
		nvgTextAlign(context.vg, NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);
//		npos = nvgTextGlyphPositions(context.vg, w->x + w->style.paddingx, w->y + w->height/2, w->text, NULL, bounds, pos, 100);
		nvgText(context.vg, w->x + w->style.paddingx, w->y + w->height/2, text, NULL);
	}

/*	nvgFillColor(context.vg, nvgRGBA(255,0,0,64));
	for (i = 0; i < npos; i++) {
		struct NVGglyphPosition* p = &pos[i];
		nvgBeginPath(context.vg);
		nvgRect(context.vg, p->x, bounds[1], p->width, bounds[3]-bounds[1]);
		nvgFill(context.vg);
	}*/
}

static void drawParagraph(struct MGwidget* w)
{
	float x = w->x + w->style.paddingx;
	float y = w->y + w->style.paddingy;
	float width = maxf(0.0f, w->width - w->style.paddingx*2);
	if (width < 1.0f) return;
	nvgFillColor(context.vg, nvgCol(w->style.contentColor));
	nvgFontSize(context.vg, w->style.fontSize);
	nvgTextLineHeight(context.vg, w->style.lineHeight > 0 ? w->style.lineHeight : 1);
	if (w->style.textAlign == MG_CENTER)
		nvgTextAlign(context.vg, NVG_ALIGN_CENTER|NVG_ALIGN_TOP);
	else if (w->style.textAlign == MG_END)
		nvgTextAlign(context.vg, NVG_ALIGN_RIGHT|NVG_ALIGN_TOP);
	else
		nvgTextAlign(context.vg, NVG_ALIGN_LEFT|NVG_ALIGN_TOP);
	nvgTextBox(context.vg, x, y, width, w->text, NULL);
}

static void drawBox(struct MGwidget* box, const float* bounds)
{
	int i;
	struct MGwidget* w;
	float bbounds[4];
	float wbounds[4];
	int debug = 0;

	nvgFontFace(context.vg, "sans");
	nvgFontSize(context.vg, TEXT_SIZE);

	nvgScissor(context.vg, (int)bounds[0], (int)bounds[1], (int)bounds[2], (int)bounds[3]);

	drawRect(box->x, box->y, box->width, box->height, &box->style);

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
		for (w = box->children; w != NULL; w = w->next) {
			unsigned char anchor = isStyleSet(&w->style, MG_ANCHOR_ARG);
			// draw anchored last
			if ((i == 0 && anchor) || (i == 1 && !anchor))
				continue;

			if (!visible(bbounds, w->x, w->y, w->width, w->height))
				continue;

			nvgScissor(context.vg, (int)bbounds[0], (int)bbounds[1], (int)bbounds[2], (int)bbounds[3]);

			switch (w->type) {
			case MG_BOX:
			case MG_PANEL:
			case MG_POPUP:
				drawBox(w, bbounds);
				break;

			case MG_TEXT:
				drawRect(w->x, w->y, w->width, w->height, &w->style);
				if (debug) drawDebugRect(w);
				isectBounds(wbounds, bbounds, w->x, w->y, w->width, w->height);
				if (wbounds[2] > 0.0f && wbounds[3] > 0.0f) {
					nvgScissor(context.vg, (int)wbounds[0], (int)wbounds[1], (int)wbounds[2], (int)wbounds[3]);
					drawText(w, w->text);
				}
				break;

			case MG_PARAGRAPH:
				drawRect(w->x, w->y, w->width, w->height, &w->style);
				if (debug) drawDebugRect(w);
				isectBounds(wbounds, bbounds, w->x, w->y, w->width, w->height);
				if (wbounds[2] > 0.0f && wbounds[3] > 0.0f) {
					nvgScissor(context.vg, (int)wbounds[0], (int)wbounds[1], (int)wbounds[2], (int)wbounds[3]);
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
					nvgScissor(context.vg, (int)wbounds[0], (int)wbounds[1], (int)wbounds[2], (int)wbounds[3]);
					drawText(w);
					if (w->uptr != NULL) {
						struct MGinputState* input = allocStateInput(w, 0);
						int j;
						if (input != NULL && input->maxText > 0) {
							input->npos = measureTextGlyphs(w, input->pos, input->maxText);
//							printf("input  max=%d i='%s' w='%s' pos=%p npos=%d\n", input->maxText, input->buf, w->text, input->pos, input->npos);
							nvgFillColor(context.vg, nvgRGBA(255,0,0,64));
							for (j = 0; j < input->npos; j++) {
								struct NVGglyphPosition* p = &input->pos[j];
								nvgBeginPath(context.vg);
								nvgRect(context.vg, p->x, w->y, p->width, w->height);
								nvgFill(context.vg);
							}
						}
					}
				}*/

				isectBounds(wbounds, bbounds, w->x, w->y, w->width, w->height);
				if (w->render != NULL && wbounds[2] > 0.0f && wbounds[3] > 0.0f) {
					w->render(w->uptr, w, context.vg, wbounds);
				}
				if (debug) drawDebugRect(w);
				break;

			case MG_CANVAS:
				isectBounds(wbounds, bbounds, w->x, w->y, w->width, w->height);
				if (w->render != NULL && wbounds[2] > 0.0f && wbounds[3] > 0.0f) {
					w->render(w->uptr, w, context.vg, wbounds);
				}
				if (debug) drawDebugRect(w);
				break;
			}
		}
	}

	if (box->style.overflow == MG_SCROLL) {
		nvgScissor(context.vg, bbounds[0], bbounds[1], bbounds[2], bbounds[3]);
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
				nvgBeginPath(context.vg);
				nvgRect(context.vg, x, y, w, h);
				nvgFillColor(context.vg, nvgRGBA(0,0,0,64));
				nvgFill(context.vg);
				nvgBeginPath(context.vg);
				nvgRect(context.vg, x2, y, w2, h);
				nvgFillColor(context.vg, nvgRGBA(0,0,0,255));
				nvgFill(context.vg);
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
				nvgBeginPath(context.vg);
				nvgRect(context.vg, x, y, w, h);
				nvgFillColor(context.vg, nvgRGBA(0,0,0,64));
				nvgFill(context.vg);
				nvgBeginPath(context.vg);
				nvgRect(context.vg, x, y2, w, h2);
				nvgFillColor(context.vg, nvgRGBA(0,0,0,255));
				nvgFill(context.vg);
			}
		}
	}
} 

static void drawPanels(const float* bounds)
{
	int i;
	if (context.vg == NULL) return;	
	for (i = 0; i < context.panelCount; i++) {
		if (context.panels[i]->active)
			drawBox(context.panels[i], bounds);
	}
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

static void offsetWidget(struct MGwidget* w, float dx, float dy)
{
	struct MGwidget* c = NULL;
	w->x += dx;
	w->y += dy;
	for (c = w->children; c != NULL; c = c->next)
		offsetWidget(c, dx, dy);
} 

static void offsetPopups()
{	
	int i;
	if (context.vg == NULL) return;	
	for (i = 0; i < context.panelCount; i++) {
		struct MGwidget* w = context.panels[i];
		if (!w->active) continue;

		if (w->type == MG_POPUP) {
			struct MGwidget* root = w->parent;
			float dx = w->parent->x + root->width;
			float dy = w->parent->y; // + w->parent->height;
			float rw = root->width;
			float rh = root->height;

			if (isStyleSet(&w->style, MG_ANCHOR_ARG)) {
				unsigned char ax = w->style.anchor & 0xf;
				unsigned char ay = (w->style.anchor >> 4) & 0xf;

				if (isStyleSet(&w->style, MG_PROPX_ARG))
					dx = root->x + calcPropDelta(ax, w->style.x, rw, w->width);
				else
					dx = root->x + calcPosDelta(ax, w->style.x, rw, w->width);

				if (isStyleSet(&w->style, MG_PROPY_ARG))
					dy = root->y + calcPropDelta(ay, w->style.y, rh, w->height);
				else
					dy = root->y + calcPosDelta(ay, w->style.y, rh, w->height);
			}

			offsetWidget(w, dx,dy);
		}
	}
}


void mgFrameEnd()
{
	float bounds[4] = {0, 0, context.width, context.height};

	offsetPopups();

	outputResPoolSize = 0;
	outputTempPoolSize = 0;

	updateLogic(bounds);
	drawPanels(bounds);

	// cleanup unused states
	garbageCollectStates();
}

static void textSize(const char* str, float size, float* w, float* h)
{
	float tw, th;
	if (context.vg == NULL) {
		*w = *h = 0;
		return;
	}
	nvgFontFace(context.vg, "sans");
	nvgFontSize(context.vg, size);
	tw = str != NULL ? nvgTextBounds(context.vg, 0,0, str, NULL, NULL) : 0;
	nvgTextMetrics(context.vg, NULL, NULL, &th);
	if (w) *w = tw;
	if (h) *h = th;
}


static void paragraphSize(const char* str, float size, float lineh, float maxw, float* w, float* h)
{
	float bounds[4];
	if (context.vg == NULL) {
		*w = *h = 0;
		return;
	}
	nvgFontFace(context.vg, "sans");
	nvgFontSize(context.vg, size);
	nvgTextLineHeight(context.vg, lineh > 0 ? lineh : 1);
	nvgTextBoxBounds(context.vg, 0,0, maxw, str, NULL, bounds);
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

	for (w = root->children; w != NULL; w = w->next) {
		if (isStyleSet(&w->style, MG_ANCHOR_ARG)) continue;
		if (w->type == MG_BOX) {
			fitToContent(w);
			applySize(w);
		}
	}

	if (root->dir == MG_COL) {
		for (w = root->children; w != NULL; w = w->next) {
			if (isStyleSet(&w->style, MG_ANCHOR_ARG)) continue;
			width = maxf(width, w->cwidth + w->style.paddingx*2);
			height += w->cheight + w->style.paddingy*2;
			if (w->next != NULL) height += w->style.spacing;
		}
	} else {
		for (w = root->children; w != NULL; w = w->next) {
			if (isStyleSet(&w->style, MG_ANCHOR_ARG)) continue;
			width += w->cwidth + w->style.paddingx*2;
			height = maxf(height, w->cheight + w->style.paddingy*2);
			if (w->next != NULL) width += w->style.spacing;
		}
	}

	root->cwidth = width;
	root->cheight = height;
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
	for (w = root->children; w != NULL; w = w->next) {

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
	for (w = root->children; w != NULL; w = w->next) {
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
	for (w = root->children; w != NULL; w = w->next) {
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

		for (w = root->children; w != NULL; w = w->next) {
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

		for (w = root->children; w != NULL; w = w->next) {
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

		for (w = root->children; w != NULL; w = w->next) {
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

		for (w = root->children; w != NULL; w = w->next) {
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
	int size;
	struct MGopt* opt = allocOpt();
	if (opt == NULL) return NULL;
	opt->type = a;
	size = strlen(s) + 1;
	opt->sval = (char*)allocInputTemp(size);
	memcpy(opt->sval, s, size);
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
	if (w->style.logic == MG_CLICK && context.clicked == w->id)
		return &context.activeHit;
	if (w->style.logic == MG_DRAG && (context.pressed == w->id || context.dragged == w->id || context.released == w->id))
		return &context.activeHit;
	return NULL;
}

static struct MGhit* hitResult2(unsigned int id)
{
	if (context.clicked == id || context.pressed == id || context.dragged == id || context.released == id)
		return &context.activeHit;
	return NULL;
}

static int mgGetHit(unsigned int id, struct MGhit** hit)
{
	if (context.active == id) {
		*hit = &context.activeHit;
		return 1;
	}
	if (context.hover == id) {
		*hit = &context.hoverHit;
		return 1;
	}
	return 0;
}
//	context.hoverHit.deltamy = context.deltamy;
//	context.activeHit.code = 0;


static unsigned char getState(struct MGwidget* w)
{
	unsigned char ret = MG_NORMAL;
	if (w == NULL) return ret;
	if (w->parent != NULL && w->bubble)
		ret = getState(w->parent);
	if (context.active == w->id) ret |= MG_ACTIVE;
	if (context.hover == w->id) ret |= MG_HOVER;
	if (context.focus == w->id) ret |= MG_FOCUS;
	return ret;
}

static unsigned char getState2(unsigned int id)
{
	unsigned char ret = MG_NORMAL;
	if (context.active == id) ret |= MG_ACTIVE;
	if (context.hover == id) ret |= MG_HOVER;
	if (context.focus == id) ret |= MG_FOCUS;
	return ret;
}

static unsigned char getChildState(struct MGwidget* w)
{
	struct MGwidget* c;
	unsigned char ret = MG_NORMAL;
	if (w == NULL) return ret;
	if (context.active == w->id) ret |= MG_ACTIVE;
	if (context.hover == w->id) ret |= MG_HOVER;
	if (context.focus == w->id) ret |= MG_FOCUS;
	for (c = w->children; c != NULL; c = c->next)
		ret |= getChildState(c);
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
	for (i = 0; i < context.tagCount; i++) {
		if (context.tags[i] != NULL)
			path[npath++] = (char*)context.tags[i];
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
	for (i = 0; i < context.tagCount; i++) {
		if (context.tags[i] != NULL)
			path[npath++] = (char*)context.tags[i];
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

static int getPath(struct MGwidget* w, const char* path[], int maxPath)
{
	int n = 0;
	if (w == NULL) return 0;
	n = getPath(w->parent, path, maxPath);
	if (n < maxPath && w->tag != NULL)
		path[n++] = w->tag;
	return n;
}

static struct MGstyle computeStyle2(struct MGwidget* w, unsigned char wstate, struct MGopt* opts, const char* subtag)
{
	int i = 0;
	const char* path[100];
	int npath = 0;
	struct MGnamedStyle* match = NULL;
	struct MGstyle style;

	memset(&style, 0, sizeof(style));

	npath = getPath(w, path, 100);
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

static struct MGstyle computeStyle3(unsigned int id, struct MGopt* opts, const char* subtag)
{
	struct MGwidget* w = findWidget(id);
	unsigned char wstate = getState(w);
	return computeStyle2(w, wstate, opts, subtag);
}

unsigned int mgPanelBegin(int dir, float x, float y, int zidx, struct MGopt* opts)
{
	struct MGwidget* w = NULL;

	pushId(context.panelCount+1);

	opts = mgOpts(mgTag("panel"), opts);

	w = allocWidget(MG_PANEL);
	w->x = x;
	w->y = y;
	w->dir = dir;
	w->tag = getTag(opts);
	w->style = computeStyle2(w, getState(w), opts, NULL);

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
	w->tag = getTag(opts);
	w->style = computeStyle2(w, getState(w), opts, NULL);

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
	int size;
	struct MGwidget* parent = getParent();
	struct MGwidget* w = allocWidget(MG_PARAGRAPH);
	if (parent != NULL)
		addChildren(parent, w);

	size = strlen(text)+1;
	w->text = (char*)allocInputTemp(size);
	memcpy(w->text, text, size);

	opts = mgOpts(mgTag("text"), opts);

	w->tag = getTag(opts);
	w->style = computeStyle2(w, getState(w), opts, NULL);

	textSize(NULL, w->style.fontSize, NULL, &th);
	paragraphSize(w->text, w->style.fontSize, w->style.lineHeight, th*20, &tw, &th);
	w->cwidth = tw;
	w->cheight = th;
	applySize(w);

	return w->id;
}

unsigned int mgText(const char* text, struct MGopt* opts)
{
	int size;
	float tw, th;
	struct MGwidget* parent = getParent();
	struct MGwidget* w = allocWidget(MG_TEXT);
	if (parent != NULL)
		addChildren(parent, w);

	size = strlen(text)+1;
	w->text = (char*)allocInputTemp(size);
	memcpy(w->text, text, size);

	opts = mgOpts(mgTag("text"), opts);
	w->tag = getTag(opts);
	w->style = computeStyle2(w, getState(w), opts, NULL);
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

	opts = mgOpts(mgTag("icon"), opts);
	w->tag = getTag(opts);
	w->style = computeStyle2(w, getState(w), opts, NULL);

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

unsigned int mgCanvas(float width, float height, MGcanvasLogicFun logic, MGcanvasRenderFun render, struct MGopt* opts)
{
	struct MGwidget* parent = getParent();
	struct MGwidget* w = allocWidget(MG_CANVAS);
	if (parent != NULL)
		addChildren(parent, w);

	w->logic = logic;
	w->render = render;
	w->cwidth = width;
	w->cheight = height;

	opts = mgOpts(mgTag("canvas"), opts);
	w->tag = getTag(opts);
	w->style = computeStyle2(w, getState(w), opts, NULL);

	return w->id;
}


float calcStyleWidth(float base, struct MGstyle* style)
{
	if (isStyleSet(style, MG_PROPWIDTH_ARG))
		return base * style->width;
	else if (isStyleSet(style, MG_WIDTH_ARG))
		return style->width;
	return base;
}

float calcStyleHeight(float base, struct MGstyle* style)
{
	if (isStyleSet(style, MG_PROPHEIGHT_ARG)) {
//		printf("height prop=%f %f\n", style->height, base * style->height);
		return base * style->height;
	} else if (isStyleSet(style, MG_HEIGHT_ARG)) {
//		printf("height abs %f\n", style->height);
		return style->height;
	}
//	printf("height base %f\n", base);
	return base;
}


struct MGsliderState {
	float value;
	float vstart;
	float vmin, vmax;
};


static void sliderDraw(void* uptr, struct MGwidget* w, struct NVGcontext* vg, const float* view)
{
	struct MGsliderState* state;
	struct MGstyle slotStyle;
	struct MGstyle barStyle;
	struct MGstyle handleStyle;
	float x, h, hr;

	(void)uptr;
	(void)view;

	if (mgIsActive(w->id)) {
		if (!mgGetStateBlock(w->id, 0, (void**)&state, NULL)) return;
	} else {
		if (!mgGetValueBlock(w->id, (void**)&state, NULL)) return;
	}

	slotStyle = computeStyle3(w->id, NULL, "slot");
	barStyle = computeStyle3(w->id, NULL, "bar");
	handleStyle = computeStyle3(w->id, NULL, "handle");

	hr = maxf(2.0f, w->height - w->style.paddingy*2) / 2.0f;

	// Slot
	h = calcStyleHeight(w->height, &slotStyle);
	drawRect(w->x+hr, w->y + w->height/2 - h/2, w->width-hr*2, h, &slotStyle);

	// Handle position
	x = w->x + hr + (state->value - state->vmin) / (state->vmax - state->vmin) * (w->width - hr*2);

	// Bar
	h = calcStyleHeight(w->height, &barStyle);
	drawRect(w->x+hr, w->y + w->height/2 - h/2, x - (w->x+hr), h, &barStyle);

	// Handle
	drawRect(x-hr, w->y + w->height/2 - hr, hr*2, hr*2, &handleStyle);
}

static void sliderLogic(void* uptr, struct MGwidget* w, int event, struct MGhit* hit)
{
	struct MGsliderState* val;
	struct MGsliderState* state;
	struct MGrect inner;
	float hr, xmin, xmax, xrange;

	(void)uptr;

	if (!mgGetValueBlock(w->id, (void**)&val, NULL)) return;

	innerBounds(w, &inner);

	hr = maxf(2.0f, inner.height) / 2.0f;

	xmin = w->x + hr;
	xmax = w->x + w->width - hr;
	xrange = maxf(1.0f, xmax - xmin);

	if (event == MG_PRESSED) {
		if (!mgAllocStateBlock(w->id, 0, (void**)&state, sizeof(struct MGsliderState))) return;
		// Capture the state of the slider when pressed.
		memcpy(state, val, sizeof(struct MGsliderState));
		float u = (state->value - state->vmin) / (state->vmax - state->vmin);
		float x = xmin + u * (xmax - xmin);
		if (hit->mx < (x-hr) || hit->mx > (x+hr)) {
			// If hit outside the handle, skip there directly.
			float v = clampf((hit->mx - xmin) / xrange, 0.0f, 1.0f);
			state->value = clampf(state->vmin + v * (state->vmax - state->vmin), state->vmin, state->vmax);
			mgSetResultFloat(w->id, state->value);
		}
		state->vstart = state->value;
	}
	if (event == MG_DRAGGED) {
		if (!mgGetStateBlock(w->id, 0, (void**)&state, NULL)) return;
		float delta = (hit->deltamx / xrange) * (state->vmax - state->vmin);
		state->value = clampf(state->vstart + delta, state->vmin, state->vmax);
		mgSetResultFloat(w->id, state->value);
	}
	if (event == MG_RELEASED) {
		// Release captured state when mouse is released.
		mgFreeStateBlock(w->id, 0);
	}
}

unsigned int mgSlider(float* value, float vmin, float vmax, struct MGopt* opts)
{
	struct MGsliderState val;
	float w, h;
	unsigned int canvas;

	opts = mgOpts(mgLogic(MG_DRAG), mgTag("slider-canvas"), opts);
	canvas = mgCanvas(DEFAULT_SLIDERW, SLIDER_HANDLE, sliderLogic, sliderDraw, opts);

	// Pass values to logid and rendering.
	val.value = *value;
	val.vmin = vmin;
	val.vmax = vmax;
	mgSetValueBlock(canvas, (void*)&val, sizeof(struct MGsliderState));

	// Get results
	mgGetResultFloat(canvas, value);

	return canvas;
}


unsigned int mgSlider2(float* value, float vmin, float vmax, struct MGopt* opts)
{
	float pc = (*value - vmin) / (vmax - vmin);
	struct MGsliderState* state;
	unsigned int slider, handle;

	slider = mgBoxBegin(MG_ROW, mgOpts(mgLogic(MG_DRAG), mgWidth(DEFAULT_SLIDERW), mgHeight(SLIDER_HANDLE+1), mgTag("slider"), opts));
		// Slot
		mgBox(mgOpts(mgPropPosition(MG_JUSTIFY,MG_CENTER,0,0.5f), mgTag("slot"), mgPropWidth(1.0f)));
		// Bar
		mgBox(mgOpts(mgPropPosition(MG_START,MG_CENTER,0,0.5f), mgTag("bar"), mgPropWidth(pc)));
		// Handle
//		handle = mgBox(mgOpts(mgLogic(MG_DRAG), mgPropPosition(MG_JUSTIFY,MG_CENTER,pc,0.5f), mgTag("handle")));
		handle = mgIcon("check", mgOpts(mgLogic(MG_DRAG), mgPropPosition(MG_JUSTIFY,MG_CENTER,pc,0.5f), mgTag("handle")));
	mgBoxEnd();

	// Handle handle
	if (mgPressed(handle)) {
		if (mgAllocStateBlock(handle, 0, (void**)&state, sizeof(struct MGsliderState))) {
			state->value = *value;
		}
	}
	if (mgDragged(handle)) {
		struct MGhit* hit = NULL;
		if (mgGetHit(handle, &hit) && mgGetStateBlock(handle, 0, (void**)&state, NULL)) {
			float xmin = hit->view.x + (SLIDER_HANDLE+1)/2;
			float xmax = hit->view.x + hit->view.width - (SLIDER_HANDLE+1)/2;
			float xrange = maxf(1.0f, xmax - xmin);
			float delta = (hit->deltamx / xrange) * (vmax - vmin);
			*value = clampf(state->value + delta, vmin, vmax);
		}
	}
	if (mgReleased(handle)) {
		mgFreeStateBlock(handle, 0);
	}

	// Handle interacting with the background.
	if (mgPressed(slider)) {
		struct MGhit* hit = NULL;
		if (mgGetHit(slider, &hit) && mgAllocStateBlock(slider, 0, (void**)&state, sizeof(struct MGsliderState))) {
			float xmin = hit->rect.x + (SLIDER_HANDLE+1)/2;
			float xmax = hit->rect.x + hit->rect.width - (SLIDER_HANDLE+1)/2;
			float xrange = maxf(1.0f, xmax - xmin);
			float v = clampf((hit->mx - xmin) / xrange, 0.0f, 1.0f);
			*value = clampf(vmin + v * (vmax - vmin), vmin, vmax);
			state->value = *value;
		}
	}
	if (mgDragged(slider)) {
		struct MGhit* hit = NULL;
		if (mgGetHit(slider, &hit) && mgGetStateBlock(slider, 0, (void**)&state, NULL)) {
			float xmin = hit->rect.x + (SLIDER_HANDLE+1)/2;
			float xmax = hit->rect.x + hit->rect.width - (SLIDER_HANDLE+1)/2;
			float xrange = maxf(1.0f, xmax - xmin);
			float delta = (hit->deltamx / xrange) * (vmax - vmin);
			*value = clampf(state->value + delta, vmin, vmax);
		}
	}
	if (mgReleased(slider)) {
		mgFreeStateBlock(slider, 0);
	}

/*	if (hres != NULL) {
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
	}*/

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

/*	if (hres != NULL) {
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
	}*/

	return scroll;
}


struct MGtextInputState {
	int maxText;
	int caretPos;
	int nglyphs;
	int selPivot;
	int selStart, selEnd;
};

static void inputDraw(void* uptr, struct MGwidget* w, struct NVGcontext* vg, const float* view)
{
	(void)uptr;

	drawRect(w->x, w->y, w->width, w->height, &w->style);
//	if (debug) drawDebugRect(w);

	nvgScissor(vg, (int)view[0], (int)view[1], (int)view[2], (int)view[3]);

	if (mgIsFocus(w->id)) {
		int j;
		float caretx = 0;
		struct MGtextInputState* state = NULL;
		char* stateText = NULL;
		struct NVGglyphPosition* stateGlyphs = NULL;

		if (!mgGetStateBlock(w->id, 0, (void**)&state, NULL)) return;
		if (!mgGetStateBlock(w->id, 1, (void**)&stateText, NULL)) return;
		if (!mgGetStateBlock(w->id, 2, (void**)&stateGlyphs, NULL)) return;

		if (state->selStart != state->selEnd && state->nglyphs > 0) {
			float sx = (state->selStart >= state->nglyphs) ? stateGlyphs[state->nglyphs-1].maxx : stateGlyphs[state->selStart].x;
			float ex = (state->selEnd >= state->nglyphs) ? stateGlyphs[state->nglyphs-1].maxx : stateGlyphs[state->selEnd].x;
			nvgFillColor(vg, nvgRGBA(255,0,0,64));
			nvgBeginPath(vg);
			nvgRect(vg, sx, w->y+w->style.paddingy, ex - sx, w->height-w->style.paddingy*2);
			nvgFill(vg);
		}

		drawText(w, stateText);

/*		nvgFillColor(vg, nvgRGBA(255,0,0,64));
		for (j = 0; j < state->nglyphs; j++) {
			struct NVGglyphPosition* p = &stateGlyphs[j];
			nvgBeginPath(vg);
			nvgRect(vg, p->minx, w->y, p->maxx - p->minx, w->height);
			nvgFill(vg);
		}*/

		if (state->nglyphs == 0) {
			if (w->style.textAlign == MG_CENTER)
				caretx = w->x + w->width/2;
			else if (w->style.textAlign == MG_END)
				caretx = w->x + w->width - w->style.paddingx;
			else
				caretx = w->x + w->style.paddingx;
		} else if (state->caretPos >= state->nglyphs) {
			caretx = stateGlyphs[state->nglyphs-1].maxx;
		} else {
			caretx = stateGlyphs[state->caretPos].x;
		}
		nvgFillColor(vg, nvgRGBA(255,0,0,255));
		nvgBeginPath(vg);
		nvgRect(vg, (int)(caretx-0.5f), w->y+w->style.paddingy, 1, w->height-w->style.paddingy*2);
		nvgFill(vg);

	} else {
		char* text = NULL;
		int maxText = 0;
		if (!mgGetValueStr(w->id, &text, &maxText)) return;
		drawText(w, text);
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

static void insertText(char* dst, int ndst, int idx, char* str, int nstr)
{
	int i, count;
	if (idx < 0 || idx >= ndst) return;
	// Make space for the new string
	for (i = ndst-1; i >= idx+nstr; i--)
		dst[i] = dst[i-nstr];
	// Insert
	count = mini(idx+nstr, ndst-1) - idx;
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

static void inputLogic(void* uptr, struct MGwidget* w, int event, struct MGhit* hit)
{
	struct MGtextInputState* state = NULL;
	char* stateText = NULL;
	struct NVGglyphPosition* stateGlyphs = NULL;
	char* text = NULL;
	int maxText = 0;

	if (!mgGetValueStr(w->id, &text, &maxText)) return;

	if (event == MG_FOCUSED) {
		if (!mgAllocStateBlock(w->id, 0, (void**)&state, sizeof(struct MGtextInputState))) return;
		if (!mgAllocStateBlock(w->id, 1, (void**)&stateText, maxText)) return;
		if (!mgAllocStateBlock(w->id, 2, (void**)&stateGlyphs, sizeof(struct NVGglyphPosition)*maxText)) return;
		memcpy(stateText, text, maxText);
		state->maxText = maxText;
		state->nglyphs = measureTextGlyphs(w, stateText, stateGlyphs, maxText);
		state->caretPos = state->nglyphs;
		state->selStart = 0;
		state->selEnd = state->nglyphs;
		state->selPivot = -1;

//		printf("%d focused\n", w->id);
	}
	if (event == MG_BLURRED) {
		mgFreeStateBlock(w->id, 0);
		mgFreeStateBlock(w->id, 1);
		mgFreeStateBlock(w->id, 2);
		printf("%d blurred\n", w->id);
	}

	if (!mgGetStateBlock(w->id, 0, (void**)&state, NULL)) return;
	if (!mgGetStateBlock(w->id, 1, (void**)&stateText, NULL)) return;
	if (!mgGetStateBlock(w->id, 2, (void**)&stateGlyphs, NULL)) return;

	if (event == MG_PRESSED) {
		if (hit->clickCount > 1) {
			state->selStart = 0;
			state->selEnd = state->selPivot = state->caretPos = state->nglyphs;
		} else {
			state->caretPos = findCaretPos(hit->mx, stateGlyphs, state->nglyphs);
			state->selStart = state->selEnd = state->selPivot = state->caretPos;
		}
	}
	if (event == MG_DRAGGED) {
		state->caretPos = findCaretPos(hit->mx, stateGlyphs, state->nglyphs);
		state->selStart = mini(state->caretPos, state->selPivot);
		state->selEnd = maxi(state->caretPos, state->selPivot);
	}
	if (event == MG_RELEASED) {
	}
	if (event == MG_KEYPRESSED) {
		printf("%d pressed:%d mods:%x\n", w->id, hit->code, hit->mods);
		if (hit->code == 263) {
			// Left
			if (hit->mods & 1) { // Shift
				if (state->selPivot == -1)
					state->selPivot = state->caretPos;
			}
			if (hit->mods & 4) { // Alt
				// Prev word
				while (state->caretPos > 0 && isSpace(stateText[(int)stateGlyphs[state->caretPos-1].str]))
					state->caretPos--;
				while (state->caretPos > 0 && !isSpace(stateText[(int)stateGlyphs[state->caretPos-1].str]))
					state->caretPos--;
			} else {
				if (state->caretPos > 0)
					state->caretPos--;
			}
			if (hit->mods & 1) { // Shift
				state->selStart = mini(state->caretPos, state->selPivot);
				state->selEnd = maxi(state->caretPos, state->selPivot);
			} else {
				if (state->selStart != state->selEnd)
					state->caretPos = state->selStart;
				state->selStart = state->selEnd = 0;
				state->selPivot = -1;
			}
		} else if (hit->code == 262) {
			// Right
			if (hit->mods & 1) { // Shift
				if (state->selPivot == -1)
					state->selPivot = state->caretPos;
			}
			if (hit->mods & 4) { // Alt
				// Next word
				while (state->caretPos < state->nglyphs && isSpace(stateText[(int)stateGlyphs[state->caretPos].str]))
					state->caretPos++;
				while (state->caretPos < state->nglyphs && !isSpace(stateText[(int)stateGlyphs[state->caretPos].str]))
					state->caretPos++;
			} else {
				if (state->caretPos < state->nglyphs)
					state->caretPos++;
			}
			if (hit->mods & 1) { // Shift
				state->selStart = mini(state->caretPos, state->selPivot);
				state->selEnd = maxi(state->caretPos, state->selPivot);
			} else {
				if (state->selStart != state->selEnd)
					state->caretPos = state->selEnd;
				state->selStart = state->selEnd = 0;
				state->selPivot = -1;
			}
		} else if (hit->code == 259) {
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
				state->nglyphs = measureTextGlyphs(w, stateText, stateGlyphs, maxText);
				// Store result
				mgSetResultStr(w->id, stateText, state->maxText);

				state->selStart = state->selEnd = 0;
				state->selPivot = -1;
			}
		} else if (hit->code == 258) {
			// Tab
			mgSetResultStr(w->id, stateText, state->maxText);
			if (hit->mods & 1)
				mgFocusPrev(w->id);
			else
				mgFocusNext(w->id);

		} else if (hit->code == 257) {
			// Enter
			mgSetResultStr(w->id, stateText, state->maxText);
			mgBlur(w->id);
		}
	}
	if (event == MG_KEYRELEASED) {
//		printf("%d released: %d\n", w->id, hit->code);
	}
	if (event == MG_CHARTYPED) {
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
				state->nglyphs = measureTextGlyphs(w, stateText, stateGlyphs, maxText);
				state->selStart = state->selEnd = 0;
				state->selPivot = -1;
			}
		}

		// Append
		cpToUTF8(hit->code, str);
		if (state->caretPos >= 0 && state->caretPos < state->nglyphs)
			ins = (int)stateGlyphs[state->caretPos].str;
		else
			ins = strlen(stateText);
		insertText(stateText, state->maxText, ins, str, strlen(str));

		state->nglyphs = measureTextGlyphs(w, stateText, stateGlyphs, maxText);
		state->caretPos = mini(state->caretPos + strlen(str), state->nglyphs);

		state->selStart = state->selEnd = 0;
		state->selPivot = -1;

		// Store result
		mgSetResultStr(w->id, stateText, state->maxText);
	}
}

unsigned int mgInput(char* text, int maxText, struct MGopt* opts)
{
	float th;
	char* res = NULL;
	int resSize = 0;
	struct MGinputState2* state = NULL;
	struct MGwidget* parent = getParent();
	struct MGwidget* w = allocWidget(MG_INPUT);
	if (parent != NULL)
		addChildren(parent, w);

	opts = mgOpts(mgTag("input"), opts);
	w->tag = getTag(opts);
	w->style = computeStyle2(w, getState(w), opts, NULL);
	w->stop = 1;

	textSize(NULL, w->style.fontSize, NULL, &th);
	w->cwidth = DEFAULT_TEXTW;
	w->cheight = th;
	applySize(w);

	w->render = inputDraw;
	w->logic = inputLogic;

	mgSetValueStr(w->id, text, maxText);

	if (mgGetResultStr(w->id, &res, &resSize)) {
		memcpy(text, res, mini(maxText, resSize));
	}

	return w->id;
}

unsigned int mgNumber(float* value, struct MGopt* opts)
{
	unsigned int h;
	char str[32];
	snprintf(str, sizeof(str), "%.2f", *value);
	str[sizeof(str)-1] = '\0';

	h = mgInput(str, sizeof(str), mgOpts(mgTag("number"), mgWidth(DEFAULT_NUMBERW), opts));
	if (mgChanged(h)) {
		float num = 0.0f;
		if (sscanf(str, "%f", &num))
			*value = num;
	}
	return h;
}

unsigned int mgNumber3(float* x, float* y, float* z, const char* units, struct MGopt* opts)
{
	unsigned int hx, hy, hz, h;
	h = mgBoxBegin(MG_ROW, mgOpts(mgTag("number3"), opts));
		hx = mgNumber(x, mgOpts(mgGrow(1)));
		hy = mgNumber(y, mgOpts(mgGrow(1)));
		hz = mgNumber(z, mgOpts(mgGrow(1)));
		if (units != NULL && strlen(units) > 0)
			mgLabel(units, mgOpts());
		if (mgChanged(hx) || mgChanged(hy) || mgChanged(hz))
			mgSetResultInt(h, 1);
	mgBoxEnd();
	return h;
}

unsigned int mgColor(float* r, float* g, float* b, float* a, struct MGopt* opts)
{
	unsigned int hr, hg, hb, ha, h;
	h = mgBoxBegin(MG_ROW, mgOpts(mgTag("color"), opts));
		mgLabel("R", mgOpts()); hr = mgNumber(r, mgOpts(mgGrow(1)));
		mgLabel("G", mgOpts()); hg = mgNumber(g, mgOpts(mgGrow(1)));
		mgLabel("B", mgOpts()); hb = mgNumber(b, mgOpts(mgGrow(1)));
		mgLabel("A", mgOpts()); ha = mgNumber(a, mgOpts(mgGrow(1)));
		if (mgChanged(hr) || mgChanged(hg) || mgChanged(hb) || mgChanged(ha))
			mgSetResultInt(h, 1);
	mgBoxEnd();
	return h;
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
	unsigned int button = 0, popup = 0;

	button = mgBoxBegin(MG_ROW, mgOpts(mgTag("select"), opts));
		mgText(choices[*value], mgOpts(mgGrow(1)));

//		mgIcon(CHECKBOX_SIZE, CHECKBOX_SIZE, mgOpts());
		mgIcon("arrow-combo", mgOpts(mgTag("arrow")));

	mgBoxEnd();

//			mgPropPosition(MG_START,MG_CENTER,1.0f,0.5f),
//	mgPropPosition(a,b,1,0)

	popup = mgPopupBegin(button, MG_ACTIVE, MG_COL, mgOpts(mgAlign(MG_JUSTIFY), mgPropPosition(MG_START,MG_START,0.0f,1.0f)));
	for (i = 0; i < nchoises; i++) {
		if (mgClicked(mgItem(choices[i], mgOpts()))) {
			mgShowPopup(popup, 0);
			*value = i;
			mgSetResultInt(button, i);
		}
	}
	mgPopupEnd();

	return button;
}

struct MGpopupState {
	int show, showNext;
	int acting;
	int counter;
	int hover;
	float x, y;

	int trigger;

	unsigned int target;

	unsigned int parentPanel;
};

static struct MGwidget* getParentPanel(struct MGwidget* w)
{
	if (w == NULL) return NULL;
	while (w->parent != NULL)
		w = w->parent;
	return w;
}

static int isParentPanel(struct MGwidget* w, unsigned int id)
{
	if (w == NULL) return 0;
	while (w->parent != NULL) {
		w = w->parent;
		if (w->id == id) return 1;
	}
	return 0;
}

static unsigned char getPanelChildState(unsigned int id)
{
	int i;
	unsigned char state = 0;
	for (i = 0; i < context.panelCount; i++) {
		struct MGwidget* panel = context.panels[i];
		if (isParentPanel(panel, id)) {
			printf("panel %d is parent if %d\n", panel->id, id);
			state |= getChildState(panel);
		}
	}
	return state;
}

static int isAncestor(unsigned int pid, unsigned int cid)
{
	struct MGwidget* w = findWidget(cid);
	if (w == NULL) return 0;
	while (w->parent != NULL) {
		if (w->id == pid)
			return 1;
		w = w->parent;
	}
	return 0;
}

unsigned int mgPopupBegin(unsigned int target, int trigger, int dir, struct MGopt* opts)
{
	struct MGwidget* w = NULL;
	struct MGpopupState* state = NULL;
	struct MGwidget* tgt = findWidget(target);
	struct MGwidget* parentPanel = getParentPanel(tgt);
	int show = 0;

	if (tgt == NULL) return 0;

	pushId(context.panelCount+1);

	w = allocWidget(MG_POPUP);
	w->parent = tgt;

	if (!mgAllocStateBlock(w->id, 0, (void**)&state, sizeof(struct MGpopupState))) return 0;
	state->trigger = trigger;
	state->target = target;
	state->parentPanel = parentPanel != NULL ? parentPanel->id : 0;

	if (state->trigger == MG_ACTIVE) {
		// Hide when on release of any other
		if (context.pressed != 0) {
			if (context.pressed == target) {
				state->acting = 1;
				state->show = state->show ? 0 : 1;
			}
		}
		if (context.released != 0) {
			if (state->acting == 0) {
				state->show = 0;
			}
			state->acting = 0;
		}
	}

	state->x = 0;
	state->y = 0;

	show = state->show;
	w->x = state->x;
	w->y = state->y;

/*	popup = (struct MGpopupState*)allocState(w->id, 0, sizeof(struct MGpopupState));
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
				if (context.released) {
					popup->counter = 0;
				}
			}
			if (trigger == MG_ACTIVE) {
				// close on second release, the first will come from the activation
				if (context.released) {
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
	}*/

	opts = mgOpts(mgTag("popup"), opts);

	w->active = show;
	w->bubble = 0;
	w->dir = dir;
	w->tag = getTag(opts);
	w->style = computeStyle2(w, getState(w), opts, NULL);

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
		struct MGpopupState* state = NULL;
		struct MGwidget* tgt = NULL;

		if (mgGetStateBlock(w->id, 0, (void**)&state, NULL)) {
			tgt = findWidget(state->target);
			if (tgt != NULL) {

				if (state->trigger == MG_HOVER) {
					if ((getChildState(tgt) & MG_HOVER) != 0) {
						if (state->counter < 2)
							state->counter++;
					} else if (!isAncestor(w->id, context.hover)) {
						if (state->counter > 0)
							state->counter--;
					}
					state->show = state->counter == 2 ? 1 : 0;
				}
			}
		}

/*		struct MGpopupState* popup = (struct MGpopupState*)allocState(w->id, 0, sizeof(struct MGpopupState));
		if (popup != NULL) {
			if (popup->show) {
				if (popup->trigger == MG_HOVER) {
					if (getChildState(w) & MG_HOVER) {
						popup->hover = 1;
					}
				}
			}
		}*/

		layoutPanel(w);
		return w->id;
	}

	return 0;
}

void mgShowPopup(unsigned int id, int show)
{
	struct MGpopupState* state = NULL;
	struct MGwidget* w = findWidget(id);
	if (w == NULL || w->type != MG_POPUP) return;
	if (!mgGetStateBlock(id, 0, (void**)&state, NULL)) return;
	state->show = show;
}

unsigned int mgTooltip(unsigned int target, const char* message, struct MGopt* opts)
{
	mgPopupBegin(target, MG_HOVER, MG_ROW, mgOpts(mgTag("tooltip"), opts));
		mgLabel(message, mgOpts());
	return mgPopupEnd();
}

int mgClicked(unsigned int id)
{
	return context.clicked == id ? 1 : 0;
}

int mgPressed(unsigned int id)
{
	return context.pressed == id ? 1 : 0;
}

int mgDragged(unsigned int id)
{
	return context.dragged == id ? 1 : 0;
}

int mgReleased(unsigned int id)
{
	return context.released == id ? 1 : 0;
}

int mgIsActive(unsigned int id)
{
	return context.active == id ? 1 : 0;
}

int mgIsHover(unsigned int id)
{
	return context.hover == id ? 1 : 0;
}

int mgIsFocus(unsigned int id)
{
	return context.focus == id ? 1 : 0;
}

int mgChanged(unsigned int id)
{
	int i;
	for (i = 0; i < outputResPoolSize; i++) {
		if (outputResPool[i].id == id)
			return 1;
	}
	return 0;
}

void mgFocus(unsigned int id)
{
	context.forceFocus = id;
}

void mgFocusNext(unsigned int id)
{
	context.focusNext = id;
}

void mgFocusPrev(unsigned int id)
{
	context.focusPrev = id;
}

void mgBlur(unsigned int id)
{
	context.forceBlur = id;
}
