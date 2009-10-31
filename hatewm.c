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

#define DEBUG					1
#define DPRINTF 				if (DEBUG) printf

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
static Display *dpy;
static int (*xerrorxlib)(Display *, XErrorEvent *);
static Window root;

static int sw, sh;           /* X display screen geometry width, height */
Monitor m;
static int screen;
bool otherwm = false, running = true;

static void addwindow(Window w);
static void checkotherwm(void);
static void die(const char *errstr, ...);
static void eventf(const char *fmt, ...);
static long getstate(Window w);
static void init(void);
static void scan(void);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);

void h_configurerequest(XEvent *e);
void h_configurenotify(XEvent *e);
void h_destroynotify(XEvent *e);

static void (*handler[LASTEvent]) (XEvent *) = {
//	[ButtonPress] = buttonpress,
	[ConfigureRequest] = h_configurerequest,
	[ConfigureNotify] = h_configurenotify,
	[DestroyNotify] = h_destroynotify,
//	[EnterNotify] = enternotify,
//	[Expose] = expose,
//	[FocusIn] = focusin,
//	[MappingNotify] = mappingnotify,
//	[MapRequest] = maprequest,
//	[PropertyNotify] = propertynotify,
//	[UnmapNotify] = unmapnotify

};

void eventf(const char *fmt, ...) {
	int size = 1000;
	char *str = malloc(size);
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(str, size, fmt, ap);
	printf("%s", str);
	free(str);
}

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
	
	DPRINTF("Root: %i\n", root);
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
	
	c->mon = &m; // temp
	c->next = m.clients;
	m.clients = c;
	
	
	// need to throw ConfigureRequest to our listeners for each window
}

/* was xeventtostr
int xeventtostr(XEvent *e, char *str, size_t len) {
	XAnyEvent ae = e->xany;
	switch (e->type) {
		case ConfigureRequest:
				return snprintf(str, len, "%s %i %i %i %i %i\n", 
						evtype[e->type], 
						(int) ae.window, 
						e->xconfigurerequest.x, 
						e->xconfigurerequest.y, 
						e->xconfigurerequest.width, 
						e->xconfigurerequest.height);
				break;
		
		default:
				return snprintf(str, len, "%s %i\n", 
						evtype[e->type], 
						(int) ae.window);
	}
	return 0;
}
*/

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
	while(running && !XNextEvent(dpy, &ev)) {
		if(handler[ev.type]) 
			 handler[ev.type](&ev);		
	}
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

void h_configurerequest(XEvent *e) {
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XConfigureEvent ce;
	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = ev->window;
	ce.window = ev->window;
	ce.x = 100;
	ce.y = 100;
	ce.width = 200;
	ce.height = 200;
//	ce.border_width = c->bw;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(dpy, ev->window, False, StructureNotifyMask, (XEvent *)&ce);

	XSelectInput(dpy, ev->window, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);

	eventf("configurerequest %i %i %i %i %i\n", 
			(int) ((ev->window == root) ? 0 : ev->window),
			ev->x, 
			ev->y, 
			ev->width, 
			ev->height);
}

void h_configurenotify(XEvent *e) {
	XConfigureEvent *ev = &e->xconfigure;
	eventf("configurenotify %i %i %i %i %i\n", 
			(int) ((ev->window == root) ? 0 : ev->window),
			ev->x, 
			ev->y, 
			ev->width, 
			ev->height);

}

void h_destroynotify(XEvent *e) {
	eventf("destroyenotify %i\n",
			(int) e->xdestroywindow.event);
}

int main(int argc, char **argv) {
	if(!(dpy = XOpenDisplay(NULL)))
		die("cannot open display\n");
	checkotherwm();
	init();
	scan();
	run();
}
