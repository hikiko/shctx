#ifndef CTX_H
#define CTX_H

#include <EGL/egl.h>
#include <X11/Xlib.h>

struct EGL_ctx {
    EGLDisplay disp;
    EGLSurface surf;
    EGLContext ctx;
    EGLConfig config;
};

#endif //CTX_H
