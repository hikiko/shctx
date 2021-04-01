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
#include "eglext_angle.h"

#include <X11/Xlib.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ctx.h"
#include "sdr.h"

#define EGL_EGLEXT_PROTOTYPES 1
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>

// functions

static bool init();
static void cleanup();

static EGLConfig egl_choose_config();

static bool egl_init();

static bool egl_create_context(EGL_ctx *ctx, EGLContext shared, EGLint *ctx_atts);

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

static GLuint angle_gl_tex;

static int xscr;
static Display *xdpy;
static Window xroot;
static Window win;
static Window hidden_win;
static Atom xa_wm_proto;
static Atom xa_wm_del_win;
static int win_width, win_height;
static bool redraw_pending;

static EGLint ctx_atts[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_CONTEXT_OPENGL_DEBUG, EGL_TRUE,
    EGL_NONE };

static int gl_tex_dmabuf_fd;

struct tex_storage_metadata {
    EGLint fourcc;
    EGLint num_planes;
    EGLuint64KHR modifiers;
    EGLint offset;
    EGLint stride;
};
static struct tex_storage_metadata gl_dma_info;
static unsigned char pixels[256 * 256 * 4];

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
        fprintf(stderr, "bad ctx_es config\n");
        return false;
    }

    ctx_angle.config = egl_choose_config();
    if (!ctx_angle.config) {
        fprintf(stderr, "bad ctx_angle config\n");
        return false;
    }

    /* create EGL context */
    if (!egl_create_context(&ctx_es, 0, ctx_atts)) {
        printf("ctx_es failed\n");
        return false;
    }

    if (!egl_create_context(&ctx_angle, 0, ctx_atts)) {
        printf("ctx_angle failed\n");
        return false;
    }

    // On WebKit we will draw to textures so we won't need to mess with
    // visuals. For THIS test we need it for each X11 win
    EGLint vis_id;
    eglGetConfigAttrib(ctx_es.dpy, ctx_es.config, EGL_NATIVE_VISUAL_ID, &vis_id);

    EGLint angle_vis_id;
    eglGetConfigAttrib(ctx_angle.dpy, ctx_angle.config, EGL_NATIVE_VISUAL_ID, &angle_vis_id);

    // NOTE to myself:
    // create x window: this is going to be used by both contexts
    //return false;
    /////////////////////////////////////////////////////////////
    win = x_create_window(vis_id, 800, 600, "native egl");
    if (!win) {
        fprintf(stderr, "EGL x_create_window\n");
        return false;
    }
    XMapWindow(xdpy, win);
 //   XSync(xdpy, 0);

    hidden_win = x_create_window(angle_vis_id, 800, 600, "angle egl");
    if (!hidden_win) {
        fprintf(stderr, "ANGLE x_create_window\n");
        return false;
    }
// XMapWindow(xdpy, hidden_win);
   XSync(xdpy, 0);

    ////////////////////////////////////////////////////////////

    const EGLint surf_atts[] = {
        EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
        EGL_NONE
    };

    /* create EGL/ES surface */
    ctx_es.surf = eglCreateWindowSurface(ctx_es.dpy, ctx_es.config, win, surf_atts);
    if (ctx_es.surf == EGL_NO_SURFACE) {
        fprintf(stderr, "Failed to create EGL surface for win.\n");
        return false;
    }

    ctx_angle.surf = eglCreateWindowSurface(ctx_angle.dpy, ctx_angle.config, hidden_win, surf_atts);
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

    if ((ctx_angle.dpy = eglGetPlatformDisplay(EGL_PLATFORM_X11_EXT, (void *)xdpy, NULL)) == EGL_NO_DISPLAY) {
        fprintf(stderr, "Failed to get ANGLE EGL display : error : %s.\n", eglGetError() != EGL_SUCCESS ? "yes" : "no");
        return false;
    }

    if (!eglInitialize(ctx_angle.dpy, NULL, NULL)) {
        fprintf(stderr, "Failed to initialize ANGLE EGL.\n");
        return false;
    }

    return (eglGetError() == EGL_SUCCESS) && (eglGetError() == EGL_SUCCESS);
}

static EGLConfig
egl_choose_config()
{
    static const EGLint
        attr_list[] = {
            EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
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
    if (!eglChooseConfig(ctx_es.dpy, attr_list, &config, 1, &num_configs)) {
        fprintf(stderr, "Failed to find a suitable EGL config.\n");
        return 0;
    }

    return config;
}

static bool
egl_create_context(EGL_ctx *ctx, EGLContext shared, EGLint *ctx_atts)
{
    eglBindAPI(EGL_OPENGL_API);
    ctx->ctx = eglCreateContext(ctx->dpy, ctx->config, shared, ctx_atts);

    if (!ctx->ctx) {
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
    printf("cleanup\n");
    gl_cleanup();

    eglTerminate(ctx_es.dpy);
    eglTerminate(ctx_angle.dpy);

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

    // xor image
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

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // system egl we dont fill the image with pixels here! just creating it
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glFlush();

    // DMA-FIXME: create image
    EGLImage img = eglCreateImage(ctx_es.dpy, ctx_es.ctx, EGL_GL_TEXTURE_2D, (EGLClientBuffer)(uint64_t)gl_tex, 0);
    assert(img != EGL_NO_IMAGE);

    PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC eglExportDMABUFImageQueryMESA =
        (PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC)eglGetProcAddress("eglExportDMABUFImageQueryMESA");
    PFNEGLEXPORTDMABUFIMAGEMESAPROC eglExportDMABUFImageMESA =
        (PFNEGLEXPORTDMABUFIMAGEMESAPROC)eglGetProcAddress("eglExportDMABUFImageMESA");

    EGLBoolean ret;
    ret = eglExportDMABUFImageQueryMESA(ctx_es.dpy,
                                        img,
                                        &gl_dma_info.fourcc,
                                        &gl_dma_info.num_planes,
                                        &gl_dma_info.modifiers);
    if (!ret) {
        fprintf(stderr, "eglExportDMABUFImageQueryMESA failed.\n");
        return false;
    }
    ret = eglExportDMABUFImageMESA(ctx_es.dpy,
                                   img,
                                   &gl_tex_dmabuf_fd,
                                   &gl_dma_info.stride,
                                   &gl_dma_info.offset);
    if (!ret) {
        fprintf(stderr, "eglExportDMABUFImageMESA failed.\n");
        return false;
    }

    eglMakeCurrent(ctx_angle.dpy, ctx_angle.surf, ctx_angle.surf, ctx_angle.ctx);
    EGLAttrib atts[] = {
        // W, H used in TexImage2D above!
        EGL_WIDTH, 256,
        EGL_HEIGHT, 256,
        EGL_LINUX_DRM_FOURCC_EXT, gl_dma_info.fourcc,
        EGL_DMA_BUF_PLANE0_FD_EXT, gl_tex_dmabuf_fd,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, gl_dma_info.stride,
        EGL_NONE,
    };
    EGLImageKHR angle_img = eglCreateImage(ctx_angle.dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)(uint64_t)0, atts);
    assert(angle_img != EGL_NO_IMAGE);

    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES =
        (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    assert(glEGLImageTargetTexture2DOES);

    glGenTextures(1, &angle_gl_tex);
    glBindTexture(GL_TEXTURE_2D, angle_gl_tex);
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, angle_img);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    return glGetError() == GL_NO_ERROR;
}

static void
gl_cleanup()
{
    free_program(gl_prog);

    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &gl_tex);
}

static void
display()
{
    /* angle caches the last context set by angle eglMakeCurrent
     * if that's the same with the new, it doesn't bother to actually call
     * the real eglMakeCurrent. So, we make sure to invalidate the angle
     * context before switching the real context ourselves to force
     * angle to call system's eglMakeCurrent when we next call system's
     * EGLMakeCurrent
     */
    eglMakeCurrent(ctx_angle.dpy, 0, 0, 0);
    eglMakeCurrent(ctx_es.dpy, ctx_es.surf, ctx_es.surf, ctx_es.ctx);

    bind_program(gl_prog);
    glBindTexture(GL_TEXTURE_2D, gl_tex);
    glBindBuffer(GL_ARRAY_BUFFER, gl_vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    eglSwapBuffers(ctx_es.dpy, ctx_es.surf);

    eglMakeCurrent(ctx_angle.dpy, ctx_angle.surf, ctx_angle.surf, ctx_angle.ctx);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindTexture(GL_TEXTURE_2D, gl_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    eglSwapBuffers(ctx_angle.dpy, ctx_angle.surf);
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
