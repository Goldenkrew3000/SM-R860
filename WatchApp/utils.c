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
#include <linux/reboot.h>
#include <sys/syscall.h>

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

// Read the battery current (9999 = failure, way above anything possible)
int utils_read_battery_current() {
    // Open battery current file
    FILE* fd_battery_current = fopen("/sys/class/power_supply/battery/current_now", "r");
    if (!fd_battery_current) {
        printf("Could not open battery current (/sys/class/power_supply/battery/current_now).\n");
        return 9999;
    }

    // Read battery current
    char buf_battery_current[8];
    if (fgets(buf_battery_current, sizeof(buf_battery_current), fd_battery_current) == NULL) {
        printf("Could not read battery current.\n");
        fclose(fd_battery_current);
        return 9999;
    }
    fclose(fd_battery_current);

    // Return the battery current (Convert the string to an integer)
    return atoi(buf_battery_current);
}

// Read whether the wireless charger is connected or not (0 = No, 1 = Yes, 2 = Failure), No print here due to also being called a _lot_
int utils_read_wireless_charger_connected() {
    FILE* fd_wireless_charger_online = fopen("/sys/class/power_supply/wireless/online", "r");
    if (!fd_wireless_charger_online) {
        printf("%s: Could not open /sys/class/power_supply/wireless/online.\n", __func__);
        return 2;
    }

    char buf_wireless_charger_online[3];
    if (fgets(buf_wireless_charger_online, sizeof(buf_wireless_charger_online), fd_wireless_charger_online) == NULL) {
        printf("%s: Could not read /sys/class/power_supply/wireless/online.\n", __func__);
        fclose(fd_wireless_charger_online);
        return 2;
    }
    fclose(fd_wireless_charger_online);

    return atoi(buf_wireless_charger_online);
}

// Reorder ARGB to ABGR for framebuffer
void utils_reorder_color(unsigned char* data, int width, int height, int stride) {
    for (int y = 0; y < height; ++y) {
        unsigned int* pixel = (unsigned int*)(data + y * stride);
        for (int x = 0; x < width; ++x) {
            unsigned int argb = pixel[x];
            unsigned char a = (argb >> 24) & 0xff;
            unsigned char r = (argb >> 16) & 0xff;
            unsigned char g = (argb >> 8) & 0xff;
            unsigned char b = argb & 0xff;
            pixel[x] = (a << 24) | (b << 16) | (g << 8) | r;
        }
    }
}

// Shutdown the system using a syscall
void utils_syscall_shutdown() {
    printf("%s: +\n", __func__);
    syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_POWER_OFF, NULL);
}

// Reboot the system using a syscall
void utils_syscall_reboot() {
    printf("%s: +\n", __func__);
    syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART, NULL);
}
