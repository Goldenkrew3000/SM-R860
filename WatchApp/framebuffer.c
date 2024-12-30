#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>

// Global framebuffer variables
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
char* fb_ptr = NULL;

// Refresh the framebuffer
void* framebuffer_refresh_fb(void* arg) {
    printf("%s +\n", __func__);
    
    // Open framebuffer
    int fd_fb = open("/dev/fb0", O_RDWR);
    if (!fd_fb) {
        printf("Could not open framebuffer.\n");
    }

    // Fetch framebuffer info
    struct fb_var_screeninfo vinfo;
    assert(fd_fb >= 0);
    assert(ioctl(fd_fb, FBIOGET_VSCREENINFO, &vinfo) >= 0);

    // Refresh the framebuffer (60 times a second)
    while (true) {
        ioctl(fd_fb, FBIOPAN_DISPLAY, &vinfo);
        usleep(16666);
    }

    printf("%s -\n", __func__);
}

// Initialize the framebuffer by fetching all the required data, and memory mapping it (Returns 1 if success, 2 if failed)
int framebuffer_init() {
    printf("%s +\n", __func__);

    // Open framebuffer
    int fd_fb = open("/dev/fb0", O_RDWR);
    if (!fd_fb) {
        printf("Framebuffer could not be opened.\n");
        return 2;
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
    fb_ptr = mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_fb, 0);

    printf("%s -\n", __func__);
    return 1;
}

// Deinitialize the framebuffer by freeing the memory mapped area
void framebuffer_deinit() {
    printf("%s +\n", __func__);

    // Unmap the memory mapped framebuffer
    long fb_size = fb_height * fb_linelength;
    munmap(fb_ptr, fb_size);

    printf("%s -\n", __func__);
    return;
}
