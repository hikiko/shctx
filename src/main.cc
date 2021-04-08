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

#include <X11/Xlib.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ctx.h"
#include "sdr.h"

#define EGL_EGLEXT_PROTOTYPES 1
#define GL_GLEXT_PROTOTYPES 1
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

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


static EGL_ctx ctxA;
static EGL_ctx ctxB;

static GLuint texA;
static unsigned int gl_prog;
static GLuint gl_vbo;

static GLuint texB;

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
    EGL_NONE };

static int dmabuf_fd;

struct tex_storage_info {
    EGLint fourcc;
    EGLint num_planes;
    EGLuint64KHR modifiers;
    EGLint offset;
    EGLint stride;
};
static struct tex_storage_info gl_dma_info;
static unsigned char pixels[256 * 256 * 4];

int main(int argc, char **argv)
{
    if (!init()) {
        fprintf(stderr, "Failed to initialize contexts.\n");
        return 1;
    }

    if (!gl_init())
        return 1;

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
    ctxA.config = egl_choose_config();
    if (!ctxA.config) {
        fprintf(stderr, "bad ctxA config\n");
        return false;
    }

    ctxB.config = egl_choose_config();
    if (!ctxB.config) {
        fprintf(stderr, "bad ctxB config\n");
        return false;
    }

    /* create EGL context */
    if (!egl_create_context(&ctxA, 0, ctx_atts)) {
        printf("ctxA failed\n");
        return false;
    }

    if (!egl_create_context(&ctxB, 0, ctx_atts)) {
        printf("ctxB failed\n");
        return false;
    }

    EGLint vis_id;
    eglGetConfigAttrib(ctxA.dpy, ctxA.config, EGL_NATIVE_VISUAL_ID, &vis_id);

    EGLint secondary_vis_id;
    eglGetConfigAttrib(ctxB.dpy, ctxB.config, EGL_NATIVE_VISUAL_ID, &secondary_vis_id);

    win = x_create_window(vis_id, 800, 600, "EGL/dma_buf experiment");
    if (!win) {
        fprintf(stderr, "ctxA x_create_window failed.\n");
        return false;
    }
    XMapWindow(xdpy, win);

    hidden_win = x_create_window(secondary_vis_id, 800, 600, "angle egl");
    if (!hidden_win) {
        fprintf(stderr, "ctxB x_create_window failed.\n");
        return false;
    }
   XSync(xdpy, 0);


    const EGLint surf_atts[] = {
        EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
        EGL_NONE
    };

    /* create EGL/ES surface */
    ctxA.surf = eglCreateWindowSurface(ctxA.dpy, ctxA.config, win, surf_atts);
    if (ctxA.surf == EGL_NO_SURFACE) {
        fprintf(stderr, "Failed to create EGL surface for win.\n");
        return false;
    }

    ctxB.surf = eglCreateWindowSurface(ctxB.dpy, ctxB.config, hidden_win, surf_atts);
    if (ctxB.surf == EGL_NO_SURFACE) {
        fprintf(stderr, "Failed to create ANGLE EGL surface for hidden win.\n");
        return false;
    }

    redraw_pending = true;
    return true;
}

static bool
egl_init()
{
    if ((ctxA.dpy = eglGetPlatformDisplay(EGL_PLATFORM_X11_EXT, (void *)xdpy, NULL)) == EGL_NO_DISPLAY) {
        fprintf(stderr, "Failed to get EGL display.\n");
        return false;
    }

    if (!eglInitialize(ctxA.dpy, NULL, NULL)) {
        fprintf(stderr, "Failed to initialize EGL.\n");
        return false;
    }

    if ((ctxB.dpy = eglGetPlatformDisplay(EGL_PLATFORM_X11_EXT, (void *)xdpy, NULL)) == EGL_NO_DISPLAY) {
        fprintf(stderr, "Failed to get ANGLE EGL display : error : %s.\n", eglGetError() != EGL_SUCCESS ? "yes" : "no");
        return false;
    }

    if (!eglInitialize(ctxB.dpy, NULL, NULL)) {
        fprintf(stderr, "Failed to initialize ANGLE EGL.\n");
        return false;
    }

    return (eglGetError() == EGL_SUCCESS);
}

static EGLConfig
egl_choose_config()
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

    EGLConfig config;
    EGLint num_configs;
    if (!eglChooseConfig(ctxA.dpy, attr_list, &config, 1, &num_configs)) {
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

    win = XCreateWindow(xdpy, xroot,
                        0, 0, win_w, win_h,
                        0, vis_info->depth,
                        InputOutput, vis_info->visual,
                        CWBackPixel | CWColormap, &xattr);

    if (!win) {
        fprintf(stderr, "Failed to create X11 window.\n");
        return 0;
    }

    XSelectInput(xdpy, win,
                 ExposureMask | StructureNotifyMask | KeyPressMask);

    XTextProperty tex_prop;
    XStringListToTextProperty((char**)&title, 1, &tex_prop);
    XSetWMName(xdpy, win, &tex_prop);
    XFree(tex_prop.value);

    XSetWMProtocols(xdpy, win, &xa_wm_del_win, 1);

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

    eglTerminate(ctxA.dpy);
    eglTerminate(ctxB.dpy);

    XDestroyWindow(xdpy, win);
    XDestroyWindow(xdpy, hidden_win);
    XCloseDisplay(xdpy);
}

static bool
gl_init()
{
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

    // Context A that draws
    eglMakeCurrent(ctxA.dpy, ctxA.surf, ctxA.surf, ctxA.ctx);
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

    glGenTextures(1, &texA);
    glBindTexture(GL_TEXTURE_2D, texA);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glFlush();

    EGLImage imgA = eglCreateImage(ctxA.dpy, ctxA.ctx, EGL_GL_TEXTURE_2D, (EGLClientBuffer)(uint64_t)texA, 0);
    assert(imgA != EGL_NO_IMAGE);

    PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC eglExportDMABUFImageQueryMESA =
        (PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC)eglGetProcAddress("eglExportDMABUFImageQueryMESA");
    PFNEGLEXPORTDMABUFIMAGEMESAPROC eglExportDMABUFImageMESA =
        (PFNEGLEXPORTDMABUFIMAGEMESAPROC)eglGetProcAddress("eglExportDMABUFImageMESA");

    EGLBoolean ret;
    ret = eglExportDMABUFImageQueryMESA(ctxA.dpy,
                                        imgA,
                                        &gl_dma_info.fourcc,
                                        &gl_dma_info.num_planes,
                                        &gl_dma_info.modifiers);
    if (!ret) {
        fprintf(stderr, "eglExportDMABUFImageQueryMESA failed.\n");
        return false;
    }
    ret = eglExportDMABUFImageMESA(ctxA.dpy,
                                   imgA,
                                   &dmabuf_fd,
                                   &gl_dma_info.stride,
                                   &gl_dma_info.offset);
    if (!ret) {
        fprintf(stderr, "eglExportDMABUFImageMESA failed.\n");
        return false;
    }

    // Context B that fills the texture texB with a XOR pattern
    printf("Filling texB from ctxB with XOR pattern.\n");
    eglMakeCurrent(ctxB.dpy, ctxB.surf, ctxB.surf, ctxB.ctx);
    EGLAttrib atts[] = {
        // W, H used in TexImage2D above!
        EGL_WIDTH, 256,
        EGL_HEIGHT, 256,
        EGL_LINUX_DRM_FOURCC_EXT, gl_dma_info.fourcc,
        EGL_DMA_BUF_PLANE0_FD_EXT, dmabuf_fd,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, gl_dma_info.offset,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, gl_dma_info.stride,
        EGL_NONE,
    };
    EGLImageKHR imgB = eglCreateImage(ctxB.dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)(uint64_t)0, atts);
    assert(imgB != EGL_NO_IMAGE);

    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES =
        (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    assert(glEGLImageTargetTexture2DOES);

    glGenTextures(1, &texB);
    glBindTexture(GL_TEXTURE_2D, texB);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, imgB);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    return glGetError() == GL_NO_ERROR;
}

static void
gl_cleanup()
{
    eglMakeCurrent(ctxA.dpy, ctxA.surf, ctxA.surf, ctxA.ctx);
    if (gl_prog)
        free_program(gl_prog);

    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &texA);

    eglMakeCurrent(ctxB.dpy, ctxB.surf, ctxB.surf, ctxB.ctx);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &texB);
}

static int ctx;
static void
display()
{
    // context A draws using texA, we should see the pattern we
    // wrote in tex B
    eglMakeCurrent(ctxA.dpy, ctxA.surf, ctxA.surf, ctxA.ctx);
    if (++ctx == 1)
        printf("Mapping texA from ctxA to the x11 window.\n");

    bind_program(gl_prog);
    glBindTexture(GL_TEXTURE_2D, texA);
    glBindBuffer(GL_ARRAY_BUFFER, gl_vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    eglSwapBuffers(ctxA.dpy, ctxA.surf);

#if 0
    // If we need to re-write the pattern using ctxB
    eglMakeCurrent(ctxB.dpy, ctxB.surf, ctxB.surf, ctxB.ctx);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindTexture(GL_TEXTURE_2D, texB);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    eglSwapBuffers(ctxB.dpy, ctxB.surf);
#endif
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
