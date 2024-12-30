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
#include "shutdown_screen.h"
#include "../utils.h"
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

// Global variables within this file
static pthread_mutex_t mut_circle_animation;
static cairo_t* cairo_context;

void shutdown_screen_display() {
    printf("%s +\n", __func__);

    // Create cairo surface
    cairo_surface_t* cairo_surface;
    cairo_surface = cairo_image_surface_create_for_data(fb_ptr, CAIRO_FORMAT_ARGB32, fb_width, fb_height, cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, fb_width));

    // Create cairo context
    cairo_context = cairo_create(cairo_surface);

    // Clear framebuffer
    cairo_set_operator(cairo_context, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cairo_context);
    cairo_set_operator(cairo_context, CAIRO_OPERATOR_OVER);

    // Set cairo to draw lines with rounded edges
    cairo_set_line_cap(cairo_context, CAIRO_LINE_CAP_ROUND);

    // Create and start the circle animation thread
    pthread_t thr_circle_animation;
    pthread_mutex_init(&mut_circle_animation, NULL);
    pthread_create(&thr_circle_animation, NULL, shutdown_screen_draw_circle_animation, NULL);

    // Display initial shutdown text, and then countdown from 5
    pthread_mutex_lock(&mut_circle_animation);
    shutdown_screen_draw_text(5);
    pthread_mutex_unlock(&mut_circle_animation);
    for (int i = 5; i >= 0; i--) {
        pthread_mutex_lock(&mut_circle_animation);
        shutdown_screen_draw_text(i);
        pthread_mutex_unlock(&mut_circle_animation);
        sleep(1);
    }

    // Kill the circle animation thread
    pthread_cancel(thr_circle_animation);

    // Cleanup
    cairo_destroy(cairo_context);
    cairo_surface_destroy(cairo_surface);
    printf("%s -\n", __func__);
}

void shutdown_screen_draw_text(int second) {
    // Initialize Pango
    PangoLayout* pango_layout;
    PangoFontDescription* pango_font_description;

    // Create pango layout
    pango_layout = pango_cairo_create_layout(cairo_context);

    // Load Font (나눔스퀘어라운드)
    pango_font_description = pango_font_description_from_string("NanumSquareRound 40");
    pango_layout_set_font_description(pango_layout, pango_font_description);
    pango_font_description_free(pango_font_description);

    // Create shutdown text, and calculate it's size
    char buf_text_shutdown[32];
    snprintf(buf_text_shutdown, sizeof(buf_text_shutdown), "%d초 후에\n중료됨니다", second);
    pango_layout_set_text(pango_layout, buf_text_shutdown, -1);
    int text_shutdown_width = 0;
    int text_shutdown_height = 0;
    pango_layout_get_size(pango_layout, &text_shutdown_width, &text_shutdown_height); // Note this outputs Pango pixel size, which is 1/1024th of a pixel
    text_shutdown_width /= 1024;
    text_shutdown_height /= 1024;
    
    // Clear text space so the new text doesn't overlap the old text
    cairo_set_source_rgb(cairo_context, 0, 0, 0);
    cairo_rectangle(cairo_context, (fb_width / 2) - (text_shutdown_width / 2), (fb_height / 2) - (text_shutdown_height / 2), text_shutdown_width, text_shutdown_height);
    cairo_fill(cairo_context);

    // Position and draw the shutdown text
    pango_layout_set_alignment(pango_layout, PANGO_ALIGN_CENTER);
    cairo_new_path(cairo_context);
    cairo_move_to(cairo_context, (fb_width / 2) - (text_shutdown_width / 2), (fb_height / 2) - (text_shutdown_height / 2));
    cairo_set_source_rgb(cairo_context, 1, 1, 1);
    pango_cairo_show_layout(cairo_context, pango_layout);
}

void* shutdown_screen_draw_circle_animation(void* arg) {
    for (int angle = 0; angle < 361; angle++) {
        pthread_mutex_lock(&mut_circle_animation);
        shutdown_screen_draw_circle(angle);
        pthread_mutex_unlock(&mut_circle_animation);
        usleep(14 * 1000); // 360 / 5 --> 72 degrees per second
    }
}

void shutdown_screen_draw_circle(double angle) {
    // Set the line width and color
    cairo_set_line_width(cairo_context, 10);
    cairo_set_source_rgb(cairo_context, 0, 0, 1);

    // Fix angle (Cairo starts 0 degrees at the right, not the top)
    angle -= 90;

    // Calculate the X and Y of the circle pixel
    double dot_x = (fb_width / 2) + (fb_width / 2 - 6) * cos(degToRad(angle));
    double dot_y = (fb_height / 2) + (fb_width / 2 - 6) * sin(degToRad(angle));

    // Draw the circle
    cairo_new_path(cairo_context);
    cairo_move_to(cairo_context, dot_x, dot_y);
    cairo_close_path(cairo_context);
    cairo_stroke(cairo_context);
}
