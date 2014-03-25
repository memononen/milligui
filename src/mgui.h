#ifndef MGUI_H
#define MGUI_H

struct NVGcontext;

int mgInit();
void mgTerminate();

enum MUImouseButton {
	MG_MOUSE_PRESSED	= 1 << 0,
	MG_MOUSE_RELEASED	= 1 << 1,
};

enum MUIoverflow {
	MG_FIT,
	MG_HIDDEN,
	MG_SCROLL,
};

enum MUIdir {
	MG_ROW,
	MG_COL,
};

enum MUIalign {
	MG_START,
	MG_END,
	MG_CENTER,
	MG_JUSTIFY,
};

#define MG_AUTO_SIZE 0xffff


enum MGargTypes {
	MG_NONE = 0,
	MG_WIDTH_ARG,
	MG_HEIGHT_ARG,
	MG_SPACING_ARG,
	MG_PADDINGX_ARG,
	MG_PADDINGY_ARG,
	MG_GROW_ARG,
	MG_ALIGN_ARG,
	MG_OVERFLOW_ARG,
	MG_FONTSIZE_ARG,
	MG_TEXTALIGN_ARG,
};

unsigned int mgPackArg(unsigned char arg, int x);
#define mgOverflow(v)	(mgPackArg(MG_OVERFLOW_ARG, (v)))
#define mgAlign(v)		(mgPackArg(MG_ALIGN_ARG, (v)))
#define mgGrow(v)		(mgPackArg(MG_GROW_ARG, (v)))
#define mgWidth(v)		(mgPackArg(MG_WIDTH_ARG, (v)))
#define mgHeight(v)		(mgPackArg(MG_HEIGHT_ARG, (v)))
#define mgPaddingX(v)	(mgPackArg(MG_PADDINGX_ARG, (v)))
#define mgPaddingY(v)	(mgPackArg(MG_PADDINGY_ARG, (v)))
#define mgPadding(x,y)	(mgPackArg(MG_PADDINGX_ARG, (x))), (mgPackArg(MG_PADDINGY_ARG, (y)))
#define mgSpacing(v)	(mgPackArg(MG_SPACING_ARG, (v)))
#define mgFontSize(v)	(mgPackArg(MG_FONTSIZE_ARG, (v)))
#define mgTextAlign(v)	(mgPackArg(MG_TEXTALIGN_ARG, (v)))

struct MGargs {
	unsigned int set;
	unsigned short width;
	unsigned short height;
	unsigned char spacing;
	unsigned char paddingx;
	unsigned char paddingy;
	unsigned char grow;
	unsigned char align;
	unsigned char overflow;
	unsigned char fontSize;
	unsigned char textAlign;
};

enum MGwidgetType {
	MG_BOX,
	MG_TEXT,
	MG_ICON,
	MG_SLIDER,
	MG_INPUT,
};

struct MGwidget {
	unsigned int id;
	float x, y, width, height;
	struct MGargs args;
	unsigned char dir;
	unsigned char type;

	union {
		struct {
			char* text;
		} text;
		struct {
			char* text;
			int maxtext;
			float value;
		} input;
		struct {
			float value;
			float vmin;
			float vmax;
		} slider;
		struct {
			struct MGwidget* children;
		} box;
	};
	struct MGwidget* next;
};


// TODO: support zero args.
#define mgArgs(...) mgArgs_(__VA_ARGS__, MG_NONE)
struct MGargs mgArgs_(unsigned int first, ...);

void mgFrameBegin(struct NVGcontext* vg, int width, int height, int mx, int my, int mbut);
void mgFrameEnd();

int mgPanelBegin(int dir, float x, float y, float width, float height, struct MGargs args);
int mgPanelEnd();

int mgBoxBegin(int dir, struct MGargs args);
int mgBoxEnd();

int mgText(const char* text, struct MGargs args);
int mgIcon(int width, int height, struct MGargs args);
int mgSlider(float* value, float vmin, float vmax, struct MGargs args);
int mgInput(char* text, int maxtext, struct MGargs args);

// Derivative
int mgNumber(float* value, struct MGargs args);
int mgSelect(int* value, const char** choices, int nchoises, struct MGargs args);
int mgLabel(const char* text, struct MGargs args);
int mgNumber3(float* x, float* y, float* z, const char* units, struct MGargs args);
int mgColor(float* r, float* g, float* b, float* a, struct MGargs args);
int mgCheckBox(const char* text, int* value, struct MGargs args);
int mgButton(const char* text, struct MGargs args);
int mgItem(const char* text, struct MGargs args);


#endif // MGUI_H
