#ifndef MILLI_H
#define MILLI_H

struct NVGcontext;

enum MImouseButton {
	MI_MOUSE_PRESSED	= 1 << 0,
	MI_MOUSE_RELEASED	= 1 << 1,
};

enum MIoverflow {
	MI_FIT,
	MI_HIDDEN,
	MI_SCROLL,
	MI_VISIBLE,
};

enum MIdir {
	MI_ROW,
	MI_COL,
};

enum MIalign {
	MI_START,
	MI_END,
	MI_CENTER,
	MI_JUSTIFY,
};

//#define MI_AUTO_SIZE 0xffff

struct MIrect {
	float x, y, width, height;
};
typedef struct MIrect MIrect; 

struct MIpoint {
	float x, y;
};
typedef struct MIpoint MIpoint;

struct MIsize {
	float width, height;
};
typedef struct MIsize MIsize;

struct MIcolor {
	unsigned char r,g,b,a;
};
typedef struct MIcolor MIcolor;

enum MIwidgetEvent {
	MI_NONE,
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
	int type, code, mods;
};
typedef struct MIkeyPress MIkeyPress;

struct MIevent {
	int type;
	float mx, my;
	float deltamx, deltamy;
	int mbut;
	int key;
};
typedef struct MIevent MIevent;

struct MIinputState
{
	float mx, my;
	int mbut;
	MIkeyPress keys[MI_MAX_INPUTKEYS];
	int nkeys;	
};
typedef struct MIinputState MIinputState;

#define MI_FIT (-1.0f)

struct MIpopupState {
	int visible;
	int visited;
	int logic;
	MIrect rect;
};
typedef struct MIpopupState MIpopupState;

struct MIcanvasState {
	MIrect rect;
};
typedef struct MIcanvasState MIcanvasState;

typedef unsigned int MIhandle;

enum MIfontFace {
	MI_FONT_NORMAL,
	MI_FONT_ITALIC,
	MI_FONT_BOLD,
	MI_COUNT_FONTS
};

int miInit(struct NVGcontext* vg);
void miTerminate();

int miCreateFont(int face, const char* filename);
int miCreateIconImage(const char* name, const char* filename, float scale);


void miFrameBegin(int width, int height, MIinputState* input, float dt);
void miFrameEnd();

MIhandle miPanelBegin(float x, float y, float width, float height);
MIhandle miPanelEnd();

int miIsHover(MIhandle handle);
int miIsActive(MIhandle handle);
int miIsFocus(MIhandle handle);

int miFocused(MIhandle handle);
int miBlurred(MIhandle handle);

int miPressed(MIhandle handle);
int miReleased(MIhandle handle);
int miClicked(MIhandle handle);
int miDragged(MIhandle handle);
int miChanged(MIhandle handle);

void miBlur(MIhandle handle);
void miChange(MIhandle handle);

MIpoint miMousePos();
int miMouseClickCount();

MIhandle miButton(const char* label);
MIhandle miText(const char* text);
MIhandle miSlider(float* value, float vmin, float vmax);
MIhandle miInput(char* text, int maxText);

MIhandle miSliderValue(float* value, float vmin, float vmax);

enum MIpack {
	MI_TOP_BOTTOM,
	MI_BOTTOM_TOP,
	MI_LEFT_RIGHT,
	MI_RIGHT_LEFT,
	MI_FILLX,
	MI_FILLY,
};

void miPack(int pack);
void miColWidth(float width);
void miRowHeight(float height);

MIhandle miDockBegin(int pack);
MIhandle miDockEnd();

MIhandle miLayoutBegin(int pack);
MIhandle miLayoutEnd();

MIhandle miDivsBegin(int pack, int count, float* divs);
MIhandle miDivsEnd();

MIhandle miSpacer();

enum MIpopupSide {
	MI_RIGHT,
	MI_BELOW,
};

enum MIpopupLogic {
	MI_ONCLICK,
	MI_ONHOVER,
};

MIhandle miPopupBegin(MIhandle base, int logic, int side);
MIhandle miPopupEnd();

void miPopupShow(MIhandle handle);
void miPopupHide(MIhandle handle);
void miPopupToggle(MIhandle handle);

MIhandle miCanvasBegin(MIcanvasState* state, float width, float height);
MIhandle miCanvasEnd();

MIsize miMeasureText(const char* text, int face, float size);

#define MI_NOTUSED(v) do { (void)(1 ? (void)0 : ( (void)(v) ) ); } while(0)

#endif // MILLI_H
