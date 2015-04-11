// 7 april 2015
#include "uipriv.h"

typedef struct stack stack;
typedef struct stackControl stackControl;

struct stack {
	stackControl *controls;
	uintmax_t len;
	uintmax_t cap;
	int vertical;
	uintptr_t parent;
	int padded;
	int userHid;
	int containerHid;
	int userDisabled;
	int containerDisabled;
};

struct stackControl {
	uiControl *c;
	int stretchy;
	intmax_t width;		// both used by resize(); preallocated to save time and reduce risk of failure
	intmax_t height;
};

static void stackDestroy(uiControl *c)
{
	stack *s = (stack *) (c->data);
	uintmax_t i;

	for (i = 0; i < s->len; i++)
		uiControlDestroy(s->controls[i].c);
	uiFree(s->controls);
	uiFree(s);
	uiFree(c);
}

static uintptr_t stackHandle(uiControl *c)
{
	return 0;
}

static void stackSetParent(uiControl *c, uintptr_t parent)
{
	stack *s = (stack *) (c->data);
	uintmax_t i;

	s->parent = parent;
	for (i = 0; i < s->len; i++)
		uiControlSetParent(s->controls[i].c, s->parent);
	updateParent(s->parent);
}

static void stackRemoveParent(uiControl *c)
{
	stack *s = (stack *) (c->data);
	uintmax_t i;
	uintptr_t oldparent;

	oldparent = s->parent;
	s->parent = 0;
	for (i = 0; i < s->len; i++)
		uiControlRemoveParent(s->controls[i].c);
	updateParent(oldparent);
}

static void stackPreferredSize(uiControl *c, uiSizing *d, intmax_t *width, intmax_t *height)
{
	stack *s = (stack *) (c->data);
	int xpadding, ypadding;
	uintmax_t nStretchy;
	// these two contain the largest preferred width and height of all stretchy controls in the stack
	// all stretchy controls will use this value to determine the final preferred size
	intmax_t maxStretchyWidth, maxStretchyHeight;
	uintmax_t i;
	intmax_t preferredWidth, preferredHeight;

	*width = 0;
	*height = 0;
	if (s->len == 0)
		return;

	// 0) get this Stack's padding
	xpadding = 0;
	ypadding = 0;
	if (s->padded) {
		xpadding = d->xPadding;
		ypadding = d->yPadding;
	}

	// 1) initialize the desired rect with the needed padding
	if (s->vertical)
		*height = (s->len - 1) * ypadding;
	else
		*width = (s->len - 1) * xpadding;

	// 2) add in the size of non-stretchy controls and get (but not add in) the largest widths and heights of stretchy controls
	// we still add in like direction of stretchy controls
	nStretchy = 0;
	maxStretchyWidth = 0;
	maxStretchyHeight = 0;
	for (i = 0; i < s->len; i++) {
		if (!uiControlVisible(s->controls[i].c))
			continue;
		uiControlPreferredSize(s->controls[i].c, d, &preferredWidth, &preferredHeight);
		if (s->controls[i].stretchy) {
			nStretchy++;
			if (maxStretchyWidth < preferredWidth)
				maxStretchyWidth = preferredWidth;
			if (maxStretchyHeight < preferredHeight)
				maxStretchyHeight = preferredHeight;
		}
		if (s->vertical) {
			if (*width < preferredWidth)
				*width = preferredWidth;
			if (!s->controls[i].stretchy)
				*height += preferredHeight;
		} else {
			if (!s->controls[i].stretchy)
				*width += preferredWidth;
			if (*height < preferredHeight)
				*height = preferredHeight;
		}
	}

	// 3) and now we can add in stretchy controls
	if (s->vertical)
		*height += nStretchy * maxStretchyHeight;
	else
		*width += nStretchy * maxStretchyWidth;
}

static void stackResize(uiControl *c, intmax_t x, intmax_t y, intmax_t width, intmax_t height, uiSizing *d)
{
	stack *s = (stack *) (c->data);
	int xpadding, ypadding;
	uintmax_t nStretchy;
	intmax_t stretchywid, stretchyht;
	uintmax_t i;
	intmax_t preferredWidth, preferredHeight;

	if (s->len == 0)
		return;

	// -1) get this Stack's padding
	xpadding = 0;
	ypadding = 0;
	if (s->padded) {
		xpadding = d->xPadding;
		ypadding = d->yPadding;
	}

	// 0) inset the available rect by the needed padding
	if (s->vertical)
		height -= (s->len - 1) * ypadding;
	else
		width -= (s->len - 1) * xpadding;

	// 1) get width and height of non-stretchy controls
	// this will tell us how much space will be left for stretchy controls
	stretchywid = width;
	stretchyht = height;
	nStretchy = 0;
	for (i = 0; i < s->len; i++) {
		if (!uiControlVisible(s->controls[i].c))
			continue;
		if (s->controls[i].stretchy) {
			nStretchy++;
			continue;
		}
		c = s->controls[i].c;
		uiControlPreferredSize(s->controls[i].c, d, &preferredWidth, &preferredHeight);
		if (s->vertical) {		// all controls have same width
			s->controls[i].width = width;
			s->controls[i].height = preferredHeight;
			stretchyht -= preferredHeight;
		} else {				// all controls have same height
			s->controls[i].width = preferredWidth;
			s->controls[i].height = height;
			stretchywid -= preferredWidth;
		}
	}

	// 2) now get the size of stretchy controls
	if (nStretchy != 0)
		if (s->vertical)
			stretchyht /= nStretchy;
		else
			stretchywid /= nStretchy;
	for (i = 0; i < s->len; i++) {
		if (!uiControlVisible(s->controls[i].c))
			continue;
		if (s->controls[i].stretchy) {
			s->controls[i].width = stretchywid;
			s->controls[i].height = stretchyht;
		}
	}

	// 3) now we can position controls
	for (i = 0; i < s->len; i++) {
		if (!uiControlVisible(s->controls[i].c))
			continue;
		uiControlResize(s->controls[i].c, x, y, s->controls[i].width, s->controls[i].height, d);
		if (s->vertical)
			y += s->controls[i].height + ypadding;
		else
			x += s->controls[i].width + xpadding;
	}
}

static int stackVisible(uiControl *c)
{
	stack *s = (stack *) (c->data);

	return !(s->userHid);
}

static void stackShow(uiControl *c)
{
	stack *s = (stack *) (c->data);
	uintmax_t i;

	s->userHid = 0;
	if (!s->containerHid) {
		for (i = 0; i < s->len; i++)
			uiControlContainerShow(s->controls[i].c);
		updateParent(s->parent);
	}
}

static void stackHide(uiControl *c)
{
	stack *s = (stack *) (c->data);
	uintmax_t i;

	s->userHid = 1;
	for (i = 0; i < s->len; i++)
		uiControlContainerHide(s->controls[i].c);
	updateParent(s->parent);
}

static void stackContainerShow(uiControl *c)
{
	stack *s = (stack *) (c->data);
	uintmax_t i;

	s->containerHid = 0;
	if (!s->userHid) {
		for (i = 0; i < s->len; i++)
			uiControlContainerShow(s->controls[i].c);
		updateParent(s->parent);
	}
}

static void stackContainerHide(uiControl *c)
{
	stack *s = (stack *) (c->data);
	uintmax_t i;

	s->containerHid = 1;
	for (i = 0; i < s->len; i++)
		uiControlContainerHide(s->controls[i].c);
	updateParent(s->parent);
}

static void stackEnable(uiControl *c)
{
	stack *s = (stack *) (c->data);
	uintmax_t i;

	s->userDisabled = 0;
	if (!s->containerDisabled)
		for (i = 0; i < s->len; i++)
			uiControlContainerEnable(s->controls[i].c);
}

static void stackDisable(uiControl *c)
{
	stack *s = (stack *) (c->data);
	uintmax_t i;

	s->userDisabled = 1;
	for (i = 0; i < s->len; i++)
		uiControlContainerDisable(s->controls[i].c);
}

static void stackContainerEnable(uiControl *c)
{
	stack *s = (stack *) (c->data);
	uintmax_t i;

	s->containerDisabled = 0;
	if (!s->userDisabled)
		for (i = 0; i < s->len; i++)
			uiControlContainerEnable(s->controls[i].c);
}

static void stackContainerDisable(uiControl *c)
{
	stack *s = (stack *) (c->data);
	uintmax_t i;

	s->containerDisabled = 1;
	for (i = 0; i < s->len; i++)
		uiControlContainerDisable(s->controls[i].c);
}

uiControl *uiNewHorizontalStack(void)
{
	uiControl *c;
	stack *s;

	c = uiNew(uiControl);
	s = uiNew(stack);

	c->data = s;
	c->destroy = stackDestroy;
	c->handle = stackHandle;
	c->setParent = stackSetParent;
	c->removeParent = stackRemoveParent;
	c->preferredSize = stackPreferredSize;
	c->resize = stackResize;
	c->visible = stackVisible;
	c->show = stackShow;
	c->hide = stackHide;
	c->containerShow = stackContainerShow;
	c->containerHide = stackContainerHide;
	c->enable = stackEnable;
	c->disable = stackDisable;
	c->containerEnable = stackContainerEnable;
	c->containerDisable = stackContainerDisable;

	return c;
}

uiControl *uiNewVerticalStack(void)
{
	uiControl *c;
	stack *s;

	c = uiNewHorizontalStack();
	s = (stack *) (c->data);
	s->vertical = 1;
	return c;
}

#define stackCapGrow 32

void uiStackAdd(uiControl *st, uiControl *c, int stretchy)
{
	stack *s = (stack *) (st->data);

	if (s->len >= s->cap) {
		s->cap += stackCapGrow;
		s->controls = (stackControl *) uiRealloc(s->controls, s->cap * sizeof (stackControl *), "stackControl[]");
	}
	s->controls[s->len].c = c;
	s->controls[s->len].stretchy = stretchy;
	if (s->parent != 0)
		uiControlSetParent(s->controls[s->len].c, s->parent);
	s->len++;
	updateParent(s->parent);
}

int uiStackPadded(uiControl *c)
{
	stack *s = (stack *) (c->data);

	return s->padded;
}

void uiStackSetPadded(uiControl *c, int padded)
{
	stack *s = (stack *) (c->data);

	s->padded = padded;
	updateParent(s->parent);
}
