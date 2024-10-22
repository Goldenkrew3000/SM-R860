#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <linux/fb.h>
#include <sys/ioctl.h>

// Check the bootmode (General = 1, Charger = 2, Error = 3)
int utils_check_bootmode() {
    printf("%s +\n", __func__);
    char buf_cmdline[4096];

    // Open /proc/cmdline
    FILE* fp_cmdline = fopen("/proc/cmdline", "r");
    if (!fp_cmdline) {
        printf("Failed to open /proc/cmdline.\n");
        printf("%s -\n", __func__);
        return 3;
    }

    // Read /proc/cmdline into buffer
    if (fgets(buf_cmdline, sizeof(buf_cmdline), fp_cmdline) == NULL) {
        printf("Failed to read /proc/cmdline.\n");
        fclose(fp_cmdline);
        printf("%s -\n", __func__);
        return 3;
    }

    // Check for 'androidboot.mode=charger'
    if (strstr(buf_cmdline, "androidboot.mode=charger") != NULL) {
        printf("Device booted in charging mode.\n");
        fclose(fp_cmdline);
        printf("%s -\n", __func__);
        return 2;
    } else {
        printf("Device booted in general mode.\n");
        fclose(fp_cmdline);
        printf("%s -\n", __func__);
        return 1;
    }

    // Code should never reach this point, return error
    fclose(fp_cmdline);
    printf("%s -\n", __func__);
    return 3;
}

// Enable writing to the exynos framebuffer (1 = Success, 2 = Failure)
int utils_allow_exynos_fb_write() {
    printf("%s +\n", __func__);

    // Open /sys/class/graphics/f0/blank
    FILE* fp_exynos_fb_write = fopen("/sys/class/graphics/fb0/blank", "w");
    if (!fp_exynos_fb_write) {
        printf("Could not open /sys/class/graphics/fb0/blank.\n");
        fclose(fp_exynos_fb_write);
        printf("%s -\n", __func__);
        return 2;
    }

    // Disable buffering and write '0' to the file
    setvbuf(fp_exynos_fb_write, NULL, _IONBF, 0);
    fprintf(fp_exynos_fb_write, "%s", "0");
    fclose(fp_exynos_fb_write);

    printf("%s -\n", __func__);
    return 1;
}

// Read the battery percentage, (200 = failure, it's above the maximum percentage), No print here due to it being called a _lot_
int utils_read_battery_percentage() {
    // Open battery percentage file
    FILE* fd_battery_percentage = fopen("/sys/class/power_supply/battery/capacity", "r");
    if (!fd_battery_percentage) {
        printf("Could not open battery percentage (/sys/class/power_supply/battery/capacity).\n");
        return 200;
    }

    // Read battery percentage
    char buf_battery_percentage[5];
    if (fgets(buf_battery_percentage, sizeof(buf_battery_percentage), fd_battery_percentage) == NULL) {
        printf("Could not read battery percentage.\n");
        fclose(fd_battery_percentage);
        return 200;
    }
    fclose(fd_battery_percentage);

    // Return the battery percentage (Convert the string to an integer)
    return atoi(buf_battery_percentage);
}
