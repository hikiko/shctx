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
#include <EGL/eglext.h>
//#include "eglext_angle.h"

#include <X11/Xlib.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ctx.h"
#include "sdr.h"
#include "mygl.h"

// functions

static bool init();
static void cleanup();

static EGLConfig egl_choose_config();
static EGLConfig angle_egl_choose_config();

static bool egl_init();

static bool egl_create_context(EGLContext shared);
static bool angle_egl_create_context(EGLContext shared);

static Window x_create_window(int vis_id, int win_w, int win_h, const char *title);
static bool handle_xevent(XEvent *ev);

static bool gl_init();
static void gl_cleanup();

static void display();
static void reshape(int w, int h);
static bool keyboard(KeySym sym);

// variables

static EGL_ctx ctx_es;
static EGL_ctx ctx_angle;

static GLuint gl_tex;
static unsigned int gl_prog;
static GLuint gl_vbo;

static int xscr;
static Display *xdpy;
static Window xroot;
static Window win;
static Window hidden_win;
static Atom xa_wm_proto;
static Atom xa_wm_del_win;
static int win_width, win_height;
static bool redraw_pending;

int main(int argc, char **argv)
{
    if (!init()) {
        fprintf(stderr, "Failed to initialize EGL or ANGLE EGL context.\n");
        return 1;
    }

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
    if (!mygl_init()) {
        return false;
    }

    if (!(xdpy = XOpenDisplay(0))) {
        fprintf(stderr, "Failed to connect to the X server.\n");
        return 1;
    }

    xscr = DefaultScreen(xdpy);
    xroot = RootWindow(xdpy, xscr);

    xa_wm_proto = XInternAtom(xdpy, "WM_PROTOCOLS", False);
    xa_wm_del_win = XInternAtom(xdpy, "WM_DELETE_WINDOW", False);

    /* init EGL/ES ctx reqs */
    if (!egl_init()) {
        fprintf(stderr, "egl_init failed\n");
        return false;
    }

    /* select EGL/ES config */
    ctx_es.config = egl_choose_config();
    if (!ctx_es.config) {
        return false;
    }

    ctx_angle.config = angle_egl_choose_config();
    if (!ctx_angle.config) {
        return false;
    }

    /* create EGL context */
    if (!egl_create_context(0)) {
        return false;
    }

    /* create ANGLE context */
    if (!angle_egl_create_context(ctx_es.ctx)) {
        return false;
    }

    // On WebKit we will draw to textures so we won't need to mess with
    // visuals. For THIS test we need it for each X11 win
    EGLint vis_id;
    eglGetConfigAttrib(ctx_es.dpy, ctx_es.config, EGL_NATIVE_VISUAL_ID, &vis_id);
    printf("NATIVE visual id: %d\n", vis_id);

    EGLint angle_vis_id;
    angle_eglGetConfigAttrib(ctx_angle.dpy, ctx_angle.config, EGL_NATIVE_VISUAL_ID, &angle_vis_id);
    printf("ANGLE visual id: %d\n", angle_vis_id);

    // NOTE to myself:
    // create x window: this is going to be used by both contexts
    /////////////////////////////////////////////////////////////
    win = x_create_window(vis_id, 800, 600, "native egl");
    if (!win) {
        fprintf(stderr, "EGL x_create_window\n");
        return false;
    }
    XMapWindow(xdpy, win);
    XSync(xdpy, 0);

    hidden_win = x_create_window(angle_vis_id, 800, 600, "angle egl");
    if (!hidden_win) {
        fprintf(stderr, "ANGLE x_create_window\n");
        return false;
    }
    //XMapWindow(xdpy, hidden_win);
    XSync(xdpy, 0);

    ////////////////////////////////////////////////////////////

    /* create EGL/ES surface */
    ctx_es.surf = eglCreateWindowSurface(ctx_es.dpy, ctx_es.config, win, 0);
    if (ctx_es.surf == EGL_NO_SURFACE) {
        fprintf(stderr, "Failed to create EGL surface for win.\n");
        return false;
    }

    ctx_angle.surf = angle_eglCreateWindowSurface(ctx_angle.dpy, ctx_angle.config, hidden_win, 0);
    if (ctx_angle.surf == EGL_NO_SURFACE) {
        fprintf(stderr, "Failed to create ANGLE EGL surface for hidden win.\n");
        return false;
    }

    redraw_pending = true;
    return true;
}

static bool
egl_init()
{
/*
    const char *client_extensions = angle_eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (!client_extensions) {
        fprintf(stderr, "ANGLE EGL extensions not found.\n");
        return false;
    }

    if (!strstr(client_extensions, "ANGLE_platform_angle")) {
        fprintf(stderr, "ANGLE_platform_angle extension not found.\n");
        return false;
    }

    if (!strstr(client_extensions, "EGL_ANGLE_platform_angle_opengl")) {
        fprintf(stderr, "EGL_ANGLE_platform_angle_opengl not found");
        return false;
    }

    if (!strstr(client_extensions, "EGL_ANGLE_platform_angle_device_type_egl")) {
        fprintf(stderr, "EGL_ANGLE_platform_angle_device_type_egl not found.\n");
        return false;
    }

    if (!strstr(client_extensions, "EGL_EXT_platform_base")) {
        fprintf(stderr, "EGL_EXT_platform_base not found.\n");
        return false;
    }

    if (!strstr(client_extensions, "ANGLE_x11_visual")) {
        fprintf(stderr, "ANGLE_x11_visual not found.\n");
        return false;
    }
*/

    // create an EGL display
    //if ((ctx_es.dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY)) == EGL_NO_DISPLAY) {
    // NOTE to myself:
    // following line I used in EGL/GLES2 example causes error in angle (see extensions):
    if ((ctx_es.dpy = eglGetPlatformDisplay(EGL_PLATFORM_X11_EXT, (void *)xdpy, NULL)) == EGL_NO_DISPLAY) {
        fprintf(stderr, "Failed to get EGL display.\n");
        return false;
    }

    if (!eglInitialize(ctx_es.dpy, NULL, NULL)) {
        fprintf(stderr, "Failed to initialize EGL.\n");
        return false;
    }

    /*
    static const EGLAttrib angle_atts[] = {
        EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE,
        EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE,
        EGL_NONE
    };
*/
//    if ((ctx_angle.dpy = angle_eglGetPlatformDisplay(EGL_PLATFORM_ANGLE_ANGLE, (void *)xdpy, angle_atts)) == EGL_NO_DISPLAY) {
    if ((ctx_angle.dpy = angle_eglGetDisplay(EGL_DEFAULT_DISPLAY)) == EGL_NO_DISPLAY) {
        fprintf(stderr, "Failed to get ANGLE EGL display : error : %s.\n", eglGetError() != EGL_SUCCESS ? "yes" : "no");
        return false;
    }

    if (!angle_eglInitialize(ctx_angle.dpy, NULL, NULL)) {
        fprintf(stderr, "Failed to initialize ANGLE EGL.\n");
        return false;
    }

    return (eglGetError() == EGL_SUCCESS) && (angle_eglGetError() == EGL_SUCCESS);
}

static EGLConfig
egl_choose_config()
{
    static const EGLint
        attr_list[] = {
            EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PIXMAP_BIT,
            EGL_RED_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_DEPTH_SIZE, 16,
            EGL_STENCIL_SIZE, EGL_DONT_CARE,
            EGL_NONE
        };

    // select an EGL configuration
    EGLConfig config;
    EGLint num_configs;
    if (!eglChooseConfig(ctx_es.dpy, attr_list, &config, 1, &num_configs)) {
        fprintf(stderr, "Failed to find a suitable EGL config.\n");
        return 0;
    }

    return config;
}

static EGLConfig
angle_egl_choose_config()
{
    static const EGLint
        attr_list[] = {
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

    // select an EGL configuration
    EGLConfig config;
    EGLint num_configs;
    if (!angle_eglChooseConfig(ctx_angle.dpy, attr_list, &config, 1, &num_configs)) {
        fprintf(stderr, "Failed to find a suitable EGL config.\n");
        return 0;
    }

    EGLint err = angle_eglGetError();
    if (err != EGL_SUCCESS) {
        fprintf(stderr, "Error in %s: 0x%x\n", __func__, err);
        return 0;
    }
    return config;
}

static EGLint ctx_atts[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
//    EGL_CONTEXT_OPENGL_DEBUG, EGL_TRUE,
    EGL_NONE };

static bool
angle_egl_create_context(EGLContext shared)
{
    static EGLint angle_ctx_atts[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        // EGL_CONTEXT_OPENGL_DEBUG, EGL_TRUE,
        EGL_NONE };

    EGLenum api = angle_eglQueryAPI();

    switch(api) {
        case EGL_OPENGL_API:
            printf("EGL opengl API\n");
            break;
        case EGL_OPENGL_ES_API:
            printf("EGL opengl ES API\n");
            break;
        case EGL_NONE:
        default:
            printf("No API\n");
            break;
    }

    ctx_angle.ctx = angle_eglCreateContext(ctx_angle.dpy, ctx_angle.config, shared, angle_ctx_atts);

    if (!ctx_angle.ctx) {
        fprintf(stderr, "Failed to create ANGLE EGL context %s. Error:\n", __func__);
        EGLint err = angle_eglGetError();
        switch (err) {
        case EGL_BAD_MATCH:
            fprintf(stderr, "BAD CONTEXT: current rendering API is EGL NONE?\n");
            break;
        case EGL_BAD_ATTRIBUTE:
            fprintf(stderr, "BAD ATTRIBUTE: one or more attributes in %s are wrong\n", __func__);
            break;
        case EGL_SUCCESS:
            fprintf(stderr, "EGL_SUCCESS after ANGLE create context.\n");
            break;
        default:
            fprintf(stderr, "EGL error code: 0x%x\n", err);
            break;
        };

        return false;
    }

    return true;
}

static bool
egl_create_context(EGLContext shared)
{
    eglBindAPI(EGL_OPENGL_API);
    ctx_es.ctx = eglCreateContext(ctx_es.dpy, ctx_es.config, shared, ctx_atts);

    if (!ctx_es.ctx) {
        fprintf(stderr, "Failed to create EGL context.\n");
        return false;
    }

    return (eglGetError() == EGL_SUCCESS);
}

Window
x_create_window(int vis_id, int win_w, int win_h, const char *title)
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
    XStringListToTextProperty((char**)&title, 1, &tex_prop);
    XSetWMName(xdpy, win, &tex_prop);
    XFree(tex_prop.value);

    // Window manager protocols
    XSetWMProtocols(xdpy, win, &xa_wm_del_win, 1);
    //XMapWindow(xdpy, win);
    //XSync(xdpy, 0);

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

    /* because ANGLE backend is EGL, cleanup angle* first */
    angle_eglDestroySurface(ctx_angle.dpy, &ctx_angle.surf);
    eglDestroySurface(ctx_es.dpy, &ctx_es.surf);

    angle_eglDestroyContext(ctx_angle.dpy, &ctx_angle.ctx);
    eglDestroyContext(ctx_es.dpy, &ctx_es.ctx);

    angle_eglTerminate(ctx_angle.dpy);
    eglTerminate(ctx_es.dpy);

    XDestroyWindow(xdpy, win);
    XDestroyWindow(xdpy, hidden_win);
    XCloseDisplay(xdpy);
}

static bool
gl_init()
{
    // Context that draws
    eglMakeCurrent(ctx_es.dpy, ctx_es.surf, ctx_es.surf, ctx_es.ctx);
    static const float vertices[] = {
        1.0, 1.0,
        1.0, 0.0,
        0.0, 1.0,
        0.0, 0.0
    };

    glGenBuffers(1, &gl_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, gl_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STATIC_DRAW);

    gl_prog = create_program_load("data/texmap.vert", "data/texmap.frag");
    glClearColor(1.0, 1.0, 0.0, 1.0);

    // Context that creates the image
    angle_eglMakeCurrent(ctx_angle.dpy, ctx_angle.surf, ctx_angle.surf, ctx_angle.ctx);
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

    angle_glGenTextures(1, &gl_tex);
    angle_glBindTexture(GL_TEXTURE_2D, gl_tex);

    angle_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    angle_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    angle_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    angle_glFinish();

    angle_glClearColor(0.0, 1.0, 0.0, 1.0);
    return angle_glGetError() == GL_NO_ERROR;
}

static void
gl_cleanup()
{
    free_program(gl_prog);

    angle_glBindTexture(GL_TEXTURE_2D, 0);
    angle_glDeleteTextures(1, &gl_tex);
}

static void
display()
{
#if 0
    angle_eglMakeCurrent(ctx_angle.dpy, ctx_angle.surf, ctx_angle.surf, ctx_angle.ctx);
    angle_glClear(GL_COLOR_BUFFER_BIT);
    angle_eglSwapBuffers(ctx_angle.dpy, ctx_angle.surf);
    // make the EGL context current
#endif
    eglMakeCurrent(ctx_es.dpy, ctx_es.surf, ctx_es.surf, ctx_es.ctx);

    bind_program(gl_prog);
    glBindTexture(GL_TEXTURE_2D, gl_tex);
    glBindBuffer(GL_ARRAY_BUFFER, gl_vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(ctx_es.dpy, ctx_es.surf);

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
