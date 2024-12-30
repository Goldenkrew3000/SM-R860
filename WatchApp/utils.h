#ifndef _UTILS_H
#define _UTILS_H

#define degToRad(angleInDegrees) ((angleInDegrees) * M_PI / 180.0)
int utils_check_bootmode();
int utils_allow_exynos_fb_write();
int utils_read_battery_percentage();
int utils_read_battery_current();
int utils_read_wireless_charger_connected();
void utils_reorder_color(unsigned char* data, int width, int height, int stride);
void utils_syscall_shutdown();
void utils_syscall_reboot();

#endif
