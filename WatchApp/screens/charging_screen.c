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
#include "charging_screen.h"
#include "../utils.h"
#include "../framebuffer.h"
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
static pthread_mutex_t mut_outer_circle_animation;
static cairo_t* cairo_context;
static bool isRunning;
static int outerCircleMode = 1; // Outer circle mode. 1 = continous line, 2 = rotating dots

void charging_screen_display() {
    printf("%s +\n", __func__);
    isRunning = true;

    // Create cairo surface
    cairo_surface_t* cairo_surface;
    cairo_surface = cairo_image_surface_create_for_data(fb_ptr, CAIRO_FORMAT_ARGB32, fb_width, fb_height, cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, fb_width));

    // Create cairo context
    cairo_context = cairo_create(cairo_surface);

    // Clear framebuffer
    cairo_set_operator(cairo_context, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cairo_context);
    cairo_set_operator(cairo_context, CAIRO_OPERATOR_OVER);
    framebuffer_refresh();

    // Set cairo to draw lines with rounded edges
    cairo_set_line_cap(cairo_context, CAIRO_LINE_CAP_ROUND);

    // Create and start the outer circle animation thread
    pthread_t thr_outer_circle_animation;
    pthread_mutex_init(&mut_outer_circle_animation, NULL);
    pthread_create(&thr_outer_circle_animation, NULL, charging_screen_draw_outer_circle_animation, NULL);

    // Draw initial battery percentage text
    int battery_percentage = utils_read_battery_percentage();
    // TODO error checking
    pthread_mutex_lock(&mut_outer_circle_animation);
    charging_screen_draw_text(battery_percentage);
    pthread_mutex_unlock(&mut_outer_circle_animation);

    // Draw initial animated inner circle (Note the mutex is done this way so the outer circle also animates during this time)
    for (int i = 0; i < battery_percentage; i++) {
        pthread_mutex_lock(&mut_outer_circle_animation);
        charging_screen_draw_inner_circle(i);
        pthread_mutex_unlock(&mut_outer_circle_animation);
        framebuffer_refresh();
        usleep(35 * 1000);
    }

    // Poll the battery percentage every 5 seconds, and if it changes, update the text and the inner ring
    int last_battery_percentage = 0;
    while (isRunning) {
        battery_percentage = utils_read_battery_percentage();
        if (battery_percentage != last_battery_percentage) { // TODO error checking
            pthread_mutex_lock(&mut_outer_circle_animation);
            //charging_screen_clear_inner_circle(); // Theoretically this isn't needed since the inner circle never goes down
            charging_screen_draw_inner_circle(battery_percentage);
            charging_screen_draw_text(battery_percentage);
            framebuffer_refresh();
            pthread_mutex_unlock(&mut_outer_circle_animation);
        }
        last_battery_percentage = battery_percentage;

        // Check whether the wireless charger is still connected
        int wireless_charger_connected = utils_read_wireless_charger_connected();
        if (wireless_charger_connected == 0) {
            printf("%s: Wireless charger has been disconnected.\n", __func__);
            isRunning = false;
        } else if (wireless_charger_connected == 2) {
            sleep(5); // Charger is still connected
        } else if (wireless_charger_connected == 3) {
            // TODO error handline
        }
	    usleep(35 * 1000);
    }

    // Stop the outer circle animation thread
    pthread_cancel(thr_outer_circle_animation);

    // -
    

}

void charging_screen_draw_outer_circle(double angle, int line_width, double r, double g, double b) {
    // Set the line width (The circle is actually just a line with rounded edges), and set the color
    cairo_set_line_width(cairo_context, line_width);
    cairo_set_source_rgb(cairo_context, b, g, r); // Colors are reversed (r=b,g=g,b=r) in the framebuffer

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

void charging_screen_draw_inner_circle(int percentage) {
    // Set line width and color
    cairo_set_line_width(cairo_context, 10);
    cairo_set_source_rgb(cairo_context, 0, 1, 0);

    // Calculate the angle based on the battery percentage
    int battery_angle = (percentage * 360) / 100;

    // Draw the circle
    cairo_new_path(cairo_context);
    cairo_arc(cairo_context, fb_width / 2, fb_height / 2, (fb_width / 2) - 75, 3 * M_PI / 2, 3 * M_PI / 2 + degToRad(battery_angle));
    cairo_stroke(cairo_context);
}

void charging_screen_clear_inner_circle() {
    // Set line width and color
    cairo_set_line_width(cairo_context, 12); // +2 due to some weirdness that happens around the outside
    cairo_set_source_rgb(cairo_context, 0, 0, 0);

    // Draw the circle
    cairo_new_path(cairo_context);
    cairo_arc(cairo_context, fb_width / 2, fb_height / 2, (fb_width / 2) - 75, 3 * M_PI / 2, 3 * M_PI / 2 + degToRad(360));
    cairo_stroke(cairo_context);
}

void* charging_screen_draw_outer_circle_animation(void* arg) {
    int angle = 0;
    while (true) {
        pthread_mutex_lock(&mut_outer_circle_animation);
        if (outerCircleMode == 1) {
            charging_screen_draw_outer_circle(angle, 10, 1, 0, 0);
            charging_screen_draw_outer_circle(angle + 45, 10, 1, 0.49, 0);
            charging_screen_draw_outer_circle(angle + 90, 10, 1, 1, 0);
            charging_screen_draw_outer_circle(angle + 135, 10, 0, 1, 0);
            charging_screen_draw_outer_circle(angle + 180, 10, 0, 0, 1);
            charging_screen_draw_outer_circle(angle + 225, 10, 0.29, 0, 0.5);
            charging_screen_draw_outer_circle(angle + 270, 10, 0.58, 0, 0.82);
            charging_screen_draw_outer_circle(angle + 315, 10, 1, 0.07, 0.57);
        } else if (outerCircleMode == 2) {
            charging_screen_draw_outer_circle(angle - 1, 12, 0, 0, 0);
            charging_screen_draw_outer_circle(angle, 10, 1, 0, 0);
            charging_screen_draw_outer_circle(angle + 45 - 1, 12, 0, 0, 0);
            charging_screen_draw_outer_circle(angle + 45, 10, 1, 0.49, 0);
            charging_screen_draw_outer_circle(angle + 90 - 1, 12, 0, 0, 0);
            charging_screen_draw_outer_circle(angle + 90, 10, 1, 1, 0);
            charging_screen_draw_outer_circle(angle + 135 - 1, 12, 0, 0, 0);
            charging_screen_draw_outer_circle(angle + 135, 10, 0, 1, 0);
            charging_screen_draw_outer_circle(angle + 180 - 1, 12, 0, 0, 0);
            charging_screen_draw_outer_circle(angle + 180, 10, 0, 0, 1);
            charging_screen_draw_outer_circle(angle + 225 - 1, 12, 0, 0, 0);
            charging_screen_draw_outer_circle(angle + 225, 10, 0.29, 0, 0.5);
            charging_screen_draw_outer_circle(angle + 270 - 1, 12, 0, 0, 0);
            charging_screen_draw_outer_circle(angle + 270, 10, 0.58, 0, 0.82);
            charging_screen_draw_outer_circle(angle + 315 - 1, 12, 0, 0 ,0);
            charging_screen_draw_outer_circle(angle + 315, 10, 1, 0.07, 0.57);
        }
        pthread_mutex_unlock(&mut_outer_circle_animation);
        angle++;
        if (angle >= 361) {
            angle = 0;
        }
        framebuffer_refresh();
        usleep(50 * 1000);
    }
}

void charging_screen_draw_text(int percentage) {
    // Initialize Pango
    PangoLayout* pango_layout;
    PangoFontDescription* pango_font_description;

    // Create pango layout
    pango_layout = pango_cairo_create_layout(cairo_context);

    // Load Font (나눔스퀘어라운드)
    pango_font_description = pango_font_description_from_string("NanumSquareRound 40");
    pango_layout_set_font_description(pango_layout, pango_font_description);
    pango_font_description_free(pango_font_description);

    // Create charging text, and calculate it's size
    char buf_text_charging[32];
    snprintf(buf_text_charging, sizeof(buf_text_charging), "충전 중\n%d%%", percentage);
    pango_layout_set_text(pango_layout, buf_text_charging, -1);
    int text_charging_width = 0;
    int text_charging_height = 0;
    pango_layout_get_size(pango_layout, &text_charging_width, &text_charging_height); // Note this outputs Pango pixel size, which is 1/1024th of a pixel
    text_charging_width /= 1024;
    text_charging_height /= 1024;

    // Clear text space so the new text doesn't overlap the old text
    cairo_set_source_rgb(cairo_context, 0, 0, 0);
    cairo_rectangle(cairo_context, (fb_width / 2) - (text_charging_width / 2), (fb_height / 2) - (text_charging_height / 2), text_charging_width, text_charging_height);
    cairo_fill(cairo_context);

    // Position and draw the charging text
    pango_layout_set_alignment(pango_layout, PANGO_ALIGN_CENTER);
    cairo_new_path(cairo_context);
    cairo_move_to(cairo_context, (fb_width / 2) - (text_charging_width / 2), (fb_height / 2) - (text_charging_height / 2));
    cairo_set_source_rgb(cairo_context, 1, 1, 1);
    pango_cairo_show_layout(cairo_context, pango_layout);
}
