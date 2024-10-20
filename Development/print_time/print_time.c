#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include <fcntl.h>
#include <linux/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <unistd.h>

// Cairo Imports
#include <cairo/cairo.h>

#include "ntp.h"

// Global variables
int fb_width = 0;
int fb_height = 0;
int fb_vwidth = 0;
int fb_vheight = 0;
long fb_bpp = 0;
long fb_bytes = 0;
long fb_linelength = 0;
int fb_red_offset = 0;
int fb_red_length = 0;
int fb_green_offset = 0;
int fb_green_length = 0;
int fb_blue_offset = 0;
int fb_blue_length = 0;
int fb_alpha_offset = 0;
int fb_alpha_length = 0;

int main(void) {
    printf("Fetching framebuffer parameters...\n");
    
    // Open framebuffer
    int fd_fb = open("/dev/fb0", O_RDWR);
    if (!fd_fb) {
        printf("Framebuffer could not be opened.\n");
        return 1;
    } else {
        printf("Framebuffer opened.\n");
    }

    // Fetch framebuffer info
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    ioctl(fd_fb, FBIOGET_VSCREENINFO, &vinfo);
    ioctl(fd_fb, FBIOGET_FSCREENINFO, &finfo);
    fb_width = vinfo.xres;
    fb_height = vinfo.yres;
    fb_bpp = vinfo.bits_per_pixel;
    fb_bytes = fb_bpp / 8;
    fb_vwidth = vinfo.xres_virtual;
    fb_vheight = vinfo.yres_virtual;
    fb_linelength = finfo.line_length;
    fb_red_offset = vinfo.red.offset;
    fb_red_length = vinfo.red.length;
    fb_green_offset = vinfo.green.offset;
    fb_green_length = vinfo.green.length;
    fb_blue_offset = vinfo.blue.offset;
    fb_blue_length = vinfo.blue.length;
    fb_alpha_offset = vinfo.transp.offset;
    fb_alpha_length = vinfo.transp.length;

    printf("Physical framebuffer size: %dx%d\nVirtual framebuffer size: %dx%d\n", fb_width, fb_height, fb_vwidth, fb_vheight);
    printf("Framebuffer BPP: %ld, framebuffer line length: %ld\n", fb_bpp, fb_linelength);
    printf("Red offset / length: %d / %d, Green offset / length: %d / %d, Blue offset / length: %d / %d, Alpha offset / length: %d / %d\n", fb_red_offset, fb_red_length, fb_green_offset, fb_green_length, fb_blue_offset, fb_blue_length, fb_alpha_offset, fb_alpha_length);

    // Memory map the framebuffer
    long fb_size = fb_height * fb_linelength;
    char *fb_ptr = mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_fb, 0);

    // Create cairo surface
    cairo_surface_t* surface;
    surface = cairo_image_surface_create_for_data(fb_ptr, CAIRO_FORMAT_ARGB32, fb_width, fb_height, cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, fb_width));
   
    // Create cairo context
    cairo_t* cr;
    cr = cairo_create(surface);

    // Fetch NTP Time
    printf("Fetching NTP Time...\n");
    long ntp_timestamp = (long)ntp_fetch();
    printf("NTP Timestamp: %ld\n", ntp_timestamp);

    // Apply timezone to Unix timestamp
    long utc_offset = 10 * 3600; // UTC + 10
    long ntp_timestamp_timezone_adjusted = ntp_timestamp + utc_offset;
    struct tm* local_time = localtime(&ntp_timestamp_timezone_adjusted);
    char formatted_time[12];
    strftime(formatted_time, sizeof(formatted_time), "%I : %M %p", local_time);
    printf("Formatted time: %s\n", formatted_time);

    // Clear cairo screen
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    // Display time on the framebuffer
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    
    
    cairo_select_font_face(cr, "serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 80.0);
    cairo_move_to(cr, 35, 220);
    cairo_show_text(cr, formatted_time);
}
