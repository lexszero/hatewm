#include <errno.h>
#include <locale.h>
#include <stdarg.h>
#include <stdbool.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>

#define DEBUG		1
#define DPRINTF 	if (DEBUG) printf

typedef struct Client Client;
typedef struct Monitor Monitor;
struct Client {
	char name[256];
	int x, y, w, h;
	bool urgent, visible;
	Client *next;
	Monitor *mon;
	Window win;
};
struct Monitor {
	int num;
	int x, y, w, h;
	Client *clients;
	Client *sel;
};

// X shits
enum { NetSupported, NetWMName, NetLast };              /* EWMH atoms */
enum { WMProtocols, WMDelete, WMState, WMLast };        /* default atoms */
static Atom wmatom[WMLast], netatom[NetLast];

static char *evtype[LASTEvent] = {
	[ConfigureRequest] = "configurerequest",
	[ConfigureNotify] = "configurenotify",
	[DestroyNotify] = "destroynotify",
	[EnterNotify] = "enternotify",
	[Expose] = "expose",
	[FocusIn] = "focusin",
	[MappingNotify] = "mappingnotify",
	[MapRequest] = "maprequest",
	[PropertyNotify] = "propertynotify",
	[UnmapNotify] = "unmapnotify"
};

static Display *dpy;
static int (*xerrorxlib)(Display *, XErrorEvent *);
static Window root;

static int sw, sh;           /* X display screen geometry width, height */
Monitor m;
static int screen;
bool otherwm = false, running = true;

static void checkotherwm(void);
static void die(const char *errstr, ...);
static long getstate(Window w);
static void init(void);
static void scan(void);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);

void checkotherwm(void) {
	xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSync(dpy, false);
	if(otherwm)
		die("another window manager is already running\n");
	XSetErrorHandler(xerror);
	XSync(dpy, false);
}

void die(const char *errstr, ...) {
	va_list ap;
	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(-1);
}

long getstate(Window w) {
	int format, status;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	status = XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
	                            &real, &format, &n, &extra, (unsigned char **)&p);
	if(status != Success)
		return -1;
	if(n != 0)
		result = *p;
	XFree(p);
	return result;
}

void init(void) {
	XSetWindowAttributes wa;

	/* init screen */
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	
	DPRINTF("Root: %x\n", root);
	m.num = 0;
	m.x = 0;
	m.y = 0;
	m.w = sw;
	m.h = sh;
	m.clients = NULL;

	/* init atoms */
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);

	/* select for events */
	wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask|ButtonPressMask
	                |EnterWindowMask|LeaveWindowMask|StructureNotifyMask
	                |PropertyChangeMask;
	XSelectInput(dpy, root, wa.event_mask);
}

void addwindow(Window w) {
	XWindowAttributes wa;
	
	if (!XGetWindowAttributes(dpy, w, &wa) || ! ((wa.map_state == IsViewable || getstate(w) == IconicState)))
		return;
	
	Client *c = malloc(sizeof(Client));
	c->win = w;
	c->x = wa.x;
	c->y = wa.y;
	c->w = wa.width;
	c->h = wa.height;
	DPRINTF("%x %i %i %i %i\n", c->win, c->x, c->y, c->w, c->h);
	
	c->mon = &m; // temp
	c->next = m.clients;
	m.clients = c;
	// need to throw ConfigureRequest to our listeners for each window
}

int xeventtostr(XEvent *ev, char *str, size_t len) {
	switch (ev->type) {
		case (ConfigureRequest): 
				{
					XConfigureRequestEvent e = ev->xconfigurerequest;
					return snprintf(str, len, "%s %x %i %i %i %i\n", evtype[ev->type], (int) e.window, e.x, e.y, e.width, e.height);
					break;
				}
		default:
				return snprintf(str, len, "%s\n", evtype[ev->type]);
	}
	return 0;
}

void scan(void) {
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	
	// Initially get all windows
	if(XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for(i = 0; i < num; i++)
			addwindow(wins[i]);
		if(wins)
			XFree(wins);
	}
}

void run() {
	XEvent ev;
	XSync(dpy, False);
	char *str = malloc(1024);
	while(running && !XNextEvent(dpy, &ev)) {
		if(evtype[ev.type]) {
			xeventtostr(&ev, str, 1024);
			DPRINTF("%s", str);
		}
	}
	free(str);
}

int xerrorstart(Display *dpy, XErrorEvent *ee) {
	otherwm = true;
	return -1;
}

int xerror(Display *dpy, XErrorEvent *ee) {
	if(ee->error_code == BadWindow
	|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "fatal error: request code=%d, error code=%d\n", ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}

int main(int argc, char **argv) {
	if(!(dpy = XOpenDisplay(NULL)))
		die("cannot open display\n");
	checkotherwm();
	init();
	scan();
	run();
}
