#include <EGL/egl.h>
#include <GLES3/gl32.h>

#include <X11/Xlib.h>

#include <stdlib.h>
#include <stdio.h>

#include "ctx.h"

// functions
static bool init();
static void cleanup();

static EGLConfig egl_choose_config(EGLDisplay disp);
static bool egl_init();
static bool egl_create_context(EGL_ctx *ctx_es);

static Window x_create_window(int vis_id, int win_w, int win_h);
static bool handle_xevent(XEvent *ev);

static bool gl_init();

static void display();
static void reshape(int w, int h);
static bool keyboard(KeySym sym);

// variables
static EGL_ctx ctx_es;
//static EGL_ctx ctx_angle;

static GLuint gl_tex;
static unsigned char *pixels;

static int xscr;
static Display *xdpy;
static Window xroot;
static Window win;
static Atom xa_wm_proto;
static Atom xa_wm_del_win;
static int win_width, win_height;
static bool redraw_pending;

int main(int argc, char **argv)
{
    if (!init()) {
        fprintf(stderr, "Failed to initialize EGL context.\n");
        return 1;
    }

    eglMakeCurrent(ctx_es.disp, ctx_es.surf, ctx_es.surf, ctx_es.ctx);
    if (!gl_init())
        return 1;

    // event loop
    for (;;) {
        XEvent xev;
        XNextEvent(xdpy, &xev);
        if (!handle_xevent(&xev))
            break;
        if (redraw_pending) {
            redraw_pending = false;
            display();
        }
    }

    cleanup();
    return 0;
}

static bool
init()
{
    if (!(xdpy = XOpenDisplay(0))) {
        fprintf(stderr, "Failed to connect to the X server.\n");
        return 1;
    }

    xscr = DefaultScreen(xdpy);
    xroot = RootWindow(xdpy, xscr);

    xa_wm_proto = XInternAtom(xdpy, "WM_PROTOCOLS", False);
    xa_wm_del_win = XInternAtom(xdpy, "WM_DELETE_WINDOW", False);

    // EGL ctx
    if (!egl_init())
        return false;

    ctx_es.config = egl_choose_config(ctx_es.disp);
    if (!ctx_es.config)
        return false;

    // NOTE to myself:
    // This visual id that is taken by EGL and should match the angle visual
    // but only here that I want the 2 contexts to share the same X window.
    // WebKit is rendering to textures that are composited afterwards, so I won't
    // have to care about it.
    EGLint vis_id;
    eglGetConfigAttrib(ctx_es.disp, ctx_es.config, EGL_NATIVE_VISUAL_ID, &vis_id);

    // NOTE to myself:
    // create x window: this is going to be used by both contexts
    ////////////////////////////////////////////////////////////
    win = x_create_window(vis_id, 800, 600);
    if (!win)
        return false;
    ////////////////////////////////////////////////////////////

    ctx_es.surf = eglCreateWindowSurface(ctx_es.disp, ctx_es.config, win, 0);
    if (ctx_es.surf == EGL_NO_SURFACE) {
        fprintf(stderr, "Failed to create EGL surface for win.\n");
        return false;
    }

    eglBindAPI(EGL_OPENGL_ES_API);

    if (!egl_create_context(&ctx_es))
        return false;

    if (eglGetError() != EGL_SUCCESS) {
        fprintf(stderr, "EGL error detected.\n");
        return false;
    }

    return true;
}

static bool
egl_init()
{
    // create an EGL display
    if ((ctx_es.disp = eglGetDisplay(EGL_DEFAULT_DISPLAY)) == EGL_NO_DISPLAY) {
        fprintf(stderr, "Failed to get EGL display.\n");
        return false;
    }

    if (!eglInitialize(ctx_es.disp, NULL, NULL)) {
        fprintf(stderr, "Failed to initialize EGL.\n");
        return false;
    }

    return (eglGetError() == EGL_SUCCESS);
}

static EGLConfig
egl_choose_config(EGLDisplay disp)
{
    // select an EGL configuration
    EGLint attr_list[] = {
        EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_STENCIL_SIZE, EGL_DONT_CARE,
        EGL_NONE
    };

    EGLConfig config;
    EGLint num_configs;
    if (!eglChooseConfig(disp, attr_list, &config, 1, &num_configs)) {
        fprintf(stderr, "Failed to find a suitable EGL config.\n");
        return 0;
    }

    return config;
}

static bool
egl_create_context(EGL_ctx *ctx_es)
{
    eglBindAPI(EGL_OPENGL_ES_API);

    EGLint ctx_atts[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE };

    ctx_es->ctx = eglCreateContext(ctx_es->disp, ctx_es->config, /* shared_context_here */ EGL_NO_CONTEXT, ctx_atts);
    if (!ctx_es->ctx) {
        fprintf(stderr, "Failed to create EGL context.\n");
        return false;
    }

    return (eglGetError() == EGL_SUCCESS);
}

Window
x_create_window(int vis_id, int win_w, int win_h)
{
    Window win;

    XVisualInfo vis_info_match = {0};
    vis_info_match.visualid = vis_id;

    XVisualInfo *vis_info;
    int num_visuals;

    if (!(vis_info = XGetVisualInfo(xdpy, VisualIDMask, &vis_info_match, &num_visuals))) {
        fprintf(stderr, "Failed to get visual info (X11).\n");
        return 0;
    }

    XSetWindowAttributes xattr;
    xattr.background_pixel = BlackPixel(xdpy, xscr);
    xattr.colormap = XCreateColormap(xdpy, xroot, vis_info->visual, AllocNone);

    // create an X window
    win = XCreateWindow(xdpy, xroot,
                               0, 0, win_w, win_h,
                               0, vis_info->depth,
                               InputOutput, vis_info->visual,
                               CWBackPixel | CWColormap, &xattr);

    if (!win) {
        fprintf(stderr, "Failed to create X11 window.\n");
        return 0;
    }

    // X events we receive
    XSelectInput(xdpy, win,
                 ExposureMask | StructureNotifyMask | KeyPressMask);

    // Window title
    XTextProperty tex_prop;
    const char *title = "Shared context proof of concept";
    XStringListToTextProperty((char**)&title, 1, &tex_prop);
    XSetWMName(xdpy, win, &tex_prop);
    XFree(tex_prop.value);

    // Window manager protocols
    XSetWMProtocols(xdpy, win, &xa_wm_del_win, 1);
    XMapWindow(xdpy, win);
    XSync(xdpy, 0);
    XSync(xdpy, 0);

    return win;
}

static bool
handle_xevent(XEvent *ev)
{
    static bool mapped;
    KeySym sym;

    switch(ev->type) {
    case MapNotify:
        mapped = true;
        break;
    case UnmapNotify:
        mapped = false;
        break;
    case ConfigureNotify:
        if (ev->xconfigure.width != win_width ||
                ev->xconfigure.height != win_height)
        {
            win_width = ev->xconfigure.width;
            win_height = ev->xconfigure.height;

            reshape(win_width, win_height);
        }
        break;
    case ClientMessage:
        if (ev->xclient.message_type == xa_wm_proto) {
            if ((Atom)ev->xclient.data.l[0] == xa_wm_del_win) {
                return false;
            }
        }
        break;
    case Expose:
        if (mapped)
            redraw_pending = true;
        break;
    case KeyPress:
        if(!(sym = XLookupKeysym(&ev->xkey, 0)))
            break;
        if (!keyboard(sym))
            return false;
        break;
    default:
        break;
    }
    return true;
}

static void
cleanup()
{
    eglTerminate(ctx_es.disp);
}

static bool
gl_init()
{
    glGenTextures(1, &gl_tex);
    glBindTexture(GL_TEXTURE_2D, gl_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, win_width, win_height, 0, GL_RGBA, GL_FLOAT, pixels);
	glBindTexture(GL_TEXTURE_2D, 0);

    return true; //glGetError() == GL_NO_ERROR;
}

static void
display()
{
    // make the EGL context current
    eglMakeCurrent(ctx_es.disp, ctx_es.surf, ctx_es.surf, ctx_es.ctx);

    glClearColor(1.0, 1.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    eglSwapBuffers(ctx_es.disp, ctx_es.surf);

    // make the angle context current
}

static void
reshape(int w, int h)
{
    // FIXME
    //glViewport(0, 0, w, h);
}

static bool
keyboard(KeySym sym)
{
    switch (sym) {
    case XK_Escape:
        return false;
    default:
        break;
    }
    return true;
}
