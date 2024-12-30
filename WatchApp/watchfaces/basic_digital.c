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
#include "basic_digital.h"
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
bool running = true;
bool screen_on = true;

void basic_digital_screen_display() {
    printf("%s +\n", __func__);

    // Create cairo surface and context
    cairo_surface_t* cairo_surface;
    cairo_surface = cairo_image_surface_create_for_data(fb_ptr, CAIRO_FORMAT_ARGB32, fb_width, fb_height, cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, fb_width));
    cairo_context = cairo_create(cairo_surface);

    cairo_set_operator(cairo_context, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cairo_context);
    cairo_set_operator(cairo_context, CAIRO_OPERATOR_OVER);

    // Initialize Pango, create layout, and load font
    PangoLayout* pango_time_layout;
    PangoLayout* pango_charge_layout;

    PangoFontDescription* pango_time_font_description;
    PangoFontDescription* pango_charge_font_description;

    pango_time_layout = pango_cairo_create_layout(cairo_context);
    pango_charge_layout = pango_cairo_create_layout(cairo_context);

    pango_time_font_description = pango_font_description_from_string("NanumSquareRound 42");
    pango_layout_set_font_description(pango_time_layout, pango_time_font_description);
    pango_font_description_free(pango_time_font_description);

    pango_charge_font_description = pango_font_description_from_string("NanumSquareRound 22");
    pango_layout_set_font_description(pango_charge_layout, pango_charge_font_description);
    pango_font_description_free(pango_charge_font_description);

    char buf_text_time[16];
    int text_time_width = 0;
    int text_time_height = 0;
    char buf_text_charge[8];
    int text_charge_width = 0;
    int text_charge_height = 0;

    while(running) {
        if (screen_on) {
            // Check if the time or battery percent are different
            if (strstr(buf_text_time, ntp_getTimeString()) == NULL) {
                // Time or battery percent is different, update the display
                printf("%s: Time or battery percent has changed, updating display.\n", __func__);

                // Clear the screen
                cairo_set_operator(cairo_context, CAIRO_OPERATOR_CLEAR);
                cairo_paint(cairo_context);
                cairo_set_operator(cairo_context, CAIRO_OPERATOR_OVER);

                // Create the text strings
                snprintf(buf_text_time, sizeof(buf_text_time), "%s", ntp_getTimeString());
                snprintf(buf_text_charge, sizeof(buf_text_charge), "%d%%", utils_read_battery_percentage());

                pango_layout_set_text(pango_time_layout, buf_text_time, -1);
                pango_layout_get_size(pango_time_layout, &text_time_width, &text_time_height);
                text_time_width /= 1024;
                text_time_height /= 1024;

                pango_layout_set_text(pango_charge_layout, buf_text_charge, -1);
                pango_layout_get_size(pango_charge_layout, &text_charge_width, &text_charge_height);
                text_charge_width /= 1024;
                text_charge_height /= 1024;

                // Position and draw the text boxes
                cairo_new_path(cairo_context);

                pango_layout_set_alignment(pango_time_layout, PANGO_ALIGN_CENTER);
                cairo_set_source_rgb(cairo_context, 1, 1, 1);
                cairo_move_to(cairo_context, (fb_width / 2) - (text_time_width / 2), (fb_height / 2) - (text_time_height / 2));
                pango_cairo_show_layout(cairo_context, pango_time_layout);

                pango_layout_set_alignment(pango_charge_layout, PANGO_ALIGN_CENTER);
                cairo_set_source_rgb(cairo_context, 1, 1, 1);
                cairo_move_to(cairo_context, (fb_width / 2) - (text_charge_width / 2), 325 - (text_charge_height / 2)); // Was 325
                pango_cairo_show_layout(cairo_context, pango_charge_layout);
            }

            // Wait a second for refresh
            usleep(5000 * 1000);
        }
        
        // Wait 250ms between checks
        usleep (250 * 1000);
    }

    printf("%s -\n", __func__);
}
