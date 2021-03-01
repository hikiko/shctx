#include <dlfcn.h>
#include <stdio.h>
#include "mygl.h"

void *egl_so;
void *gl_so;

PFNEGLMAKECURRENTPROC angle_eglMakeCurrent;
PFNEGLGETCONFIGATTRIBPROC angle_eglGetConfigAttrib;
PFNEGLCREATEWINDOWSURFACEPROC angle_eglCreateWindowSurface;
PFNEGLGETDISPLAYPROC angle_eglGetDisplay;
PFNEGLINITIALIZEPROC angle_eglInitialize;
PFNEGLBINDAPIPROC angle_eglBindAPI;
PFNEGLGETERRORPROC angle_eglGetError;
PFNEGLCHOOSECONFIGPROC angle_eglChooseConfig;
PFNEGLCREATECONTEXTPROC angle_eglCreateContext;
PFNEGLTERMINATEPROC angle_eglTerminate;
PFNEGLSWAPBUFFERSPROC angle_eglSwapBuffers;
PFNEGLQUERYAPIPROC angle_eglQueryAPI;
PFNEGLQUERYSTRINGPROC angle_eglQueryString;
PFNEGLGETPLATFORMDISPLAYPROC angle_eglGetPlatformDisplay;
PFNEGLDESTROYCONTEXTPROC angle_eglDestroyContext;
PFNEGLDESTROYSURFACEPROC angle_eglDestroySurface;

PFNGLGENBUFFERSPROC angle_glGenBuffers;
PFNGLBINDBUFFERPROC angle_glBindBuffer;
PFNGLBUFFERDATAPROC angle_glBufferData;
PFNGLCLEARCOLORPROC angle_glClearColor;
PFNGLGENTEXTURESPROC angle_glGenTextures;
PFNGLBINDTEXTUREPROC angle_glBindTexture;
PFNGLTEXPARAMETERIPROC angle_glTexParameteri;
PFNGLTEXIMAGE2DPROC angle_glTexImage2D;
PFNGLFINISHPROC angle_glFinish;
PFNGLGETERRORPROC angle_glGetError;
PFNGLDELETETEXTURESPROC angle_glDeleteTextures;
PFNGLDELETEPROGRAMPROC angle_glDeleteProgram;
PFNGLCLEARPROC angle_glClear;
PFNGLVERTEXATTRIBPOINTERPROC angle_glVertexAttribPointer;
PFNGLENABLEVERTEXATTRIBARRAYPROC angle_glEnableVertexAttribArray;
PFNGLDRAWARRAYSPROC angle_glDrawArrays;
PFNGLVIEWPORTPROC angle_glViewport;
PFNGLCREATESHADERPROC angle_glCreateShader;
PFNGLCREATEPROGRAMPROC angle_glCreateProgram;
PFNGLGETUNIFORMLOCATIONPROC angle_glGetUniformLocation;
PFNGLGETATTRIBLOCATIONPROC angle_glGetAttribLocation;
PFNGLSHADERSOURCEPROC angle_glShaderSource;
PFNGLCOMPILESHADERPROC angle_glCompileShader;
PFNGLGETSHADERIVPROC angle_glGetShaderiv;
PFNGLGETSHADERINFOLOGPROC angle_glGetShaderInfoLog;
PFNGLDELETESHADERPROC angle_glDeleteShader;
PFNGLATTACHSHADERPROC angle_glAttachShader;
PFNGLLINKPROGRAMPROC angle_glLinkProgram;
PFNGLGETPROGRAMIVPROC angle_glGetProgramiv;
PFNGLGETPROGRAMINFOLOGPROC angle_glGetProgramInfoLog;
PFNGLUSEPROGRAMPROC angle_glUseProgram;
PFNGLGETINTEGERVPROC angle_glGetIntegerv;
PFNGLUNIFORM1IPROC angle_glUniform1i;

#define LOAD_FUNC(name, type, func) \
	if (!(func = (type)dlsym(so, name))) { \
		fprintf(stderr, "Failed to load %s\n", name); \
		return false; \
	}

bool mygl_init()
{
	if (!(egl_so = dlopen(ANGLEPATH "/libEGL.so", RTLD_LAZY))) {
		fprintf(stderr, "Failed to open ANGLE EGL library.\n");
		return false;
	}
	void *so = egl_so;

    LOAD_FUNC("eglMakeCurrent", PFNEGLMAKECURRENTPROC, angle_eglMakeCurrent);
    LOAD_FUNC("eglGetConfigAttrib", PFNEGLGETCONFIGATTRIBPROC, angle_eglGetConfigAttrib);
    LOAD_FUNC("eglCreateWindowSurface", PFNEGLCREATEWINDOWSURFACEPROC, angle_eglCreateWindowSurface);
    LOAD_FUNC("eglGetDisplay", PFNEGLGETDISPLAYPROC, angle_eglGetDisplay);
    LOAD_FUNC("eglInitialize", PFNEGLINITIALIZEPROC, angle_eglInitialize);
    LOAD_FUNC("eglBindAPI", PFNEGLBINDAPIPROC, angle_eglBindAPI);
    LOAD_FUNC("eglGetError", PFNEGLGETERRORPROC, angle_eglGetError);
    LOAD_FUNC("eglChooseConfig", PFNEGLCHOOSECONFIGPROC, angle_eglChooseConfig);
    LOAD_FUNC("eglCreateContext", PFNEGLCREATECONTEXTPROC, angle_eglCreateContext);
    LOAD_FUNC("eglTerminate", PFNEGLTERMINATEPROC, angle_eglTerminate);
    LOAD_FUNC("eglSwapBuffers", PFNEGLSWAPBUFFERSPROC, angle_eglSwapBuffers);
    LOAD_FUNC("eglQueryAPI", PFNEGLQUERYAPIPROC, angle_eglQueryAPI);
    LOAD_FUNC("eglQueryString", PFNEGLQUERYSTRINGPROC, angle_eglQueryString);
    LOAD_FUNC("eglGetPlatformDisplay", PFNEGLGETPLATFORMDISPLAYPROC, angle_eglGetPlatformDisplay);
    LOAD_FUNC("eglDestroyContext", PFNEGLDESTROYCONTEXTPROC, angle_eglDestroyContext);
    LOAD_FUNC("eglDestroySurface", PFNEGLDESTROYSURFACEPROC, angle_eglDestroySurface);

	if (!(gl_so = dlopen(ANGLEPATH "/libGLESv2.so", RTLD_LAZY))) {
		fprintf(stderr, "Failed to open ANGLE GLESv2 library.\n");
		return false;
	}

	so = gl_so;

    LOAD_FUNC("glGenBuffers", PFNGLGENBUFFERSPROC, angle_glGenBuffers);
    LOAD_FUNC("glBindBuffer", PFNGLBINDBUFFERPROC, angle_glBindBuffer);
    LOAD_FUNC("glBufferData", PFNGLBUFFERDATAPROC, angle_glBufferData);
    LOAD_FUNC("glClearColor", PFNGLCLEARCOLORPROC, angle_glClearColor);
    LOAD_FUNC("glGenTextures", PFNGLGENTEXTURESPROC, angle_glGenTextures);
    LOAD_FUNC("glBindTexture", PFNGLBINDTEXTUREPROC, angle_glBindTexture);
    LOAD_FUNC("glTexParameteri", PFNGLTEXPARAMETERIPROC, angle_glTexParameteri);
    LOAD_FUNC("glTexImage2D", PFNGLTEXIMAGE2DPROC, angle_glTexImage2D);
    LOAD_FUNC("glFinish", PFNGLFINISHPROC, angle_glFinish);
    LOAD_FUNC("glGetError", PFNGLGETERRORPROC, angle_glGetError);
    LOAD_FUNC("glDeleteTextures", PFNGLDELETETEXTURESPROC, angle_glDeleteTextures);
    LOAD_FUNC("glDeleteProgram", PFNGLDELETEPROGRAMPROC, angle_glDeleteProgram);
    LOAD_FUNC("glClear", PFNGLCLEARPROC, angle_glClear);
    LOAD_FUNC("glVertexAttribPointer", PFNGLVERTEXATTRIBPOINTERPROC, angle_glVertexAttribPointer);
    LOAD_FUNC("glEnableVertexAttribArray", PFNGLENABLEVERTEXATTRIBARRAYPROC, angle_glEnableVertexAttribArray);
    LOAD_FUNC("glDrawArrays", PFNGLDRAWARRAYSPROC, angle_glDrawArrays);
    LOAD_FUNC("glViewport", PFNGLVIEWPORTPROC, angle_glViewport);
    LOAD_FUNC("glCreateShader", PFNGLCREATESHADERPROC, angle_glCreateShader);
    LOAD_FUNC("glCreateProgram", PFNGLCREATEPROGRAMPROC, angle_glCreateProgram);
    LOAD_FUNC("glGetUniformLocation", PFNGLGETUNIFORMLOCATIONPROC, angle_glGetUniformLocation);
    LOAD_FUNC("glGetAttribLocation", PFNGLGETATTRIBLOCATIONPROC, angle_glGetAttribLocation);
    LOAD_FUNC("glShaderSource", PFNGLSHADERSOURCEPROC, angle_glShaderSource);
    LOAD_FUNC("glCompileShader", PFNGLCOMPILESHADERPROC, angle_glCompileShader);
    LOAD_FUNC("glGetShaderiv", PFNGLGETSHADERIVPROC, angle_glGetShaderiv);
    LOAD_FUNC("glGetShaderInfoLog", PFNGLGETSHADERINFOLOGPROC, angle_glGetShaderInfoLog);
    LOAD_FUNC("glDeleteShader", PFNGLDELETESHADERPROC, angle_glDeleteShader);
    LOAD_FUNC("glAttachShader", PFNGLATTACHSHADERPROC, angle_glAttachShader);
    LOAD_FUNC("glLinkProgram", PFNGLLINKPROGRAMPROC, angle_glLinkProgram);
    LOAD_FUNC("glGetProgramiv", PFNGLGETPROGRAMIVPROC, angle_glGetProgramiv);
    LOAD_FUNC("glGetProgramInfoLog", PFNGLGETPROGRAMINFOLOGPROC, angle_glGetProgramInfoLog);
    LOAD_FUNC("glUseProgram", PFNGLUSEPROGRAMPROC, angle_glUseProgram);
    LOAD_FUNC("glGetIntegerv", PFNGLGETINTEGERVPROC, angle_glGetIntegerv);
    LOAD_FUNC("glUniform1i", PFNGLUNIFORM1IPROC, angle_glUniform1i);

	return true;
}
