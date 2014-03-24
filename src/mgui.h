#ifndef MGUI_H
#define MGUI_H

struct NVGcontext;

int mgInit();
void mgTerminate();

enum MUImouseButton {
	MG_MOUSE_PRESSED	= 1 << 0,
	MG_MOUSE_RELEASED	= 1 << 1,
};

enum MUIvalue {
	MG_AUTO = -1,
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

#define MG_AUTO_SIZE -1


enum MGargs {
	MG_NONE = 0,
	MG_OVERFLOW_BASE,
	MG_ALIGN_BASE,
	MG_GROW_BASE,
	MG_WIDTH_BASE,
	MG_HEIGHT_BASE,
	MG_PADDING_BASE,
	MG_SPACING_BASE,
	MG_FONTSIZE_BASE,
	MG_TEXTALIGN_BASE,
};

#define mgOverflow(a) (int)((MG_OVERFLOW_BASE | ((int)(a)<<8)))
#define mgAlign(a) (int)((MG_ALIGN_BASE | ((int)(a)<<8)))
#define mgGrow(a) (int)((MG_GROW_BASE | ((int)(a)<<8)))
#define mgWidth(a) (int)((MG_WIDTH_BASE | ((int)(a)<<8)))
#define mgHeight(a) (int)((MG_HEIGHT_BASE | ((int)(a)<<8)))
#define mgPadding(a,b) (int)((MG_PADDING_BASE | ((int)(a)<<16) | ((int)(b)<<8)))
#define mgSpacing(a) (int)((MG_SPACING_BASE | ((int)(a)<<8)))
#define mgFontSize(a) (int)((MG_FONTSIZE_BASE | ((int)(a)<<8)))
#define mgTextAlign(a) (int)((MG_TEXTALIGN_BASE | ((int)(a)<<8)))

void mgBeginFrame(struct NVGcontext* vg, int width, int height, int mx, int my, int mbut);
void mgEndFrame();

#define mgPanelBegin(...) mgPanelBegin_(__VA_ARGS__, MG_NONE)
int mgPanelBegin_(int dir, float x, float y, float width, float height, ...);
int mgPanelEnd();

#define mgDivBegin(...) mgDivBegin_(__VA_ARGS__, MG_NONE)
int mgDivBegin_(int dir, ...);
int mgDivEnd();

#define mgText(...) mgText_(__VA_ARGS__, MG_NONE)
int mgText_(const char* text, ...);

#define mgIcon(...) mgIcon_(__VA_ARGS__, MG_NONE)
int mgIcon_(int width, int height, ...);

#define mgSlider(...) mgSlider_(__VA_ARGS__, MG_NONE)
int mgSlider_(float* value, float vmin, float vmax, ...);

#define mgNumber(...) mgNumber_(__VA_ARGS__, MG_NONE)
int mgNumber_(float* value, ...);

#define mgTextBox(...) mgTextBox_(__VA_ARGS__, MG_NONE)
int mgTextBox_(char* text, int maxtext, ...);

// Derivative
int mgSelect(int* value, const char** choices, int nchoises);
int mgLabel(const char* text);
int mgNumber3(float* x, float* y, float* z, const char* units);
int mgColor(float* r, float* g, float* b, float* a);
int mgCheckBox(const char* text, int* value);
int mgButton(const char* text);
int mgItem(const char* text);


#endif // MGUI_H
