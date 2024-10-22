#ifndef _UTILS_H
#define _UTILS_H

#define degToRad(angleInDegrees) ((angleInDegrees) * M_PI / 180.0)
int utils_check_bootmode();
int utils_allow_exynos_fb_write();
int utils_read_battery_percentage();

#endif
