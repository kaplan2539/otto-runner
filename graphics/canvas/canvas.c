#include "canvas.h"
#include <GLES/gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <syslog.h>
#include <assert.h>
int stak_canvas_create(stak_canvas_s* canvas, stak_canvas_flags flags, uint32_t canvas_w, uint32_t canvas_h) {
//#define RENDER_WINDOW_ONSCREEN
    bcm_host_init();
    canvas->opengl_resource = vc_dispmanx_resource_create(VC_IMAGE_RGB565, 512, 512, &canvas->opengl_ptr);
    if (!canvas->opengl_resource)
    {
        syslog(LOG_ERR, "Unable to create OpenGL screen buffer");
        return -1;
    }
#ifndef RENDER_WINDOW_ONSCREEN
    // open offscreen display and 512x512 resource
    canvas->display = vc_dispmanx_display_open_offscreen( canvas->opengl_resource, DISPMANX_NO_ROTATE);
#else
    // open main display
    canvas->display = vc_dispmanx_display_open( 0 );
#endif

    canvas->scaled_resource = vc_dispmanx_resource_create(VC_IMAGE_RGB565, 96, 96, &canvas->scaled_ptr);
    if (!canvas->scaled_resource)
    {
        syslog(LOG_ERR, "Unable to create LCD output buffer");
        return -1;
    }

    canvas->update = vc_dispmanx_update_start( 0 );

    graphics_get_display_size(canvas->display , &canvas->screen_width, &canvas->screen_height);

    vc_dispmanx_rect_set( &canvas->opengl_rect, 0, 0, canvas->screen_width, canvas->screen_height );
    vc_dispmanx_rect_set( &canvas->scaled_rect, 0, 0, canvas->screen_width << 16, canvas->screen_height << 16 );
    vc_dispmanx_rect_set( &canvas->fb_rect, 0, 0, canvas_w, canvas_h );

    VC_DISPMANX_ALPHA_T alpha = { DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS, 255, 0 };

    canvas->opengl_element = vc_dispmanx_element_add(canvas->update, canvas->display, 1 , &canvas->opengl_rect,
                             canvas->opengl_resource, &canvas->scaled_rect,
                             DISPMANX_PROTECTION_NONE, &alpha, 0, DISPMANX_NO_ROTATE);

    assert(canvas->opengl_element != 0);

    canvas->nativewindow.element = canvas->opengl_element;
    canvas->nativewindow.width = canvas->screen_width;
    canvas->nativewindow.height = canvas->screen_height;
    vc_dispmanx_update_submit_sync( canvas->update );

    EGLBoolean result;
    EGLint num_config;


    static const EGLint attribute_list[] =
    {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_LUMINANCE_SIZE, EGL_DONT_CARE,          //EGL_DONT_CARE
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_SAMPLES,        1,
        EGL_NONE
    };

    EGLConfig config;


    // get an EGL display connection
    canvas->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    eglBindAPI(EGL_OPENVG_API);

    // initialize the EGL display connection
    result = eglInitialize(canvas->egl_display, NULL, NULL);

    // get an appropriate EGL frame buffer configuration
    result = eglChooseConfig(canvas->egl_display, attribute_list, &config, 1, &num_config);

    // create an EGL rendering context
    canvas->context = eglCreateContext(canvas->egl_display, config, EGL_NO_CONTEXT, NULL);

    // create an EGL surface
    canvas->surface = eglCreateWindowSurface( canvas->egl_display, config, &canvas->nativewindow, NULL );

    // connect the context to the surface
    result = eglMakeCurrent(canvas->egl_display, canvas->surface, canvas->surface, canvas->context);
    return 0;
}

int stak_canvas_destroy(stak_canvas_s* canvas) {
    // clear screen
    glClear( GL_COLOR_BUFFER_BIT );
    eglSwapBuffers(canvas->egl_display, canvas->surface);

    // Release OpenGL resources
    eglMakeCurrent( canvas->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
    eglDestroySurface( canvas->egl_display, canvas->surface );
    eglDestroyContext( canvas->egl_display, canvas->context );
    eglTerminate( canvas->egl_display );
    eglReleaseThread();
    vc_dispmanx_element_remove(canvas->update, canvas->opengl_element);
    vc_dispmanx_resource_delete(canvas->opengl_resource);
    vc_dispmanx_resource_delete(canvas->scaled_resource);
    vc_dispmanx_display_close(canvas->display);
    return 0;
}
int stak_canvas_copy(stak_canvas_s* canvas, char* dst, uint32_t pitch) {
    vc_dispmanx_snapshot(canvas->display, canvas->scaled_resource, 0);
    vc_dispmanx_resource_read_data(canvas->scaled_resource, &canvas->fb_rect, dst, pitch);
    return 0;
}
int stak_canvas_swap(stak_canvas_s* canvas) {
    eglSwapBuffers(canvas->egl_display, canvas->surface);
    return 0;
}