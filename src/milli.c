#include "milli.h"
#include "nanovg.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include "nanosvg.h"


static int mini(int a, int b) { return a < b ? a : b; }
static int maxi(int a, int b) { return a > b ? a : b; }
static float minf(float a, float b) { return a < b ? a : b; }
static float maxf(float a, float b) { return a > b ? a : b; }
static float clampf(float a, float mn, float mx) { return a < mn ? mn : (a > mx ? mx : a); }
static float absf(float a) { return a < 0.0f ? -a : a; }

static struct NVGcolor milli__nvgColMilli(struct MIcolor col)
{
	struct NVGcolor c;
	c.r = col.r / 255.0f;
	c.g = col.g / 255.0f;
	c.b = col.b / 255.0f;
	c.a = col.a / 255.0f;
	return c;
}

static struct NVGcolor milli__nvgColUint(unsigned int col)
{
	struct NVGcolor c;
	c.r = (col & 0xff) / 255.0f;
	c.g = ((col >> 8) & 0xff) / 255.0f;
	c.b = ((col >> 16) & 0xff) / 255.0f;
	c.a = ((col >> 24) & 0xff) / 255.0f;
	return c;
}


// TODO move resource handling to render abstraction.
#define MI_MAX_ICONS 100
struct MIiconImage
{
	char* name;
	struct NSVGimage* image;
};
static struct MIiconImage* icons[MI_MAX_ICONS];
static int iconCount = 0;

static struct MIiconImage* findIcon(const char* name)
{
	int i = 0;
	for (i = 0; i < iconCount; i++) {
		if (strcmp(icons[i]->name, name) == 0)
			return icons[i];
	}
	printf("Could not find icon '%s'\n", name);
	return NULL;
}

static void milli__scaleIcon(struct NSVGimage* image, float scale)
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

	if (iconCount >= MI_MAX_ICONS)
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
		milli__scaleIcon(icon->image, scale);

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

static void drawIcon(struct NVGcontext* vg, struct MIrect* rect, struct NSVGimage* image, struct MIcolor* color)
{
	int i;
	struct NSVGshape* shape = NULL;
	struct NSVGpath* path;
	float sx, sy, s;

	if (image == NULL) return;

	if (color != NULL) {
		nvgFillColor(vg, milli__nvgColMilli(*color));
		nvgStrokeColor(vg, milli__nvgColMilli(*color));
	}
	sx = rect->width / image->width;
	sy = rect->height / image->height;
	s = minf(sx, sy);

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
				nvgFillColor(vg, milli__nvgColUint(shape->fill.color));
			nvgFill(vg);
//			printf("image %s\n", w->icon.icon->name);
//			nvgDebugDumpPathCache(vg);
		}
		if (shape->stroke.type == NSVG_PAINT_COLOR) {
			if (color == NULL)
				nvgStrokeColor(vg, milli__nvgColUint(shape->stroke.color));
			nvgStrokeWidth(vg, shape->strokeWidth);
			nvgStroke(vg);
		}
	}

	nvgRestore(vg);
}


static struct MIvar* milli__findVar(struct MIcell* cell, const char* name)
{
	struct MIvar* v;
	int nameLen;
	if (cell == NULL) return NULL;
	if (cell->vars == NULL) return NULL;
	if (name == NULL) return NULL;
	for (v = cell->vars; v != NULL; v = v->next) {
		printf("%p %s==%s\n", cell, v->name, name);
		if (strcmp(v->name, name) == 0)
			return v;
	}
	return NULL;
}

static void milli__addVar(struct MIcell* cell, const char* name, const char* key)
{
	struct MIvar** prev = NULL;
	struct MIvar* var = NULL;
	int nameLen, keyLen;

	if (name == NULL || key == NULL) return;
	nameLen = strlen(name);
	keyLen = strlen(key);
	if (nameLen == 0 || keyLen == 0) return;

	// Check if the var exists
	if (milli__findVar(cell, name) != NULL)
		return;

	// Create new var
	var = (struct MIvar*)calloc(1, sizeof(struct MIvar));
	if (var == NULL) goto error;
	// Store name
	var->name = strdup(name);
	if (var->name == NULL) goto error;
	// Store key
	var->key = strdup(key);
	if (var->key == NULL) goto error;
	// Add to linked list
	prev = &cell->vars;
	while (*prev != NULL)
		prev = &(*prev)->next;
	*prev = var;

	return;

error:
	if (var != NULL) {
		free(var->name);
		free(var->key);
		free(var);
	}
}

static void milli__freeVars(struct MIvar* v)
{
	while (v != NULL) {
		struct MIvar* next = v->next;
		free(v->name);
		free(v->key);
		free(v);
		v = next;
	}
}


int miInit()
{
	return 1;
}

void miTerminate()
{
	deleteIcons();
}

void miFrameBegin(struct NVGcontext* vg, int width, int height, struct MIinputState* input)
{
	MILLI_NOTUSED(vg);
	MILLI_NOTUSED(width);
	MILLI_NOTUSED(height);
	MILLI_NOTUSED(input);
}

void miFrameEnd()
{

}

static int milli__isspace(char c)
{
	return strchr(" \t\n\v\f\r", c) != 0;
}

static int milli__isdigit(char c)
{
	return strchr("0123456789", c) != 0;
}

static int milli__isnum(char c)
{
	return strchr("0123456789+-.eE", c) != 0;
}

static struct MIparam* milli__allocParam(const char* key, int keyLen, const char* val, int valLen)
{
	struct MIparam* p = (struct MIparam*)calloc(1, sizeof(struct MIparam));
	if (p == NULL) goto error;

	p->key = (char*)malloc(keyLen+1);
	if (p->key == NULL) goto error;
	memcpy(p->key, key, keyLen);
	p->key[keyLen] = '\0';

	p->val = (char*)malloc(valLen+1);
	if (p->val == NULL) goto error;
	memcpy(p->val, val, valLen);
	p->val[valLen] = '\0';

	return p;

error:
	if (p != NULL) {
		free(p->key);
		free(p->val);
		free(p);
	}
	return NULL;
}

struct MIparam* miParseParams(const char* s)
{
	struct MIparam* ret = NULL;
	struct MIparam** cur = &ret;

	while (*s) {
		const char *key = NULL, *val = NULL;
		int keyLen = 0, valLen = 0;

		// Skip white space before the param name
		while (*s && milli__isspace(*s)) s++;
		if (!*s) break;
		key = s;
		// Find end of the attrib name.
		while (*s && !milli__isspace(*s) && *s != '=') s++;
		keyLen = (int)(s - key);

		// Skip until the beginning of the value.
		while (*s && (milli__isspace(*s) || *s == '=')) s++;
		if (*s == '\'' || *s == '\"') {
			// Parse quoted value
			char quote = *s;
			s++; // skip quote
			val = s;
			while (*s && *s != quote) s++;
			valLen = (int)(s - val);
			s++; // skip quote
		} else {
			// Parse unquoted value
			val = s;
			while (*s && !milli__isspace(*s)) s++;
			valLen = (int)(s - val);
		}

		if (key != NULL && keyLen > 0 && val != NULL && valLen > 0) {
			struct MIparam* p = milli__allocParam(key, keyLen, val, valLen);
			if (p != NULL) {
				*cur = p;
				cur = &p->next;
			}
		}
	}
	
	return ret;
}

void miFreeParams(struct MIparam* p)
{
	if (p == NULL) return;
	miFreeParams(p->next);
	free(p->key);
	free(p->val);
	free(p);
}

int miCellParam(struct MIcell* cell, struct MIparam* p)
{
	if (strcmp(p->key, "id") == 0) {
		if (p->val != NULL && strlen(p->val) > 0) {
			if (cell->id != NULL) free(cell->id);
			cell->id = strdup(p->val);
			return 1;
		}
	} else if (strcmp(p->key, "grow") == 0) {
		int grow = -1;
		sscanf(p->val, "%d", &grow);
		if (grow >= 0 && grow < 100) {
			cell->grow = grow;
			return 1;
		}
	} else if (strcmp(p->key, "paddingx") == 0) {
		int pad = -1;
		sscanf(p->val, "%d", &pad);
		if (pad >= 0 && pad < 100) {
			cell->paddingx = pad;
			return 1;
		}
	} else if (strcmp(p->key, "paddingy") == 0) {
		int pad = -1;
		sscanf(p->val, "%d", &pad);
		if (pad >= 0 && pad < 100) {
			cell->paddingy = pad;
			return 1;
		}
	} else if (strcmp(p->key, "padding") == 0) {
		int padx = -1, pady = -1;
		sscanf(p->val, "%d %d", &padx, &pady);
		if (padx >= 0 && padx < 100 && pady >= 0 && pady < 100) {
			cell->paddingx = padx;
			cell->paddingy = pady;
			return 1;
		}
	} else if (strcmp(p->key, "spacing") == 0) {
		int spacing = -1;
		sscanf(p->val, "%d", &spacing);
		if (spacing >= 0 && spacing < 100) {
			cell->spacing = spacing;
			return 1;
		}
	}
	return 0;
}


void milli__boxRender(struct MIcell* cell, struct NVGcontext* vg, struct MIrect* view)
{
	struct MIbox* box = (struct MIbox*)cell;
	struct MIcell* child;
	struct MIrect rect = box->cell.frame;

	nvgBeginPath(vg);
	nvgRect(vg, rect.x, rect.y, rect.width, rect.height);
	nvgFillColor(vg, nvgRGBA(255,255,255,32));
	nvgFill(vg);

	// Render children
	for (child = box->cell.children; child != NULL; child = child->next) {
		if (child->render != NULL)
			child->render(child, vg, &rect);
	}
}

int milli__boxLayout(struct MIcell* cell, struct NVGcontext* vg)
{
	struct MIbox* box = (struct MIbox*)cell;
	struct MIcell* child;
	float x, y, bw, bh;
	float sum = 0, avail = 0, packSpacing = 0;
	int ngrow = 0, nitems = 0;
	int reflow = 0;

	x = box->cell.frame.x + box->cell.paddingx;
	y = box->cell.frame.y + box->cell.paddingy;
	bw = maxf(0, box->cell.frame.width - box->cell.paddingx*2);
	bh = maxf(0, box->cell.frame.height - box->cell.paddingy*2);
//	x = box->x; // + root->style.paddingx;
//	y = root->y + root->style.paddingy;
//	rw = maxf(0, root->width - root->style.paddingx*2);
//	rh = maxf(0, root->height - root->style.paddingy*2);

	// Calculate desired sizes of the boxes.
	for (child = box->cell.children; child != NULL; child = child->next) {

/*		if (isStyleSet(&w->style, MG_PROPWIDTH_ARG)) {
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
		}*/

		child->frame.width = child->content.width + child->paddingx*2;
		child->frame.height = child->content.height + child->paddingy*2;

		// Handle justify align already here.
		if (box->align == MI_JUSTIFY) {
			if (box->dir == MI_COL)
				child->frame.width = bw;
			else
				child->frame.height = bh;
		}

		child->frame.width = minf(child->frame.width, bw);
		child->frame.height = minf(child->frame.height, bh);
	}


	// Layout box model widgets
	if (box->dir == MI_COL) {

		for (child = box->cell.children; child != NULL; child = child->next) {
			sum += child->frame.height;
			if (child->next != NULL) sum += child->spacing;
			ngrow += child->grow;
			nitems++;
		}

		avail = bh - sum;
		if (box->overflow != MI_FIT)
			avail = maxf(0, avail); 

		if (ngrow == 0 && avail > 0) {
			if (box->pack == MI_START)
				y += 0;
			else if (box->pack == MI_CENTER)
				y += avail/2;
			else if (box->pack == MI_END)
				y += avail;
			else if (box->pack == MI_JUSTIFY) {
				packSpacing = avail / nitems;
				y += packSpacing/2;
			}
			avail = 0;
		}

		for (child = box->cell.children; child != NULL; child = child->next) {
			child->frame.x = x;
			child->frame.y = y;
			if (ngrow > 0)
				child->frame.height += (float)child->grow/(float)ngrow * avail;
			else if (avail < 0)
				child->frame.height += 1.0f/(float)nitems * avail;

			switch (box->align) {
			case MI_END:
				child->frame.x += bw - child->frame.width;
				break;
			case MI_CENTER:
				child->frame.x += bw/2 - child->frame.width/2;
				break;
			default: // MI_START and MI_JUSTIFY
				break;
			}
			y += child->frame.height + child->spacing + packSpacing;

			if (child->layout)
				reflow |= child->layout(child, vg);
		}

	} else {

		for (child = box->cell.children; child != NULL; child = child->next) {
			sum += child->frame.width;
			if (child->next != NULL) sum += child->spacing;
			ngrow += child->grow;
			nitems++;
		}

		avail = bw - sum;
		if (box->overflow != MI_FIT)
			avail = maxf(0, avail); 

		if (ngrow == 0 && avail > 0) {
			if (box->pack == MI_START)
				x += 0;
			else if (box->pack == MI_CENTER)
				x += avail/2;
			else if (box->pack == MI_END)
				x += avail;
			else if (box->pack == MI_JUSTIFY) {
				packSpacing = avail / nitems;
				x += packSpacing/2;
			}
			avail = 0;
		}

		for (child = box->cell.children; child != NULL; child = child->next) {
			child->frame.x = x;
			child->frame.y = y;
			if (ngrow > 0)
				child->frame.width += (float)child->grow/(float)ngrow * avail;
			else if (avail < 0)
				child->frame.width += 1.0f/(float)nitems * avail;

			switch (box->align) {
			case MI_END:
				child->frame.y += bh - child->frame.height;
				break;
			case MI_CENTER:
				child->frame.y += bh/2 - child->frame.height/2;
				break;
			default: // MI_START and MI_JUSTIFY
				break;
			}

			x += child->frame.width + child->spacing + packSpacing;

			if (child->layout)
				reflow |= child->layout(child, vg);
		}
	}

	return reflow;
}

static void milli__boxMeasure(struct MIcell* cell, struct NVGcontext* vg)
{
	struct MIbox* box = (struct MIbox*)cell;
	struct MIsize* size = &box->cell.content;
	struct MIcell* child;

	size->width = 0;
	size->height = 0;

	// First measure children.
	for (child = box->cell.children; child != NULL; child = child->next) {
		if (child->measure != NULL)
			child->measure(child, vg);
		else
			child->content.width = child->content.height = 0;
	}

	// Adapt to child size based on direction.
	if (box->dir == MI_COL) {
		// Col
		for (child = box->cell.children; child != NULL; child = child->next) {
			size->width = maxf(size->width, child->content.width + child->paddingx*2);
			size->height += child->content.height + child->paddingy*2;
			if (child->next != NULL) size->height += child->spacing;
		}
	} else {
		// Row
		for (child = box->cell.children; child != NULL; child = child->next) {
			size->width += child->content.width + child->paddingx*2;
			size->height = maxf(size->height, child->content.height + child->paddingy*2);
			if (child->next != NULL) size->width += child->spacing;
		}
	}

	// Apply forced width
	if (box->width > 0)
		size->width = box->width;
	if (box->height > 0)
		size->height = box->height;
}

static int milli__boxLogic(struct MIcell* cell, int event, struct MIinputState* input)
{
	struct MIbox* box = (struct MIbox*)cell;
	MILLI_NOTUSED(event);
	MILLI_NOTUSED(input);
	return 0;
}

static int milli__boxParseAlign(const char* str)
{
	switch(str[0]) {
	case 's': return MI_START;
	case 'e': return MI_END;
	case 'c': return MI_CENTER;
	case 'j': return MI_JUSTIFY;
	}
	return -1;
}

static int milli__boxParseOverflow(const char* str)
{
	switch(str[0]) {
	case 'f': return MI_FIT;
	case 'h': return MI_HIDDEN;
	case 's': return MI_SCROLL;
	case 'v': return MI_VISIBLE;
	}
	return -1;
}

static int milli__boxParseDir(const char* str)
{
	switch(str[0]) {
	case 'r': return MI_ROW;
	case 'c': return MI_COL;
	}
	return -1;
}

static void milli__boxParam(struct MIcell* cell, struct MIparam* p)
{
	struct MIbox* box = (struct MIbox*)cell;
	int valid = 0;

	if (miCellParam(cell, p)) return;

	if (strcmp(p->key, "dir") == 0) {
		int dir = milli__boxParseDir(p->val);
		if (dir != -1) {
			box->dir = dir;
			valid = 1;
		}
	} else if (strcmp(p->key, "align") == 0) {
		int align = milli__boxParseAlign(p->val);
		if (align != -1) {
			box->align = align;
			valid = 1;
		}
	} else if (strcmp(p->key, "pack") == 0) {
		int pack = milli__boxParseAlign(p->val);
		if (pack != -1) {
			box->pack = pack;
			valid = 1;
		}
	} else if (strcmp(p->key, "overflow") == 0) {
		int overflow = milli__boxParseOverflow(p->val);
		if (overflow != -1) {
			box->overflow = overflow;
			valid = 1;
		}
	} else if (strcmp(p->key, "width") == 0) {
		float width = -1;
		sscanf(p->val, "%f", &width);
		if (width > 0 && width < 10000) {
			box->width = width;
			valid = 1;
		}
	} else if (strcmp(p->key, "height") == 0) {
		float height = -1;
		sscanf(p->val, "%f", &height);
		if (height > 0 && height < 10000) {
			box->height = height;
			valid = 1;
		}
	}
	if (!valid)
		printf("Box: invalid parameter: %s=%s\n", p->key, p->val);
}

static void milli__boxDtor(struct MIcell* cell)
{
	struct MIbox* box = (struct MIbox*)cell;
}

struct MIcell* miCreateBox(const char* params)
{
	struct MIbox* box = (struct MIbox*)calloc(1, sizeof(struct MIbox));
	if (box == NULL) goto error;
	box->cell.render = milli__boxRender;
	box->cell.layout = milli__boxLayout;
	box->cell.logic = milli__boxLogic;
	box->cell.measure = milli__boxMeasure;
	box->cell.param = milli__boxParam;
	box->cell.dtor = milli__boxDtor;
	box->align = MI_START;
	box->pack = MI_START;
	box->overflow = MI_HIDDEN;
	miSet((struct MIcell*)box, params);
	return (struct MIcell*)box;
error:
	return NULL;
}


static void milli__textRender(struct MIcell* cell, struct NVGcontext* vg, struct MIrect* view)
{
	struct MItext* text = (struct MItext*)cell;
	struct MIrect rect = text->cell.frame;
	MILLI_NOTUSED(view);

	if (text->text == NULL) return;	

	nvgBeginPath(vg);
	nvgRect(vg, rect.x, rect.y, rect.width, rect.height);
	nvgFillColor(vg, nvgRGBA(0,255,0,32));
	nvgFill(vg);

	nvgFillColor(vg, nvgRGBA(255,255,255,255));

	nvgFontFace(vg, text->fontFace);
	nvgFontSize(vg, text->fontSize);

	if (text->align == MI_CENTER) {
		nvgTextAlign(vg, NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
		nvgText(vg, cell->frame.x + cell->frame.width/2, cell->frame.y + cell->frame.height/2, text->text, NULL);
	} else if (text->align == MI_END) {
		nvgTextAlign(vg, NVG_ALIGN_RIGHT|NVG_ALIGN_MIDDLE);
		nvgText(vg, cell->frame.x + cell->frame.width - cell->paddingx, cell->frame.y + cell->frame.height/2, text->text, NULL);
	} else {
		nvgTextAlign(vg, NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);
		nvgText(vg, cell->frame.x + cell->paddingx, cell->frame.y + cell->frame.height/2, text->text, NULL);
	}
}

int milli__textLayout(struct MIcell* cell, struct NVGcontext* vg)
{
	MILLI_NOTUSED(cell);
	MILLI_NOTUSED(vg);
/*	struct MItext* text = (struct MItext*)cell;

	nvgFontFace(vg, text->fontFace);
	nvgFontSize(vg, text->fontSize);
	text->cell.content.width = text->text != NULL ? nvgTextBounds(vg, 0,0, text->text, NULL, NULL) : 0;
	nvgTextMetrics(vg, NULL, NULL, &text->cell.content.height);
*/
	return 0;
}

static void milli__textMeasure(struct MIcell* cell, struct NVGcontext* vg)
{
	struct MItext* text = (struct MItext*)cell;
	struct MIsize* size = &text->cell.content;
	nvgFontFace(vg, text->fontFace);
	nvgFontSize(vg, text->fontSize);
	size->width = text->text != NULL ? nvgTextBounds(vg, 0,0, text->text, NULL, NULL) : 10;
	nvgTextMetrics(vg, NULL, NULL, &size->height);
}

static int milli__textLogic(struct MIcell* cell, int event, struct MIinputState* input)
{
	struct MItext* text = (struct MItext*)cell;
	MILLI_NOTUSED(event);
	MILLI_NOTUSED(input);
	return 0;
}

static void milli__textParam(struct MIcell* cell, struct MIparam* p)
{
	struct MItext* text = (struct MItext*)cell;
	int valid = 0;

	if (miCellParam(cell, p)) return;

	if (strcmp(p->key, "label") == 0) {
		if (p->val != NULL && strlen(p->val) > 0) {
			if (text->text != NULL) free(text->text);
			text->text = strdup(p->val);
			valid = 1;
		}
	} else if (strcmp(p->key, "font-size") == 0) {
		float size = -1;
		sscanf(p->val, "%f", &size);
		if (size >= 0 && size < 100) {
			text->fontSize = size;
			valid = 1;
		}
	} else if (strcmp(p->key, "line-height") == 0) {
		float height = -1;
		sscanf(p->val, "%f", &height);
		if (height >= 0 && height < 5) {
			text->lineHeight = height;
			valid = 1;
		}
	} else if (strcmp(p->key, "font-face") == 0) {
		if (p->val != NULL && strlen(p->val) > 0) {
			if (text->fontFace != NULL) free(text->fontFace);
			text->fontFace = strdup(p->val);
			valid = 1;
		}
	} else if (strcmp(p->key, "align") == 0) {
		int align = milli__boxParseAlign(p->val);
		if (align != -1) {
			text->align = align;
			valid = 1;
		}
	} else if (strcmp(p->key, "pack") == 0) {
		int pack = milli__boxParseAlign(p->val);
		if (pack != -1) {
			text->pack = pack;
			valid = 1;
		}
	}

	if (!valid)
		printf("Text: invalid parameter: %s=%s\n", p->key, p->val);
}

static void milli__textDtor(struct MIcell* cell)
{
	struct MItext* text = (struct MItext*)cell;
	free(text->text);
	free(text->fontFace);
}

struct MIcell* miCreateText(const char* params)
{
	struct MItext* text = (struct MItext*)calloc(1, sizeof(struct MItext));
	if (text == NULL) goto error;
	text->cell.render = milli__textRender;
	text->cell.layout = milli__textLayout;
	text->cell.logic = milli__textLogic;
	text->cell.measure = milli__textMeasure;
	text->cell.param = milli__textParam;
	text->cell.dtor = milli__textDtor;
	text->fontFace = strdup("sans");
	text->fontSize = 18.0f;
	text->lineHeight = 1.2f;
	text->align = MI_START;
	text->pack = MI_CENTER;
	miSet((struct MIcell*)text, params);
	return (struct MIcell*)text;
error:
	return NULL;
}



static void milli__iconRender(struct MIcell* cell, struct NVGcontext* vg, struct MIrect* view)
{
	struct MIicon* icon = (struct MIicon*)cell;
	struct MIrect rect = icon->cell.frame;
	struct MIcolor color = {255,255,255,255};

	nvgBeginPath(vg);
	nvgRect(vg, rect.x, rect.y, rect.width, rect.height);
	nvgFillColor(vg, nvgRGBA(255,0,0,32));
	nvgFill(vg);

	drawIcon(vg, &rect, icon->image->image, &color);
}

int milli__iconLayout(struct MIcell* cell, struct NVGcontext* vg)
{
	MILLI_NOTUSED(cell);
	MILLI_NOTUSED(vg);
/*	struct MIicon* icon = (struct MIicon*)cell;
	if (icon->image != NULL) {
		icon->cell.content.width = icon->image->image->width;
		icon->cell.content.height = icon->image->image->height;
	} else {
		icon->cell.content.width = 1;
		icon->cell.content.height = 1;
	}
	if (icon->width > 0) icon->cell.content.width = icon->width;
	if (icon->height > 0) icon->cell.content.height = icon->height;
*/
	return 0;
}

static void milli__iconMeasure(struct MIcell* cell, struct NVGcontext* vg)
{
	struct MIicon* icon = (struct MIicon*)cell;
	struct MIsize* size = &icon->cell.content;
	if (icon->image != NULL) {
		size->width = icon->image->image->width;
		size->height = icon->image->image->height;
	} else {
		size->width = 1;
		size->height = 1;
	}
	if (icon->width > 0) size->width = icon->width;
	if (icon->height > 0) size->height = icon->height;
}

static int milli__iconLogic(struct MIcell* cell, int event, struct MIinputState* input)
{
	struct MIicon* icon = (struct MIicon*)cell;
	MILLI_NOTUSED(event);
	MILLI_NOTUSED(input);
	return 0;
}

static void milli__iconParam(struct MIcell* cell, struct MIparam* p)
{
	struct MIicon* icon = (struct MIicon*)cell;
	int valid = 0;

	if (miCellParam(cell, p)) return;

	if (strcmp(p->key, "icon") == 0) {
		struct MIiconImage* image = findIcon(p->val);
		if (image != NULL) {
			icon->image = image;
			valid = 1;
		}
	} else if (strcmp(p->key, "width") == 0) {
		float width = -1;
		sscanf(p->val, "%f", &width);
		if (width > 0 && width < 10000) {
			icon->width = width;
			valid = 1;
		}
	} else if (strcmp(p->key, "height") == 0) {
		float height = -1;
		sscanf(p->val, "%f", &height);
		if (height > 0 && height < 10000) {
			icon->width = height;
			valid = 1;
		}
	}

	if (!valid)
		printf("Icon: invalid parameter: %s=%s\n", p->key, p->val);

//	static struct MIiconImage* findIcon(const char* name)

}

static void milli__iconDtor(struct MIcell* cell)
{
	struct MIicon* icon = (struct MIicon*)cell;
}

struct MIcell* miCreateIcon(const char* params)
{
	struct MIicon* icon = (struct MIicon*)calloc(1, sizeof(struct MIicon));
	if (icon == NULL) goto error;
	icon->cell.render = milli__iconRender;
	icon->cell.layout = milli__iconLayout;
	icon->cell.logic = milli__iconLogic;
	icon->cell.measure = milli__iconMeasure;
	icon->cell.param = milli__iconParam;
	icon->cell.dtor = milli__iconDtor;
	icon->width = -1;
	icon->height = -1;
	miSet((struct MIcell*)icon, params);
	return (struct MIcell*)icon;
error:
	return NULL;
}



static void milli__templateRender(struct MIcell* cell, struct NVGcontext* vg, struct MIrect* view)
{
	struct MItemplate* tmpl = (struct MItemplate*)cell;
	if (tmpl->host->render)
		tmpl->host->render(tmpl->host, vg, view);
}

static int milli__templateLayout(struct MIcell* cell, struct NVGcontext* vg)
{
	struct MItemplate* tmpl = (struct MItemplate*)cell;
	tmpl->host->frame = tmpl->cell.frame;
	if (tmpl->host->layout)
		return tmpl->host->layout(tmpl->host, vg);
	return 0;
}

static void milli__templateMeasure(struct MIcell* cell, struct NVGcontext* vg)
{
	struct MItemplate* tmpl = (struct MItemplate*)cell;
	if (tmpl->host->measure) {
		tmpl->host->measure(tmpl->host, vg);
		tmpl->cell.content = tmpl->host->content;
	} else {
		tmpl->cell.content.width = tmpl->cell.content.height = 0;
	}
}

static int milli__templateLogic(struct MIcell* cell, int event, struct MIinputState* input)
{
	struct MItemplate* tmpl = (struct MItemplate*)cell;
	if (tmpl->host->logic)
		return tmpl->host->logic(tmpl->host, event, input);
	return 0;
}


static int milli__templateSetVar(struct MIcell* cell, char* name, char* val)
{
	struct MIvar* var;

	if (cell == NULL) return 0;

	var = milli__findVar(cell, name);
	if (var != NULL) {
		struct MIparam p = {var->key, val, NULL};
//		printf("Template calling: %s=%s -> %s=%s\n", name,val, p.key, p.val);
		if (cell->param != NULL)
			cell->param(cell, &p);
		return 1;
	}

	if (milli__templateSetVar(cell->next, name, val)) return 1;
	if (milli__templateSetVar(cell->children, name, val)) return 1;
	return 0;
}

static void milli__templateParam(struct MIcell* cell, struct MIparam* p)
{
	struct MItemplate* tmpl = (struct MItemplate*)cell;

//	printf("template %s=%s\n", p->key, p->val);

	// First try to user variables
	if (milli__templateSetVar(tmpl->host, p->key, p->val))
		return;

	// Finally pass to host
	if (tmpl->host->param)
		tmpl->host->param(tmpl->host, p);

	tmpl->cell.grow = tmpl->host->grow;
	tmpl->cell.paddingx = tmpl->host->paddingx;
	tmpl->cell.paddingy = tmpl->host->paddingy;
	tmpl->cell.spacing = tmpl->host->spacing;

/*		int valid = 0;
		if (miCellParam(cell, p)) continue;
		if (strcmp(p->key, "icon") == 0) {
			struct MIiconImage* image = findIcon(p->val);
			if (image != NULL) {
				icon->image = image;
				valid = 1;
			}
		} else if (strcmp(p->key, "width") == 0) {
			float width = -1;
			sscanf(p->val, "%f", &width);
			if (width > 0 && width < 10000) {
				icon->width = width;
				valid = 1;
			}
		} else if (strcmp(p->key, "height") == 0) {
			float height = -1;
			sscanf(p->val, "%f", &height);
			if (height > 0 && height < 10000) {
				icon->width = height;
				valid = 1;
			}
		}
		if (!valid)
			printf("Icon: invalid parameter: %s=%s\n", p->key, p->val);*/
}

static void milli__templateDtor(struct MIcell* cell)
{
	struct MItemplate* tmpl = (struct MItemplate*)cell;
	if (tmpl->host->dtor)
		return tmpl->host->dtor(tmpl->host);
}

struct MIcell* miCreateTemplate(struct MIcell* host)
{
	struct MItemplate* tmpl = (struct MItemplate*)calloc(1, sizeof(struct MItemplate));
	if (tmpl == NULL) goto error;
	tmpl->cell.render = milli__templateRender;
	tmpl->cell.layout = milli__templateLayout;
	tmpl->cell.logic = milli__templateLogic;
	tmpl->cell.measure = milli__templateMeasure;
	tmpl->cell.param = milli__templateParam;
	tmpl->cell.dtor = milli__templateDtor;
	tmpl->host = host;
	tmpl->cell.grow = tmpl->host->grow;
	tmpl->cell.paddingx = tmpl->host->paddingx;
	tmpl->cell.paddingy = tmpl->host->paddingy;
	tmpl->cell.spacing = tmpl->host->spacing;

//	miSet((struct MIcell*)tmpl, params);
	return (struct MIcell*)tmpl;
error:
	return NULL;
}


struct MIcell* miCreateButton(const char* params)
{
	struct MIcell* tmpl = NULL;
	struct MIcell* button = NULL;

	button = miCreateBox("id={id} dir=row align=justify padding='5 5' spacing=5 height=20");
		miAddChild(button, miCreateText("label={label}"));

	tmpl = miCreateTemplate(button);
	miSet(tmpl, params);

	return tmpl;
}



struct MIcell* miCreateIconButton(const char* params)
{
	struct MIcell* tmpl = NULL;
	struct MIcell* button = NULL;

	button = miCreateBox("id={id} dir=row align=justify padding='5 5' spacing=5 height=20");
		miAddChild(button, miCreateIcon("icon={icon} spacing=5"));
		miAddChild(button, miCreateText("label={label}"));

	tmpl = miCreateTemplate(button);
	miSet(tmpl, params);

	return tmpl;
}



void miAddChild(struct MIcell* parent, struct MIcell* child)
{
	struct MIcell** prev = NULL;
	if (parent == NULL) return;
	if (child == NULL) return;
	prev = &parent->children;
	while (*prev != NULL)
		prev = &(*prev)->next;
	*prev = child;
	child->parent = parent;
}


void miFreeCell(struct MIcell* cell)
{
	if (cell == NULL) return;
	miFreeCell(cell->children);
	miFreeCell(cell->next);
	if (cell->dtor) cell->dtor(cell);
	free(cell->id);
	milli__freeVars(cell->vars);
	free(cell);
}

void miSet(struct MIcell* cell, const char* params)
{
	struct MIparam* p = miParseParams(params);
	struct MIparam* it = p;
	for (it = p; it != NULL; it = it->next) {
		if (it->val[0] == '{') {
			char* name = strdup(it->val+1);
			name[strlen(name)-1] = '\0';
			milli__addVar(cell, name, it->key);
			free(name);
		} else {
			cell->param(cell, it);
		}
	}
	miFreeParams(p);
}

void miLayout(struct MIcell* cell, struct NVGcontext* vg)
{
	if (cell == NULL) return;

	if (cell->measure != NULL)
		cell->measure(cell, vg);

	cell->frame.x = 0;
	cell->frame.y = 0;
	cell->frame.width = cell->content.width + cell->paddingx*2;
	cell->frame.height = cell->content.height + cell->paddingy*2;
	if (cell->layout != NULL)
		cell->layout(cell, vg);
}

void miRender(struct MIcell* cell, struct NVGcontext* vg)
{
	if (cell == NULL) return;
	if (cell->render != NULL)
		cell->render(cell, vg, NULL);
}
