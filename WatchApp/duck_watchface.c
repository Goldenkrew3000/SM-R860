#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <math.h>
#include <fcntl.h>
#include <pthread.h>
#include <linux/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include "duck_watchface.h"
#include "utils.h"
#define USE_FREETYPE 1

// Global framebuffer variables
extern int fb_width;
extern int fb_height;
extern int fb_vwidth;
extern int fb_vheight;
extern long fb_bpp;
extern long fb_bytes;
extern long fb_linelength;
extern int fb_red_offset;
extern int fb_red_length;
extern int fb_green_offset;
extern int fb_green_length;
extern int fb_blue_offset;
extern int fb_blue_length;
extern int fb_alpha_offset;
extern int fb_alpha_length;
extern char* fb_ptr;

static cairo_t* cairo_context;

void duck_watchface_display() {
    //
    //

    // Create cairo surface
    cairo_surface_t* cairo_surface;
    cairo_surface = cairo_image_surface_create_for_data(fb_ptr, CAIRO_FORMAT_ARGB32, fb_width, fb_height, cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, fb_width));

    // Create cairo context
    cairo_context = cairo_create(cairo_surface);

    // Clear framebuffer
    cairo_set_operator(cairo_context, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cairo_context);
    cairo_set_operator(cairo_context, CAIRO_OPERATOR_OVER);
   



    // --
    cairo_surface_t* image = cairo_image_surface_create_from_png("resources/duck_stage.png");
    cairo_status_t status = cairo_surface_status(image);
    if (status != CAIRO_STATUS_SUCCESS) {
        //fail
    }

    int width = cairo_image_surface_get_width(image);
    int height = cairo_image_surface_get_height(image);

    utils_reorder_color((unsigned char*)cairo_image_surface_get_data(image), width, height, cairo_image_surface_get_stride(image));

    cairo_save(cairo_context); // WHAT DOES THIS DO

    cairo_translate(cairo_context, 28, 40);

    cairo_set_source_surface(cairo_context, image, 0, 0);
    cairo_paint(cairo_context);

    cairo_restore(cairo_context); // AGAIN WHAT DOES THIS DO
}
