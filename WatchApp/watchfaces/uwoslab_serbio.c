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
#include "uwoslab_serbio.h"
#include "../utils.h"
#include "../ntp_handler.h"
#define USE_FREETYPE 1

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
static pthread_mutex_t mut_uwoslab_serbio;
static cairo_t* cairo_context;

void uwoslab_serbio_screen_display() {
    printf("%s +\n", __func__);

    // Create cairo surface and context
    cairo_surface_t* cairo_surface;
    cairo_surface = cairo_image_surface_create_for_data(fb_ptr, CAIRO_FORMAT_ARGB32, fb_width, fb_height, cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, fb_width));
    cairo_context = cairo_create(cairo_surface);

    cairo_set_operator(cairo_context, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cairo_context);
    cairo_set_operator(cairo_context, CAIRO_OPERATOR_OVER);

    //pthread_mutex_init(&mut_uwoslab_serbio, NULL);
    //pthread_mutex_lock(&mut_uwoslab_serbio);
    uwoslab_serbio_background_display("../resources/uwoslab_serbio_bg.png"); 
    //pthread_mutex_unlock(&mut_uwoslab_serbio);
    
    // Initialize Pango, create layout, and load font
    PangoLayout* pango_layout;
    PangoFontDescription* pango_font_description;
    pango_layout = pango_cairo_create_layout(cairo_context);
    pango_font_description = pango_font_description_from_string("NanumSquareRound 40");
    pango_layout_set_font_description(pango_layout, pango_font_description);
    pango_font_description_free(pango_font_description);

    // Create charging text, and calculate it's size
    char buf_text_time[16];
    snprintf(buf_text_time, sizeof(buf_text_time), "%s", ntp_getTimeString());

    pango_layout_set_text(pango_layout, buf_text_time, -1);
    int text_time_width = 0;
    int text_time_height = 0;
    pango_layout_get_size(pango_layout, &text_time_width, &text_time_height); // Note this outputs Pango pixel size, which is 1/1024th of a pixel
    text_time_width /= 1024;
    text_time_height /= 1024;






    // Position and draw the charging text
    pango_layout_set_alignment(pango_layout, PANGO_ALIGN_CENTER);
    cairo_new_path(cairo_context);
    cairo_move_to(cairo_context, (fb_width / 2) - (text_time_width / 2), (fb_height / 2) - (text_time_height / 2));
    cairo_set_source_rgb(cairo_context, 1, 1, 1);
    pango_cairo_show_layout(cairo_context, pango_layout);


    printf("%s -\n", __func__);
}

void uwoslab_serbio_background_display(char* file_location) {
    printf("%s +\n", __func__);

    // Import background image
    cairo_surface_t* serbio_background = cairo_image_surface_create_from_png(file_location);
    cairo_status_t serbio_background_status = cairo_surface_status(serbio_background);
    if (serbio_background_status != CAIRO_STATUS_SUCCESS) {
        printf("%s: Error loading watchface background image.\n", __func__);
    }
    int serbio_background_width = cairo_image_surface_get_width(serbio_background);
    int serbio_background_height = cairo_image_surface_get_height(serbio_background);
    utils_reorder_color((unsigned char*)cairo_image_surface_get_data(serbio_background), serbio_background_width, serbio_background_height, cairo_image_surface_get_stride(serbio_background));

    // Display background image
    cairo_save(cairo_context);
    cairo_translate(cairo_context, 0, 0);
    cairo_set_source_surface(cairo_context, serbio_background, 0, 0);
    cairo_paint(cairo_context);
    cairo_restore(cairo_context);

    printf("%s -\n", __func__);
}
