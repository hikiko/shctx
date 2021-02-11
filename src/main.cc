/*
 * Copyright © 2021 Igalia S.L.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Author:
 *    Eleni Maria Stea <estea@igalia.com>
 */


#include <EGL/egl.h>
#include <GLES3/gl32.h>

#include <X11/Xlib.h>

#include <stdlib.h>
#include <stdio.h>

#include "ctx.h"
#include "sdr.h"

// functions
static bool init();
static void cleanup();

static EGLConfig egl_choose_config(EGLDisplay disp);
static bool egl_init();
static bool egl_create_context(EGL_ctx *ctx_es);

static Window x_create_window(int vis_id, int win_w, int win_h);
static bool handle_xevent(XEvent *ev);

static bool gl_init();
static void gl_cleanup();

static void display();
static void reshape(int w, int h);
static bool keyboard(KeySym sym);

// variables
static EGL_ctx ctx_es;
//static EGL_ctx ctx_angle;

static GLuint gl_tex;
static unsigned int gl_prog;
static GLuint gl_fbo;
static GLuint gl_rbo;
static GLuint gl_vbo;

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
    gl_cleanup();
    // FIXME
    // destroy context, surface, display
    // FIXME
    // destroy x window x dpy
    eglTerminate(ctx_es.disp);
}

static bool
gl_init()
{
	static const float vertices[] = {
		1.0, 1.0,
		1.0, 0.0,
		0.0, 1.0,
		0.0, 0.0
	};

	glGenBuffers(1, &gl_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, gl_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STATIC_DRAW);

	// xor image
	unsigned char pixels[256 * 256 * 4];
	unsigned char *pptr = pixels;
	for (int i = 0; i < 256; i++) {
		for (int j = 0; j < 256; j++) {
			int r = (i ^ j);
			int g = (i ^ j) << 1;
			int b = (i ^ j) << 2;

			*pptr++ = r;
			*pptr++ = g;
			*pptr++ = b;
			*pptr++ = 255;
		}
	}

    glGenTextures(1, &gl_tex);
    glBindTexture(GL_TEXTURE_2D, gl_tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    gl_prog = create_program_load("data/texmap.vert", "data/texmap.frag");
    glClearColor(1.0, 1.0, 0.0, 1.0);

    return glGetError() == GL_NO_ERROR;
}

static void
gl_cleanup()
{
    free_program(gl_prog);
    glBindTexture(GL_TEXTURE_2D, 0);

    glDeleteTextures(1, &gl_tex);
    glDeleteProgram(gl_prog);

    glDeleteFramebuffers(1, &gl_fbo);
    glDeleteRenderbuffers(1, &gl_rbo);
}

static void
display()
{
    // make the EGL context current
    eglMakeCurrent(ctx_es.disp, ctx_es.surf, ctx_es.surf, ctx_es.ctx);

    glClear(GL_COLOR_BUFFER_BIT);
	bind_program(gl_prog);
	glBindTexture(GL_TEXTURE_2D, gl_tex);
	glBindBuffer(GL_ARRAY_BUFFER, gl_vbo);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    eglSwapBuffers(ctx_es.disp, ctx_es.surf);

    // make the angle context current
}

static void
reshape(int w, int h)
{
	glViewport(0, 0, w, h);
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
