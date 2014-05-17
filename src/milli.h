#ifndef MILLI_H
#define MILLI_H

struct NVGcontext;

int miInit();
void miTerminate();

enum MImouseButton {
	MI_MOUSE_PRESSED	= 1 << 0,
	MI_MOUSE_RELEASED	= 1 << 1,
};

enum MUIoverflow {
	MI_FIT,
	MI_HIDDEN,
	MI_SCROLL,
	MI_VISIBLE,
};

enum MUIdir {
	MI_ROW,
	MI_COL,
};

enum MUIalign {
	MI_START,
	MI_END,
	MI_CENTER,
	MI_JUSTIFY,
};

//#define MI_AUTO_SIZE 0xffff

struct MIrect {
	float x, y, width, height;
};

struct MIpoint {
	float x, y;
};

struct MIsize {
	float width, height;
};

struct MIcolor {
	unsigned char r,g,b,a;
};

enum MIwidgetEvent {
	MI_FOCUSED,
	MI_BLURRED,
	MI_CLICKED,
	MI_PRESSED,
	MI_RELEASED,
	MI_DRAGGED,
	MI_ENTERED,
	MI_EXITED,
	MI_KEYPRESSED,
	MI_KEYRELEASED,
	MI_CHARTYPED,
};


#define MI_MAX_INPUTKEYS 32
struct MIkeyPress {
	int type, code;
};

struct MIevent {
	int type;
	float mx, my;
	float deltamx, deltamy;
	int mbut;
	int key;
};

struct MIinputState
{
	float mx, my;
	int mbut;
	struct MIkeyPress keys[MI_MAX_INPUTKEYS];
	int nkeys;	
};

void miFrameBegin(struct NVGcontext* vg, int width, int height, struct MIinputState* input);
void miFrameEnd();

struct MIparam {
	char* key;
	char* val;
	struct MIparam* next;
};

struct MIcell;

typedef void (*MIrenderFun)(struct MIcell* cell, struct NVGcontext* vg, struct MIrect* view);
typedef int (*MIlayoutFun)(struct MIcell* cell, struct NVGcontext* vg);
typedef int (*MIlogicFun)(struct MIcell* cell, struct MIevent* event);
typedef void (*MImeasureFun)(struct MIcell* cell, struct NVGcontext* vg);
typedef void (*MIparamFun)(struct MIcell* cell, struct MIparam* param);
typedef void (*MIdtorFun)(struct MIcell* cell);

struct MIvar {
	char* name;
	char* key;
	struct MIvar* next;
};

struct MIcell {
	char* id;

	struct MIrect frame;
	struct MIsize content;
	unsigned char grow;
	unsigned char paddingx, paddingy;
	unsigned char spacing;

	unsigned char hover;
	unsigned char active;

	MIrenderFun render;
	MIlayoutFun layout;
	MIlogicFun logic;
	MIparamFun param;
	MImeasureFun measure;
	MIdtorFun dtor;

	struct MIvar* vars;

	struct MIcell* parent;
	struct MIcell* children;
	struct MIcell* next;
};

struct MIbox {
	struct MIcell cell;
	unsigned char dir;
	unsigned char align;
	unsigned char pack;
	unsigned char overflow;
	float width, height;
};

struct MItext {
	struct MIcell cell;
	char* text;
//	int maxText;
	char* fontFace;
	float fontSize;
	float lineHeight;
	unsigned char align;
	unsigned char pack;
};

struct MIiconImage;

struct MIicon {
	struct MIcell cell;
	struct MIiconImage* image;
	float width, height;
};

struct MItemplate {
	struct MIcell cell;
};

struct MIslider {
	struct MIcell cell;
	float width, height;
	float value;
	float vmin, vmax;
	float vstart;
};


struct MIparam* miParseParams(const char* params);
void miFreeParams(struct MIparam* p);
int miCellParam(struct MIcell* cell, struct MIparam* p);

struct MIcell* miCreateBox(const char* params);
struct MIcell* miCreateText(const char* params);
struct MIcell* miCreateIcon(const char* params);
struct MIcell* miCreateSlider(const char* params);

struct MIcell* miCreateButton(const char* params);
struct MIcell* miCreateIconButton(const char* params);

struct MIcell* miCreatetemplate(struct MIcell* host);

void miAddChild(struct MIcell* parent, struct MIcell* child);

void miFreeCell(struct MIcell* cell);

void miSet(struct MIcell* cell, const char* params);

#define MILLI_NOTUSED(v) do { (void)(1 ? (void)0 : ( (void)(v) ) ); } while(0)

void miLayout(struct MIcell* cell, struct NVGcontext* vg);
void miInput(struct MIcell* cell, struct MIinputState* input);
void miRender(struct MIcell* cell, struct NVGcontext* vg);

int miCreateIconImage(const char* name, const char* filename, float scale);

//int mgCreateIcon(const char* name, const char* filename);

#endif // MILLI_H
