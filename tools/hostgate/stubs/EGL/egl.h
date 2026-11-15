#pragma once
// Host-gate stub for the EGL surface GameHooks touches (F-003).
typedef void* EGLDisplay;
typedef void* EGLSurface;
typedef int EGLBoolean;
typedef unsigned int EGLenum;
typedef void* (*__eglMustCastToProperFunctionPointerType)(void);
#define EGL_TRUE 1
#define EGL_FALSE 0
#define EGL_NO_CONTEXT ((void*)0)
#ifdef __cplusplus
extern "C" {
#endif
EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surf);
void* eglGetCurrentContext(void);
#ifdef __cplusplus
}
#endif
